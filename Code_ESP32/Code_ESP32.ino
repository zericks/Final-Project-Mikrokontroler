#define BLYNK_TEMPLATE_ID "TMPL6yk6QP70r"
#define BLYNK_TEMPLATE_NAME "Smart Door Lock"
#define BLYNK_AUTH_TOKEN "utOmQnLzZbOkcW8HhtPvfsy676TYmfs4"
#define BLYNK_PRINT Serial

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include "SoftwareSerial.h"
SoftwareSerial foto(16, 17); // RX, TX

// Wi-Fi credentials
char ssid[] = "Keluarga Cemara";
char pass[] = "trioami2903";

// Define pins for RFID, buzzer, solenoid, and touch sensor
#define RFID_SDA_PIN 21  // Pin SDA for RFID
#define RFID_RST_PIN 22  // Pin RST for RFID
#define BUZZER 2         // Pin Buzzer
#define BUZZER_BELL 34   // Pin Buzzer Bell
#define Button 35        // Pin Button Bell
#define SOLENOID 15       // Pin Relay Solenoid
#define TOUCH 25         // Pin Touch Sensor
#define ledR 14          // Pin LED Merah
#define ledY 12          // Pin LED Kuning
#define ledG 13          // Pin LED Hijau
// Define pins for LCD I2C
#define LCD_SDA_PIN 27   // Pin SDA for LCD
#define LCD_SCL_PIN 33   // Pin SCL for LCD
// DEfine pins for Ultrasonic Sensor
#define TRIG_PIN 5
#define ECHO_PIN 4

// Pengaturan Serial untuk komunikasi dengan ESP32-CAM
#define SERIAL_CAM Serial2

// Instantiate objects
MFRC522 rfid(RFID_SDA_PIN, RFID_RST_PIN);  // Create MFRC522 instance
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Create LCD instance with I2C address 0x27

// Variables to store access permissions
bool masterCardAccess = true;
bool registeredCardAccess = true;

// Flag to track solenoid state
bool solenoidState = 0;

const int EEPROM_SIZE = 512;

void setup() {
  Serial.begin(9600);  // Initialize serial communications with the PC
  foto.begin(9600); // Initialize serial communication with ESP32-CAM
  // Memulai Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // Initialize I2C for LCD with custom pins
  Wire.begin(LCD_SDA_PIN, LCD_SCL_PIN);

  // Inisialisasi RFID
  SPI.begin();         // Initialize SPI bus
  rfid.PCD_Init();    // Initialize MFRC522

  pinMode(BUZZER, OUTPUT); // Set buzzer pin as output
  pinMode(BUZZER_BELL, OUTPUT);
  pinMode(Button, INPUT_PULLUP);
  pinMode(SOLENOID, OUTPUT); // Set solenoid pin as output
  pinMode(TOUCH, INPUT); // Set touch sensor pin as input
  pinMode(ledR, OUTPUT);
  pinMode(ledG, OUTPUT);
  pinMode(ledY, OUTPUT);
  // SetUp Sensor Ultrasonik
  pinMode(TRIG_PIN, OUTPUT);  // Atur pin TRIG sebagai output
  pinMode(ECHO_PIN, INPUT);   // Atur pin ECHO sebagai input
  // Ensure solenoid is off (locked)
  digitalWrite(SOLENOID, LOW);

  // Initialize LCD
  lcd.begin();
  lcd.backlight();    // Turn on backlight
  lcd.setCursor(0, 0);
  lcd.print("Akses Siap!");
  digitalWrite(BUZZER, HIGH);
  digitalWrite(ledR, HIGH);
  digitalWrite(ledY, HIGH);
  digitalWrite(ledG, HIGH);
  delay(1000);
  lcd.clear();
  digitalWrite(ledR, LOW);
  digitalWrite(ledY, LOW);
  digitalWrite(ledG, LOW);
  digitalWrite(BUZZER, LOW);

  EEPROM.begin(EEPROM_SIZE); // Initialize EEPROM with size 512 bytes
}

void loop() {
  Blynk.run();

  lcd.setCursor(0, 0);
  lcd.print("Tap ID CARD ANDA");

  // Pembacaan jarak menggunakan sensor ultrasonik
  long distance = readUltrasonicDistance();
  Serial.print("Jarak: ");
  Serial.println(distance);

  // Kondisi untuk mengaktifkan atau menonaktifkan RFID berdasarkan jarak
  if (distance <= 30) {
    // Jika jarak <= 30 cm, aktifkan RFID
    rfid.PICC_IsNewCardPresent();  // Memastikan RFID aktif
    rfid.PICC_ReadCardSerial();
  } else {
    // Jika jarak > 30 cm, matikan RFID
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }

  // Pengaturan Bell Pintu Depan
  if(digitalRead(BUZZER_BELL) == HIGH){
    bellPintu();
  }

  // Pengaturan pintu dalam
  if (digitalRead(TOUCH) == HIGH) {
    digitalWrite(SOLENOID, HIGH);
  } else {
    digitalWrite(SOLENOID, LOW);
  }

  // Pengaturan pintu luar
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
  }

  String id;
  for (int i = 0; i <= 3; i++) {
    id = id + (rfid.uid.uidByte[i] < 0x10 ? "0" : "") + String(rfid.uid.uidByte[i], HEX);
  }
  id.toUpperCase();
  Serial.print("UID Card: ");
  Serial.println(id);

  if (id == "73D48730") {     // Akses Master Card (+- id)
    if (masterCardAccess) {
      Serial.println("Masuk ke akses kartu master");
      handleMasterCard();
    } else {
      aksesDitolak();
    }
  } else if (isAccessCard(id)) {  // Akses pintu untuk membuka relay
    if (registeredCardAccess) {
      aksesDiterima();
    } else {
      aksesDitolak();
    }
  } else {                    // Akses kartu ditolak
    aksesDitolak();
  }

  // Halt PICC
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

// Blynk virtual pin handlers
BLYNK_WRITE(V0) { // Mengatur hak akses kartu master
  masterCardAccess = param.asInt();
  Serial.print("Akses Kartu Master: ");
  Serial.println(masterCardAccess ? "Diizinkan" : "Ditolak");
}

BLYNK_WRITE(V1) { // Mengatur hak akses kartu yang terdaftar
  registeredCardAccess = param.asInt();
  Serial.print("Akses Kartu Terdaftar: ");
  Serial.println(registeredCardAccess ? "Diizinkan" : "Ditolak");
}

BLYNK_WRITE(V2) { // Membuka solenoid secara manual
  int buttonState = param.asInt();

  if (buttonState == 1) {
    digitalWrite(SOLENOID, HIGH); // Unlock solenoid
    Serial.println("Solenoid Unlocked.");
    delay(5000);
    digitalWrite(SOLENOID, LOW); // Lock solenoid
    Serial.println("Solenoid Locked.");
  }
}

BLYNK_WRITE(V4) { // Mengambil gambar secara manual
  int buttonState = param.asInt();

  if (buttonState == 1) {
    Serial.println("Mengambil gambar dari Blynk");
    foto.println("KIRIM_FOTO");
  }
}

long readUltrasonicDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH);
  long distance = duration * 0.0343 / 2;
  return distance;
}

void handleMasterCard() {
  lcd.clear();
  digitalWrite(ledY, HIGH);
  tone(BUZZER, 2000);  // Frekuensi 2000 Hz
  delay(1000);              // Durasi 500 ms
  noTone(BUZZER);
  lcd.setCursor(0, 0);
  lcd.print("Akses Master");
  lcd.setCursor(0, 1);
  lcd.print("Diberikan");
  delay(1000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("TAP ID CARD");

  // Jika uid belum ada di database, masuk ke addCard(). Jika sudah ada, masuk ke deleteCard()
  // Tunggu kartu baru untuk ditambahkan atau dihapus
  while (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    // Tunggu hingga ada kartu baru
    delay(100);
  }

  String newId;
  for (int i = 0; i <= 3; i++) {
    newId = newId + (rfid.uid.uidByte[i] < 0x10 ? "0" : "") + String(rfid.uid.uidByte[i], HEX);
  }
  newId.toUpperCase();

  // Jika kartu yang ditap adalah kartu master, proses dibatalkan
  if (newId == "73D48730") { // Ganti dengan UID master yang sesuai
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Akses Master");
    lcd.setCursor(0, 1);
    lcd.print("Batal Proses");
    delay(2000);
    lcd.clear();
    digitalWrite(ledY, LOW);
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    return;  // Keluar dari fungsi
  }

  if (isAccessCard(newId)) {
    deleteCard(newId);
  } else {
    addCard(newId);
  }

  // Halt PICC
  digitalWrite(ledY, LOW);
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

bool isAccessCard(String id) {
  for (int i = 0; i < EEPROM_SIZE; i += 8) {
    String storedId = "";
    for (int j = 0; j < 8; j++) {
      char readChar = char(EEPROM.read(i + j));
      if (readChar == '\0') break;
      storedId += readChar;
    }
    if (storedId == id) {
      return true;
    }
  }
  return false;
}

void addCard(String id) {
  for (int i = 0; i < EEPROM_SIZE; i += 8) {
    if (EEPROM.read(i) == 0xFF) {
      for (int j = 0; j < id.length(); j++) {
        EEPROM.write(i + j, id[j]);
      }
      EEPROM.write(i + id.length(), '\0'); // Add null terminator
      EEPROM.commit();
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("ID Ditambahkan");
      Serial.println(id + " Berhasil ditambahkan");
      delay(2000);
      lcd.clear();
      return;
    }
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Memori Penuh");
  delay(2000);
  lcd.clear();
}

void deleteCard(String id) {
  for (int i = 0; i < EEPROM_SIZE; i += 8) {
    String storedId = "";
    for (int j = 0; j < 8; j++) {
      char readChar = char(EEPROM.read(i + j));
      if (readChar == '\0') break;
      storedId += readChar;
    }
    if (storedId == id) {
      for (int j = 0; j < 8; j++) {
        EEPROM.write(i + j, 0xFF);
      }
      EEPROM.commit();
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("ID Dihapus");
      Serial.println(id + " Berhasil dihapus");
      delay(2000);
      lcd.clear();
      return;
    }
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ID Tidak Ditemukan");
  delay(2000);
  lcd.clear();
}

void aksesDiterima() {
  lcd.clear();
  digitalWrite(ledG, HIGH);
  Serial.println("Akses diterima");
  lcd.setCursor(0, 0);
  lcd.print("Akses diterima");
  digitalWrite(SOLENOID, HIGH);
  tone(BUZZER, 2000);  // Frekuensi 2000 Hz
  delay(500);              // Durasi 500 ms
  noTone(BUZZER);
  delay(4500);
  digitalWrite(SOLENOID, LOW);
  digitalWrite(ledG, LOW);
  return;
}

void aksesDitolak() {
  lcd.clear();
  Serial.println("Akses ditolak");
  lcd.setCursor(0, 0);
  lcd.print("Akses Ditolak");

  // Mengirim perintah ke ESP32-CAM untuk mengambil foto
  Serial.println("Mengambil Gambar");
  foto.println("KIRIM_FOTO");
  
  digitalWrite(ledR, HIGH);
  tone(BUZZER, 1000);  // Frekuensi 1000 Hz
  delay(250);              // Durasi 250 ms
  noTone(BUZZER);
  delay(250);              // Jeda 250 ms
  tone(BUZZER, 1000);  // Frekuensi 1000 Hz
  delay(250);              // Durasi 250 ms
  noTone(BUZZER);
  delay(500);
  digitalWrite(ledR, LOW);
  return;
}

void bellPintu(){
  Serial.println("Bell pintu ditekan");

  // Mengirim perintah ke ESP32-CAM untuk mengambil foto
  Serial.println("Mengambil Gambar");
  foto.println("KIRIM_FOTO");

  tone(BUZZER_BELL, 523, 500);
  delay(1000);
  tone(BUZZER_BELL, 392, 500);
  delay(1000);
}
