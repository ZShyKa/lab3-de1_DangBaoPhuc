// Lab 3 - Đề 1: FreeRTOS cho hệ thống giám sát và cảnh báo người chạy
// ESP32 + BMP180 + MQTT (nâng cấp từ Lab 2)
// Họ tên: ................  Lớp: ................
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_BMP085.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

// ---------- Cấu hình (giữ nguyên như Lab 2 để dùng lại web) ----------
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";
const char* MQTT_HOST = "broker.hivemq.com";
const uint16_t MQTT_PORT = 1883;
#define PREFIX "lab2_utc_runner01/"   // PHẢI trùng PREFIX trong index.html

#define PIN_SDA 21
#define PIN_SCL 22
#define PIN_LED 26
#define PIN_BUZZER 27

const float TEMP_LOW  = 35.0;   // °C, dưới ngưỡng -> cảnh báo
const float TEMP_HIGH = 38.5;   // °C, vượt ngưỡng -> cảnh báo
const float ALT_JUMP  = 3.0;    // m, độ cao đổi quá mức này giữa 2 lần đo (1 s) -> cảnh báo
const float SEA_LEVEL_PA = 101325;

// ---------- Kiểu dữ liệu đi qua hàng đợi ----------
struct SensorData { float temp; float pres; float alt; };   // vào "data Queue"
struct AlertData  { float temp; float alt; };               // vào "data Alert"

// Trạng thái dùng chung giữa các task, được bảo vệ bằng stateMutex
struct SharedState {
  bool autoMode;
  bool manualLed;
  bool manualBuzzer;
  bool ledOn;
  bool buzzerOn;
  char alert[48];
};

// ---------- Đối tượng toàn cục ----------
Adafruit_BMP085 bmp;
WiFiClient espClient;
PubSubClient mqtt(espClient);         // CHỈ Task truyền data được dùng

QueueHandle_t dataQueue;              // "data Queue": Task đọc -> Task truyền
QueueHandle_t alertQueue;             // "data Alert": Task đọc -> Task cảnh báo
SemaphoreHandle_t stateMutex;         // bảo vệ biến shared
SemaphoreHandle_t statusSem;          // semaphore nhị phân: báo Task truyền gửi lại trạng thái

SharedState shared = { true, false, false, false, false, "OK" };

// ---------- Điều khiển Loa ----------
void buzzerSet(bool on) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  if (on) tone(PIN_BUZZER, 1000); else noTone(PIN_BUZZER);
#else
  ledcWriteTone(0, on ? 1000 : 0);
#endif
}

// ---------- Nhận lệnh từ Web (chạy bên trong Task truyền data) ----------
void onMessage(char* topic, byte* payload, unsigned int len) {
  char v[16];
  unsigned int n = len < sizeof(v) - 1 ? len : sizeof(v) - 1;
  for (unsigned int i = 0; i < n; i++) v[i] = toupper(payload[i]);
  v[n] = 0;

  xSemaphoreTake(stateMutex, portMAX_DELAY);
  if (!strcmp(topic, PREFIX "cmd/mode"))        shared.autoMode = (strcmp(v, "MANUAL") != 0);
  else if (!strcmp(topic, PREFIX "cmd/led"))    shared.manualLed = !strcmp(v, "ON");
  else if (!strcmp(topic, PREFIX "cmd/buzzer")) shared.manualBuzzer = !strcmp(v, "ON");
  xSemaphoreGive(stateMutex);

  xSemaphoreGive(statusSem);   // báo gửi lại trạng thái
}

void publishStatus() {
  SharedState s;
  xSemaphoreTake(stateMutex, portMAX_DELAY);
  s = shared;
  xSemaphoreGive(stateMutex);

  mqtt.publish(PREFIX "status/mode",   s.autoMode ? "AUTO" : "MANUAL", true);
  mqtt.publish(PREFIX "status/led",    s.ledOn ? "ON" : "OFF", true);
  mqtt.publish(PREFIX "status/buzzer", s.buzzerOn ? "ON" : "OFF", true);
  mqtt.publish(PREFIX "status/alert",  s.alert, true);
}

// =====================================================================
// TASK 1: TRUYỀN DATA - Core 0
// Kết nối WiFi; nếu thành công thì đọc "data Queue" và gửi lên MQTT broker
// =====================================================================
void taskTransmit(void* pv) {
  Serial.printf("[TX] Task truyen data chay tren Core %d\r\n", xPortGetCoreID());

  WiFi.begin(WIFI_SSID, WIFI_PASS, 6);
  while (WiFi.status() != WL_CONNECTED) vTaskDelay(pdMS_TO_TICKS(300));
  Serial.println("[TX] WiFi OK");

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMessage);

  SensorData d;
  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.reconnect();
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }
    if (!mqtt.connected()) {
      String id = "esp32-rtos-" + String(random(0xffff), HEX);
      if (mqtt.connect(id.c_str())) {
        mqtt.subscribe(PREFIX "cmd/mode");
        mqtt.subscribe(PREFIX "cmd/led");
        mqtt.subscribe(PREFIX "cmd/buzzer");
        xSemaphoreGive(statusSem);          // gửi trạng thái ngay khi vừa kết nối
        Serial.println("[TX] MQTT OK");
      } else {
        Serial.printf("[TX] MQTT loi rc=%d\r\n", mqtt.state());
        vTaskDelay(pdMS_TO_TICKS(2000));
        continue;
      }
    }

    mqtt.loop();

    // Chờ tối đa 50 ms để lấy dữ liệu từ data Queue
    if (xQueueReceive(dataQueue, &d, pdMS_TO_TICKS(50)) == pdTRUE) {
      char buf[16];
      snprintf(buf, sizeof(buf), "%.1f", d.temp);
      mqtt.publish(PREFIX "sensor/temperature", buf);
      snprintf(buf, sizeof(buf), "%.1f", d.pres);
      mqtt.publish(PREFIX "sensor/pressure", buf);
      snprintf(buf, sizeof(buf), "%.1f", d.alt);
      mqtt.publish(PREFIX "sensor/altitude", buf);
    }

    // Có thay đổi trạng thái (từ Task cảnh báo hoặc lệnh Web) -> gửi lại
    if (xSemaphoreTake(statusSem, 0) == pdTRUE) publishStatus();
  }
}

// =====================================================================
// TASK 2: ĐỌC DATA - Core 1
// Đọc BMP180, in Serial, gửi đủ dữ liệu vào data Queue,
// gửi (nhiệt độ + độ cao) vào data Alert
// =====================================================================
void taskRead(void* pv) {
  Serial.printf("[READ] Task doc data chay tren Core %d\r\n", xPortGetCoreID());
  TickType_t last = xTaskGetTickCount();

  for (;;) {
    SensorData d;
    d.temp = bmp.readTemperature();                  // °C
    d.pres = bmp.readPressure() / 100.0f;            // hPa
    d.alt  = bmp.readAltitude(SEA_LEVEL_PA);         // m

    Serial.printf("[READ] T=%.1f C | P=%.1f hPa | Alt=%.1f m\r\n", d.temp, d.pres, d.alt);

    if (xQueueSend(dataQueue, &d, 0) != pdTRUE)
      Serial.println("[READ] data Queue day, bo mau nay");

    AlertData a = { d.temp, d.alt };
    xQueueSend(alertQueue, &a, 0);

    vTaskDelayUntil(&last, pdMS_TO_TICKS(1000));     // chu kỳ đúng 1 giây
  }
}

// =====================================================================
// TASK 3: CẢNH BÁO - Core 1
// Đọc data Alert, kiểm tra ngưỡng, điều khiển Led/Loa
// =====================================================================
void taskAlert(void* pv) {
  Serial.printf("[ALERT] Task canh bao chay tren Core %d\r\n", xPortGetCoreID());

  AlertData a;
  float prevAlt = NAN;
  char text[48] = "OK";

  for (;;) {
    // Chờ dữ liệu tối đa 200 ms; hết hạn vẫn chạy tiếp để kịp phản ứng với lệnh MANUAL
    if (xQueueReceive(alertQueue, &a, pdMS_TO_TICKS(200)) == pdTRUE) {
      char t[48];
      t[0] = 0;
      if (a.temp > TEMP_HIGH)      strcpy(t, "Nhiet do qua cao");
      else if (a.temp < TEMP_LOW)  strcpy(t, "Nhiet do qua thap");
      if (!isnan(prevAlt) && fabsf(a.alt - prevAlt) > ALT_JUMP) {
        if (t[0]) strcat(t, " + ");
        strcat(t, "Do cao thay doi dot ngot");
      }
      prevAlt = a.alt;
      strlcpy(text, t[0] ? t : "OK", sizeof(text));
    }

    xSemaphoreTake(stateMutex, portMAX_DELAY);
    bool changed = false;

    if (strcmp(shared.alert, text) != 0) {
      strlcpy(shared.alert, text, sizeof(shared.alert));
      changed = true;
    }

    bool alerting = (strcmp(text, "OK") != 0);
    bool wantLed  = shared.autoMode ? alerting : shared.manualLed;
    bool wantBuzz = shared.autoMode ? alerting : shared.manualBuzzer;

    if (wantLed != shared.ledOn) {
      shared.ledOn = wantLed;
      digitalWrite(PIN_LED, wantLed);
      changed = true;
    }
    if (wantBuzz != shared.buzzerOn) {
      shared.buzzerOn = wantBuzz;
      buzzerSet(wantBuzz);
      changed = true;
    }
    xSemaphoreGive(stateMutex);

    if (changed) {
      Serial.printf("[ALERT] %s | Led=%d Loa=%d\r\n", text, wantLed, wantBuzz);
      xSemaphoreGive(statusSem);
    }
  }
}

// ---------- Khởi tạo hệ thống ----------
void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED, OUTPUT);
#if ESP_ARDUINO_VERSION_MAJOR < 3
  ledcSetup(0, 1000, 8);
  ledcAttachPin(PIN_BUZZER, 0);
#else
  pinMode(PIN_BUZZER, OUTPUT);
#endif

  Wire.begin(PIN_SDA, PIN_SCL);
  if (!bmp.begin()) {
    Serial.println("Khong tim thay BMP180! Kiem tra SDA/SCL.");
    while (true) delay(1000);
  }

  dataQueue  = xQueueCreate(10, sizeof(SensorData));
  alertQueue = xQueueCreate(5,  sizeof(AlertData));
  stateMutex = xSemaphoreCreateMutex();
  statusSem  = xSemaphoreCreateBinary();

  // (hàm, tên, stack (byte), tham số, ưu tiên, handle, core)
  xTaskCreatePinnedToCore(taskTransmit, "Task_TruyenData", 8192, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(taskRead,     "Task_DocData",    4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(taskAlert,    "Task_CanhBao",    4096, NULL, 3, NULL, 1);
}

void loop() {
  vTaskDelete(NULL);   // loop() không còn việc gì, xóa task mặc định
}