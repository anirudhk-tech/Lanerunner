#include <arm_neon.h>
#include <cstdint>
#include <cstdio>

int main () {
  alignas(16) uint8_t data[16] = {
    'a','b',',','c','d','\n','e','f',
    'g',',','h','i','j','k','l','\n'
  };

  uint8x16_t v = vld1q_u8(data);

  uint8x16_t comma = vdupq_n_u8(static_cast<uint8_t>(','));

  uint8x16_t eq = vceqq_u8(v, comma);

  uint8x16_t ones = vshrq_n_u8(eq, 7);

  alignas(16) uint8_t out[16];
  vst1q_u8(out, ones);

  std::printf("Comma hits: ");
  for (int i = 0; i < 16; i++) std::printf("%u", (unsigned)out[i]);
  std::printf("\n");

  return 0;
}
