#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <list>

#define _DEBUG_MAIN
// #define _DEBUG_PRINT

struct _coco_memory_data {
  uint64_t orig_ptr;
  uint64_t start_ptr;
  uint64_t end_ptr;
  size_t size;
  bool allocated = false;
};

using namespace std;

unordered_map<uint64_t, struct _coco_memory_data> memory_locations;
unordered_map<string, list<uint64_t>> function_stack;

// Defines the size of the buffer area surrounding the allocated
// buffer.
constexpr uint8_t _coco_buffer_size = 32;
// please mr. pooh, it is all I want.
constexpr uint8_t _coco_guard_phrase[32] = {
    0xB1, 0x6B, 0x00, 0xB5, 0xB0, 0xBA, 0xBA, 0xBE, 0xB1, 0x6B, 0x00,
    0xB5, 0xB0, 0xBA, 0xBA, 0xBE, 0xB1, 0x6B, 0x00, 0xB5, 0xB0, 0xBA,
    0xBA, 0xBE, 0xB1, 0x6B, 0x00, 0xB5, 0xB0, 0xBA, 0xBA, 0xBE};

namespace {
// You can write anything here and it will be invisible to the outside as it

}

extern "C" {

void _coco_check_load_store_ptr(const uint64_t base_ptr) {
  const uint64_t target_ptr = base_ptr;
  #ifdef _DEBUG_PRINT
  fprintf(stderr, "Runtime check: %lu, %lu\n", base_ptr, target_ptr);
  #endif
  const struct _coco_memory_data *memory = NULL;

  for (const std::pair<const uint64_t, struct _coco_memory_data> &Loc :
       memory_locations) {
    #ifdef _DEBUG_PRINT
    fprintf(stderr, "%lu >- %lu && %lu <= %lu = %s\n", base_ptr,
           Loc.second.orig_ptr, base_ptr, Loc.second.end_ptr,
           (base_ptr >= Loc.second.orig_ptr && base_ptr <= Loc.second.end_ptr)
               ? "true"
               : "false");
    #endif
    if (base_ptr >= Loc.second.orig_ptr && base_ptr <= Loc.second.end_ptr) {
      memory = &Loc.second;
    }
  }

  if (memory == NULL) {
    #ifdef _DEBUG_PRINT
    fprintf(stderr, "Memory %lu not found\n", target_ptr);
    #endif
    return;
  } else {
    #ifdef _DEBUG_PRINT
    fprintf(stderr, "Memory found: %lu - %lu - %lu\n", memory->start_ptr,
            memory->end_ptr, memory->size);
    #endif
  }

  if (!memory->allocated) return;

// Target is the pointer we want, if it is higher than the start of red zone,
// it will becomem a number greater than > _coco_buffer_size. If it is lower
// than our redzone, it will overflow into a large number
    #ifdef _DEBUG_PRINT
    fprintf(stderr, "Check pointers: %lu - %lu\n", target_ptr - memory->orig_ptr,
          memory->end_ptr - target_ptr);
    #endif

  if ((target_ptr - memory->orig_ptr) < _coco_buffer_size) exit(66);
  if ((memory->end_ptr - target_ptr) <= _coco_buffer_size) exit(66);
}

void releaseAllocatedMemory(char* fname_ptr) {
  // The operation AllocInst itself is undefined if there is insufficient stack space for the allocation.
  // ’Alloca’d memory is automatically released when the function returns.
  // Means we need to drop the memoryLocations after the return (after CallInst).
  string fname = fname_ptr;
  if (function_stack.find(fname) == function_stack.end()) return;;

  const auto stack_list = function_stack[fname];

  // Mimic drops.
  function_stack[fname].clear();
}

void _coco_free(void *ptr) {
  struct _coco_memory_data &memory = memory_locations[(uint64_t)ptr];
  memory.allocated = false;
  memset((void *)memory.orig_ptr, 0, _coco_buffer_size);
  free((void *)memory.orig_ptr);
}

void *_coco_alloc(const uint64_t ptr, size_t size) {
  memcpy(reinterpret_cast<uint8_t *>(ptr), _coco_guard_phrase, _coco_buffer_size);
  memcpy(reinterpret_cast<uint8_t *>(ptr) + _coco_buffer_size + size, _coco_guard_phrase,
         _coco_buffer_size);

#ifdef _DEBUG_PRINT
  fprintf(stderr, "adding ptr: %lu to %lu\n", ptr,
          ptr + _coco_buffer_size * 2 + size);
#endif

  struct _coco_memory_data memory = {
      .orig_ptr = ptr,
      .start_ptr = ptr + _coco_buffer_size,
      .end_ptr = ptr + _coco_buffer_size * 2 + size,
      .size = size,
      .allocated = true,
  };

  memory_locations[memory.start_ptr] = memory;
  return reinterpret_cast<void *>(memory.start_ptr);
}

// Initialize the memory space with the buffer guard.
void *_coco_stack_alloc(char* fnamePtr, const uint64_t ptr, size_t size) {
  string fname = fnamePtr;

  void* ret = _coco_alloc(ptr, size);

  // store the allocated address into the function stack list so that we can release them later.
  function_stack[fname].push_back((uint64_t) ret);

  return ret;
}

void *_coco_malloc(const size_t size) {
  const uint64_t ptr = reinterpret_cast<uint64_t>(malloc(size + _coco_buffer_size * 2));

  return _coco_alloc(ptr, size);
}
// Declare your custom malloc/free functions in this scope.
}

#ifdef _DEBUG_MAIN
int main(void) {
  char *ptr = (char *)_coco_malloc(128);

  if (memcmp(ptr - _coco_buffer_size, _coco_guard_phrase, _coco_buffer_size) ==
      0)
    puts("Guard start phrase intact");
  else
    puts("Guard start phrase broken");

  if (memcmp(ptr + 128, _coco_guard_phrase, _coco_buffer_size) == 0)
    puts("Guard end phrase intact");
  else
    puts("Guard end phrase broken");

  strncpy(ptr, "winnie the pooh", 15);
  if (strncmp(ptr, "winnie the pooh", 15) == 0)
    puts("Copied string correctly");
  else
    puts("Copied string incorrectly");

  if (memcmp(ptr - _coco_buffer_size, _coco_guard_phrase, _coco_buffer_size) ==
      0)
    puts("Guard start phrase intact");
  else
    puts("Guard start phrase broken");
  if (memcmp(ptr + 128, _coco_guard_phrase, _coco_buffer_size) == 0)
    puts("Guard end phrase intact");
  else
    puts("Guard end phrase broken");

  _coco_free(ptr);
}
#endif
