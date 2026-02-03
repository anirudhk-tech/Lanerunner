#pragma once

#include <arm_neon.h>
#include <cmath>
#include <cstddef>

namespace lanerunner {

class Runner {
public:
  Runner();

  void parse_buffer(const uint8_t* data, size_t len);
};

}
