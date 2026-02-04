# LaneRunner

A high-performance SIMD-accelerated CSV parser for Apple Silicon.

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Installation](#installation)
- [Quick Start](#quick-start)
- [API Reference](#api-reference)
- [Performance](#performance)
- [Design](#design)
- [Limitations](#limitations)
- [Platform Support](#platform-support)
- [License](#license)

## Overview

LaneRunner is a CSV parser that leverages ARM NEON SIMD instructions to process 64 bytes per iteration. Instead of scanning characters one-by-one, it loads four 128-bit vector registers simultaneously, detects structural characters (`,`, `\n`, `"`) in parallel, and collapses results into 64-bit bitmasks for fast iteration.

This parsing strategy is ideal for:

- **Data pipelines** — high-throughput CSV ingestion
- **ETL workloads** — bulk processing of delimited files
- **Analytics engines** — fast structural indexing before parsing values
- **Log processors** — scanning large CSV exports

## Features

- **1.9x faster** — vectorized detection outperforms scalar byte-by-byte scanning
- **64 bytes/iteration** — processes 4× 128-bit NEON registers per loop
- **Quote-aware** — correctly handles delimiters inside quoted fields
- **Branchless iteration** — uses `ctzll` to find set bits without branching
- **Zero dependencies** — pure C++17, ARM intrinsics only
- **Modern CMake** — sanitizer support, compile commands export

## Installation

### CMake (FetchContent)

```cmake
include(FetchContent)
FetchContent_Declare(
  lanerunner
  GIT_REPOSITORY https://github.com/youruser/lanerunner.git
  GIT_TAG        main
)
FetchContent_MakeAvailable(lanerunner)

target_link_libraries(your_target PRIVATE lanerunner_lib)
```

### Manual

```bash
git clone https://github.com/youruser/lanerunner.git
cd lanerunner
cmake -S . -B build
cmake --build build -j
```

Link against `lanerunner_lib` and add `include/` to your include path.

## Quick Start

```cpp
#include "lanerunner/runner.hpp"
#include <fstream>
#include <vector>

int main() {
    // Load CSV into memory
    std::ifstream file("data.csv", std::ios::binary);
    std::vector<uint8_t> buffer(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    );

    // Parse with SIMD acceleration
    lanerunner::Runner runner;
    runner.parse_buffer(buffer.data(), buffer.size());
}
```

## API Reference

### `lanerunner::Runner`

The primary CSV parser class.

#### Constructor

```cpp
Runner();
```

Creates a parser instance. Initializes internal quote-tracking state.

#### `parse_buffer(data, len)`

```cpp
void parse_buffer(const uint8_t* data, size_t len);
```

Parses a CSV buffer using SIMD-accelerated structural detection. Processes 64 bytes per iteration, identifying row boundaries while respecting quoted fields.

| Parameter | Type | Description |
|-----------|------|-------------|
| `data` | `const uint8_t*` | Pointer to CSV data |
| `len` | `size_t` | Length of buffer in bytes |

---

### Internal Functions

These are implementation details but document the SIMD approach:

#### `scan64_csv_structural(p) -> Masks64`

Loads 64 bytes from `p` into four NEON registers, compares against `,`, `\n`, and `"`, returns three 64-bit bitmasks indicating positions of each character.

#### `mask16_from_eq(eq) -> uint16_t`

Converts a 16-byte NEON comparison result (`0x00`/`0xFF` per lane) into a 16-bit scalar bitmask using weighted multiplication and horizontal adds.

#### `compute_quote_mask(quote_bits) -> uint64_t`

Computes a bitmask where bits are set for positions inside quoted regions. Uses XOR prefix sum to propagate quote state, with carry across 64-byte boundaries.

## Performance

LaneRunner significantly outperforms scalar parsing for structural detection. The included benchmark (`examples/main.cpp`) runs 100 iterations over a CSV file:

| Method | Relative Speed | Description |
|--------|---------------|-------------|
| Scalar baseline | 1.0x | Byte-by-byte newline counting |
| **LaneRunner** | **1.9x** | SIMD structural detection |

*Results from Apple Silicon, Release build with `-O3`. Run `./build/lanerunner_bench input.csv` to measure on your system.*

> **Important:** Build with `-O3` optimization (default in CMakeLists.txt). Debug builds will not demonstrate SIMD benefits due to function call overhead.

### Why it's fast

1. **Parallel comparison** — `vceqq_u8` compares 16 bytes against a delimiter in one instruction
2. **Wide loads** — processes 64 bytes per iteration vs. 1 byte for scalar
3. **Branchless bit iteration** — `__builtin_ctzll` finds positions without branching
4. **Quote state via arithmetic** — XOR prefix sum avoids conditional logic

## Design

### SIMD Pipeline

```
┌─────────────────────────────────────────────────────────────────┐
│                     64-byte input chunk                         │
├───────────────┬───────────────┬───────────────┬─────────────────┤
│   16 bytes    │   16 bytes    │   16 bytes    │    16 bytes     │
│  uint8x16_t   │  uint8x16_t   │  uint8x16_t   │   uint8x16_t    │
└───────┬───────┴───────┬───────┴───────┬───────┴────────┬────────┘
        │               │               │                │
        ▼               ▼               ▼                ▼
    vceqq_u8        vceqq_u8        vceqq_u8         vceqq_u8
   (compare)       (compare)       (compare)        (compare)
        │               │               │                │
        ▼               ▼               ▼                ▼
   mask16_from_eq  mask16_from_eq  mask16_from_eq   mask16_from_eq
        │               │               │                │
        └───────────────┴───────┬───────┴────────────────┘
                                │
                                ▼
                    ┌───────────────────────┐
                    │  64-bit bitmasks      │
                    │  comma | newline |    │
                    │        quote          │
                    └───────────┬───────────┘
                                │
                                ▼
                    ┌───────────────────────┐
                    │  XOR prefix sum       │
                    │  (quote regions)      │
                    └───────────┬───────────┘
                                │
                                ▼
                    ┌───────────────────────┐
                    │  Masked newlines      │
                    │  (outside quotes)     │
                    └───────────────────────┘
```

### Bitmask Extraction

NEON lacks a direct `movemask` instruction (unlike x86 SSE/AVX). LaneRunner extracts bitmasks via weighted multiplication:

```cpp
// Shift comparison result (0x00 or 0xFF) to 0 or 1
uint8x16_t bits = vshrq_n_u8(eq, 7);

// Multiply by positional weights [1,2,4,8,16,32,64,128]
// Then horizontal add to collapse into scalar
uint16x4_t sum16 = vpaddl_u8(weighted);
uint32x2_t sum32 = vpaddl_u16(sum16);
uint64x1_t sum64 = vpaddl_u32(sum32);
```

### Quote-Aware Parsing

Delimiters inside quoted fields must be ignored. LaneRunner computes a "quote mask" where set bits indicate positions inside quotes:

```cpp
uint64_t mask = quote_bits;
mask ^= mask << 1;
mask ^= mask << 2;
mask ^= mask << 4;
mask ^= mask << 8;
mask ^= mask << 16;
mask ^= mask << 32;
```

This XOR prefix sum flips all bits after each quote character, creating regions of 1s inside quoted fields. A carry bit propagates state across 64-byte boundaries.

### Project Structure

```
lanerunner/
├── include/
│   └── lanerunner/
│       └── runner.hpp      # Public API
├── src/
│   └── runner.cpp          # NEON SIMD implementation
├── examples/
│   └── main.cpp            # Benchmark harness
└── CMakeLists.txt          # Build configuration
```

## Limitations

| Limitation | Explanation |
|------------|-------------|
| Apple Silicon only | Uses ARM NEON intrinsics; no x86 AVX/SSE backend |
| 64-byte alignment | Input length should be padded to 64 bytes for optimal performance |
| No value extraction | Detects structure only; does not parse field values |
| Not thread-safe | Single-threaded; external synchronization required for concurrent use |
| Global quote state | `prev_iter_inside_quote` is static; reset between files |

### When NOT to use LaneRunner

- You need x86/Windows support
- You need full CSV parsing with value extraction and type conversion
- Input files are smaller than a few KB (setup overhead dominates)

## Platform Support

| Platform | Architecture | Status |
|----------|--------------|--------|
| macOS | Apple Silicon (M1/M2/M3/M4) | ✅ Supported |
| Linux | ARM64 with NEON | ✅ Supported |
| macOS | Intel | ❌ Not supported |
| Windows | Any | ❌ Not supported |

### Requirements

- C++17 compiler (Apple Clang 12+, GCC 10+, Clang 10+)
- CMake 3.20+
- ARM64 processor with NEON

## Building

```bash
# Standard build
cmake -S . -B build
cmake --build build -j

# With AddressSanitizer
cmake -S . -B build-asan -DLANE_ASAN=ON
cmake --build build-asan -j

# With ThreadSanitizer
cmake -S . -B build-tsan -DLANE_TSAN=ON
cmake --build build-tsan -j

# Run benchmark
./build/lanerunner_bench input.csv
```

The CMake configuration enables `-O3 -Wall -Wextra -Wpedantic` by default.

## References

- [simdjson](https://github.com/simdjson/simdjson) — SIMD JSON parsing techniques
- [ARM NEON Intrinsics](https://developer.arm.com/architectures/instruction-sets/intrinsics/) — Official reference
- Langdale & Lemire, *"Parsing Gigabytes of JSON per Second"* (VLDB 2019)

## License

MIT License. See [LICENSE](LICENSE) for details.

---

**LaneRunner** - High-performance SIMD CSV parsing for Apple Silicon.
