# Logos-SIMD (Fortnight 2)

Goal: SIMD-accelerated (ARM NEON) JSON/CSV parsing with high throughput. (Project Logos Fortnight 2)

## Build
- Normal: cmake -S . -B build && cmake --build build -j
- ASan: cmake -S . -B build-asan -DLOGOS_ASAN=ON && cmake --build build-asan -j
- TSan: cmake -S . -B build-tsan -DLOGOS_TSAN=ON && cmake --build build-tsan -j

## Benchmarks
TODO: Add nanosecond-precision benchmarks (required).

