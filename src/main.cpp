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
    .frame_size = FRAMESIZE_VGA,
    .jpeg_quality = 12,
    .fb_count = 1};

// Глобальные переменные
httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL;

// WebSocket клиент
static httpd_handle_t ws_client = NULL;

// HTML страница с одной кнопкой по WebSocket
static const char *MAIN_page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32-CAM + WebSocket</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial; text-align: center; margin: 20px; }
        button { 
            padding: 20px 40px; 
            font-size: 20px; 
            background: #4CAF50; 
            color: white; 
            border: none; 
            border-radius: 8px; 
            cursor: pointer;
            margin: 10px;
        }
        button:active { background: #45a049; transform: scale(0.98); }
        .status { margin: 15px; padding: 10px; border-radius: 5px; }
        .connected { background: #d4edda; color: #155724; }
        .disconnected { background: #f8d7da; color: #721c24; }
        img { max-width: 100%; height: auto; border: 1px solid #ddd; border-radius: 5px; }
    </style>
</head>
<body>
    <h1>ESP32-CAM + WebSocket Button</h1>
    
    <div class="status disconnected" id="status">
        Disconnected - Click to connect
    </div>
    
    <img src="http://IP_ADDRESS:81/stream">
    
    <br>
    
    <button onclick="sendButtonClick()" id="mainButton" disabled>
        CLICK ME (WebSocket)
    </button>

    <script>
        var ws = null;
        var currentUrl = window.location.hostname;
        
        // Автоподстановка IP для видео
        document.querySelector('img').src = 'http://' + currentUrl + ':81/stream';
        
        function connectWebSocket() {
            if (ws && ws.readyState === WebSocket.OPEN) return;
            
            ws = new WebSocket('ws://' + currentUrl + '/ws');
            
            ws.onopen = function() {
                console.log('WebSocket connected');
                document.getElementById('status').className = 'status connected';
                document.getElementById('status').textContent = 'Connected ✅';
                document.getElementById('mainButton').disabled = false;
            };
            
            ws.onmessage = function(event) {
                console.log('Received:', event.data);
                document.getElementById('status').textContent = 'Server: ' + event.data;
            };
            
            ws.onclose = function() {
                console.log('WebSocket disconnected');
                document.getElementById('status').className = 'status disconnected';
                document.getElementById('status').textContent = 'Disconnected - Click to reconnect';
                document.getElementById('mainButton').disabled = true;
                setTimeout(connectWebSocket, 2000);
            };
            
            ws.onerror = function(error) {
                console.log('WebSocket error:', error);
            };
        }
        
        function sendButtonClick() {
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send('button_click');
                document.getElementById('status').textContent = 'Button clicked! Waiting response...';
            }
        }
        
        // Автоподключение при загрузке
        document.getElementById('status').onclick = connectWebSocket;
        connectWebSocket();
    </script>
</body>
</html>
)rawliteral";

// Функция инициализации камеры
esp_err_t init_camera() {
  esp_err_t err = esp_camera_init(&camera_config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return err;
  }
  Serial.println("Camera initialized");
  return ESP_OK;
}

// WebSocket handler
static esp_err_t websocket_handler(httpd_req_t *req) {
  if (req->method == HTTP_GET) {
    Serial.println("WebSocket connection requested");
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
      Serial.printf("WebSocket received: %s\n", buf);

      if (strcmp((char *)buf, "button_click") == 0) {
        Serial.println("=== WEB SOCKET BUTTON CLICKED ===");
        Serial.println("Message: Hello from WebSocket!");
        Serial.println("Time: " + String(millis()));
        Serial.println("=== ========================= ===");

        // Send response back
        const char *response = "Button received! Check Serial monitor";
        httpd_ws_frame_t resp_pkt;
        resp_pkt.payload = (uint8_t *)response;
        resp_pkt.len = strlen(response);
        resp_pkt.type = HTTPD_WS_TYPE_TEXT;

        httpd_ws_send_frame(req, &resp_pkt);
      }
    }

    free(buf);
  }

  return ESP_OK;
}

// Обработчик для стрима
static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t *_jpg_buf = NULL;
  char part_buf[64];

  res = httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=frame");
  if (res != ESP_OK)
    return res;

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

    if (res != ESP_OK)
      break;
    delay(50);
  }
  return res;
}

// Обработчик для главной страницы
static esp_err_t index_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  String page = MAIN_page;
  page.replace("IP_ADDRESS", WiFi.localIP().toString().c_str());
  return httpd_resp_send(req, page.c_str(), page.length());
}

// Запуск HTTP серверов
void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  config.ctrl_port = 32768; // Явно указываем порт управления

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
  config.ctrl_port = 32769; // Разный порт управления!

  httpd_uri_t stream_uri = {
      .uri = "/stream",
      .method = HTTP_GET,
      .handler = stream_handler,
      .user_ctx = NULL};

  Serial.printf("Starting stream server on port: '%d'\n", config.server_port);
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
  } else {
    Serial.println("Stream server start failed - but main server is running");
  }
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);
  Serial.println("=== ESP32-CAM + WebSocket ===");

  if (init_camera() != ESP_OK) {
    delay(5000);
    ESP.restart();
  }

  WiFi.begin("bogdan", "seredenko ");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected! IP: " + WiFi.localIP().toString());
    startCameraServer();
  }
}

void loop() {
  delay(1000);
}