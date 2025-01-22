#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#define _DEBUG_MAIN

struct _coco_memory_data {
  uint64_t orig_ptr;
  uint64_t start_ptr;
  uint64_t end_ptr;
  size_t size;
  bool allocated = false;
};

std::unordered_map<uint64_t, struct _coco_memory_data> memory_locations;

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

void _coco_check_load_store_ptr(const uint64_t base_ptr,
                                const uint64_t offset) {
  const uint64_t target_ptr = base_ptr + offset;
  fprintf(stderr, "Runtime check: %lu, %lu, %lu\n", base_ptr, offset,
          target_ptr);
  const struct _coco_memory_data &memory = memory_locations[target_ptr];
  // if (!memory.allocated) return;
  fprintf(stderr, "Data: %lu - %lu\n", offset, memory.size);

  // Target is the pointer we want, if it is higher than the start of red zone,
  // it will becomem a number greater than > _coco_buffer_size. If it is lower
  // than our redzone, it will overflow into a large number

  if ((target_ptr - memory.orig_ptr) < _coco_buffer_size) exit(66);

  if ((target_ptr - memory.end_ptr) < _coco_buffer_size) exit(66);
}

void _coco_free(void *ptr) {
  struct _coco_memory_data &memory = memory_locations[(uint64_t)ptr];
  memory.allocated = false;
  memset((void *)memory.orig_ptr, 0, _coco_buffer_size);
  free((void *)memory.orig_ptr);
}

void *_coco_malloc(size_t size) {
  uint64_t ptr = (uint64_t)malloc(size + _coco_buffer_size * 2);
  memcpy((uint8_t *)ptr, _coco_guard_phrase, _coco_buffer_size);
  memcpy((uint8_t *)ptr + _coco_buffer_size + size, _coco_guard_phrase,
         _coco_buffer_size);

  fprintf(stderr, "adding ptr: %lu to %lu\n", ptr,
          ptr + _coco_buffer_size * 2 + size);
  struct _coco_memory_data memory = {
      .orig_ptr = ptr,
      .start_ptr = ptr + _coco_buffer_size,
      .end_ptr = ptr + _coco_buffer_size + size,
      .size = size,
      .allocated = true,
  };

  memory_locations[memory.start_ptr] = memory;
  return (void *)memory.start_ptr;
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
