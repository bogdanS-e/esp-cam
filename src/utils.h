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

const char *frameSizeToString(framesize_t size) {
  switch (size) {
  case FRAMESIZE_96X96:
    return "FRAMESIZE_96X96";
  case FRAMESIZE_QQVGA:
    return "FRAMESIZE_QQVGA";
  case FRAMESIZE_QCIF:
    return "FRAMESIZE_QCIF";
  case FRAMESIZE_HQVGA:
    return "FRAMESIZE_HQVGA";
  case FRAMESIZE_240X240:
    return "FRAMESIZE_240X240";
  case FRAMESIZE_QVGA:
    return "FRAMESIZE_QVGA";
  case FRAMESIZE_CIF:
    return "FRAMESIZE_CIF";
  case FRAMESIZE_HVGA:
    return "FRAMESIZE_HVGA";
  case FRAMESIZE_VGA:
    return "FRAMESIZE_VGA";
  case FRAMESIZE_SVGA:
    return "FRAMESIZE_SVGA";
  case FRAMESIZE_XGA:
    return "FRAMESIZE_XGA";
  case FRAMESIZE_HD:
    return "FRAMESIZE_HD";
  case FRAMESIZE_SXGA:
    return "FRAMESIZE_SXGA";
  case FRAMESIZE_UXGA:
    return "FRAMESIZE_UXGA";
  case FRAMESIZE_FHD:
    return "FRAMESIZE_FHD";
  case FRAMESIZE_P_HD:
    return "FRAMESIZE_P_HD";
  case FRAMESIZE_P_3MP:
    return "FRAMESIZE_P_3MP";
  case FRAMESIZE_QXGA:
    return "FRAMESIZE_QXGA";
  case FRAMESIZE_QHD:
    return "FRAMESIZE_QHD";
  case FRAMESIZE_WQXGA:
    return "FRAMESIZE_WQXGA";
  case FRAMESIZE_P_FHD:
    return "FRAMESIZE_P_FHD";
  case FRAMESIZE_QSXGA:
    return "FRAMESIZE_QSXGA";
  default:
    return "UNKNOWN";
  }
}

framesize_t stringToFrameSize(const char *name) {
  if (!name)
    return FRAMESIZE_INVALID;

  if (strcmp(name, "FRAMESIZE_96X96") == 0)
    return FRAMESIZE_96X96;
  if (strcmp(name, "FRAMESIZE_QQVGA") == 0)
    return FRAMESIZE_QQVGA;
  if (strcmp(name, "FRAMESIZE_QCIF") == 0)
    return FRAMESIZE_QCIF;
  if (strcmp(name, "FRAMESIZE_HQVGA") == 0)
    return FRAMESIZE_HQVGA;
  if (strcmp(name, "FRAMESIZE_240X240") == 0)
    return FRAMESIZE_240X240;
  if (strcmp(name, "FRAMESIZE_QVGA") == 0)
    return FRAMESIZE_QVGA;
  if (strcmp(name, "FRAMESIZE_CIF") == 0)
    return FRAMESIZE_CIF;
  if (strcmp(name, "FRAMESIZE_HVGA") == 0)
    return FRAMESIZE_HVGA;
  if (strcmp(name, "FRAMESIZE_VGA") == 0)
    return FRAMESIZE_VGA;
  if (strcmp(name, "FRAMESIZE_SVGA") == 0)
    return FRAMESIZE_SVGA;
  if (strcmp(name, "FRAMESIZE_XGA") == 0)
    return FRAMESIZE_XGA;
  if (strcmp(name, "FRAMESIZE_HD") == 0)
    return FRAMESIZE_HD;
  if (strcmp(name, "FRAMESIZE_SXGA") == 0)
    return FRAMESIZE_SXGA;
  if (strcmp(name, "FRAMESIZE_UXGA") == 0)
    return FRAMESIZE_UXGA;
  if (strcmp(name, "FRAMESIZE_FHD") == 0)
    return FRAMESIZE_FHD;
  if (strcmp(name, "FRAMESIZE_P_HD") == 0)
    return FRAMESIZE_P_HD;
  if (strcmp(name, "FRAMESIZE_P_3MP") == 0)
    return FRAMESIZE_P_3MP;
  if (strcmp(name, "FRAMESIZE_QXGA") == 0)
    return FRAMESIZE_QXGA;
  if (strcmp(name, "FRAMESIZE_QHD") == 0)
    return FRAMESIZE_QHD;
  if (strcmp(name, "FRAMESIZE_WQXGA") == 0)
    return FRAMESIZE_WQXGA;
  if (strcmp(name, "FRAMESIZE_P_FHD") == 0)
    return FRAMESIZE_P_FHD;
  if (strcmp(name, "FRAMESIZE_QSXGA") == 0)
    return FRAMESIZE_QSXGA;

  return FRAMESIZE_INVALID; // если не нашли совпадение
}

int getClientRSSI() {
  wifi_sta_list_t wifi_sta_list;
  esp_wifi_ap_get_sta_list(&wifi_sta_list);

  if (wifi_sta_list.num == 0) {
    return 0;
  }

  return wifi_sta_list.sta[0].rssi;
}

void blink(int pin, int count, int delayMs = 200) {
  for (int i = 0; i < count; i++) {
    digitalWrite(pin, LOW);
    delay(delayMs);
    digitalWrite(pin, HIGH);
      delay(delayMs);
  }
}