#include <WiFi.h>
#include <FirebaseESP32.h>
#include <ESP32Servo.h> 

// ========== 1. CẤU HÌNH WIFI & FIREBASE ==========
const char* ssid = "Batman6176"; //ten wifi
const char* password = "nhatnguyen6176";  //pass wifi

#define FIREBASE_HOST "smarthome-iot-2d485-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "uRVh255jtrxe9rl8VHgg2On8zpA0zj7j4SL8RbrV"

// ========== 2. CHÂN GPIO ==========
const int gasPin = 34;        
const int lightSensorPin = 35; 

// --- OUTPUT ĐỘNG CƠ ---
const int mainDoorPin = 13;    
const int window1Pin = 25;     
const int window2Pin = 26;     

// --- OUTPUT ĐÈN & CÒI ---
const int buzzPin = 12;
const int alertLedPin = 14;    
const int smartLedPin = 27;    

// KHOẢNG TRỄ (HYSTERESIS)
const int GAS_HIGH_THRESHOLD = 1000; 
const int GAS_LOW_THRESHOLD = 900;   

// ========== 3. KHAI BÁO BIẾN ==========
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

Servo mainDoor;   
Servo window1; 
Servo window2;  

int gasValue = 0;
int isDark = 0; 

// Biến trạng thái
bool isAutoMode = true;       
bool smartLightState = false; 
bool lastSmartLightState = false; 
bool isGasAlarmActive = false; 

// --- TIMER QUẢN LÝ (QUAN TRỌNG) ---
// 1. Timer đọc Config Đèn (Auto/Manual)
unsigned long lastConfigCheck = 0;
const unsigned long CONFIG_CHECK_INTERVAL = 2000; // 2 giây check 1 lần

// 2. Timer gửi dữ liệu (Data Upload) - để 1s cho ổn định
unsigned long lastFirebaseUpdate = 0;
const unsigned long FIREBASE_UPDATE_INTERVAL = 1000; 

// 3. [MỚI] Timer check lệnh AI - Để tránh làm lag cảm biến
unsigned long lastAICheck = 0;
const unsigned long AI_CHECK_INTERVAL = 1500; // 1.5 giây mới check lệnh mở cửa 1 lần

void setup() {
  Serial.begin(115200);
  
  pinMode(gasPin, INPUT);
  pinMode(lightSensorPin, INPUT);
  pinMode(buzzPin, OUTPUT);
  pinMode(alertLedPin, OUTPUT); 
  pinMode(smartLedPin, OUTPUT); 
  
  mainDoor.attach(mainDoorPin, 500, 2400); mainDoor.write(0); 
  window1.attach(window1Pin, 500, 2400); window1.write(0); 
  window2.attach(window2Pin, 500, 2400); window2.write(0); 

  Serial.println("\nConnecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nConnected!");
  
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  
  config.timeout.socketConnection = 30000; 
  config.timeout.serverResponse = 10 * 1000;
  config.timeout.rtdbKeepAlive = 45 * 1000;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Cấu hình tối ưu RAM
  fbdo.setBSSLBufferSize(4096, 512); 
  fbdo.setResponseSize(1024);
}

// --- HÀM IN LOG ---
void printSystemStatus() {
  String gasStatus = isGasAlarmActive ? "!!! NGUY HIEM !!!" : "An toan";
  String doorStatus = isGasAlarmActive ? "MO (KHI GAS)" : "DONG/AI";
  
  Serial.println("---------------- [ TRANG THAI HE THONG ] ----------------");
  Serial.printf("| INPUT | Gas(D34): %-4d [%s]\n", gasValue, gasStatus.c_str());
  Serial.printf("| STATE | Alarm Mode:    %s\n", isGasAlarmActive ? "ON" : "OFF");
  Serial.println("|-------+------------------------------------------------");
  Serial.printf("| MOTOR | All Doors:     %s\n", doorStatus.c_str());
  Serial.println("=========================================================\n");
}

// --- HÀM XỬ LÝ ĐÈN THÔNG MINH ---
void handleSmartLight() {
  unsigned long currentMillis = millis();

  // 1. Đọc Config từ mạng (KHÔNG LIÊN TỤC)
  if (currentMillis - lastConfigCheck >= CONFIG_CHECK_INTERVAL) {
    if (Firebase.ready()) {
      // Chỉ khi đến lượt mới đi hỏi Firebase
      if (Firebase.getBool(fbdo, "/smarthome/config/auto_mode")) {
        if (fbdo.dataType() == "boolean") isAutoMode = fbdo.boolData();
      }
      if (!isAutoMode) {
        if (Firebase.getBool(fbdo, "/smarthome/commands/smart_light")) {
           if (fbdo.dataType() == "boolean") smartLightState = fbdo.boolData();
        }
      }
    }
    lastConfigCheck = currentMillis;
  }

  // 2. Xử lý Logic (LIÊN TỤC - REALTIME)
  if (isAutoMode) {
    // Phản hồi ngay lập tức với cảm biến
    if (isDark == HIGH) smartLightState = true;
    else smartLightState = false;
  } 

  // 3. Điều khiển phần cứng (LIÊN TỤC)
  digitalWrite(smartLedPin, smartLightState ? HIGH : LOW);

  // 4. Gửi trạng thái ngược lại (Chỉ khi thay đổi)
  if (smartLightState != lastSmartLightState) {
    if (Firebase.ready()) {
       if (isAutoMode) {
          Firebase.setBool(fbdo, "/smarthome/status/smart_light", smartLightState);
       }
    }
    lastSmartLightState = smartLightState; 
  }
}

// --- HÀM AI COMMAND (ĐÃ SỬA ĐỂ KHÔNG GÂY LAG) ---
void checkAICommand() {
  unsigned long currentMillis = millis();
  
  // [QUAN TRỌNG] Chỉ kiểm tra AI mỗi 1.5 giây một lần
  // Nếu chưa đến giờ kiểm tra, hàm này thoát ngay lập tức -> CPU rảnh để xử lý đèn
  if (currentMillis - lastAICheck < AI_CHECK_INTERVAL) {
    return; 
  }

  // Đến đây nghĩa là đã đủ 1.5 giây, mới bắt đầu hỏi Firebase
  if (Firebase.ready()) {
    if (Firebase.getBool(fbdo, "/smarthome/commands/ai_door")) {
      if (fbdo.dataType() == "boolean" && fbdo.boolData() == true) {
          Serial.println("\n[EVENT] >>> AI FACE ID OK! <<<");
          
          mainDoor.write(90); 
          tone(buzzPin, 2000, 100); delay(200); tone(buzzPin, 2000, 100);
          
          delay(5000); // Chấp nhận delay ở đây vì đang mở cửa
          
          mainDoor.write(0);
          Firebase.setBool(fbdo, "/smarthome/commands/ai_door", false);
      }
    }
  }
  
  // Cập nhật lại thời gian đã check
  lastAICheck = currentMillis;
}

void updateFirebaseData() {
  if (Firebase.ready()) {
    Firebase.setInt(fbdo, "/smarthome/data/gas", gasValue);
    Firebase.setInt(fbdo, "/smarthome/data/light_sensor", isDark);
  }
}

void loop() {
  // 1. ĐỌC CẢM BIẾN (Chạy tốc độ ánh sáng)
  gasValue = analogRead(gasPin);
  isDark = digitalRead(lightSensorPin); 

  // 2. LOGIC AN TOÀN (GAS) - ƯU TIÊN SỐ 1
  if (gasValue >= GAS_HIGH_THRESHOLD) {
    isGasAlarmActive = true; 
  } else if (gasValue < GAS_LOW_THRESHOLD) {
    isGasAlarmActive = false; 
  }

  if (isGasAlarmActive) {
    // Báo động thì xử lý ngay
    tone(buzzPin, 1000);
    mainDoor.write(90); window1.write(90); window2.write(90);
    digitalWrite(alertLedPin, HIGH); delay(50); digitalWrite(alertLedPin, LOW); delay(50);
  } else {
    noTone(buzzPin);
    window1.write(0); window2.write(0); mainDoor.write(0);
    digitalWrite(alertLedPin, LOW); 

    // 3. LOGIC THÔNG MINH
    handleSmartLight(); // Đã tối ưu (Non-blocking)
    checkAICommand();   // [ĐÃ SỬA] Đã tối ưu (Timer 1.5s)
  }

  // 4. GỬI DỮ LIỆU ĐỊNH KỲ (Đã tăng lên 3s theo yêu cầu)
  if (millis() - lastFirebaseUpdate >= FIREBASE_UPDATE_INTERVAL) {
    updateFirebaseData();
    lastFirebaseUpdate = millis();
  }
  
  // In log debug
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 1000) {
    printSystemStatus(); 
    lastPrint = millis();
  }
}