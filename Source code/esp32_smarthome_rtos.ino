
// ===========================================================
//  ESP32 SMART HOME - RTOS + FSM + SCHEDULER DESIGN
//  Kiến trúc: FreeRTOS Tasks + Finite State Machine + Timer Scheduler
// ===========================================================

#include <WiFi.h>
#include <FirebaseESP32.h>
#include <ESP32Servo.h>

// ======================== CONFIG ========================
const char* ssid     = "Batman6176";
const char* password = "nhatnguyen6176";

#define FIREBASE_HOST "smarthome-iot-2d485-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "uRVh255jtrxe9rl8VHgg2On8zpA0zj7j4SL8RbrV"

// ======================== GPIO ==========================
const int GAS_PIN        = 34;
const int LIGHT_PIN      = 35;
const int BUZZ_PIN       = 12;
const int ALERT_LED_PIN  = 14;
const int SMART_LED_PIN  = 27;
const int MAIN_DOOR_PIN  = 13;
const int WINDOW1_PIN    = 25;
const int WINDOW2_PIN    = 26;

// ======================== THRESHOLDS ====================
const int GAS_HIGH = 1000;
const int GAS_LOW  = 900;

// ======================== FIREBASE ======================
FirebaseData  fbdo;
FirebaseAuth  auth;
FirebaseConfig fconfig;

// ======================== SERVO =========================
Servo mainDoor, window1, window2;

// ===========================================================
//  PHẦN 1: FINITE STATE MACHINE (FSM)
//  Mô tả trạng thái tổng thể của hệ thống
// ===========================================================

// --- Trạng thái FSM cho GAS ALARM ---
typedef enum {
  GAS_STATE_SAFE,       // Không có khí gas nguy hiểm
  GAS_STATE_ALARM       // Khí gas vượt ngưỡng -> báo động
} GasAlarmState_t;

// --- Trạng thái FSM cho DOOR ---
typedef enum {
  DOOR_STATE_CLOSED,    // Cửa đóng
  DOOR_STATE_OPENING,   // Cửa đang mở (chờ timer)
  DOOR_STATE_OPEN,      // Cửa mở hoàn toàn
  DOOR_STATE_CLOSING    // Cửa đang đóng lại
} DoorState_t;

// --- Trạng thái FSM cho SMART LIGHT ---
typedef enum {
  LIGHT_STATE_OFF,      // Đèn tắt
  LIGHT_STATE_ON        // Đèn bật
} LightState_t;

// ======================== STATE VARIABLES ===============
volatile GasAlarmState_t gasState   = GAS_STATE_SAFE;
volatile DoorState_t     doorState  = DOOR_STATE_CLOSED;
volatile LightState_t    lightState = LIGHT_STATE_OFF;

// ======================== SHARED DATA (thread-safe) =====
// Dùng SemaphoreMutex để bảo vệ dữ liệu chia sẻ giữa các task
SemaphoreHandle_t xDataMutex;

struct SensorData {
  int  gasValue;
  int  lightValue;
  bool isAutoMode;
  bool manualDoorCmd;    // Lệnh từ AI/app mở cửa
  bool manualLightCmd;   // Lệnh tay bật/tắt đèn
  // [FIX BUG 1] Flag báo Firebase task upload light state
  // LightFSM KHÔNG gọi Firebase trực tiếp để tránh race condition trên fbdo
  bool lightStateChanged;  // set true khi đèn thay đổi, Firebase task đọc rồi clear
  bool lightStateValue;    // giá trị cần upload
} sensorData = {0, 0, true, false, false, false, false};

// ======================== TASK HANDLES ==================
TaskHandle_t hTaskSensor    = NULL;
TaskHandle_t hTaskGasFSM    = NULL;
TaskHandle_t hTaskDoorFSM   = NULL;
TaskHandle_t hTaskLightFSM  = NULL;
TaskHandle_t hTaskFirebase  = NULL;
TaskHandle_t hTaskLogger    = NULL;

// ======================== QUEUES ========================
// Queue để truyền lệnh mở cửa từ Firebase -> Door FSM
QueueHandle_t xDoorCmdQueue;

// ======================== EVENT GROUPS ==================
// Dùng EventGroup để sync trạng thái giữa các task
#include "freertos/event_groups.h"
EventGroupHandle_t xSystemEvents;
#define EVT_GAS_ALARM     (1 << 0)  // Bit 0: Có báo động gas
#define EVT_WIFI_READY    (1 << 1)  // Bit 1: WiFi đã kết nối
#define EVT_FB_READY      (1 << 2)  // Bit 2: Firebase sẵn sàng

// ===========================================================
//  PHẦN 2: TASK SCHEDULER - Cooperative Timer
//  Các task dùng vTaskDelay / xTaskDelayUntil để tạo
//  lịch chạy định kỳ — KHÔNG dùng delay() blocking
// ===========================================================

// -------------------------------------------------------
// TASK 1: Sensor Reading Task (Priority: HIGHEST = 5)
// Đọc cảm biến với tần suất cao nhất: 50ms/lần
// -------------------------------------------------------
void vTaskSensorRead(void* pvParam) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xPeriod = pdMS_TO_TICKS(50); // 50ms

  for (;;) {
    int gas   = analogRead(GAS_PIN);
    int light = digitalRead(LIGHT_PIN);

    // Bảo vệ ghi dữ liệu chia sẻ bằng mutex
    if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      sensorData.gasValue   = gas;
      sensorData.lightValue = light;
      xSemaphoreGive(xDataMutex);
    }

    // Nhả CPU, chờ đến chu kỳ tiếp theo (NON-BLOCKING)
    vTaskDelayUntil(&xLastWakeTime, xPeriod);
  }
}

// -------------------------------------------------------
// TASK 2: Gas Alarm FSM Task (Priority: HIGH = 4)
// Chạy mỗi 100ms, xử lý FSM gas alarm với hysteresis + debounce
// -------------------------------------------------------
void vTaskGasFSM(void* pvParam) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xPeriod = pdMS_TO_TICKS(100); // 100ms

  // --- DEBOUNCE COUNTER ---
  // Phải có N lần đọc LIÊN TIẾP mới chuyển state
  // Tránh PWM servo/tone gây nhiễu ADC làm FSM oscillate
  const uint8_t CONFIRM_COUNT = 3;   // 3 x 100ms = 300ms xác nhận
  uint8_t alarmConfirm = 0;          // đếm số lần liên tiếp gas >= HIGH
  uint8_t safeConfirm  = 0;          // đếm số lần liên tiếp gas < LOW

  for (;;) {
    int gas = 0;  // khởi tạo = 0: an toàn nếu mutex thất bại (sẽ không trigger alarm)
    if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      gas = sensorData.gasValue;
      xSemaphoreGive(xDataMutex);
    }

    // ====== FSM TRANSITIONS (với debounce) ======
    switch (gasState) {

      case GAS_STATE_SAFE:
        if (gas >= GAS_HIGH) {
          alarmConfirm++;
          safeConfirm = 0;  // reset counter ngược
          if (alarmConfirm >= CONFIRM_COUNT) {
            // Xác nhận đủ N lần -> chuyển ALARM
            alarmConfirm = 0;
            gasState = GAS_STATE_ALARM;
            xEventGroupSetBits(xSystemEvents, EVT_GAS_ALARM);
            Serial.printf("[FSM/GAS] SAFE -> ALARM (gas=%d, confirmed %dx)\n", gas, CONFIRM_COUNT);
            mainDoor.write(90);
            window1.write(90);
            window2.write(90);
            tone(BUZZ_PIN, 1000);
          } else {
            Serial.printf("[FSM/GAS] Gas HIGH detected (%d/%d), gas=%d\n", alarmConfirm, CONFIRM_COUNT, gas);
          }
        } else {
          alarmConfirm = 0;  // không liên tiếp -> reset
        }
        break;

      case GAS_STATE_ALARM:
        // [FIX BUG 3] Liên tục ghi servo + còi trong alarm (giống code cũ dòng 200-202)
        // Tránh servo bị drift và còi bị tắt do side effect
        tone(BUZZ_PIN, 1000);
        mainDoor.write(90);
        window1.write(90);
        window2.write(90);

        // LED nháy (không dùng delay!)
        static bool ledToggle = false;
        ledToggle = !ledToggle;
        digitalWrite(ALERT_LED_PIN, ledToggle ? HIGH : LOW);

        if (gas < GAS_LOW) {
          safeConfirm++;
          alarmConfirm = 0;  // reset counter ngược
          if (safeConfirm >= CONFIRM_COUNT) {
            // Xác nhận đủ N lần -> chuyển SAFE
            safeConfirm = 0;
            gasState = GAS_STATE_SAFE;
            xEventGroupClearBits(xSystemEvents, EVT_GAS_ALARM);
            Serial.printf("[FSM/GAS] ALARM -> SAFE (gas=%d, confirmed %dx)\n", gas, CONFIRM_COUNT);
            noTone(BUZZ_PIN);
            mainDoor.write(0);
            window1.write(0);
            window2.write(0);
            digitalWrite(ALERT_LED_PIN, LOW);
          } else {
            Serial.printf("[FSM/GAS] Gas LOW detected (%d/%d), gas=%d\n", safeConfirm, CONFIRM_COUNT, gas);
          }
        } else {
          safeConfirm = 0;  // không liên tiếp -> reset
        }
        break;
    }

    vTaskDelayUntil(&xLastWakeTime, xPeriod);
  }
}

// -------------------------------------------------------
// TASK 3: Door FSM Task (Priority: MEDIUM = 3)
// Nhận lệnh từ Queue, xử lý trạng thái cửa
// Chỉ hoạt động khi KHÔNG có gas alarm (check EventGroup)
// -------------------------------------------------------
void vTaskDoorFSM(void* pvParam) {
  bool cmdOpen = false;
  TickType_t doorOpenedAt = 0;
  const TickType_t DOOR_OPEN_DURATION = pdMS_TO_TICKS(5000); // 5 giây

  for (;;) {
    // Nếu đang báo động gas, task ngủ - nhường cho Gas FSM
    EventBits_t events = xEventGroupGetBits(xSystemEvents);
    if (events & EVT_GAS_ALARM) {
      vTaskDelay(pdMS_TO_TICKS(200));
      continue;
    }

    // ====== FSM TRANSITIONS ======
    switch (doorState) {

      case DOOR_STATE_CLOSED:
        // Chờ lệnh mở từ Queue (timeout 100ms)
        if (xQueueReceive(xDoorCmdQueue, &cmdOpen, pdMS_TO_TICKS(100)) == pdTRUE && cmdOpen) {
          doorState = DOOR_STATE_OPENING;
          Serial.println("[FSM/DOOR] CLOSED -> OPENING");
          mainDoor.write(90);
          tone(BUZZ_PIN, 2000, 100);
          vTaskDelay(pdMS_TO_TICKS(200));
          tone(BUZZ_PIN, 2000, 100);
          doorOpenedAt = xTaskGetTickCount();
        }
        break;

      case DOOR_STATE_OPENING:
        // Chờ servo đến vị trí -> OPEN
        doorState = DOOR_STATE_OPEN;
        Serial.println("[FSM/DOOR] OPENING -> OPEN");
        break;

      case DOOR_STATE_OPEN:
        // Kiểm tra xem đã hết 5s chưa
        if ((xTaskGetTickCount() - doorOpenedAt) >= DOOR_OPEN_DURATION) {
          doorState = DOOR_STATE_CLOSING;
          Serial.println("[FSM/DOOR] OPEN -> CLOSING");
        } else {
          vTaskDelay(pdMS_TO_TICKS(100));
        }
        break;

      case DOOR_STATE_CLOSING:
        mainDoor.write(0);
        doorState = DOOR_STATE_CLOSED;
        Serial.println("[FSM/DOOR] CLOSING -> CLOSED");
        break;
    }
  }
}

// -------------------------------------------------------
// TASK 4: Smart Light FSM Task (Priority: LOW = 2)
// Chạy mỗi 200ms, xử lý logic đèn tự động / thủ công
// -------------------------------------------------------
void vTaskLightFSM(void* pvParam) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xPeriod = pdMS_TO_TICKS(200); // 200ms

  for (;;) {
    // Không xử lý đèn khi đang báo động
    EventBits_t events = xEventGroupGetBits(xSystemEvents);
    if (events & EVT_GAS_ALARM) {
      vTaskDelayUntil(&xLastWakeTime, xPeriod);
      continue;
    }

    // Khởi tạo trước: nếu mutex thất bại thì giữ hành vi an toàn (auto=true, đèn theo cảm biến)
    bool autoMode = true, manualCmd = false;
    int  light = 0;

    if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      autoMode  = sensorData.isAutoMode;
      manualCmd = sensorData.manualLightCmd;
      light     = sensorData.lightValue;
      xSemaphoreGive(xDataMutex);
    }

    // ====== FSM TRANSITIONS ======
    LightState_t nextState = lightState;

    if (autoMode) {
      // AUTO: quyết định theo cảm biến ánh sáng
      if (light == HIGH) nextState = LIGHT_STATE_ON;
      else               nextState = LIGHT_STATE_OFF;
    } else {
      // MANUAL: quyết định theo lệnh từ Firebase/App
      nextState = manualCmd ? LIGHT_STATE_ON : LIGHT_STATE_OFF;
    }

    // [FIX BUG 2] Điều khiển phần cứng LIÊN TỤC (giống code cũ dòng 135)
    // Tránh trường hợp nhiễu điện làm đèn sai trạng thái mà không được ghi lại
    digitalWrite(SMART_LED_PIN, nextState == LIGHT_STATE_ON ? HIGH : LOW);

    // [FIX BUG 1] Chỉ upload Firebase khi state THAY ĐỔI
    // KHÔNG gọi Firebase trực tiếp ở đây! Dùng flag để báo Firebase task
    // tránh race condition trên fbdo (fbdo đang được Firebase task dùng ở Core 0)
    if (nextState != lightState) {
      lightState = nextState;
      Serial.printf("[FSM/LIGHT] -> %s (%s)\n",
        lightState == LIGHT_STATE_ON ? "ON" : "OFF",
        autoMode ? "Auto" : "Manual");

      if (autoMode) {
        // Set flag để Firebase task upload (an toàn, mutex-protected)
        if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
          sensorData.lightStateChanged = true;
          sensorData.lightStateValue   = (lightState == LIGHT_STATE_ON);
          xSemaphoreGive(xDataMutex);
        }
      }
    }

    vTaskDelayUntil(&xLastWakeTime, xPeriod);
  }
}

// -------------------------------------------------------
// TASK 5: Firebase I/O Task (Priority: LOW = 2)
// Scheduler: Upload sensor data mỗi 1s
//            Poll config/commands mỗi 2s
//            Poll AI door command mỗi 1.5s
// -------------------------------------------------------
void vTaskFirebase(void* pvParam) {
  // Chờ WiFi sẵn sàng trước khi bắt đầu
  xEventGroupWaitBits(xSystemEvents, EVT_WIFI_READY, pdFALSE, pdTRUE, portMAX_DELAY);

  // Dùng Task Notification hoặc xTaskDelayUntil để tạo scheduler chuẩn
  // Lưu các "hạn deadline" cho từng subtask
  TickType_t tNextUpload    = xTaskGetTickCount();
  TickType_t tNextConfig    = xTaskGetTickCount();
  TickType_t tNextAICheck   = xTaskGetTickCount();

  const TickType_t T_UPLOAD  = pdMS_TO_TICKS(1000);   // 1s
  const TickType_t T_CONFIG  = pdMS_TO_TICKS(2000);   // 2s
  const TickType_t T_AI      = pdMS_TO_TICKS(1500);   // 1.5s
  const TickType_t T_LOOP    = pdMS_TO_TICKS(100);    // vòng lặp chính mỗi 100ms

  for (;;) {
    if (!Firebase.ready()) {
      vTaskDelay(T_LOOP);
      continue;
    }

    xEventGroupSetBits(xSystemEvents, EVT_FB_READY);
    TickType_t now = xTaskGetTickCount();

    // --- Subtask A: Upload sensor data ---
    if ((now - tNextUpload) >= T_UPLOAD) {
      // Khởi tạo: nếu mutex thất bại thì gotData=false, bỏ qua upload lần này
      int  gas = 0, light = 0;
      bool lightChanged = false, lightVal = false;
      bool gotData = false;

      if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        gas          = sensorData.gasValue;
        light        = sensorData.lightValue;
        lightChanged = sensorData.lightStateChanged;
        lightVal     = sensorData.lightStateValue;
        if (lightChanged) sensorData.lightStateChanged = false; // clear flag
        xSemaphoreGive(xDataMutex);
        gotData = true;
      }

      // Chỉ upload khi lấy được dữ liệu thực (tránh gửi rác lên Firebase)
      if (gotData) {
        Firebase.setInt(fbdo, "/smarthome/data/gas",          gas);
        Firebase.setInt(fbdo, "/smarthome/data/light_sensor", light);

        // [FIX BUG 1] Upload light status TỪ Firebase task (không từ LightFSM)
        // -> an toàn vì fbdo chỉ được dùng bởi 1 task duy nhất (task này)
        if (lightChanged) {
          Firebase.setBool(fbdo, "/smarthome/status/smart_light", lightVal);
        }
      }

      tNextUpload = now;
    }

    // --- Subtask B: Poll config (auto_mode + manual light) ---
    if ((now - tNextConfig) >= T_CONFIG) {
      bool autoMode = true;
      bool manualLight = false;

      if (Firebase.getBool(fbdo, "/smarthome/config/auto_mode")) {
        if (fbdo.dataType() == "boolean") autoMode = fbdo.boolData();
      }
      if (!autoMode) {
        if (Firebase.getBool(fbdo, "/smarthome/commands/smart_light")) {
          if (fbdo.dataType() == "boolean") manualLight = fbdo.boolData();
        }
      }

      if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        sensorData.isAutoMode     = autoMode;
        sensorData.manualLightCmd = manualLight;
        xSemaphoreGive(xDataMutex);
      }
      tNextConfig = now;
    }

    // --- Subtask C: Poll AI door command ---
    if ((now - tNextAICheck) >= T_AI) {
      if (Firebase.getBool(fbdo, "/smarthome/commands/ai_door")) {
        if (fbdo.dataType() == "boolean" && fbdo.boolData() == true) {
          Serial.println("[FB] AI Door Command received -> Queue");
          bool openCmd = true;
          xQueueSend(xDoorCmdQueue, &openCmd, 0); // Non-blocking send
          Firebase.setBool(fbdo, "/smarthome/commands/ai_door", false); // Reset
        }
      }
      tNextAICheck = now;
    }

    vTaskDelay(T_LOOP); // Nhả CPU cho task khác
  }
}

// -------------------------------------------------------
// TASK 6: Logger Task (Priority: IDLE = 1)
// In ra Serial Monitor mỗi 1 giây
// -------------------------------------------------------
void vTaskLogger(void* pvParam) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xPeriod = pdMS_TO_TICKS(1000);

  const char* gasStateStr[]  = {"SAFE", "ALARM"};
  const char* doorStateStr[] = {"CLOSED", "OPENING", "OPEN", "CLOSING"};
  const char* lightStateStr[]= {"OFF", "ON"};

  for (;;) {
    // Khởi tạo trước để tránh in giá trị rác nếu mutex thất bại
    int gas = 0, light = 0;
    bool autoMode = true;
    if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      gas      = sensorData.gasValue;
      light    = sensorData.lightValue;
      autoMode = sensorData.isAutoMode;
      xSemaphoreGive(xDataMutex);
    }

    EventBits_t evts = xEventGroupGetBits(xSystemEvents);

    Serial.println("======== [ SYSTEM STATUS ] ========");
    Serial.printf("  Gas   : %-4d | FSM: %s\n",   gas,   gasStateStr[gasState]);
    Serial.printf("  Light : %-4d | FSM: %s\n",   light, lightStateStr[lightState]);
    Serial.printf("  Door  : FSM=%s\n",            doorStateStr[doorState]);
    Serial.printf("  Mode  : %s | WiFi:%s FB:%s\n",
      autoMode ? "AUTO" : "MANUAL",
      (evts & EVT_WIFI_READY) ? "OK" : "--",
      (evts & EVT_FB_READY)   ? "OK" : "--"
    );
    Serial.println("====================================\n");

    // In stack watermark để debug bộ nhớ
    Serial.printf("  Stack free (Sensor):  %d\n", uxTaskGetStackHighWaterMark(hTaskSensor));
    Serial.printf("  Stack free (GasFSM):  %d\n", uxTaskGetStackHighWaterMark(hTaskGasFSM));
    Serial.printf("  Stack free (Firebase):%d\n", uxTaskGetStackHighWaterMark(hTaskFirebase));

    vTaskDelayUntil(&xLastWakeTime, xPeriod);
  }
}

// ===========================================================
//  SETUP
// ===========================================================
void setup() {
  Serial.begin(115200);

  // GPIO setup
  pinMode(GAS_PIN,       INPUT);
  pinMode(LIGHT_PIN,     INPUT);
  pinMode(BUZZ_PIN,      OUTPUT);
  pinMode(ALERT_LED_PIN, OUTPUT);
  pinMode(SMART_LED_PIN, OUTPUT);

  mainDoor.attach(MAIN_DOOR_PIN, 500, 2400); mainDoor.write(0);
  window1.attach(WINDOW1_PIN,    500, 2400); window1.write(0);
  window2.attach(WINDOW2_PIN,    500, 2400); window2.write(0);

  // Tạo mutex, queue, event group
  xDataMutex    = xSemaphoreCreateMutex();
  xDoorCmdQueue = xQueueCreate(5, sizeof(bool));
  xSystemEvents = xEventGroupCreate();

  // Kết nối WiFi
  Serial.print("Connecting WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println(" Connected!");
  xEventGroupSetBits(xSystemEvents, EVT_WIFI_READY);

  // Firebase init
  fconfig.host = FIREBASE_HOST;
  fconfig.signer.tokens.legacy_token = FIREBASE_AUTH;
  fconfig.timeout.socketConnection = 30000;
  fconfig.timeout.serverResponse   = 10000;
  fconfig.timeout.rtdbKeepAlive    = 45000;
  Firebase.begin(&fconfig, &auth);
  Firebase.reconnectWiFi(true);
  fbdo.setBSSLBufferSize(4096, 512);
  fbdo.setResponseSize(1024);

  // ======================================================
  //  TẠO CÁC RTOS TASKS VỚI PRIORITY RÕ RÀNG
  //  Thứ tự ưu tiên (cao -> thấp):
  //    5: SensorRead   (realtime nhất)
  //    4: GasFSM       (safety-critical)
  //    3: DoorFSM      (event-driven)
  //    2: LightFSM     (comfort)
  //    2: Firebase     (network I/O, chậm nhất)
  //    1: Logger       (idle-level)
  // ======================================================
  xTaskCreatePinnedToCore(vTaskSensorRead, "SensorRead", 2048, NULL, 5, &hTaskSensor,   1);
  xTaskCreatePinnedToCore(vTaskGasFSM,     "GasFSM",     2048, NULL, 4, &hTaskGasFSM,   1);
  xTaskCreatePinnedToCore(vTaskDoorFSM,    "DoorFSM",    3072, NULL, 3, &hTaskDoorFSM,  1);
  xTaskCreatePinnedToCore(vTaskLightFSM,   "LightFSM",   2048, NULL, 2, &hTaskLightFSM, 1);
  xTaskCreatePinnedToCore(vTaskFirebase,   "Firebase",   8192, NULL, 2, &hTaskFirebase,  0); // Core 0 riêng cho WiFi
  xTaskCreatePinnedToCore(vTaskLogger,     "Logger",     2048, NULL, 1, &hTaskLogger,    1);

  Serial.println("[SETUP] All RTOS tasks created. System running.");
}

// ===========================================================
//  LOOP - Để trống! FreeRTOS scheduler quản lý tất cả
// ===========================================================
void loop() {
  vTaskDelay(portMAX_DELAY); // Nhường vĩnh viễn cho scheduler
}
