#include "lanerunner/runner.hpp"

#include <arm_neon.h>
#include <cstddef>

namespace lanerunner {

Runner::Runner () {};

size_t Runner::scan_buffer (const uint8_t* p, size_t n, char target) {
  const uint8_t* end = p + n;
  size_t count = 0;

  uint8x16_t target_v = vdupq_n_u8(static_cast<uint8_t>(target)); // vector duplicate quadword immediate scalar (n) into 8 unsigned lanes

  while((size_t)(end - p) >= 64) {
    uint8x16_t first_block = vld1q_u8(p);
    uint8x16_t second_block = vld1q_u8(p + 16);
    uint8x16_t third_block = vld1q_u8(p + 32);
    uint8x16_t fourth_block = vld1q_u8(p + 48);

    uint8x16_t eq1 = vceqq_u8(first_block, target_v);
    uint8x16_t eq2 = vceqq_u8(second_block, target_v);
    uint8x16_t eq3 = vceqq_u8(third_block, target_v);
  uint8x16_t eq4 = vceqq_u8(fourth_block, target_v);

    uint8x16_t ones1 = vshrq_n_u8(eq1, 7);
    uint8x16_t ones2 = vshrq_n_u8(eq2, 7);
    uint8x16_t ones3 = vshrq_n_u8(eq3, 7);
    uint8x16_t ones4 = vshrq_n_u8(eq4, 7);

    uint32x4_t sum1 = vpaddlq_u8(ones1); // NEON only supports adjacent operations
    uint32x4_t sum2 = vpaddlq_u8(ones2);
    uint32x4_t sum3 = vpaddlq_u8(ones3);
    uint32x4_t sum4 = vpaddlq_u8(ones4);

    uint32x4_t total = {};
    total = vaddq_u32(sum1, sum2);
    total = vaddq_u32(total, sum3);
    total = vaddq_u32(total, sum4);

    count += static_cast<uint8_t>(vgetq_lane_u32(total, 0));

    p += 64;
  }

  // cleanup for tail, negligible (don't need SIMD)
  while (p < end) {
    if (*p == target) {
      count += 1;
    }

    p++;
  }

  return count;
}

} // namespace lanerunner
