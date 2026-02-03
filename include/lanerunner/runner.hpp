#pragma once

#include <arm_neon.h>
#include <cstddef>

namespace lanerunner {

class Runner {
public:
  Runner();

  size_t scan_buffer (const uint8_t* p, size_t n, char target);

};

}
