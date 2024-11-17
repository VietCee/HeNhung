#include <WiFi.h>
#include <ESP32Servo.h>
#include <LiquidCrystal_I2C.h>
#include <FirebaseESP32.h>
#include <SPI.h>
#include <MFRC522.h>
#include <time.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#define SERVO_PIN 25
Servo servoMotor;
#define IR_IN_PIN1 32  
#define IR_IN_PIN2 33  
#define IR_IN_PIN3 34  
LiquidCrystal_I2C lcd(0x27, 16, 2);

const char* ssid = "Tang 3";
const char* password = "66668888";

FirebaseConfig firebaseConfig;
FirebaseAuth firebaseAuth;
FirebaseData firebaseData;

int spotsLeft = 3;
int lastIrStatus1 = HIGH;
int lastIrStatus2 = HIGH;
int lastIrStatus3 = HIGH;  

#define RST_PIN 4
#define SS_PIN 5
MFRC522 mfrc522(SS_PIN, RST_PIN);

float totalAmount = 0.0;
int swipeCountRegular = 0;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7 * 3600);

// Cấu hình NTP
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7 * 3600;  // GMT+7 cho Việt Nam
const int daylightOffset_sec = 0;


bool monthlyPassActive = false;

bool parkingFull = false;  // Biến flag kiểm tra trạng thái bãi đỗ xe

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");

  // Cấu hình thời gian từ NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  firebaseConfig.host = "final-project-parking-car-default-rtdb.firebaseio.com";
  firebaseConfig.api_key = "AIzaSyCFnFAYJYhU_4EEXLg2__ckZrUVIbCXpiA";
  firebaseConfig.database_url = "https://final-project-parking-car-default-rtdb.firebaseio.com";

  firebaseAuth.user.email = "buiviet57.vm@gmail.com";
  firebaseAuth.user.password = "Buiviet57#";

  Firebase.begin(&firebaseConfig, &firebaseAuth);
  Firebase.reconnectWiFi(true);

  if (Firebase.ready()) {
    Serial.println("Firebase is ready!");
  } else {
    Serial.print("Firebase write failed: ");
    Serial.println(firebaseData.errorReason());
  }

  servoMotor.attach(SERVO_PIN);
  servoMotor.write(0);
  delay(1000);
  servoMotor.write(90);
  delay(1000);
  servoMotor.write(0);

  pinMode(IR_IN_PIN1, INPUT);
  pinMode(IR_IN_PIN2, INPUT);
  pinMode(IR_IN_PIN3, INPUT);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Parking System");
  delay(2000);
  lcd.clear();

  timeClient.begin();
  timeClient.update();

  SPI.begin();
  mfrc522.PCD_Init();
  Serial.println("Place your RFID card...");
}

void loop() {
  int irStatus1 = digitalRead(IR_IN_PIN1);
  int irStatus2 = digitalRead(IR_IN_PIN2);
  int irStatus3 = digitalRead(IR_IN_PIN3);

  spotsLeft = 3;

  // Cập nhật trạng thái cảm biến và số chỗ còn trống
  if (irStatus1 == LOW) {
    spotsLeft--;
    if (lastIrStatus1 != LOW) {
      Serial.println("Sensor 1: Có xe đỗ");
      Firebase.setString(firebaseData, "/parking_spots/spot1", "Có xe đỗ");
    }
  } else {
    if (lastIrStatus1 != HIGH) {
      Serial.println("Sensor 1: Còn trống");
      Firebase.setString(firebaseData, "/parking_spots/spot1", "Còn trống");
    }
  }
  lastIrStatus1 = irStatus1;

  if (irStatus2 == LOW) {
    spotsLeft--;
    if (lastIrStatus2 != LOW) {
      Serial.println("Sensor 2: Có xe đỗ");
      Firebase.setString(firebaseData, "/parking_spots/spot2", "Có xe đỗ");
    }
  } else {
    if (lastIrStatus2 != HIGH) {
      Serial.println("Sensor 2: Còn trống");
      Firebase.setString(firebaseData, "/parking_spots/spot2", "Còn trống");
    }
  }
  lastIrStatus2 = irStatus2;

  if (irStatus3 == LOW) {
    spotsLeft--;
    if (lastIrStatus3 != LOW) {
      Serial.println("Sensor 3: Có xe đỗ");
      Firebase.setString(firebaseData, "/parking_spots/spot3", "Có xe đỗ");
    }
  } else {
    if (lastIrStatus3 != HIGH) {
      Serial.println("Sensor 3: Còn trống");
      Firebase.setString(firebaseData, "/parking_spots/spot3", "Còn trống");
    }
  }
  lastIrStatus3 = irStatus3;

  lcd.setCursor(0, 0);
  lcd.print("Spots left: ");
  lcd.print(spotsLeft);

  if (spotsLeft < 0) {
    spotsLeft = 0;
  }

  Firebase.setInt(firebaseData, "/parking_spots/spots_left", spotsLeft);

  // Kiểm tra nếu bãi đỗ xe đầy
  if (spotsLeft == 0 && !parkingFull) {
    lcd.setCursor(0, 1);
    lcd.print("Parking Full");
    Serial.println("Parking is full. No card swipes allowed.");
    parkingFull = true;  // Đánh dấu bãi đỗ xe đầy
  }

  // Kiểm tra nếu có chỗ trống trở lại
  if (spotsLeft > 0 && parkingFull) {
    lcd.setCursor(0, 1);
    lcd.print("                ");  // Xóa dòng hiển thị "Parking Full"
    parkingFull = false;  // Đánh dấu lại trạng thái bãi đỗ xe không đầy
  }

  // Kiểm tra lệnh từ Serial Monitor
  if (Serial.available() > 0) {
    String input = Serial.readString();
    input.trim();

    if (input == "vethang" && !monthlyPassActive) {
      Serial.println("Đăng ký thẻ...");
      Serial.println("Vui lòng quét mã thẻ để đăng ký...");
      while (!mfrc522.PICC_IsNewCardPresent()) {
        delay(500);
      }

      if (mfrc522.PICC_ReadCardSerial()) {
        String cardID = "";
        for (byte i = 0; i < mfrc522.uid.size; i++) {
          cardID += String(mfrc522.uid.uidByte[i], HEX);
        }
        Serial.println("Card ID: " + cardID);

        struct tm timeInfo;
        if (!getLocalTime(&timeInfo)) {
          Serial.println("Không thể lấy thời gian");
          return;
        }
        char dateStr[20];
        strftime(dateStr, sizeof(dateStr), "%d/%m/%Y", &timeInfo);
        String registrationDate = String(dateStr);

        // Tính toán ngày hết hạn của vé tháng
        char expiryDate[20];
        time_t now = time(nullptr);
        struct tm expiryTime;
        localtime_r(&now, &expiryTime);
        expiryTime.tm_mday += 30;
        mktime(&expiryTime);  // Cập nhật thời gian hợp lệ
        strftime(expiryDate, sizeof(expiryDate), "%d/%m/%Y", &expiryTime);

        String monthlyPassPath = "/monthly_pass/" + cardID;
        Firebase.setString(firebaseData, monthlyPassPath + "/status", "Inactive");
        Firebase.setString(firebaseData, monthlyPassPath + "/registration_date", registrationDate);
        Firebase.setString(firebaseData, monthlyPassPath + "/expiry_date", String(expiryDate));

        monthlyPassActive = true;
        Serial.println("Vé tháng đã được đăng ký!");
        delay(3000);
      }
    }
  }

  // Kiểm tra nếu thẻ RFID được quẹt và không có lỗi bãi đỗ xe
  if (!parkingFull && mfrc522.PICC_IsNewCardPresent()) {
    if (mfrc522.PICC_ReadCardSerial()) {
      String cardID = "";
      for (byte i = 0; i < mfrc522.uid.size; i++) {
        cardID += String(mfrc522.uid.uidByte[i], HEX);
      }
      Serial.println("Card ID: " + cardID);

      lcd.setCursor(0, 1);
      lcd.print("ID:");
      lcd.print(cardID);
      delay(2000);

      lcd.setCursor(0, 1);  
      lcd.print("                "); 

      String path = "/parking_data/" + cardID;
      String monthlyPassPath = "/monthly_pass/" + cardID;

      // Kiểm tra nếu thẻ là vé tháng
      if (Firebase.getString(firebaseData, monthlyPassPath + "/status")) {
        String cardStatus = firebaseData.stringData();

        if (cardStatus == "Active") {
          // Thẻ tháng còn hiệu lực
          int swipeCountMonthly;
          if (Firebase.getInt(firebaseData, monthlyPassPath + "/swipe_count")) {
            swipeCountMonthly = firebaseData.intData() + 1;
          } else {
            swipeCountMonthly = 1;
          }
          Firebase.setInt(firebaseData, monthlyPassPath + "/swipe_count", swipeCountMonthly);

          Serial.println("Thẻ tháng hợp lệ");

          String timeStr = timeClient.getFormattedTime();
          if (swipeCountMonthly % 2 == 1) {
            Firebase.setString(firebaseData, monthlyPassPath + "/entry_time", timeStr);
          } else {
            Firebase.setString(firebaseData, monthlyPassPath + "/exit_time", timeStr);
          }

          servoMotor.write(90); 
          delay(2000);
          servoMotor.write(0); 
          return;

        } else if (cardStatus == "Inactive") {
          // Thẻ tháng không hợp lệ
          Serial.println("Thẻ tháng không hợp lệ. Vui lòng thanh toán để kích hoạt lại.");
          lcd.setCursor(0, 1);
          lcd.print("Inactive Card");  // Hiển thị trạng thái trên màn hình
          delay(2000); // Hiển thị thông báo trong 2 giây
          lcd.setCursor(0, 1);
          lcd.print("                ");  // Xóa dòng hiển thị
          return;
        }
      }


      // Xử lý cho vé thường
      if (Firebase.getInt(firebaseData, path + "/swipe_count")) {
        swipeCountRegular = firebaseData.intData() + 1;
      } else {
        swipeCountRegular = 1;  
      }

      // Kiểm tra giới hạn tổng tiền
      if (Firebase.getFloat(firebaseData, path + "/total_amount")) {
        totalAmount = firebaseData.floatData();
      } else {
        totalAmount = 0.0;  
      }   

      if (totalAmount >= 100000) {
        Serial.println("Tổng số tiền vượt quá giới hạn. Vui lòng thanh toán.");
        lcd.setCursor(0, 1);
        lcd.print("Pay overdue!"); 
        delay(2000);
        lcd.setCursor(0, 1);
        lcd.print("                "); 
        return;  
      }


      Firebase.setInt(firebaseData, path + "/swipe_count", swipeCountRegular);
      Serial.println("Thẻ thường");
      
      String timeStr = timeClient.getFormattedTime();
      if (swipeCountRegular % 2 == 1) {
        Firebase.setString(firebaseData, path + "/entry_time", timeStr);
      } else {
        Firebase.setString(firebaseData, path + "/exit_time", timeStr);
      }

      // Tăng tiền gửi xe
      totalAmount += 10000; 
      Firebase.setFloat(firebaseData,path + "/total_amount", totalAmount);

      servoMotor.write(90); 
      delay(2000);
      servoMotor.write(0); 
    }
  }
}
