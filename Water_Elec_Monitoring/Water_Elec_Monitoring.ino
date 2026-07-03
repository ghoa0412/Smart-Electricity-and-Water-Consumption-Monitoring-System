#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <time.h>

// ====== 1. CẤU HÌNH WIFI ======
#define WIFI_SSID "Home"
#define WIFI_PASSWORD "03091968"

// ====== 2. CẤU HÌNH FIREBASE ======
#define API_KEY "AIzaSyCIadBXM4pFg880vCDWUsv098FuzjVvNRU"
#define DATABASE_URL "https://project-a664b-default-rtdb.firebaseio.com"

#define USER_EMAIL "doan@test.com"
#define USER_PASSWORD "123456"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// ====== 3. CẤU HÌNH PHẦN CỨNG ======
#define FLOW_SENSOR 27
#define CURRENT_SENSOR 34

LiquidCrystal_I2C lcd(0x27, 16, 2);

volatile int pulseCount = 0;
float totalLiters = 0;
const float literPerPulse = 0.00222222;

float energy_Wh = 0;
float calibrationFactor = 0.19;
float smoothPower = 0;

unsigned long lastDisplayTime = 0;
unsigned long lastEnergyTime = 0;
unsigned long lastFirebaseTime = 0;

// ====== 4. CẤU HÌNH TELEGRAM BOT ======
#define BOT_TOKEN "8645121525:AAFsYJRPUDrpV2fk7UOHHjS-LGzBPZZEs_E"
#define CHAT_ID "7532577901"

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
unsigned long lastBotTime = 0;
const unsigned long botRequestDelay = 1000;
bool powerAlertSent = false;

// ====== 5. BIẾN ĐO ĐỘ TRỄ ======
unsigned long t_adc_single_us = 0;  // Một lần analogRead()
unsigned long t1_sensor_us = 0;     // Đọc + xử lý toàn bộ dòng điện

unsigned long samplingWindow_ms = 0; 
unsigned long t2_firebase_ms = 0;   // Upload Firebase + phản hồi

long t3_dashboard_ms = -1;          // Firebase -> Dashboard
float t_monitor_ms = 0;             // Tổng gần đúng

// ====== HÀM NGẮT ĐẾM XUNG NƯỚC ======
void IRAM_ATTR pulseCounter() {
  pulseCount++;
}

// ====== LẤY THỜI GIAN THỰC ======
String getDateTimeString() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "Chua dong bo NTP";

  char buffer[25];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buffer);
}

// ====== ĐỌC ADC MỘT LẦN ĐỂ ĐO t1 ======
int readADCWithTiming() {
  unsigned long startMicros = micros();

  int adcValue = analogRead(CURRENT_SENSOR);

  t_adc_single_us = micros() - startMicros;
  return adcValue;
}

// ====== ĐỌC ĐIỆN ÁP ĐỈNH - ĐỈNH CỦA CẢM BIẾN DÒNG ======
float getVPP() {
  int maxValue = 0;
  int minValue = 4095;
  uint32_t start_time = millis();

  while ((millis() - start_time) < 200) {
    int value = readADCWithTiming();
    if (value > maxValue) maxValue = value;
    if (value < minValue) minValue = value;
  }

  return (maxValue - minValue) * 3.3 / 4095.0;
}

// ====== ĐỌC DÒNG ĐIỆN ======
float readCurrent() {
  unsigned long samplingStart = millis();
float sum = 0;
  for (int i = 0; i < 3; i++) {
    float Vpp = getVPP();
    float Vrms = (Vpp / 2.0) * 0.707;
    float current = (Vrms / 0.1) * calibrationFactor;
    sum += current;
  }

  samplingWindow_ms = millis() - samplingStart;

  float avgCurrent = sum / 3.0;
  if (avgCurrent < 0.20) avgCurrent = 0;
  return avgCurrent;
}

// ====== HÀM XỬ LÝ TELEGRAM ======
void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != CHAT_ID) {
      bot.sendMessage(chat_id, "Xin loi, ban khong co quyen truy cap he thong!", "");
      continue;
    }

    String text = bot.messages[i].text;
    Serial.println(">>> Nhan lenh tu Bot: " + text);

    if (text == "/start") {
      String msg = "🤖 CHÀO MỪNG BẠN ĐẾN VỚI HỆ THỐNG GIÁM SÁT IoT!\n\n";
      msg += "Hãy gõ các lệnh sau để tra cứu:\n";
      msg += "⚡ /dien - Xem thông số điện năng\n";
      msg += "💧 /nuoc - Xem thông số lưu lượng nước\n";
      msg += "⏱ /latency - Xem t1, t2, t3";
      bot.sendMessage(chat_id, msg, "");
    }
    else if (text == "/dien") {
      String msg = "⚡ THÔNG SỐ ĐIỆN HIỆN TẠI:\n";
      msg += "- Công suất: " + String(smoothPower, 1) + " W\n";
      msg += "- Điện năng tiêu thụ: " + String(energy_Wh, 3) + " Wh";
      bot.sendMessage(chat_id, msg, "");
    }
    else if (text == "/nuoc") {
      String msg = "💧 THÔNG SỐ NƯỚC HIỆN TẠI:\n";
      msg += "- Tổng lưu lượng: " + String(totalLiters, 2) + " Lít";
      bot.sendMessage(chat_id, msg, "");
    }
    else if (text == "/latency") {
      String msg = "KET QUA LATENCY:\n";

      msg += "- ADC 1 lan: ";
      msg += String(t_adc_single_us);
      msg += " us\n";

      msg += "- t1 Sensor: ";
      msg += String(t1_sensor_us / 1000.0, 2);
      msg += " ms\n";

      msg += "- Sampling RMS: ";
      msg += String(samplingWindow_ms);
      msg += " ms\n";

      msg += "- t2 Firebase: ";
      msg += String(t2_firebase_ms);
      msg += " ms\n";

    if (t3_dashboard_ms >= 0) {
      msg += "- t3 Dashboard: ";
      msg += String(t3_dashboard_ms);
      msg += " ms\n";

      msg += "- T monitor: ";
      msg += String(t_monitor_ms, 2);
      msg += " ms";
    } else {
      msg += "- t3 Dashboard: Chua co du lieu Dashboard";
  }
    bot.sendMessage(chat_id, msg, "");
    }
  }
}

// ====== ĐỌC t3 DO DASHBOARD GHI LÊN FIREBASE ======
void readT3FromFirebase() {
  if (Firebase.RTDB.getInt(
        &fbdo,
        "ThongSo/Latency/Dashboard/T3_Dashboard_ms"
      )) {

    t3_dashboard_ms = fbdo.intData();
  }
}

// ====== GỬI DỮ LIỆU VÀ ĐỘ TRỄ LÊN FIREBASE ======
void sendDataToFirebase() {
  FirebaseJson dataJson;

  // ====== DỮ LIỆU GIÁM SÁT ======
  dataJson.set("CongSuat_W", smoothPower);
  dataJson.set("DienNang_Wh", energy_Wh);
  dataJson.set("TongNuoc_L", totalLiters);

  dataJson.set("ESP_Time", getDateTimeString());

  // Firebase tự ghi thời gian máy chủ
  dataJson.set("Server_Timestamp_ms/.sv", "timestamp");

  // ====== ĐO t2 ======
  unsigned long t2Start = millis();

  bool ok = Firebase.RTDB.updateNode(
    &fbdo,
    "ThongSo/HienTai",
    &dataJson
  );

  t2_firebase_ms = millis() - t2Start;

  if (ok) {
    Serial.print("Firebase OK | t2 = ");
    Serial.print(t2_firebase_ms);
    Serial.println(" ms");
  } else {
    Serial.println("Firebase loi: " + fbdo.errorReason());
  }

  // Đọc t3 mới nhất do Dashboard ghi về
  readT3FromFirebase();

  // Tổng độ trễ chỉ mang tính gần đúng
  t_monitor_ms = (t1_sensor_us / 1000.0) + t2_firebase_ms;

  if (t3_dashboard_ms >= 0) {
    t_monitor_ms += t3_dashboard_ms;
  }

  // ====== GỬI THÔNG TIN LATENCY CỦA ESP32 ======
  FirebaseJson latencyJson;

  latencyJson.set("T_ADC_Single_us", t_adc_single_us);
  latencyJson.set("T1_Sensor_us", t1_sensor_us);
  latencyJson.set("Sampling_Window_ms", samplingWindow_ms);
  latencyJson.set("T2_Firebase_ms", t2_firebase_ms);
  latencyJson.set("T_Monitor_Approx_ms", t_monitor_ms);

  latencyJson.set("ESP_Time", getDateTimeString());
  latencyJson.set("Server_Timestamp_ms/.sv", "timestamp");

  if (Firebase.RTDB.updateNode(
        &fbdo,
        "ThongSo/Latency/ESP",
        &latencyJson
      )) {

    Serial.println("Da cap nhat Latency ESP");
  } else {
    Serial.println("Loi ghi Latency: " + fbdo.errorReason());
  }

  // ====== IN RA SERIAL MONITOR ======
  Serial.println("======= LATENCY =======");

  Serial.print("ADC 1 lan: ");
  Serial.print(t_adc_single_us);
  Serial.println(" us");

  Serial.print("t1 Sensor: ");
  Serial.print(t1_sensor_us / 1000.0, 3);
  Serial.println(" ms");

  Serial.print("Sampling RMS: ");
  Serial.print(samplingWindow_ms);
  Serial.println(" ms");

  Serial.print("t2 Firebase: ");
  Serial.print(t2_firebase_ms);
  Serial.println(" ms");

  Serial.print("t3 Dashboard: ");
  if (t3_dashboard_ms >= 0) {
    Serial.print(t3_dashboard_ms);
    Serial.println(" ms");
  } else {
    Serial.println("Chua co du lieu");
  }

  Serial.print("T monitor gan dung: ");
  Serial.print(t_monitor_ms, 2);
  Serial.println(" ms");

  Serial.println("=======================");
}

// ====== SETUP ======
void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(FLOW_SENSOR, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR), pulseCounter, FALLING);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Dang ket noi");
  lcd.setCursor(0, 1);
  lcd.print("WiFi...      ");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Dang ket noi WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nWiFi OK!");
  Serial.print("IP ESP32: ");
  Serial.println(WiFi.localIP());

  Serial.print("Dang dong bo thoi gian mang");
  configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  time_t nowTime = time(nullptr);
  while (nowTime < 24 * 3600) {
    Serial.print(".");
    delay(500);
    nowTime = time(nullptr);
  }
  Serial.println("\nDong bo thoi gian thanh cong!");

  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  lcd.setCursor(0, 0);
  lcd.print("WiFi OK!        ");
  lcd.setCursor(0, 1);
  lcd.print("Ket noi Data... ");

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  delay(2000);
  lcd.clear();
  lastEnergyTime = millis();

  bot.sendMessage(CHAT_ID, "🟢 Hệ thống Giám sát ESP32 đã khởi động và kết nối mạng!", "");
}

// ====== LOOP ======
void loop() {
  unsigned long now = millis();
  unsigned long t1Start = micros();

  float current = readCurrent();
  float power = 220.0 * current;

  if (power < 45.0) {
    power = 0;
    smoothPower = 0;
  } else {
    smoothPower = 0.6 * smoothPower + 0.4 * power;
  }

// t1: thời gian đọc ACS712 và xử lý công suất
  t1_sensor_us = micros() - t1Start;

// Lấy lại thời gian sau khi đọc mẫu RMS xong
  now = millis();

  float deltaTime = (now - lastEnergyTime) / 1000.0;
    lastEnergyTime = now;

  if (power > 0) {
    energy_Wh += (power * deltaTime) / 3600.0;
  }

  // 2. CẬP NHẬT LCD MỖI 1 GIÂY
  if (now - lastDisplayTime >= 1000) {
    lastDisplayTime = now;

    noInterrupts();
    int pulseCopy = pulseCount;
    pulseCount = 0;
    interrupts();

    totalLiters += pulseCopy * literPerPulse;

    lcd.setCursor(0, 0);
    lcd.print("Total W:");
    lcd.print(totalLiters, 1);
    lcd.print("L    ");

    lcd.setCursor(0, 1);
    lcd.print("Total E:");
    lcd.print(energy_Wh, 2);
    lcd.print("Wh   ");

    Serial.printf("Water: %.2f L | Power: %.1f W | Energy: %.3f Wh | t1: %.2f ms | ADC: %lu us | Sampling: %lu ms\n",
                  totalLiters, smoothPower, energy_Wh, t1_sensor_us / 1000.0, t_adc_single_us, samplingWindow_ms);
  }

  // 3. GỬI FIREBASE MỖI 3 GIÂY
  if (Firebase.ready() && (now - lastFirebaseTime >= 3000)) {
    lastFirebaseTime = now;
    sendDataToFirebase();
  }

  // 4. KIỂM TRA TELEGRAM MỖI 1 GIÂY
  if (now - lastBotTime > botRequestDelay) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastBotTime = now;
  }

  // 5. CẢNH BÁO QUÁ TẢI
  if (smoothPower > 57.0 && !powerAlertSent) {
    bot.sendMessage(CHAT_ID, "⚠️ CẢNH BÁO NGUY HIỂM: Hệ thống đang quá tải (" + String(smoothPower, 1) + " W)!", "");
    powerAlertSent = true;
  }
  else if (smoothPower < 56.0) {
    powerAlertSent = false;
  }
}