# LaneRunner

A high-performance CSV parser leveraging ARM NEON SIMD instructions for Apple Silicon. Achieves **1.9x speedup** over scalar parsing through vectorized structural character detection.

## Performance

| Method | Throughput | Relative |
|--------|-----------|----------|
| Scalar baseline | 1.0x | — |
| **LaneRunner (NEON)** | **1.9x** | +90% faster |

*Benchmarked on Apple Silicon with 100 iterations over real-world CSV data.*

## How It Works

LaneRunner processes CSV data 64 bytes at a time using four 128-bit NEON vector registers. The algorithm identifies structural characters (`,`, `\n`, `"`) in parallel across all 64 bytes simultaneously, then collapses results into 64-bit bitmasks for fast iteration.

### Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                     64-byte input chunk                         │
├───────────────┬───────────────┬───────────────┬─────────────────┤
│   16 bytes    │   16 bytes    │   16 bytes    │    16 bytes     │
│   (uint8x16)  │   (uint8x16)  │   (uint8x16)  │    (uint8x16)   │
└───────┬───────┴───────┬───────┴───────┬───────┴────────┬────────┘
        │               │               │                │
        ▼               ▼               ▼                ▼
   ┌─────────┐    ┌─────────┐    ┌─────────┐     ┌─────────┐
   │  vceqq  │    │  vceqq  │    │  vceqq  │     │  vceqq  │
   │ (SIMD)  │    │ (SIMD)  │    │ (SIMD)  │     │ (SIMD)  │
   └────┬────┘    └────┬────┘    └────┬────┘     └────┬────┘
        │               │               │                │
        ▼               ▼               ▼                ▼
   ┌─────────┐    ┌─────────┐    ┌─────────┐     ┌─────────┐
   │ 16-bit  │    │ 16-bit  │    │ 16-bit  │     │ 16-bit  │
   │  mask   │    │  mask   │    │  mask   │     │  mask   │
   └────┬────┘    └────┬────┘    └────┬────┘     └────┬────┘
        │               │               │                │
        └───────────────┴───────┬───────┴────────────────┘
                                │
                                ▼
                    ┌───────────────────────┐
                    │   64-bit bitmask      │
                    │ (comma | newline |    │
                    │        quote)         │
                    └───────────┬───────────┘
                                │
                                ▼
                    ┌───────────────────────┐
                    │  Quote-aware masking  │
                    │   (XOR prefix sum)    │
                    └───────────┬───────────┘
                                │
                                ▼
                    ┌───────────────────────┐
                    │   Clean structural    │
                    │      positions        │
                    └───────────────────────┘
```

### Key Techniques

**Vectorized Character Detection**  
Uses `vceqq_u8` to compare 16 bytes against delimiter characters simultaneously, producing match vectors that are then collapsed into bitmasks.

**Efficient Bitmask Extraction**  
The `mask16_from_eq` function converts NEON comparison results to scalar bitmasks using weighted multiplication and horizontal adds (`vpaddl_u8` → `vpaddl_u16` → `vpaddl_u32`).

**Quote-Aware Parsing**  
Implements a carry-less multiply / XOR prefix sum to track quote state across 64-byte boundaries. This ensures delimiters inside quoted fields are correctly ignored:

```cpp
// XOR prefix sum - propagates quote state through the bitmask
mask ^= mask << 1;
mask ^= mask << 2;
mask ^= mask << 4;
mask ^= mask << 8;
mask ^= mask << 16;
mask ^= mask << 32;
```

**Branchless Iteration**  
Uses `__builtin_ctzll` (count trailing zeros) to find set bits without branching, and clears them with `bits &= (bits - 1)`.

## Building

### Requirements
- Apple Silicon Mac (M1/M2/M3/M4)
- CMake 3.20+
- C++17 compiler (Clang/Apple Clang)

### Standard Build

```bash
cmake -S . -B build
cmake --build build -j
```

### With Sanitizers

```bash
# AddressSanitizer
cmake -S . -B build-asan -DLANE_ASAN=ON
cmake --build build-asan -j

# ThreadSanitizer
cmake -S . -B build-tsan -DLANE_TSAN=ON
cmake --build build-tsan -j
```

## Usage

### Running the Benchmark

```bash
./build/lanerunner_bench input.csv
```

### Library Integration

```cpp
#include <lanerunner/runner.hpp>

// Load your CSV data into a buffer
std::vector<uint8_t> data = load_file("data.csv");

// Parse with SIMD acceleration
lanerunner::Runner runner;
runner.parse_buffer(data.data(), data.size());
```

### CMake Integration

```cmake
add_subdirectory(lanerunner)
target_link_libraries(your_target PRIVATE lanerunner_lib)
```

## Project Structure

```
lanerunner/
├── include/
│   └── lanerunner/
│       └── runner.hpp      # Public API
├── src/
│   └── runner.cpp          # NEON SIMD implementation
├── examples/
│   └── main.cpp            # Benchmark harness
└── CMakeLists.txt
```

## Technical Details

| Aspect | Implementation |
|--------|---------------|
| SIMD Width | 128-bit (NEON) |
| Chunk Size | 64 bytes/iteration |
| Registers Used | 4× uint8x16_t per chunk |
| Structural Chars | `,` `\n` `"` |
| Quote Handling | XOR prefix sum with carry |
| Compiler Flags | `-O3 -Wall -Wextra -Wpedantic` |

## References

- [simdjson](https://github.com/simdjson/simdjson) — Inspiration for SIMD parsing techniques
- [ARM NEON Intrinsics Reference](https://developer.arm.com/architectures/instruction-sets/intrinsics/)
- Langdale & Lemire, *"Parsing Gigabytes of JSON per Second"* (2019)

## License

MIT
