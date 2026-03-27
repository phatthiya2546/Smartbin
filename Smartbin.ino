#define BLYNK_TEMPLATE_ID "TMPL63o5j-ldg"
#define BLYNK_TEMPLATE_NAME "Smart Waste Bin"
#define BLYNK_AUTH_TOKEN "WRjl3hiLaF-9tKTKSzHnQV9IUUblih_X"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <ESP32Servo.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// ====== WiFi Configuration ======
char ssid[] = "YMP";
char pass[] = "yam04112546"; 

// ====== LINE Configuration ======
String token = "W+zi78ASXJq+UPelyU1+FhG8AqmdCJ6E/CuzYIyZYDNJCH17Tg6UEhs5N/pJ9z/3rp+vpd6YfwcXUgnR3w6M8L5qBCm7NkE3MYvaav5j4ku4ZmXWvufp1NIRzzso/rHwjZn2H6gCLCDR0Nkd3A04+QdB04t89/1O/w1cDnyilFU=";
String userId = "U57ea00c13a3f6a6553acd6fb74d9bfdb";
// ===== OBJECT =====
Servo myServo;

// ====== Google Sheets Configuration ======
void sendToSheet(String type, int delta, int total, int fill) {
  if (WiFi.status() == WL_CONNECTED) {

    HTTPClient http;
     WiFiClientSecure client;
    client.setInsecure();

    String url = "https://script.google.com/macros/s/AKfycby2XHp559cWUcOWN4t75lM_IcorT98TqP1S8gFEOYWBd1ugbsSNoJ576GwK4ZuvY8w_pw/exec";
  url += "?type=" + type;
  url += "&delta=" + String(delta);
  url += "&total=" + String(total);
  url += "&fill=" + String(fill);

    http.begin(client, url);   
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); 
    int code = http.GET();

    Serial.print("Sheet: ");
    Serial.println(code);
    Serial.println(url);

    http.end();
  }
}

// ---------------- PIN ----------------
const int servoPin      = 18;
const int prox_metal    = 17;
const int prox_infrared = 19;

// ---------------- ANGLE ----------------
const int SERVO_ANGLE_0   = 0;
const int SERVO_ANGLE_90  = 90;
const int SERVO_ANGLE_180 = 165;

// ---------------- CONFIG ----------------
const unsigned long STABLE_TIME = 1000; 

// ---------------- STATE ----------------
bool objectDetected = false;
bool actionDone     = false;
bool readyForNext = true;

unsigned long detectStartTime = 0;

// ---------------- COUNT ----------------
int metalCount = 0;
int plasticCount = 0;

bool binFullSent = false;
bool nearFullSent = false;
bool plasticFullSent = false;
bool metalFullSent = false;
bool metalNearFullSent = false;
bool plasticNearFullSent = false;

      //⭐โลหะเต็ม
void sendLine(String message) {
  Serial.println(">>> CALL sendLine");   // ✅ เช็คว่าเข้าไหม 

  if(WiFi.status() == WL_CONNECTED) {
    Serial.println(">>> WiFi OK");

     WiFiClientSecure client;
    client.setInsecure();  // ⭐ สำคัญมาก

    HTTPClient http;
    http.begin(client,"https://api.line.me/v2/bot/message/push");
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + token);

     // ⭐ ป้องกัน JSON พัง
    message.replace("\n", "\\n");


    String payload = "{\"to\":\"" + userId + "\",\"messages\":[{\"type\":\"text\",\"text\":\"" + message + "\"}]}";
    int httpResponseCode = http.POST(payload);

    Serial.print("LINE Response: ");
    Serial.println(httpResponseCode);

    String response = http.getString();
Serial.println(response);
    http.end();
     } else {
    Serial.println("❌ WiFi หลุด");
  }
}
  
const int MAX_CAPACITY = 10; // ความจุถัง (ปรับได้)
int binLevel = 0;

void setup() {
  Serial.begin(115200);

   // ⭐ Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

sendLine("ทดสอบ LINE OA");

  pinMode(prox_metal, INPUT);
  pinMode(prox_infrared, INPUT);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  myServo.setPeriodHertz(50);
  myServo.attach(servoPin, 500, 2400);
  myServo.write(SERVO_ANGLE_90);

  Serial.println("Start...");
}

void loop() {
 Blynk.run(); // ⭐ สำคัญมาก


  int metalState = digitalRead(prox_metal);
  int irState    = digitalRead(prox_infrared);

  Serial.print("Metal = ");
  Serial.print(metalState);
  Serial.print(" IR = ");
  Serial.println(irState);
  Serial.print("TYPE: ");
  Serial.print("actionDone: ");
  Serial.println(actionDone);
 

  // ---------------- ตรวจว่ามีวัตถุ ----------------
  // เงื่อนไข: IR LOW = มีวัตถุ (คุณใช้แบบนี้อยู่)
  if (irState == LOW && readyForNext) {

    // เพิ่งเจอวัตถุครั้งแรก
    if (!objectDetected) {
      objectDetected = true;
      actionDone = false;
      detectStartTime = millis();

      Serial.println("Object detected -> start timer");
    }

    if (!actionDone && (millis() - detectStartTime >= STABLE_TIME)) {

      Serial.println("Stable 1 sec -> classify"); // รอครบ 1 วิ
      int totalTrash = metalCount + plasticCount;
      binLevel = (totalTrash * 100) / MAX_CAPACITY;
      binLevel = constrain(binLevel, 0, 100);
   
    // ---------------- ตัดสินใจ ----------------
if (metalState == LOW) {

  // ===== โลหะ =====
  myServo.write(SERVO_ANGLE_0);
  Serial.println("Metal -> Servo 0");

  metalCount++;
  Blynk.virtualWrite(V0, metalCount);
  sendLine("โลหะ +1 | รวม: " + String(metalCount));

  binLevel = (totalTrash * 100) / MAX_CAPACITY;
  binLevel = constrain(binLevel, 0, 100);

  sendToSheet("Metal", 1, totalTrash, binLevel);
    delay(500);
  if (metalCount >= 7 && metalCount < 10 && !metalNearFullSent) {
    sendLine("⚠️ โลหะใกล้เต็มแล้ว!");
    metalNearFullSent = true;
  }

  if (metalCount >= 10 && !metalFullSent) {
    sendLine("♻️ โลหะเต็มแล้ว!");
    metalFullSent = true;
  }

  delay(200);

} else {

  // ===== พลาสติก =====
  myServo.write(SERVO_ANGLE_180);
  Serial.println("Plastic -> Servo 180");

  plasticCount++;
  Blynk.virtualWrite(V1, plasticCount);
  sendLine("พลาสติก +1 | รวม: " + String(plasticCount));

  binLevel = (totalTrash * 100) / MAX_CAPACITY;
  binLevel = constrain(binLevel, 0, 100);

  sendToSheet("Plastic", 1, totalTrash, binLevel);
    delay(500);
  if (plasticCount >= 7 && plasticCount < 10 && !plasticNearFullSent) {
    sendLine("⚠️ พลาสติกใกล้เต็มแล้ว!");
    plasticNearFullSent = true;
  }

  if (plasticCount >= 10 && !plasticFullSent) {
    sendLine("🧴 พลาสติกเต็มแล้ว!");
    plasticFullSent = true;
  }

  delay(200);
}

binLevel = (totalTrash * 100) / MAX_CAPACITY;
binLevel = constrain(binLevel, 0, 100);


    Blynk.virtualWrite(V2, binLevel);

          // รีเซ็ต
    if (binLevel < 50) {
        nearFullSent = false;
        binFullSent = false;
    if (metalCount < 5) metalNearFullSent = false;
if (plasticCount < 5) plasticNearFullSent = false;
}

      // กลับกลาง
      myServo.write(SERVO_ANGLE_90);
      Serial.println("Servo -> 90");

      actionDone = true;

     
    }
  }

  // ---------------- วัตถุออก ----------------
  else {
    if (objectDetected) {
      Serial.println("Object removed -> reset");
    }

    objectDetected = false;
    actionDone = false;

    readyForNext = true;
  }
}
BLYNK_WRITE(V3) {   // ⭐ ปุ่ม Reset

  int buttonState = param.asInt();

  if (buttonState == 1) {

    // ===== รีเซตค่า =====
    metalCount = 0;
    plasticCount = 0;
    binLevel = 0;

    // ===== รีเซต flag =====
    binFullSent = false;
    nearFullSent = false;
    plasticFullSent = false;
    metalFullSent = false;
    metalNearFullSent = false;
    plasticNearFullSent = false;

    // ===== อัปเดต Blynk =====
    Blynk.virtualWrite(V0, metalCount);
    Blynk.virtualWrite(V1, plasticCount);
    Blynk.virtualWrite(V2, binLevel);


    // ⭐ ปิดปุ่มเอง
    Blynk.virtualWrite(V3, 0);
  }
}