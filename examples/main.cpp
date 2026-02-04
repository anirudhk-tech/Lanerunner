#include "lanerunner/runner.hpp"

#include <fstream>
#include <vector>
#include <iterator>
#include <cstdint>
#include <iostream>
#include <chrono>

static uint64_t baseline_count_newlines(const uint8_t* p, size_t n) {
  uint64_t c = 0;
  for (size_t i = 0; i < n; ++i) c += (p[i] == '\n');
  return c;
}

int main(int argc, char** argv) {
  const char* path = (argc > 1) ? argv[1] : "input.csv";

  std::ifstream in(path, std::ios::binary);
  if (!in) { std::cerr << "open failed\n"; return 1; }

  std::vector<uint8_t> data{
    std::istreambuf_iterator<char>(in),
    std::istreambuf_iterator<char>()
  };

  lanerunner::Runner runner;

  constexpr int iters = 100;

  volatile uint64_t sink = 0;

  sink ^= baseline_count_newlines(data.data(), data.size());
  runner.parse_buffer(data.data(), data.size());

  auto t0 = std::chrono::steady_clock::now();
  for (int k = 0; k < iters; ++k) sink ^= baseline_count_newlines(data.data(), data.size());
  auto t1 = std::chrono::steady_clock::now();

  auto t2 = std::chrono::steady_clock::now();
  for (int k = 0; k < iters; ++k) runner.parse_buffer(data.data(), data.size());
  auto t3 = std::chrono::steady_clock::now();

  auto base_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count(); 
  auto simd_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t3 - t2).count();

  std::cout << "bytes " << data.size() << " iters " << iters << "\n";
  std::cout << "baseline_ns_total " << base_ns << " baseline_ns_iter " << (base_ns / iters) << "\n";
  std::cout << "runner_ns_total " << simd_ns << " runner_ns_iter " << (simd_ns / iters) << "\n";
  std::cout << "sink " << sink << "\n";

  return 0;
}

