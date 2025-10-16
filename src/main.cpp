#include "LittleFS.h"
#include "esp_camera.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "fb_gfx.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/soc.h"
#include <WiFi.h>

// Конфигурация камеры для AI-Thinker
#define CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

// Настройки камеры
static camera_config_t camera_config = {
    .pin_pwdn = PWDN_GPIO_NUM,
    .pin_reset = RESET_GPIO_NUM,
    .pin_xclk = XCLK_GPIO_NUM,
    .pin_sscb_sda = SIOD_GPIO_NUM,
    .pin_sscb_scl = SIOC_GPIO_NUM,
    .pin_d7 = Y9_GPIO_NUM,
    .pin_d6 = Y8_GPIO_NUM,
    .pin_d5 = Y7_GPIO_NUM,
    .pin_d4 = Y6_GPIO_NUM,
    .pin_d3 = Y5_GPIO_NUM,
    .pin_d2 = Y4_GPIO_NUM,
    .pin_d1 = Y3_GPIO_NUM,
    .pin_d0 = Y2_GPIO_NUM,
    .pin_vsync = VSYNC_GPIO_NUM,
    .pin_href = HREF_GPIO_NUM,
    .pin_pclk = PCLK_GPIO_NUM,
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_JPEG,
    .frame_size = FRAMESIZE_QVGA,
    .jpeg_quality = 12,
    .fb_count = 2};

// Глобальные переменные
httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL;

// Простой флаг блокировки
static bool client_active = false;

// Состояние вспышки
static bool flash_state = false;
#define FLASH_GPIO_NUM 4

// Функция инициализации камеры
esp_err_t init_camera() {
  esp_err_t err = esp_camera_init(&camera_config);
  if (err != ESP_OK) {
    Serial.printf("Ошибка инициализации камеры: 0x%x\n", err);
    return err;
  }
  Serial.println("Камера инициализирована");

  // Инициализация вспышки
  pinMode(FLASH_GPIO_NUM, OUTPUT);
  digitalWrite(FLASH_GPIO_NUM, LOW);
  Serial.println("Flash initialized on GPIO 4");

  return ESP_OK;
}

// Функция переключения вспышки
void toggle_flash() {
  flash_state = !flash_state;
  digitalWrite(FLASH_GPIO_NUM, flash_state ? HIGH : LOW);
  Serial.printf("Flash toggled to %s\n", flash_state ? "ON" : "OFF");
}

// WebSocket handler
static esp_err_t websocket_handler(httpd_req_t *req) {
  if (req->method == HTTP_GET) {
    Serial.println("WebSocket connection requested");

    // 🔥 Отправляем состояние вспышки сразу при подключении
    const char *response = flash_state ? "Flash-ON" : "Flash-OFF";
    httpd_ws_frame_t resp_pkt;
    resp_pkt.payload = (uint8_t *)response;
    resp_pkt.len = strlen(response);
    resp_pkt.type = HTTPD_WS_TYPE_TEXT;
    httpd_ws_send_frame(req, &resp_pkt);

    Serial.printf("Sent initial flash state: %s\n", response);
    return ESP_OK;
  }

  // WebSocket frame structure
  httpd_ws_frame_t ws_pkt;
  uint8_t *buf = NULL;

  memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
  ws_pkt.type = HTTPD_WS_TYPE_TEXT;

  // Получаем длину данных
  esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
  if (ret != ESP_OK) {
    Serial.println("httpd_ws_recv_frame failed");
    return ret;
  }

  if (ws_pkt.len) {
    // Выделяем буфер для данных
    buf = (uint8_t *)calloc(1, ws_pkt.len + 1);
    if (buf == NULL) {
      Serial.println("Failed to allocate memory");
      return ESP_ERR_NO_MEM;
    }

    ws_pkt.payload = buf;

    // Получаем данные
    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret == ESP_OK) {
      buf[ws_pkt.len] = '\0';
      Serial.printf("Received: %s\n", buf);

      if (strcmp((char *)buf, "toggle_flash") == 0) {
        toggle_flash();

        // Send response back
        const char *response = flash_state ? "Flash-ON" : "Flash-OFF";
        httpd_ws_frame_t resp_pkt;
        resp_pkt.payload = (uint8_t *)response;
        resp_pkt.len = strlen(response);
        resp_pkt.type = HTTPD_WS_TYPE_TEXT;

        httpd_ws_send_frame(req, &resp_pkt);
      }

      if (strcmp((char *)buf, "ping") == 0) {
        const char *response = "pong";
        httpd_ws_frame_t resp_pkt;
        resp_pkt.payload = (uint8_t *)response;
        resp_pkt.len = strlen(response);
        resp_pkt.type = HTTPD_WS_TYPE_TEXT;
        httpd_ws_send_frame(req, &resp_pkt);
        Serial.println("Ping received, sent pong");
      }
    }

    free(buf);
  }

  return ESP_OK;
}

// Обработчик для стрима
static esp_err_t stream_handler(httpd_req_t *req) {
  // Блокируем доступ если уже есть клиент
  if (client_active) {
    Serial.println("Stream rejected - client already active");
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }

  client_active = true;
  Serial.println("Stream started - client locked");

  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t *_jpg_buf = NULL;
  char part_buf[64];

  res = httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=frame");
  if (res != ESP_OK) {
    client_active = false;
    return res;
  }

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      delay(100);
      continue;
    }

    if (fb->format != PIXFORMAT_JPEG) {
      bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
      esp_camera_fb_return(fb);
      fb = NULL;
      if (!jpeg_converted)
        continue;
    } else {
      _jpg_buf_len = fb->len;
      _jpg_buf = fb->buf;
    }

    if (res == ESP_OK) {
      size_t hlen = snprintf(part_buf, 64, "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", _jpg_buf_len);
      res = httpd_resp_send_chunk(req, part_buf, hlen);
    }

    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }

    if (fb) {
      esp_camera_fb_return(fb);
      _jpg_buf = NULL;
    } else if (_jpg_buf) {
      free(_jpg_buf);
      _jpg_buf = NULL;
    }

    if (res != ESP_OK) {
      break;
    }
    delay(50);
  }

  client_active = false;
  Serial.println("Stream ended - client unlocked");
  return res;
}

// Обработчик для главной страницы
static esp_err_t index_handler(httpd_req_t *req) {
  // Блокируем доступ если уже есть клиент со стримом
  if (client_active) {
    // Загружаем HTML из LittleFS
    File file = LittleFS.open("/busy.min.html", "r");
    if (!file) {
      Serial.println("Failed to open busy.min.html from LittleFS");
      httpd_resp_send_500(req);
      return ESP_FAIL;
    }

    // Устанавливаем правильные заголовки для UTF-8
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Type", "text/html; charset=utf-8");

    // Читаем и отправляем файл чанками
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

  // Загружаем HTML из LittleFS
  File file = LittleFS.open("/index.min.html", "r");
  if (!file) {
    Serial.println("Failed to open index.min.html from LittleFS");
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  // Устанавливаем правильные заголовки для UTF-8
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Content-Type", "text/html; charset=utf-8");

  // Читаем и отправляем файл чанками
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

// Запуск HTTP серверов
void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  config.ctrl_port = 32768;

  // Главный сервер на порту 80
  httpd_uri_t index_uri = {
      .uri = "/",
      .method = HTTP_GET,
      .handler = index_handler,
      .user_ctx = NULL};

  // WebSocket handler
  httpd_uri_t ws_uri = {
      .uri = "/ws",
      .method = HTTP_GET,
      .handler = websocket_handler,
      .user_ctx = NULL,
      .is_websocket = true};

  Serial.printf("Starting web server on port: '%d'\n", config.server_port);
  if (httpd_start(&camera_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &ws_uri);
    Serial.println("WebSocket handler registered on /ws");
  }

  // Сервер для стрима на порту 81
  config.server_port = 81;
  config.ctrl_port = 32769;

  httpd_uri_t stream_uri = {
      .uri = "/stream",
      .method = HTTP_GET,
      .handler = stream_handler,
      .user_ctx = NULL};

  Serial.printf("Starting stream server on port: '%d'\n", config.server_port);
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
    Serial.println("Stream server started on port 81");
  }
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  Serial.println("=== ESP32-CAM with WebSocket Flash Control ===");

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed!");
    return;
  }
  Serial.println("LittleFS mounted successfully");

  // Инициализация камеры
  if (init_camera() != ESP_OK) {
    Serial.println("Перезагрузка через 5 секунд...");
    delay(5000);
    ESP.restart();
  }

  // Подключение к WiFi
  Serial.print("Подключение к WiFi");
  WiFi.begin("bogdan", "seredenko ");

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi подключен!");
    Serial.print("IP адрес: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nОшибка подключения к WiFi!");
    return;
  }

  // Запуск серверов
  startCameraServer();
}

void loop() {
  delay(1000);
}