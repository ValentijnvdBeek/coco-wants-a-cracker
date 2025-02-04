## How to Check the Result

1. Pull the Docker image that contains Clang-10.

2. Mount the

   ```
   example.cpp
   ```

   file into the Docker directory and compile it using:

   ```sh
   clang -emit-llvm example.cpp -o example.bc
   clang -emit-llvm -S -O0 random_registers.cpp -o random_registers.ll
   clang -emit-llvm -S -O0 retpoline.cpp -o retlopine.ll
   ```

   Then, copy

   ```
   example.bc
   random_registers.ll
   retpoline.ll   
   ```

   out of the Docker container. A variant generated using godbolt at -O0 on LLVM 10.1 is included for your convience. The `00` is to ensure that no optimizations are done at the frontend, but it is not required to run the code succesfully.

3. Run:

   ```sh
   echo -e "==========\nInsert NOOP\n=========="

   # Generate a version with random NOOP operations in it
   ./build/bin/llc -num_noop=<num> ./example.bc -o example.s
   ./build/bin/llc ./example.bc -o example_no_nop.s

   echo "!!!!OUTPUT!!!!"
   # Report the number of NOP operations inside of the code
   cat example.s | grep nop | wc -l 
   cat example_no_nop.s | grep nop | wc -l 

   echo -e "==========\nRandom Registers\n=========="
   # Generate two runs using the Coco and Basic allocators
   ./build/bin/llc -regalloc=coco random_registers.ll -o coco_run_1.s
   ./build/bin/llc -regalloc=coco random_registers.ll -o coco_run_2.s
   ./build/bin/llc -regalloc=basic random_registers.ll -o basic_run_1.s
   ./build/bin/llc -regalloc=basic random_registers.ll -o basic_run_2.s

   # Notice how the registers are different 
   echo "!!!!OUTPUT!!!!"
   diff -q basic_run_1.s basic_run_2.s
   diff -q coco_run_1.s coco_run_2.s

   # Build a retpolined program. run and show the calls 
   # implementation
   echo -e "==========\nRetpoline\n=========="
   ./build/bin/llc retpoline.ll -o retpoline.s    
   clang retpoline.s -o retpoline
   echo "!!!!CALLS!!!!"
   cat retpoline.s | grep -e "callq.*\.L" -A11
   echo "!!!!OUTPUT!!!!"
   ./retpoline 
   ```

## Implementation Details
In this section, a brief explanation of the implementation for each of the three assignments is given. These are in order as given in the assignment text.

### Random register allocation
As requested by the assignment text is most of the code a copy of the basic register allocator already inside of LLVM. The only difference is in the `RACoco::selectOrSplit` function call. This decides whether a register can be allocated and to which physical register it ought to go. 

Our code adds a random number generation, in our case the one defined by C++ since the provided LLVM snippet didn't work, and then shuffles the standard AllocationOrder. This randomly shifts the order that the registers are allocated in and ensures that each run gives a different register. 

To doubly make sure, the code also suffles the registers that are spill candidates, but this shouldn't be necessary in most cases.


### NOP insertion
For many instruction sets, `insertNoop` is not implemented, so we need to implement our own version using `getNoop` from `TargetInstrInfo` to retrieve the no-op opcode for different architectures.

However, some architectures require an immediate operand (e.g., `imm(0)`) as part of the no-op, such as AArch64. Therefore, our implementation is not entirely target-independent across all architectures. However, it should at least work for MIPS, ARM, and x86.

After crafting the target-independent no-op, we can insert a random number of them before the function's entry point and `ret` instruction, as well as before and after call sites.

### Retpoline
Retpoline in implemented in a new `MachineFunctionPass` inside of the file `lib/x86/X86RetPolinePass.cpp`. The algorithm used is a relatively simply one that fetches the terminator of the basic block, which should always be a call/jump/return, and checks if it is a return (required by the assignment text). This is sufficient since the `ret` instruction is a terminator and cannot occur be earlier in the basic block[1].

It will then add two new sections onto the Machine Function. The first is the `LoopGuard` which traps the speculative execution inside of an infinite loop. Our second block isn the `ReturnNew` block which we call into, sets the stack pointer to the old return, and then returns the original calling function. Since we are looping over the `MachineFunction` blocks, we register both of these pointers so that we don't iterate over them in our pass. 

Three things of note happen to those block: they are set as possible targets of indirect jumps, they are added as successors to the original `MachineBlock` so that the optimizer doesn't delete them and `LiveIn` information is added. Also, note that our implementation adds the `LFENCE` instructions after the `PAUSE`. This detail is taken from the comment left in `RetpolineThunkInserter::populateThunk` that is already in the codebase which indicates that the original retpoline doesn't work for AMD chips (which is what I am using now).

[1] https://llvm.org/docs/LangRef.html#terminator-instructions