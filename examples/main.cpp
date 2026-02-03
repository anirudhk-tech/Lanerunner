#include "lanerunner/runner.hpp"

#include <arm_neon.h>
#include <iostream>
#include <stdexcept>
#include <cstdint>
#include <cstdlib>

int main () {
  unsigned int seed;
  unsigned int length; 
  int perc;

  uint8_t constant = 'A';
  uint8_t target = 'O';

  try {
    std::cout << "Lanerunner.\n";
    std::cout << "Please enter a seed:\n";

    std::cin >> seed;

    std::cout << "How long should the string be?\n";

    std::cin >> length;

    std::cout << "What percentage should be the target?\n";
    
    std::cin >> perc;

    if (perc > 100) {
         throw std::invalid_argument("Percentage is greater than 100, exiting.");
    }

    uint8_t* buffer = new uint8_t[length];

    for (unsigned int i = 0; i < length; i++) {
      buffer[i] = (rand_r(&seed) % 100 < perc) ? target : constant;
    }

    lanerunner::Runner runner;

    size_t count = runner.scan_buffer(buffer, length, target);

    std::cout << "The target appeared " << count << " times!";

    delete[] buffer;

    return 0;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;

    return 1;
  }
}
