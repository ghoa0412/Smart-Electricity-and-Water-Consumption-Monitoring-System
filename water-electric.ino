#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// Thư viện cho Telegram Bot
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <time.h> // THÊM THƯ VIỆN ĐỂ ĐỒNG BỘ THỜI GIAN

// ====== 1. CẤU HÌNH WIFI ======
#define WIFI_SSID "Home"
#define WIFI_PASSWORD "03091968"

// ====== 2. CẤU HÌNH FIREBASE ======
#define API_KEY "AIzaSyD54qsuXw0WSrppvaNGgsy3Ol9iMasw1Qc"
#define DATABASE_URL "https://tieuthudiennuoc-default-rtdb.asia-southeast1.firebasedatabase.app"

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


// ====== CÁC HÀM CON ======
void IRAM_ATTR pulseCounter() {
  pulseCount++;
}

float getVPP() {
  int maxValue = 0;
  int minValue = 4095;
  uint32_t start_time = millis();

  while ((millis() - start_time) < 200) {
    int value = analogRead(CURRENT_SENSOR);
    if (value > maxValue) maxValue = value;
    if (value < minValue) minValue = value;
  }
  return (maxValue - minValue) * 3.3 / 4095.0;
}

float readCurrent() {
  float sum = 0;
  for (int i = 0; i < 3; i++) {
    float Vpp = getVPP();
    float Vrms = (Vpp / 2.0) * 0.707;
    float current = (Vrms / 0.1) * calibrationFactor;
    sum += current;
  }
  float avgCurrent = sum / 3.0;
  
  // BỘ LỌC CẤP 1: Chặn dòng ảo dưới 0.20A
  if (avgCurrent < 0.20) avgCurrent = 0; 
  return avgCurrent;
}

// Hàm xử lý Telegram
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
      msg += "💧 /nuoc - Xem thông số lưu lượng nước";
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
  }
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

  // --- ĐỒNG BỘ THỜI GIAN (NTP) ---
  Serial.print("Dang dong bo thoi gian mang");
  configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov"); // GMT+7 VN
  time_t now = time(nullptr);
  while (now < 24 * 3600) {
    Serial.print(".");
    delay(500);
    now = time(nullptr);
  }
  Serial.println("\nDong bo thoi gian thanh cong!");
  // ---------------------------------------------

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

// ====== VÒNG LẶP CHÍNH ======
void loop() {
  unsigned long now = millis();

  // 1. ĐỌC CẢM BIẾN VÀ TÍNH TOÁN
  float current = readCurrent();
  float power = 220.0 * current;
  
  // BỘ LỌC CẤP 2: Chặn công suất ảo dưới 45W (dành cho bóng 65W)
  if (power < 45.0) {
    power = 0;
    smoothPower = 0;
  } else {
    smoothPower = 0.6 * smoothPower + 0.4 * power;
  }

  float deltaTime = (now - lastEnergyTime) / 1000.0;
  lastEnergyTime = now;
  
  if (power > 0) {
    energy_Wh += (power * deltaTime) / 3600.0;
  }

  // 2. CẬP NHẬT MÀN HÌNH LCD (Mỗi 1 giây)
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

    Serial.printf("Water: %.2f L | Power: %.1f W | Energy: %.3f Wh\n", totalLiters, smoothPower, energy_Wh);
  }

  // 3. GỬI DỮ LIỆU LÊN FIREBASE (Mỗi 3 giây)
  if (Firebase.ready() && (now - lastFirebaseTime >= 3000)) {
    lastFirebaseTime = now;
    
    bool ok1 = Firebase.RTDB.setFloat(&fbdo, "ThongSo/HienTai/CongSuat_W", smoothPower);
    bool ok2 = Firebase.RTDB.setFloat(&fbdo, "ThongSo/HienTai/DienNang_Wh", energy_Wh);
    bool ok3 = Firebase.RTDB.setFloat(&fbdo, "ThongSo/HienTai/TongNuoc_L", totalLiters);
    
    if (ok1 && ok2 && ok3) {
      Serial.println(">>> Firebase: Da gui du lieu len may!");
    } else {
      Serial.println("XXX Firebase Loi: " + fbdo.errorReason());
    }
  }

  // 4. KIỂM TRA TIN NHẮN TỪ TELEGRAM BOT (Mỗi 1 giây)
  if (now - lastBotTime > botRequestDelay) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastBotTime = now;
  }

  // 5. CHỨC NĂNG CẢNH BÁO TỰ ĐỘNG
  if (smoothPower > 57.0 && !powerAlertSent) {
    bot.sendMessage(CHAT_ID, "⚠️ CẢNH BÁO NGUY HIỂM: Hệ thống đang quá tải (" + String(smoothPower, 1) + " W)!", "");
    powerAlertSent = true; 
  } 
  else if (smoothPower < 56.0) {
    powerAlertSent = false; 
  }
}