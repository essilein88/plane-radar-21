#pragma once

#include <cstdint>

namespace hardware::touch {

struct Point {
  uint16_t x;
  uint16_t y;
};

void init();

/** Returns true once for each new touch-down edge. */
bool readTap(Point* out);

}  // namespace hardware::touch
