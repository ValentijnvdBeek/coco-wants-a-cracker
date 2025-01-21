#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#define _DEBUG_MAIN

// Defines the size of the buffer area surrounding the allocated
// buffer.
constexpr uint8_t _coco_buffer_size = 32;
// please mr. poo, it is all I want.
constexpr uint8_t _coco_guard_phrase[32] = {
    0xB1, 0x6B, 0x00, 0xB5, 0xB0, 0xBA, 0xBA, 0xBE, 0xB1, 0x6B, 0x00,
    0xB5, 0xB0, 0xBA, 0xBA, 0xBE, 0xB1, 0x6B, 0x00, 0xB5, 0xB0, 0xBA,
    0xBA, 0xBE, 0xB1, 0x6B, 0x00, 0xB5, 0xB0, 0xBA, 0xBA, 0xBE};

namespace {
// You can write anything here and it will be invisible to the outside as it

}

extern "C" {
void _coco_free(void *ptr) {
  memset(ptr, 0, _coco_buffer_size);
  free((uint8_t *)ptr - _coco_buffer_size);
}

void *_coco_malloc(size_t size) {
  void *ptr = malloc(size + _coco_buffer_size * 2);
  memcpy((uint8_t *)ptr, _coco_guard_phrase, _coco_buffer_size);
  memcpy((uint8_t *)ptr + _coco_buffer_size + size, _coco_guard_phrase,
         _coco_buffer_size);

  return (uint8_t *)ptr + _coco_buffer_size;
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
