#include "esp_camera.h"
#include <esp_timer.h>

inline uint64_t nowMs() {
  return esp_timer_get_time() / 1000ULL;
}

inline uint64_t elapsedSince(uint64_t time) {
  const uint64_t now = nowMs();
  int64_t delta = (int64_t)(now - time);

  if (delta < 0) {
    delta = 0;
  }

  return delta;
}