#include "LittleFS.h"
#include "car.h"
#include "esp_camera.h"
#include "esp_http_server.h"
#include <WiFiManager.h>

bool isClientActive = false;
static httpd_handle_t stream_httpd = NULL;
static httpd_handle_t camera_httpd = NULL;
extern Car car;
extern WiFiManager wm;

// Send response via WebSocket
void sendResponse(httpd_req_t *req, const char *message) {
  if (!req || !message) {
    return;
  }

  httpd_ws_frame_t res;
  res.payload = (uint8_t *)message;
  res.len = strlen(message);
  res.type = HTTPD_WS_TYPE_TEXT;

  esp_err_t err = httpd_ws_send_frame(req, &res);

  Serial.printf("Send: %s\n", message);

  if (err != ESP_OK) {
    Serial.printf("Failed to send WS response: 0x%x\n", err);
  }
}

static esp_err_t serveStaticFile(httpd_req_t *req, const char *filepath) {
  String path = filepath;

  const char *type = "text/plain";
  if (path.endsWith(".html"))
    type = "text/html";
  else if (path.endsWith(".js"))
    type = "application/javascript";
  else if (path.endsWith(".css"))
    type = "text/css";
  else if (path.endsWith(".png"))
    type = "image/png";
  else if (path.endsWith(".jpg") || path.endsWith(".jpeg"))
    type = "image/jpeg";
  else if (path.endsWith(".ico"))
    type = "image/x-icon";

  File file = LittleFS.open(path, "r");
  if (!file) {
    Serial.printf("404 Not Found: %s\n", path.c_str());
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, type);

  char chunk[512];
  size_t chunksize;
  do {
    chunksize = file.readBytes(chunk, sizeof(chunk));
    if (chunksize > 0) {
      if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
        file.close();
        return ESP_FAIL;
      }
    }
  } while (chunksize > 0);

  file.close();
  httpd_resp_send_chunk(req, NULL, 0);
  return ESP_OK;
}

void handleCarCommand(const char *command, httpd_req_t *req) {
  Serial.printf("Command handler received: %s\n", command);

  if (strcmp(command, "toggleFlash") == 0) {
    car.toggleFlash();

    const char *response = car.getFlashState() ? "Flash-ON" : "Flash-OFF";
    sendResponse(req, response);

    return;
  }

  if (strncmp(command, "cameraDrag_", 11) == 0) {
    int x, y;

    if (sscanf(command + 11, "%d_%d", &x, &y) == 2) {
      car.setCameraX(x);
    }

    return;
  }

  if (strncmp(command, "frameSize_", 10) == 0) {
    const char *sizeName = command + 10;
    sensor_t *s = esp_camera_sensor_get();

    if (!s) {
      return;
    }

    framesize_t newSize = stringToFrameSize(sizeName);

    s->set_framesize(s, newSize);
    Serial.printf("✅ Frame size changed to %s\n", sizeName);

    return;
  }

  if (strcmp(command, "reset") == 0) {
    wm.resetSettings();
    ESP.restart();

    return;
  }

  if (strcmp(command, "ping") == 0) {
    int rssi = (WiFi.getMode() & WIFI_MODE_AP) ? getClientRSSI() : WiFi.RSSI();

    char response[32];

    snprintf(response, sizeof(response), "pong-%d", abs(rssi));
    sendResponse(req, response);

    return;
  }

  if (strcmp(command, "forward") == 0) {
    car.moveForward();

    return;
  }

  if (strcmp(command, "backward") == 0) {
    car.moveBackward();

    return;
  }

  if (strcmp(command, "left") == 0) {
    car.turnLeft();

    return;
  }

  if (strcmp(command, "right") == 0) {
    car.turnRight();

    return;
  }

  if (strcmp(command, "forward-left") == 0) {
    car.moveForwardLeft();

    return;
  }

  if (strcmp(command, "forward-right") == 0) {
    car.moveForwardRight();

    return;
  }

  if (strcmp(command, "backward-left") == 0) {
    car.moveBackwardLeft();

    return;
  }

  if (strcmp(command, "backward-right") == 0) {
    car.moveBackwardRight();

    return;
  }

  if (strcmp(command, "stop") == 0) {
    car.stop();

    return;
  }

  Serial.printf("Unknown command: %s\n", command);
}

static esp_err_t websocketHandler(httpd_req_t *req) {
  if (req->method == HTTP_GET) {
    Serial.println("WebSocket connection requested" + String(WiFi.status()));
    const char *message = car.getFlashState() ? "Flash-ON" : "Flash-OFF";

    sendResponse(req, message);
    vTaskDelay(300 / portTICK_PERIOD_MS);

    char wifiStatus[16];
    snprintf(wifiStatus, sizeof(wifiStatus), "WIFI-%d", WiFi.status() == WL_CONNECTED);
    sendResponse(req, wifiStatus);
    vTaskDelay(300 / portTICK_PERIOD_MS);

    sensor_t *s = esp_camera_sensor_get();
    if (s) {
      const char *frameSizeName = frameSizeToString(s->status.framesize);
      char frameMsg[64];
      snprintf(frameMsg, sizeof(frameMsg), "FRAMESIZE-%s", frameSizeName);
      sendResponse(req, frameMsg);
      Serial.printf("📸 Current frame size: %s\n", frameSizeName);
    } else {
      Serial.println("⚠️ Camera sensor not found!");
    }

    return ESP_OK;
  }

  httpd_ws_frame_t wsFrame;
  memset(&wsFrame, 0, sizeof(wsFrame));
  wsFrame.type = HTTPD_WS_TYPE_TEXT;

  esp_err_t ret = httpd_ws_recv_frame(req, &wsFrame, 0);
  if (ret != ESP_OK)
    return ret;

  if (!wsFrame.len)
    return ESP_OK;

  uint8_t *buffer = (uint8_t *)calloc(1, wsFrame.len + 1);
  if (!buffer)
    return ESP_ERR_NO_MEM;

  wsFrame.payload = buffer;
  ret = httpd_ws_recv_frame(req, &wsFrame, wsFrame.len);

  if (ret == ESP_OK) {
    buffer[wsFrame.len] = '\0';
    handleCarCommand((char *)buffer, req);
  }

  free(buffer);

  return ret;
}

static esp_err_t streamHandler(httpd_req_t *req) {
  if (isClientActive) {
    Serial.println("Stream rejected - client already active");
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }

  isClientActive = true;
  Serial.println("Stream started - client locked");

  camera_fb_t *frameBuffer = NULL;
  esp_err_t res = ESP_OK;
  size_t jpgBufferLength = 0;
  uint8_t *jpgBuffer = NULL;
  char part_buf[64];

  sensor_t *s = esp_camera_sensor_get();
  if (!s) {
    Serial.println("NO SENSOR DETECTED");
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }

  res = httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=frame");

  if (res != ESP_OK) {
    isClientActive = false;
    return res;
  }

  while (true) {
    frameBuffer = esp_camera_fb_get();
    if (!frameBuffer) {
      delay(100);
      continue;
    }

    if (frameBuffer->format != PIXFORMAT_JPEG) {
      bool convertedJpeg = frame2jpg(frameBuffer, 80, &jpgBuffer, &jpgBufferLength);

      esp_camera_fb_return(frameBuffer);
      frameBuffer = NULL;

      if (!convertedJpeg) {
        continue;
      }
    } else {
      jpgBufferLength = frameBuffer->len;
      jpgBuffer = frameBuffer->buf;
    }

    if (res == ESP_OK) {
      size_t headerLength = snprintf(part_buf, 64, "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", jpgBufferLength);
      res = httpd_resp_send_chunk(req, part_buf, headerLength);
    }

    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)jpgBuffer, jpgBufferLength);
    }

    if (frameBuffer) {
      esp_camera_fb_return(frameBuffer);
      jpgBuffer = NULL;
    } else if (jpgBuffer) {
      free(jpgBuffer);
      jpgBuffer = NULL;
    }

    if (res != ESP_OK) {
      break;
    }

    delay(50);
  }

  isClientActive = false;
  Serial.println("Stream ended - client unlocked");
  return res;
}

static esp_err_t capturePhotoHandler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  sensor_t *s = esp_camera_sensor_get();

  if (!s) {
    Serial.println("Camera sensor not found");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  fb = esp_camera_fb_get();

  if (!fb) {
    Serial.println("Camera capture failed");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  char ts[32];
  snprintf(ts, 32, "%lld.%06ld", fb->timestamp.tv_sec, fb->timestamp.tv_usec);
  httpd_resp_set_hdr(req, "X-Timestamp", (const char *)ts);

  if (fb->format == PIXFORMAT_JPEG) {
    res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    Serial.printf("VGA JPEG sent: %u bytes\n", fb->len);
  }

  esp_camera_fb_return(fb);
  return res;
}

static esp_err_t indexHandler(httpd_req_t *req) {
  const char *htmlToSend = isClientActive ? "/busy.min.html" : "/index.min.html";
  return serveStaticFile(req, htmlToSend);
}

static esp_err_t styleHandler(httpd_req_t *req) {
  return serveStaticFile(req, "/style.min.css");
}

static esp_err_t scriptHandler(httpd_req_t *req) {
  return serveStaticFile(req, "/script.min.js");
}

void startCarServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 82;
  config.ctrl_port = 32768;

  httpd_uri_t index_uri = {
      .uri = "/",
      .method = HTTP_GET,
      .handler = indexHandler,
      .user_ctx = NULL};
  httpd_uri_t ws_uri = {
      .uri = "/ws",
      .method = HTTP_GET,
      .handler = websocketHandler,
      .user_ctx = NULL,
      .is_websocket = true};
  httpd_uri_t photo_uri = {
      .uri = "/capture_photo",
      .method = HTTP_GET,
      .handler = capturePhotoHandler,
      .user_ctx = NULL};
  httpd_uri_t script_uri = {
      .uri = "/script.js",
      .method = HTTP_GET,
      .handler = scriptHandler,
      .user_ctx = NULL};
  httpd_uri_t style_uri = {
      .uri = "/style.css",
      .method = HTTP_GET,
      .handler = styleHandler,
      .user_ctx = NULL};

  Serial.printf("Starting web server on port: '%d'\n", config.server_port);

  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &photo_uri);
    httpd_register_uri_handler(camera_httpd, &ws_uri);
    httpd_register_uri_handler(camera_httpd, &script_uri);
    httpd_register_uri_handler(camera_httpd, &style_uri);
    Serial.println("WebSocket handler registered on /ws");
  }

  // Server for streaming on port 81
  config.server_port = 81;
  config.ctrl_port = 32769;

  httpd_uri_t stream_uri = {
      .uri = "/stream",
      .method = HTTP_GET,
      .handler = streamHandler,
      .user_ctx = NULL};

  Serial.printf("Starting stream server on port: '%d'\n", config.server_port);
  
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
    Serial.println("Stream server started on port 81");
  }
}