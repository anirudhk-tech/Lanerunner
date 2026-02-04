#include "lanerunner/runner.hpp"

#include <arm_neon.h>
#include <cstddef>
#include <cstdint>

namespace {

struct Masks64 {
  uint64_t comma;
uint64_t nl;
  uint64_t quote;
};

static inline uint64_t mask64_from_4x16(uint16_t m0, uint16_t m1, uint16_t m2, uint16_t m3) {
  return (uint64_t)m0
    | ((uint64_t)m1 << 16)
    | ((uint64_t)m2 << 32)
    | ((uint64_t)m3 << 48);
}

static inline uint16_t mask16_from_eq(uint8x16_t eq) {
  uint8x16_t bits01 = vshrq_n_u8(eq, 7);

  static const uint8_t w_lo_arr[8] = {1, 2, 4, 8, 16, 32, 64, 128};
  static const uint8_t w_hi_arr[8] = {1, 2, 4, 8, 16, 32, 64, 128};

  uint8x8_t w_lo = vld1_u8(w_lo_arr);
  uint8x8_t w_hi = vld1_u8(w_hi_arr);

  uint8x8_t lo = vget_low_u8(bits01);
  uint8x8_t hi = vget_high_u8(bits01);

  uint8x8_t lo_weighted = vmul_u8(lo, w_lo);
  uint8x8_t hi_weighted = vmul_u8(hi, w_hi);

  uint16x4_t lo16 = vpaddl_u8(lo_weighted);
  uint16x4_t hi16 = vpaddl_u8(hi_weighted);
  uint32x2_t lo32 = vpaddl_u16(lo16);
  uint32x2_t hi32 = vpaddl_u16(hi16);
  uint64x1_t lo64 = vpaddl_u32(lo32);
  uint64x1_t hi64 = vpaddl_u32(hi32);

  uint8_t lo_mask = (uint8_t)vget_lane_u64(lo64, 0);
  uint8_t hi_mask = (uint8_t)vget_lane_u64(hi64, 0);

  return (uint16_t) lo_mask | ((uint16_t) hi_mask << 8);
}

static inline Masks64 scan64_csv_structural(const uint8_t* p) {
  uint8x16_t a0 = vld1q_u8(p);
  uint8x16_t a1 = vld1q_u8(p + 16);
  uint8x16_t a2 = vld1q_u8(p + 32);
  uint8x16_t a3 = vld1q_u8(p + 48);

  uint8x16_t comma_v = vdupq_n_u8((uint8_t)',');
  uint8x16_t nl_v = vdupq_n_u8((uint8_t)'\n');
  uint8x16_t quote_v = vdupq_n_u8((uint8_t)'"');

  uint16_t c0 = mask16_from_eq(vceqq_u8(a0, comma_v));
  uint16_t c1 = mask16_from_eq(vceqq_u8(a1, comma_v));
  uint16_t c2 = mask16_from_eq(vceqq_u8(a2, comma_v));
  uint16_t c3 = mask16_from_eq(vceqq_u8(a3, comma_v));

  uint16_t n0 = mask16_from_eq(vceqq_u8(a0, nl_v));
  uint16_t n1 = mask16_from_eq(vceqq_u8(a1, nl_v));
  uint16_t n2 = mask16_from_eq(vceqq_u8(a2, nl_v));
  uint16_t n3 = mask16_from_eq(vceqq_u8(a3, nl_v));

  uint16_t q0 = mask16_from_eq(vceqq_u8(a0, quote_v));
  uint16_t q1 = mask16_from_eq(vceqq_u8(a1, quote_v));
  uint16_t q2 = mask16_from_eq(vceqq_u8(a2, quote_v));
  uint16_t q3 = mask16_from_eq(vceqq_u8(a3, quote_v));

  Masks64 out;
  out.comma = mask64_from_4x16(c0, c1, c2, c3);
  out.nl = mask64_from_4x16(n0, n1, n2, n3);
  out.quote = mask64_from_4x16(q0, q1, q2, q3);

  return out;
}

uint64_t prev_iter_inside_quote = 0;

uint64_t compute_quote_mask(uint64_t quote_bits) {
  uint64_t mask = quote_bits;

  mask ^= mask << 1;
  mask ^= mask << 2;
  mask ^= mask << 4;
  mask ^= mask << 8;
  mask ^= mask << 16;
  mask ^= mask << 32;

  mask ^= prev_iter_inside_quote;
  
  prev_iter_inside_quote = (uint64_t)((int64_t)mask >> 63);

  return mask;
}

} // namespace anonymous

namespace lanerunner {

Runner::Runner () {};

void Runner::parse_buffer(const uint8_t* data, size_t len) {
    size_t row_start = 0; 

    for (size_t i = 0; i < len; i += 64) {
      Masks64 masks = scan64_csv_structural(data + i);
      
      uint64_t inside_quotes = compute_quote_mask(masks.quote);

      uint64_t clean_newlines = masks.nl & ~inside_quotes;
      uint64_t nl_bits = clean_newlines;

      while (nl_bits != 0) {
        int pos = __builtin_ctzll(nl_bits);
        size_t abs = i + static_cast<size_t>(pos);
        
        row_start = abs + 1;
        
        nl_bits &= (nl_bits - 1);
      }
    }
  }

} // namespace lanerunner
