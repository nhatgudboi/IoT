/*
 * ============================================================
 * SmartHome IoT Dashboard - ESP32 (FreeRTOS Edition)
 * Nhóm: N6_HTTT_PTIT
 * Kiến trúc: FreeRTOS Multi-Task
 * ============================================================
 *
 * PHÂN TẦNG KIẾN TRÚC (Architecture Layers):
 * ┌─────────────────────────────────────────────┐
 * │         APPLICATION LAYER                   │
 * │  Task_Firebase | Task_SmartLight | Task_AI  │
 * ├─────────────────────────────────────────────┤
 * │         SAFETY LAYER (HIGHEST PRIO)         │
 * │         Task_GasSafety (ISR → Notify)       │
 * ├─────────────────────────────────────────────┤
 * │         HARDWARE ABSTRACTION LAYER          │
 * │  GPIO | ADC | PWM (Servo) | UART            │
 * └─────────────────────────────────────────────┘
 *
 * TASK MAP:
 * ┌───────────────────┬─────────┬────────────┬────────────┐
 * │ Task Name         │ Core    │ Priority   │ Period     │
 * ├───────────────────┼─────────┼────────────┼────────────┤
 * │ Task_GasSafety    │ Core 1  │ 5 (HIGH)   │ ISR + 20ms │
 * │ Task_FirebaseUpld │ Core 0  │ 3 (MED)    │ 1000ms     │
 * │ Task_SmartLight   │ Core 1  │ 2 (MED-LO) │ 100ms      │
 * │ Task_AICommand    │ Core 0  │ 1 (LOW)    │ 1500ms     │
 * │ Task_StatusLog    │ Core 1  │ 1 (LOW)    │ 2000ms     │
 * └───────────────────┴─────────┴────────────┴────────────┘
 */

#include <WiFi.h>
#include <FirebaseESP32.h>
#include <ESP32Servo.h>

// FreeRTOS headers (built-in với ESP32 Arduino)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"

// ============================================================
// 1. CẤU HÌNH WIFI & FIREBASE
// ============================================================
const char* ssid     = "Batman6176";
const char* password = "nhatnguyen6176";

#define FIREBASE_HOST "smarthome-iot-2d485-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "uRVh255jtrxe9rl8VHgg2On8zpA0zj7j4SL8RbrV"

// ============================================================
// 2. KHAI BÁO CHÂN GPIO (Hardware Abstraction Layer)
// ============================================================
// --- INPUT ---
#define PIN_GAS_SENSOR    34
#define PIN_LIGHT_SENSOR  35

// --- SERVO (OUTPUT) ---
#define PIN_MAIN_DOOR     13
#define PIN_WINDOW_1      25
#define PIN_WINDOW_2      26

// --- LED & BUZZER (OUTPUT) ---
#define PIN_BUZZER        12
#define PIN_ALERT_LED     14
#define PIN_SMART_LED     27

// ============================================================
// 3. NGƯỠNG & HẰNG SỐ HỆ THỐNG
// ============================================================
#define GAS_HIGH_THRESHOLD   1000
#define GAS_LOW_THRESHOLD    900
#define AI_DOOR_OPEN_TIME_MS 5000  // Thời gian cửa mở sau khi AI nhận diện

// Task periods (ms)
#define PERIOD_GAS_CHECK      20
#define PERIOD_FIREBASE_UPLD  1000
#define PERIOD_SMART_LIGHT    100
#define PERIOD_AI_CMD         1500
#define PERIOD_STATUS_LOG     2000
#define PERIOD_FB_CONFIG      2000

// ============================================================
// 4. BIẾN CHIA SẺ GIỮA CÁC TASK (Shared State)
//    → Bảo vệ bằng Mutex để tránh race condition
// ============================================================
struct SystemState {
    int  gasValue         = 0;
    int  isDark           = 0;
    bool isGasAlarm       = false;
    bool smartLightOn     = false;
    bool isAutoMode       = true;
    bool aiDoorOpen       = false;       // true = đang mở cửa AI
    bool aiDoorPending    = false;       // true = Firebase ra lệnh mở, chờ xử lý
};

volatile SystemState sysState;          // volatile → báo compiler không cache

// ============================================================
// 5. RTOS PRIMITIVES
// ============================================================
SemaphoreHandle_t xStateMutex;          // Bảo vệ sysState khi đọc/ghi từ nhiều task
SemaphoreHandle_t xFirebaseMutex;       // Bảo vệ fbdo (FirebaseData không thread-safe)

TaskHandle_t xTaskGasSafety    = NULL;
TaskHandle_t xTaskFirebase     = NULL;
TaskHandle_t xTaskSmartLight   = NULL;
TaskHandle_t xTaskAICommand    = NULL;
TaskHandle_t xTaskStatusLog    = NULL;

// Timer FreeRTOS để đóng cửa AI sau 5 giây (non-blocking!)
TimerHandle_t xAIDoorCloseTimer = NULL;

// ============================================================
// 6. FIREBASE & SERVO OBJECTS
// ============================================================
FirebaseData  fbdo_upload;   // Object riêng cho upload data  (Task_Firebase)
FirebaseData  fbdo_cmd;      // Object riêng cho đọc command  (Task_AI, Task_SmartLight)
FirebaseAuth  auth;
FirebaseConfig config;

Servo mainDoor;
Servo window1;
Servo window2;

// ============================================================
// 7. HARDWARE ABSTRACTION LAYER (HAL)
//    Các hàm điều khiển phần cứng tập trung tại đây
// ============================================================

/** Mở toàn bộ cửa (Gas Emergency) */
inline void HAL_OpenAllDoors() {
    mainDoor.write(90);
    window1.write(90);
    window2.write(90);
}

/** Đóng toàn bộ cửa */
inline void HAL_CloseAllDoors() {
    mainDoor.write(0);
    window1.write(0);
    window2.write(0);
}

/** Mở cửa chính (AI Unlock) */
inline void HAL_OpenMainDoor() {
    mainDoor.write(90);
}

/** Đóng cửa chính */
inline void HAL_CloseMainDoor() {
    mainDoor.write(0);
}

/** Bật còi báo động */
inline void HAL_BuzzerAlert() {
    tone(PIN_BUZZER, 1000);
}

/** Tắt còi */
inline void HAL_BuzzerOff() {
    noTone(PIN_BUZZER);
}

/** Beep ngắn khi AI mở cửa (non-blocking: dùng tone() tự tắt theo duration) */
inline void HAL_BuzzerBeepAI() {
    tone(PIN_BUZZER, 2000, 100);
}

/** Đọc giá trị gas (ADC) */
inline int HAL_ReadGas() {
    return analogRead(PIN_GAS_SENSOR);
}

/** Đọc cảm biến ánh sáng (Digital) */
inline int HAL_ReadLight() {
    return digitalRead(PIN_LIGHT_SENSOR);
}

// ============================================================
// 8. CALLBACK TIMER: Đóng cửa AI sau 5 giây
//    Chạy trong context của Timer daemon task, KHÔNG dùng delay()
// ============================================================
void vAIDoorCloseCallback(TimerHandle_t xTimer) {
    HAL_CloseMainDoor();

    // Cập nhật state và gửi lại Firebase (dùng mutex)
    if (xSemaphoreTake(xStateMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        sysState.aiDoorOpen = false;
        xSemaphoreGive(xStateMutex);
    }

    if (xSemaphoreTake(xFirebaseMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        if (Firebase.ready()) {
            Firebase.setBool(fbdo_cmd, "/smarthome/commands/ai_door", false);
        }
        xSemaphoreGive(xFirebaseMutex);
    }

    Serial.println("[TIMER] Cua AI da dong sau 5 giay.");
}

// ============================================================
// 9. TASK ĐỊNH NGHĨA
// ============================================================

/**
 * Task 1: GAS SAFETY (Priority: 5 - CAO NHẤT)
 * Core: 1 | Period: 20ms
 * Mục đích: Đọc cảm biến gas và kích hoạt chế độ báo động.
 * ĐÂY LÀ TASK AN TOÀN - không bao giờ bị block bởi task khác.
 */
void vTask_GasSafety(void* pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(PERIOD_GAS_CHECK);

    for (;;) {
        // Đọc cảm biến (HAL)
        int rawGas  = HAL_ReadGas();
        int rawDark = HAL_ReadLight();

        // Tính trạng thái alarm (Hysteresis)
        bool newAlarm;
        if (xSemaphoreTake(xStateMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            newAlarm = sysState.isGasAlarm; // giữ trạng thái cũ trước
            xSemaphoreGive(xStateMutex);
        } else {
            newAlarm = false;
        }

        if (rawGas >= GAS_HIGH_THRESHOLD) newAlarm = true;
        else if (rawGas < GAS_LOW_THRESHOLD) newAlarm = false;

        // Ghi state (mutex ngắn, không block lâu)
        if (xSemaphoreTake(xStateMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            sysState.gasValue  = rawGas;
            sysState.isDark    = rawDark;  // Giờ là giá trị analog
            sysState.isGasAlarm = newAlarm;
            xSemaphoreGive(xStateMutex);
        }

        // Điều khiển phần cứng NGAY LẬP TỨC (không qua task khác)
        if (newAlarm) {
            HAL_BuzzerAlert();
            HAL_OpenAllDoors();
            // LED nháy bằng millis() thay vì delay()
            static unsigned long lastLedToggle = 0;
            static bool ledState = false;
            unsigned long now = millis();
            if (now - lastLedToggle >= 100) {
                ledState = !ledState;
                digitalWrite(PIN_ALERT_LED, ledState ? HIGH : LOW);
                lastLedToggle = now;
            }
        } else {
            HAL_BuzzerOff();
            HAL_CloseAllDoors();          // Task_AI sẽ override mainDoor nếu cần
            digitalWrite(PIN_ALERT_LED, LOW);
        }

        // Delay chính xác tới chu kỳ tiếp theo (FreeRTOS tick-accurate)
        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

/**
 * Task 2: FIREBASE UPLOAD (Priority: 3)
 * Core: 0 | Period: 1000ms
 * Mục đích: Đẩy dữ liệu cảm biến lên Firebase định kỳ.
 * Chạy trên Core 0 để không cạnh tranh CPU với Safety Task.
 */
void vTask_FirebaseUpload(void* pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(PERIOD_FIREBASE_UPLD);

    for (;;) {
        // Đọc state an toàn
        int gasSnap  = 0;
        int darkSnap = 0;
        if (xSemaphoreTake(xStateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            gasSnap  = sysState.gasValue;
            darkSnap = sysState.isDark;
            xSemaphoreGive(xStateMutex);
        }

        // Ghi Firebase (mutex để không xung đột với task AI)
        if (xSemaphoreTake(xFirebaseMutex, pdMS_TO_TICKS(500)) == pdTRUE) {
            if (Firebase.ready()) {
                Firebase.setInt(fbdo_upload, "/smarthome/data/gas",          gasSnap);
                Firebase.setInt(fbdo_upload, "/smarthome/data/light_sensor",  darkSnap);
            }
            xSemaphoreGive(xFirebaseMutex);
        }

        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

/**
 * Task 3: SMART LIGHT CONTROL (Priority: 2)
 * Core: 1 | Period: 100ms (nhanh để phản hồi cảm biến)
 * Mục đích: Đọc config auto/manual, điều khiển đèn thông minh.
 */
void vTask_SmartLight(void* pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(PERIOD_SMART_LIGHT);

    unsigned long lastConfigFetch = 0;
    bool lastLightState = false;

    for (;;) {
        unsigned long now = millis();

        // Đọc config Firebase mỗi 2 giây (tránh spam request)
        if (now - lastConfigFetch >= PERIOD_FB_CONFIG) {
            if (xSemaphoreTake(xFirebaseMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
                if (Firebase.ready()) {
                    // Đọc chế độ Auto/Manual
                    if (Firebase.getBool(fbdo_cmd, "/smarthome/config/auto_mode")) {
                        if (fbdo_cmd.dataType() == "boolean") {
                            bool autoMode = fbdo_cmd.boolData();
                            if (xSemaphoreTake(xStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                                sysState.isAutoMode = autoMode;
                                xSemaphoreGive(xStateMutex);
                            }
                        }
                    }
                    // Nếu Manual mode → đọc lệnh bật/tắt
                    bool currentAutoMode;
                    if (xSemaphoreTake(xStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                        currentAutoMode = sysState.isAutoMode;
                        xSemaphoreGive(xStateMutex);
                    } else {
                        currentAutoMode = true;
                    }

                    if (!currentAutoMode) {
                        if (Firebase.getBool(fbdo_cmd, "/smarthome/commands/smart_light")) {
                            if (fbdo_cmd.dataType() == "boolean") {
                                bool cmd = fbdo_cmd.boolData();
                                if (xSemaphoreTake(xStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                                    sysState.smartLightOn = cmd;
                                    xSemaphoreGive(xStateMutex);
                                }
                            }
                        }
                    }
                }
                xSemaphoreGive(xFirebaseMutex);
            }
            lastConfigFetch = now;
        }

        // Đọc state, tính logic đèn
        bool autoMode, isDark, newLightState, isAlarm;
        if (xSemaphoreTake(xStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            autoMode      = sysState.isAutoMode;
            isDark        = (sysState.isDark == LOW);  // LOW = tối, bật đèn
            newLightState = sysState.smartLightOn;
            isAlarm       = sysState.isGasAlarm;
            xSemaphoreGive(xStateMutex);
        } else {
            vTaskDelayUntil(&xLastWakeTime, xPeriod);
            continue;
        }

        // Khi đang báo động gas → tắt đèn thông minh (an toàn điện)
        if (isAlarm) {
            digitalWrite(PIN_SMART_LED, LOW);
            vTaskDelayUntil(&xLastWakeTime, xPeriod);
            continue;
        }

        // Chế độ Auto: đèn theo cảm biến ánh sáng
        if (autoMode) {
            newLightState = isDark;
        }

        // Điều khiển phần cứng
        digitalWrite(PIN_SMART_LED, newLightState ? HIGH : LOW);

        // Nếu thay đổi trạng thái → cập nhật Firebase
        if (newLightState != lastLightState) {
            if (autoMode) { // chỉ ghi status khi auto mode
                if (xSemaphoreTake(xFirebaseMutex, pdMS_TO_TICKS(200)) == pdTRUE) {
                    if (Firebase.ready()) {
                        Firebase.setBool(fbdo_cmd, "/smarthome/status/smart_light", newLightState);
                    }
                    xSemaphoreGive(xFirebaseMutex);
                }
            }
            // Cập nhật state
            if (xSemaphoreTake(xStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                sysState.smartLightOn = newLightState;
                xSemaphoreGive(xStateMutex);
            }
            lastLightState = newLightState;
        }

        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

/**
 * Task 4: AI DOOR COMMAND (Priority: 1)
 * Core: 0 | Period: 1500ms
 * Mục đích: Poll Firebase để kiểm tra lệnh mở cửa AI.
 * Khi nhận lệnh → Mở cửa NGAY, giao cho FreeRTOS Timer đóng cửa sau 5s.
 * KHÔNG dùng delay() → Safety Task vẫn chạy bình thường.
 */
void vTask_AICommand(void* pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(PERIOD_AI_CMD);

    for (;;) {
        // Không xử lý AI nếu đang có báo động gas
        bool isAlarm;
        if (xSemaphoreTake(xStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            isAlarm = sysState.isGasAlarm;
            xSemaphoreGive(xStateMutex);
        } else {
            isAlarm = true; // Fail-safe
        }

        if (!isAlarm) {
            if (xSemaphoreTake(xFirebaseMutex, pdMS_TO_TICKS(300)) == pdTRUE) {
                if (Firebase.ready()) {
                    FirebaseData fbdo_ai;
                    fbdo_ai.setBSSLBufferSize(1024, 512);
                    fbdo_ai.setResponseSize(512);

                    if (Firebase.getBool(fbdo_ai, "/smarthome/commands/ai_door")) {
                        if (fbdo_ai.dataType() == "boolean" && fbdo_ai.boolData() == true) {

                            bool alreadyOpen;
                            if (xSemaphoreTake(xStateMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                                alreadyOpen = sysState.aiDoorOpen;
                                if (!alreadyOpen) sysState.aiDoorOpen = true;
                                xSemaphoreGive(xStateMutex);
                            } else {
                                alreadyOpen = true;
                            }

                            if (!alreadyOpen) {
                                Serial.println("\n[TASK_AI] >>> AI FACE ID CONFIRMED! Dang mo cua... <<<");

                                // Mở cửa VÀ Beep ngay lập tức
                                HAL_OpenMainDoor();
                                HAL_BuzzerBeepAI();  // tone() tự tắt sau 100ms, KHÔNG block

                                // Khởi động FreeRTOS Timer để đóng cửa sau 5 giây
                                // Timer chạy 1 lần (pdFALSE = one-shot)
                                if (xAIDoorCloseTimer != NULL) {
                                    xTimerReset(xAIDoorCloseTimer, pdMS_TO_TICKS(100));
                                }
                            }
                        }
                    }
                    fbdo_ai.clear();
                }
                xSemaphoreGive(xFirebaseMutex);
            }
        }

        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

/**
 * Task 5: STATUS LOG (Priority: 1)
 * Core: 1 | Period: 2000ms
 * Mục đích: In thông tin hệ thống ra Serial để debug.
 * Dùng stack nhỏ nhất, priority thấp nhất.
 */
void vTask_StatusLog(void* pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(PERIOD_STATUS_LOG);

    for (;;) {
        // Snapshot state an toàn
        int   gas, lightSensor;
        bool  alarm, autoMode, lightOn, aiDoor;
        UBaseType_t gasHighWater, fbHighWater, aiHighWater;

        if (xSemaphoreTake(xStateMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            gas         = sysState.gasValue;
            lightSensor = sysState.isDark;  // Giá trị analog cảm biến ánh sáng
            alarm       = sysState.isGasAlarm;
            autoMode    = sysState.isAutoMode;
            lightOn     = sysState.smartLightOn;
            aiDoor      = sysState.aiDoorOpen;
            xSemaphoreGive(xStateMutex);
        }

        // Đọc stack còn lại (kiểm tra stack overflow)
        gasHighWater = uxTaskGetStackHighWaterMark(xTaskGasSafety);
        fbHighWater  = uxTaskGetStackHighWaterMark(xTaskFirebase);
        aiHighWater  = uxTaskGetStackHighWaterMark(xTaskAICommand);

        Serial.println("============= [ HE THONG SMARTHOME ] =============");
        Serial.printf("| Gas: %-5d | Light Sensor: %-5d | Alarm: %-3s | AI Door: %-3s\n",
                      gas, lightSensor,
                      alarm    ? "ON"  : "OFF",
                      aiDoor   ? "OPEN": "SHUT");
        Serial.printf("| Smart Light: %-3s | Mode: %-7s | Core0 Free: %d bytes\n",
                      lightOn  ? "ON"  : "OFF",
                      autoMode ? "AUTO" : "MANUAL",
                      ESP.getFreeHeap());
        Serial.printf("| Stack HWM → GasSafety: %u | Firebase: %u | AI: %u\n",
                      gasHighWater, fbHighWater, aiHighWater);
        Serial.printf("DEBUG: Light Sensor = %d (LOW=tối) | isDark = %s | isAlarm = %s | autoMode = %s\n", 
                      lightSensor, (lightSensor == LOW) ? "YES" : "NO",
                      alarm ? "YES" : "NO", autoMode ? "YES" : "NO");
        Serial.println("===================================================\n");

        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

// ============================================================
// 10. SETUP
// ============================================================
void setup() {
    Serial.begin(115200);
    Serial.println("\n[BOOT] SmartHome ESP32 FreeRTOS Edition");
    Serial.printf("[BOOT] Free heap: %d bytes\n", ESP.getFreeHeap());

    // --- Khởi tạo GPIO ---
    pinMode(PIN_GAS_SENSOR,   INPUT);
    pinMode(PIN_LIGHT_SENSOR, INPUT);
    pinMode(PIN_BUZZER,       OUTPUT);
    pinMode(PIN_ALERT_LED,    OUTPUT);
    pinMode(PIN_SMART_LED,    OUTPUT);

    // --- Khởi tạo Servo ---
    mainDoor.attach(PIN_MAIN_DOOR, 500, 2400);  mainDoor.write(0);
    window1.attach(PIN_WINDOW_1,   500, 2400);  window1.write(0);
    window2.attach(PIN_WINDOW_2,   500, 2400);  window2.write(0);

    // --- Kết nối WiFi ---
    Serial.print("[WiFi] Connecting");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        vTaskDelay(pdMS_TO_TICKS(500));
        Serial.print(".");
    }
    Serial.printf("\n[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());

    // --- Khởi tạo Firebase ---
    config.host                           = FIREBASE_HOST;
    config.signer.tokens.legacy_token     = FIREBASE_AUTH;
    config.timeout.socketConnection       = 30000;
    config.timeout.serverResponse         = 10000;
    config.timeout.rtdbKeepAlive          = 45000;

    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);

    // Cấu hình buffer tối ưu cho từng FirebaseData object
    fbdo_upload.setBSSLBufferSize(2048, 512);
    fbdo_upload.setResponseSize(512);
    fbdo_cmd.setBSSLBufferSize(2048, 512);
    fbdo_cmd.setResponseSize(512);

    Serial.println("[Firebase] Initialized OK");

    // ============================================================
    // Khởi tạo RTOS Synchronization Primitives
    // ============================================================
    xStateMutex   = xSemaphoreCreateMutex();
    xFirebaseMutex = xSemaphoreCreateMutex();

    if (xStateMutex == NULL || xFirebaseMutex == NULL) {
        Serial.println("[ERROR] Failed to create mutex! Halting.");
        for (;;); // Halt - lỗi nghiêm trọng
    }

    // ============================================================
    // Khởi tạo FreeRTOS Timer (One-shot) để đóng cửa AI
    // ============================================================
    xAIDoorCloseTimer = xTimerCreate(
        "AIDoorClose",                  // Tên timer
        pdMS_TO_TICKS(AI_DOOR_OPEN_TIME_MS), // Thời gian chờ
        pdFALSE,                        // pdFALSE = One-shot (không lặp)
        (void*)0,
        vAIDoorCloseCallback            // Callback khi hết giờ
    );

    if (xAIDoorCloseTimer == NULL) {
        Serial.println("[ERROR] Failed to create AI door timer! Halting.");
        for (;;);
    }

    // ============================================================
    // Tạo FreeRTOS Tasks
    // ============================================================
    // Task_GasSafety: Core 1, Priority 5, Stack 3KB
    xTaskCreatePinnedToCore(
        vTask_GasSafety, "GasSafety",
        3072, NULL, 5, &xTaskGasSafety, 1
    );

    // Task_FirebaseUpload: Core 0, Priority 3, Stack 6KB (Firebase cần nhiều stack)
    xTaskCreatePinnedToCore(
        vTask_FirebaseUpload, "FBUpload",
        6144, NULL, 3, &xTaskFirebase, 0
    );

    // Task_SmartLight: Core 1, Priority 2, Stack 4KB
    xTaskCreatePinnedToCore(
        vTask_SmartLight, "SmartLight",
        4096, NULL, 2, &xTaskSmartLight, 1
    );

    // Task_AICommand: Core 0, Priority 1, Stack 6KB
    xTaskCreatePinnedToCore(
        vTask_AICommand, "AICmd",
        6144, NULL, 1, &xTaskAICommand, 0
    );

    // Task_StatusLog: Core 1, Priority 1, Stack 3KB
    xTaskCreatePinnedToCore(
        vTask_StatusLog, "StatusLog",
        3072, NULL, 1, &xTaskStatusLog, 1
    );

    Serial.println("[BOOT] All FreeRTOS tasks created. System running.");
    Serial.printf("[BOOT] Free heap after init: %d bytes\n", ESP.getFreeHeap());

    // loop() sẽ trống vì mọi thứ chạy qua FreeRTOS Tasks
}

// ============================================================
// 11. LOOP (Trống - FreeRTOS scheduler đã đảm nhận)
// ============================================================
void loop() {
    // Không làm gì ở đây.
    // FreeRTOS scheduler quản lý tất cả tasks.
    // Yield CPU để các task khác chạy, tránh watchdog timeout.
    vTaskDelay(pdMS_TO_TICKS(10000));
}