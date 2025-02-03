## How to Check the Result

1. Pull the Docker image that contains Clang-10.

2. Mount the

   ```
   example.cpp
   ```

   file into the Docker directory and compile it using:

   ```sh
   clang -emit-llvm example.cpp -o example.bc
   ```

   Then, copy

   ```
   example.bc
   ```

   out of the Docker container.

3. Run:

   ```sh
   ./build/bin/llc -num_noop=<num> ./example.bc -o example.s
   ```

4. Check if no-ops are inserted.

* We have an `example.bc` file that can be used for direct checking. It is the `recursion.c.ll` file from `assign1`, which was converted to `.bc` using `llvm-as`.

## Implementation Details

For many instruction sets, `insertNoop` is not implemented, so we need to implement our own version using `getNoop` from `TargetInstrInfo` to retrieve the no-op opcode for different architectures.

However, some architectures require an immediate operand (e.g., `imm(0)`) as part of the no-op, such as AArch64. Therefore, our implementation is not entirely target-independent across all architectures. However, it should at least work for MIPS, ARM, and x86.

After crafting the target-independent no-op, we can insert a random number of them before the function's entry point and `ret` instruction, as well as before and after call sites.