// Final Project Mikrokontroler "Sistem Keamanan Pintu Depan"
// By: Kelompok 1
// - Zerick Syahputra	23.11.5729
// - Efrizal Diaz Erlangga	23.11.5727
// - Zahra Ratri Aprilianti	23.11.5730
// - Nafiza Mahadri Widyatamaka	23.11.5741
// - Muhammad Zakiyuddin	24.21.1581

// Code untuk mikrokontroler ESP32 Devkit V1

#define BLYNK_TEMPLATE_ID "TMPL6_wl8km8S"
#define BLYNK_TEMPLATE_NAME "Smart Door Lock"
#define BLYNK_AUTH_TOKEN "MM7VaBSP1V-fZUjD4yLCPATNpp6NHIGf"
#define BLYNK_PRINT Serial

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <Keypad.h>
#include <Preferences.h>
#include <EEPROM.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include "SoftwareSerial.h"
#include <Arduino.h>
SoftwareSerial espCamSerial(16, 17);  // RX, TX
BlynkTimer timer;

// Wi-Fi credentials
char ssid[] = "Keluarga Cemara";
char pass[] = "trioami2903";

// Define pins for RFID, buzzer, solenoid, and touch sensor
#define RFID_SDA_PIN 21  // Pin SDA for RFID
#define RFID_RST_PIN 22  // Pin RST for RFID
#define BUZZER 2         // Pin Buzzer
#define BUZZER_BELL 32   // Pin Buzzer Bell
#define Button 35        // Pin Button Bell
#define SOLENOID 15      // Pin relay Solenoid
#define TOUCH 34         // Pin Touch Sensor untuk membuka solenoid

// Define pins for LCD I2C
#define LCD_SDA_PIN 27  // Pin SDA for LCD
#define LCD_SCL_PIN 33  // Pin SCL for LCD
// Pengaturan Serial untuk komunikasi dengan ESP32-CAM
#define SERIAL_CAM Serial2

// Instantiate objects
MFRC522 rfid(RFID_SDA_PIN, RFID_RST_PIN);  // Create MFRC522 instance
LiquidCrystal_I2C lcd(0x27, 16, 2);        // Create LCD instance with I2C address 0x27

// Keypad setup
const byte ROWS = 4;  // Jumlah baris pada keypad
const byte COLS = 3;  // Jumlah kolom pada keypad
char keys[ROWS][COLS] = {
  { '1', '2', '3' },
  { '4', '5', '6' },
  { '7', '8', '9' },
  { '*', '0', '#' }
};
byte rowPins[ROWS] = { 13, 12, 14, 26 };  // GPIO untuk baris
byte colPins[COLS] = { 25, 4, 5 };        // GPIO untuk kolom
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// PIN setup
const int PIN_LENGTH = 6;                    // Panjang PIN 6 digit
char currentPIN[PIN_LENGTH + 1] = "123456";  // Default PIN
char enteredPIN[PIN_LENGTH + 1] = { 0 };
int pinIndex = 0;

// Preferences instance
Preferences preferences;

const int EEPROM_SIZE = 80;  // Ukuran penyimpanan EEPROM

// Variables to store access permissions
bool masterCardAccess = true;
bool registeredCardAccess = true;

// Flag to track solenoid state
bool solenoidState = 0;

// Virtual Pin untuk masing-masing aktivitas
#define VPIN_AKSES_DITERIMA V11
#define VPIN_AKSES_DITOLAK V12
#define VPIN_MASTER_CARD V13
#define VPIN_BELL_PINTU V14
// Data SimpleChart Blynk
int aksesditerima = 0;
int aksesditolak = 0;
int aksesMaster = 0;
int bell_pintu = 0;

bool isInputtingPIN = false;  // Flag untuk menandakan input PIN sedang berlangsung
WidgetTerminal terminal(V9);  // Terminal Blynk ke Virtual Pin V9

unsigned long previousMillis = 0;
const unsigned long interval = 1000;  // Durasi dalam milidetik

void setup() {
  //Serial.begin(9600);        // Initialize serial communications with the PC
  Serial.begin(9600);
  espCamSerial.begin(9600);  // Initialize serial communication with ESP32-CAM
  // Memulai Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  EEPROM.begin(EEPROM_SIZE);  // Initialize EEPROM with size 512 bytes
  // Initialize I2C for LCD with custom pins
  Wire.begin(LCD_SDA_PIN, LCD_SCL_PIN);
  // Initialize LCD
  lcd.begin();
  lcd.backlight();  // Turn on backlight
  // Kirim pesan awal ke Terminal
  terminalMessage("Terminal Terkoneksi ke Virtual Pin V9");
  timer.setInterval(500L, readFromEspCam);
  checkWiFiConnection();  // Cek wifi esp32 devkit v1
  terminalMessage("ESP32 DevKit V1 is ready.");
  terminalMessage("Memeriksa Wi-Fi ESP32-CAM...");  // Cek wifi esp32-cam
  espCamSerial.println("CEK_WIFI");
  delay(1000);  // Berikan jeda untuk menerima respon
  // (ESP32-CAM)
  if (espCamSerial.available()) {
    String response = espCamSerial.readString();
    if (response.length() > 0) {
      terminalMessage("ESP32-CAM is ready.");
      terminalMessage("Welcome to Smart Door Lock System!");
      lcd.setCursor(0, 0);
      lcd.print("Akses Siap!");
      Serial.println("Akses Siap!");
    }
  } else {
    terminalMessage("Sistem belum siap");
    Serial.println("Akses Siap!");
    lcd.setCursor(0, 0);
    lcd.print("Akses Belum");
    lcd.setCursor(0, 1);
    lcd.print("Siap!");
  }

  // (KEYPAD) Muat PIN saat perangkat dihidupkan
  loadPIN();

  // (RFID) Inisialisasi RFID
  SPI.begin();      // Initialize SPI bus
  rfid.PCD_Init();  // Initialize MFRC522

  pinMode(BUZZER, OUTPUT);  // Set buzzer pin as output
  pinMode(BUZZER_BELL, OUTPUT);
  pinMode(Button, INPUT);
  pinMode(SOLENOID, OUTPUT);  // Set solenoid pin as output
  pinMode(TOUCH, INPUT);      // Set touch sensor pin as input
  // Ensure solenoid is off (locked)
  digitalWrite(SOLENOID, LOW);

  digitalWrite(BUZZER, HIGH);
  delay(1000);
  digitalWrite(BUZZER, LOW);
  digitalWrite(BUZZER_BELL, HIGH);
  delay(1000);
  digitalWrite(BUZZER_BELL, LOW);
  lcd.clear();
}

void loop() {
  Blynk.run();
  timer.run();
  if (!isInputtingPIN) {  // Hanya tampilkan jika tidak sedang input PIN
    displayMessage("TAP KARTU /", "MASUKKAN PIN !!", 0);
  }

  if (digitalRead(TOUCH) == HIGH) {
    digitalWrite(SOLENOID, HIGH);
  } else {
    digitalWrite(SOLENOID, LOW);
  }

  // Pengaturan keypad pin
  char key = keypad.getKey();
  if (key) {
    handleKeypadInput(key);
  }

  // Pengaturan bell pintu
  if (digitalRead(Button) == HIGH) {
    bellPintu();
  }

  // Pengaturan pintu luar
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
  }

  String id = getCardUID();
  Serial.print("UID Card: ");
  Serial.println(id);
  terminalMessage("UID Card: " + id);

  if (id == "73D48730") {  // Akses Master Card
    if (masterCardAccess) {
      Serial.println("Masuk ke akses kartu master");
      terminalMessage("Masuk ke akses kartu master");
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
  } else {  // Akses kartu ditolak
    aksesDitolak();
  }

  // Halt PICC
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

void checkWiFiConnection() {
  // Menunggu hingga koneksi Wi-Fi berhasil
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 30) {
    retries++;
    terminal.println("Mencoba menghubungkan Wi-Fi...");
    Serial.print("Mencoba menghubungkan Wi-Fi...");
    delay(1000);
  }

  if (WiFi.status() == WL_CONNECTED) {
    terminal.println("ESP32 Devkit V1: Wi-Fi terhubung!");
    Serial.println("ESP32 Devkit V1: Wi-Fi terhubung!");
  } else {
    Serial.println("ESP32 Devkit V1: Koneksi Wi-Fi gagal!");
    terminal.println("ESP32 Devkit V1: Koneksi Wi-Fi gagal!");
  }
}

void displayMessage(String line1, String line2, int delayTime) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
  delay(delayTime);
}

void terminalMessage(String message) {
  terminal.println(message);
  terminal.flush();
}

// Blynk virtual pin handlers
BLYNK_WRITE(V0) {  // Mengambil gambar secara manual
  int buttonState = param.asInt();

  if (buttonState == 1) {
    Serial.println("Mengambil gambar dari Blynk");
    terminalMessage("Mengambil gambar dari Blynk");
    espCamSerial.println("KIRIM_FOTO");
  }
}

BLYNK_WRITE(V1) {  // Membuka solenoid secara manual
  int buttonState = param.asInt();

  if (buttonState == 1) {
    digitalWrite(SOLENOID, HIGH);  // Unlock solenoid
    Serial.println("Solenoid Unlocked.");
    terminalMessage("Pintu Terbuka!!");
    delay(5000);
    digitalWrite(SOLENOID, LOW);  // Lock solenoid
    Serial.println("Solenoid Locked.");
    terminalMessage("Pintu Tertutup!!");
  }
}

BLYNK_WRITE(V2) {  // Memulai mode kartu master
  int buttonState = param.asInt();

  if (buttonState == 1) {
    Serial.println("Masuk ke akses kartu master");
    terminalMessage("Masuk ke akses kartu master");
    handleMasterCard();
  }
}

BLYNK_WRITE(V3) {  // Mengatur hak akses kartu master
  masterCardAccess = param.asInt();
  Serial.print("Akses Kartu Master: ");
  Serial.println(masterCardAccess ? "Diizinkan" : "Ditolak");
  terminal.print("Akses Kartu Master: ");
  terminal.flush();
  terminal.println(masterCardAccess ? "Diizinkan" : "Ditolak");
  terminal.flush();
}

BLYNK_WRITE(V4) {  // Mengatur hak akses kartu yang terdaftar
  registeredCardAccess = param.asInt();
  Serial.print("Akses Kartu Terdaftar: ");
  Serial.println(registeredCardAccess ? "Diizinkan" : "Ditolak");
  terminal.print("Akses Kartu Terdaftar: ");
  terminal.flush();
  terminal.println(registeredCardAccess ? "Diizinkan" : "Ditolak");
  terminal.flush();
}

BLYNK_WRITE(V5) {  // Mengganti password
  int buttonState = param.asInt();

  if (buttonState == 1) {
    Serial.println("Masuk Mode Penggantian PIN Akses");
    terminalMessage("Masuk Mode Penggantian PIN Akses");
    changePIN();
  }
}

BLYNK_WRITE(V9) {                // Penanganan input Terminal
  String input = param.asStr();  // Membaca input Terminal
  input.toUpperCase();           // Mengubah ke huruf besar agar tidak case-sensitive

  if (input == "CEK_ID") {
    listAllUIDs();                     // Panggil fungsi untuk menampilkan semua UID
  } else if (input == "CEK_MEMORI") {  // Cek jumlah memori kosong
    int freeSlots = getFreeMemorySlots();
    terminalMessage("Slot memori kosong: " + String(freeSlots) + " UID dapat ditampung.");
  } else if (input.endsWith("_HAPUS")) {
    String uidToDelete = input.substring(0, input.indexOf("_HAPUS"));  // Ekstrak UID
    if (uidToDelete.length() == 8 && isValidUID(uidToDelete)) {
      deleteCard(uidToDelete);
    } else {
      Serial.println("Format UID tidak valid.");
      terminalMessage("Format UID tidak valid.");
    }
  } else if (input == "KOSONGKAN_MEMORI") {
    clearAllMemory();  // Panggil fungsi untuk menghapus semua UID dari EEPROM
  } else if (input.endsWith("_TAMBAH")) {
    String uidToAdd = input.substring(0, input.indexOf("_TAMBAH"));  // Ekstrak UID
    if (uidToAdd.length() == 8 && isValidUID(uidToAdd)) {
      if (isCardExists(uidToAdd)) {  // Cek apakah UID sudah ada
        terminalMessage("UID " + uidToAdd + " sudah terdaftar!");
      } else {
        addCard(uidToAdd);  // Tambahkan UID ke EEPROM
      }
    } else {
      terminalMessage("Format UID tidak valid.");
    }
  } else if (input == "CEK_WIFI") {  // Cek koneksi Wi-Fi
    String wifiStatus = "Connected to: " + String(ssid) + ", IP: " + WiFi.localIP().toString();
    terminalMessage("Memeriksa Wi-Fi ESP32 Devkit V1...");
    if (WiFi.status() == WL_CONNECTED) {
      terminalMessage("Wi-Fi terhubung!");
      terminalMessage(wifiStatus);
    } else {
      terminalMessage("Wi-Fi tidak terhubung. Mohon cek koneksi.");
    }
  } else if (input == "CEK_WIFI_CAM") {
    terminalMessage("Memeriksa Wi-Fi ESP32-CAM...");
    espCamSerial.println("CEK_WIFI");
  } else if (input == "CEK_PIN") {
    Serial.print("PIN saat ini: ");
    Serial.println(currentPIN);
    loadPIN();
  } else if (input.endsWith("_GANTI_PIN")) {
    String newPIN = input.substring(0, input.indexOf("_GANTI_PIN"));  // Ekstrak PIN baru
    newPIN.trim();                                                    // Hilangkan spasi di awal dan akhir
    if (newPIN.length() == 6 && newPIN.toInt() != 0) {                // Validasi panjang dan memastikan terdiri dari angka
      for (int i = 0; i < newPIN.length(); i++) {
        if (!isDigit(newPIN.charAt(i))) {
          terminalMessage("Format PIN tidak valid. Harus berupa 6 digit angka.");
          return;
        }
      }
      strncpy(currentPIN, newPIN.c_str(), PIN_LENGTH);
      savePIN(currentPIN);
      terminalMessage("PIN berhasil diubah menjadi: " + newPIN);
    } else {
      terminalMessage("Format PIN tidak valid. Harus berupa 6 digit angka.");
    }
  } else if(input == "CEK_KOMUNIKASI"){
    espCamSerial.println("CEK_KOMUNIKASI");
  } else {
    terminalMessage("Perintah tidak dikenali.");
  }
}

void handleKeypadInput(char key) {
  if (key == '*') {
    // Reset input
    pinIndex = 0;
    memset(enteredPIN, 0, sizeof(enteredPIN));  // Bersihkan enteredPIN
    Serial.println("Input dihapus.");
    displayMessage("Masukkan PIN", " ", 2000);
    isInputtingPIN = false;  // Input PIN dihentikan
  } else if (key == '#') {
    // Ketika # ditekan, cek apakah input PIN sudah lengkap
    if (pinIndex == PIN_LENGTH) {
      // Pastikan enteredPIN diakhiri dengan null karakter ('\0')
      enteredPIN[pinIndex] = '\0';

      // Debug: Tampilkan PIN yang dimasukkan
      terminal.print("PIN yang dimasukkan : ");
      terminal.println(enteredPIN);
      terminal.flush();
      Serial.print("PIN yang dimasukkan : ");
      Serial.println(enteredPIN);
      Serial.print("PIN yang benar      : ");
      Serial.println(currentPIN);
      // Periksa apakah PIN benar atau salah
      checkPin(enteredPIN);
    } else {
      Serial.println("\nPIN tidak lengkap. Harap masukkan 6 digit.");
      terminalMessage("PIN yang dimasukkan tidak lengkap. (Mengambil Gambar)");
      displayMessage("PIN salah.", "Coba Lagi !!", 0);
      // Mengirim perintah ke ESP32-CAM untuk mengambil foto
      Serial.println("Mengambil Gambar");
      espCamSerial.println("FOTO_TOLAK");
      tone(BUZZER, 1000);  // Frekuensi 1000 Hz
      delay(250);          // Durasi 250 ms
      noTone(BUZZER);
      delay(250);          // Jeda 250 ms
      tone(BUZZER, 1000);  // Frekuensi 1000 Hz
      delay(250);          // Durasi 250 ms
      noTone(BUZZER);
      delay(500);
    }
    // Reset input setelah konfirmasi
    pinIndex = 0;
    memset(enteredPIN, 0, sizeof(enteredPIN));  // Bersihkan enteredPIN
    isInputtingPIN = false;                     // Input PIN selesai
  } else {
    // Hanya terima input numerik
    if (isdigit(key)) {
      if (pinIndex < PIN_LENGTH) {
        enteredPIN[pinIndex++] = key;

        // Tampilkan angka yang ditekan secara real-time
        Serial.print("Tombol ditekan: ");
        Serial.println(key);

        // Tampilkan progress input dengan tanda bintang (*)
        Serial.print("Masukkan: ");
        String maskedInput = "";
        for (int i = 0; i < pinIndex; i++) {
          maskedInput += '*';
          Serial.print("*");
        }
        Serial.println();
        displayMessage("Masukkan PIN", maskedInput, 0);
        isInputtingPIN = true;  // Sedang dalam proses input PIN
      }

    } else {
      Serial.println("Input tidak valid. Harap masukkan angka saja.");
      displayMessage("Masukkan PIN", "Input tidak valid", 2000);
      terminalMessage("Input tidak valid.");
    }
  }
}

// Fungsi untuk memeriksa apakah PIN yang dimasukkan benar atau salah
void checkPin(char enteredPIN[]) {
  // Bandingkan enteredPIN dengan currentPIN
  if (strcmp(enteredPIN, currentPIN) == 0) {
    aksesditerima++;
    Serial.println("PIN benar. Pintu Terbuka!!");
    terminalMessage("PIN Benar. Pintu Terbuka!!");
    displayMessage("PIN Benar", "Pintu Terbuka!!", 0);
    Blynk.virtualWrite(VPIN_AKSES_DITERIMA, 1);
    timer.setTimeout(500L, []() {
      Blynk.virtualWrite(VPIN_AKSES_DITERIMA, 0);
    });
    digitalWrite(SOLENOID, HIGH);
    tone(BUZZER, 2000);  // Frekuensi 2000 Hz
    delay(500);          // Durasi 500 ms
    noTone(BUZZER);
    delay(5000);
    digitalWrite(SOLENOID, LOW);
    terminalMessage("Pintu Tertutup!!");
    return;
  } else {
    aksesditolak++;
    Blynk.virtualWrite(VPIN_AKSES_DITOLAK, 1);  // Kirim status "1" ke Superchart
    timer.setTimeout(500L, []() {
      Blynk.virtualWrite(VPIN_AKSES_DITOLAK, 0);  // Reset status ke "0"
    });
    Serial.println("PIN salah.");
    terminalMessage("PIN salah!! (Mengambil Gambar)");
    displayMessage("PIN salah.", "Coba Lagi !!", 0);
    // Mengirim perintah ke ESP32-CAM untuk mengambil foto
    Serial.println("Mengambil Gambar");
    espCamSerial.println("FOTO_TOLAK");
    tone(BUZZER, 1000);  // Frekuensi 1000 Hz
    delay(250);          // Durasi 250 ms
    noTone(BUZZER);
    delay(250);          // Jeda 250 ms
    tone(BUZZER, 1000);  // Frekuensi 1000 Hz
    delay(250);          // Durasi 250 ms
    noTone(BUZZER);
    delay(500);
    return;
  }
}

void changePIN() {
  lcd.clear();
  displayMessage("Masukkan PIN", "Baru", 0);
  tone(BUZZER, 2000);  // Frekuensi 2000 Hz
  delay(1000);         // Durasi 500 ms
  noTone(BUZZER);

  Serial.println("Masukkan PIN baru (6 digit) diikuti dengan tanda #.");
  terminalMessage("Masukkan PIN baru (6 digit) diikuti dengan tanda #.");
  char newPIN[PIN_LENGTH + 1] = { 0 };
  int pinIndex = 0;
  bool isInputtingPIN = true;  // Flag untuk menandai proses input

  while (isInputtingPIN) {
    char key = keypad.getKey();  // Baca input dari keypad
    if (key) {
      if (key == '*') {
        // Reset input
        pinIndex = 0;
        memset(newPIN, 0, sizeof(newPIN));
        Serial.println("Input dihapus.");
        terminalMessage("Input dihapus.");
        displayMessage("PIN Baru:", " ", 0);
      } else if (key == '#') {
        // Konfirmasi PIN
        if (pinIndex == PIN_LENGTH) {
          // Simpan PIN baru
          newPIN[pinIndex] = '\0';  // Tambahkan null terminator
          strncpy(currentPIN, newPIN, PIN_LENGTH);
          savePIN(currentPIN);

          Serial.println("PIN berhasil diubah.");
          terminalMessage("PIN berhasil diubah.");
          tone(BUZZER, 2000);  // Frekuensi 2000 Hz
          delay(500);          // Durasi 500 ms
          noTone(BUZZER);
          displayMessage("PIN berhasil", "diubah", 2000);

          isInputtingPIN = false;  // Keluar dari loop
        } else {
          Serial.println("PIN harus 6 digit.");
          terminalMessage("PIN harus 6 digit.");
          displayMessage("PIN Baru", "harus 6 digit", 2000);
        }
      } else if (isdigit(key) && pinIndex < PIN_LENGTH) {
        // Masukkan angka ke PIN
        newPIN[pinIndex++] = key;

        // Tampilkan progres input angka secara langsung
        newPIN[pinIndex] = '\0';  // Tambahkan null terminator untuk string
        Serial.print("PIN Baru: ");
        Serial.println(newPIN);

        // Tampilkan progres input ke LCD
        displayMessage("PIN Baru:", newPIN, 0);

        // Jika PIN sudah lengkap, beri tahu user untuk menekan #
        if (pinIndex == PIN_LENGTH) {
          Serial.println("Tekan # untuk konfirmasi.");
          displayMessage("Konfirmasi PIN", "Tekan #", 2000);
        }

      } else {
        Serial.println("Input tidak valid. Harap masukkan angka saja.");
        terminalMessage("Input tidak valid.");
        displayMessage("Masukkan PIN Baru", "Input tidak valid", 2000);
      }
    }
  }
}

void savePIN(const char* pin) {
  preferences.begin("pin_storage", false);    // Nama namespace 'pin_storage', mode read/write
  preferences.putString("current_pin", pin);  // Menyimpan PIN
  preferences.end();
  terminal.print("PIN ");
  terminal.print(currentPIN);
  terminal.println(" berhasil disimpan");
  terminal.flush();
  Serial.println("PIN berhasil disimpan.");
}

void loadPIN() {
  preferences.begin("pin_storage", true);                            // Membuka namespace dalam mode baca saja
  String savedPIN = preferences.getString("current_pin", "123456");  // Jika belum ada PIN, gunakan "123456"
  savedPIN.toCharArray(currentPIN, PIN_LENGTH + 1);                  // Menyalin string PIN yang tersimpan ke currentPIN
  preferences.end();
  terminal.print("PIN yang tersimpan: ");
  terminal.println(currentPIN);
  terminal.flush();
  Serial.print("PIN yang tersimpan: ");
  Serial.println(currentPIN);
}

// Fungsi untuk membaca respon dari ESP32-CAM
void readFromEspCam() {
  while (espCamSerial.available() > 0) {
    String response = espCamSerial.readStringUntil('\n');
    terminalMessage("ESP32-CAM: " + response);
  }
}

int getFreeMemorySlots() {
  int usedSlots = 0;

  for (int i = 0; i < EEPROM.length(); i += 8) {  // Asumsikan setiap UID panjangnya 8 byte
    bool isEmpty = true;
    for (int j = 0; j < 8; j++) {
      char c = EEPROM.read(i + j);
      if (c != 0 && c != 255) {  // Jika ada data valid
        isEmpty = false;
        break;
      }
    }
    if (!isEmpty) {
      usedSlots++;  // Jika slot digunakan, tambahkan jumlah slot terpakai
    }
  }

  // Hitung slot yang masih kosong berdasarkan kapasitas total dan yang digunakan
  int totalSlots = EEPROM.length() / 8;  // Kapasitas total untuk UID, setiap UID = 8 byte
  return totalSlots - usedSlots;         // Slot kosong = Total - Terpakai
}

void clearAllMemory() {
  for (int i = 0; i < EEPROM.length(); i++) {  // Hapus seluruh data EEPROM
    EEPROM.write(i, 0xFF);                     // 0xFF adalah nilai kosong untuk EEPROM
  }
  EEPROM.commit();  // Simpan perubahan ke EEPROM
  Serial.println("Memori EEPROM telah dikosongkan.");
  terminalMessage("Semua UID berhasil dihapus dari memori.");
}

bool isValidUID(String uid) {
  if (uid.length() != 8) return false;  // Pastikan panjang UID adalah 8 karakter
  for (int i = 0; i < uid.length(); i++) {
    if (!isxdigit(uid[i])) return false;  // Pastikan hanya karakter heksadesimal (0-9, A-F)
  }
  return true;
}

bool isCardExists(String uid) {
  for (int i = 0; i < EEPROM.length(); i += 8) {  // Asumsikan setiap UID panjangnya 8 byte
    String storedUID = "";
    for (int j = 0; j < 8; j++) {
      char c = EEPROM.read(i + j);
      if (c == 0 || c == 255) break;  // Akhiri jika data kosong atau tidak valid
      storedUID += c;
    }
    if (storedUID == uid) {
      return true;  // UID ditemukan
    }
  }
  return false;  // UID tidak ditemukan
}

void listAllUIDs() {
  terminalMessage("Daftar UID Terdaftar di EEPROM:");
  Serial.println("Daftar UID yang terdaftar:");
  bool found = false;

  for (int i = 0; i < EEPROM_SIZE; i += 8) {
    String storedId = "";
    for (int j = 0; j < 8; j++) {
      char readChar = char(EEPROM.read(i + j));
      if (readChar == '\0' || readChar == 0xFF) break;  // Akhiri jika null atau kosong
      storedId += readChar;
    }
    if (isValidUID(storedId)) {  // Periksa apakah UID valid
      Serial.println("UID: " + storedId);
      terminal.println("UID: " + storedId);
      found = true;
    }
  }
  if (!found) {
    Serial.println("Tidak ada UID yang terdaftar.");
    terminal.println("Tidak ada UID yang terdaftar.");
  }
}

String getCardUID() {
  String id;
  for (int i = 0; i <= 3; i++) {
    id = id + (rfid.uid.uidByte[i] < 0x10 ? "0" : "") + String(rfid.uid.uidByte[i], HEX);
  }
  id.toUpperCase();
  return id;
}

void handleMasterCard() {
  lcd.clear();
  displayMessage("Akses Master", "Diberikan", 0);
  tone(BUZZER, 2000);  // Frekuensi 2000 Hz
  delay(1000);         // Durasi 500 ms
  noTone(BUZZER);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("  TAP ID CARD!  ");

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
  if (newId == "73D48730") {
    lcd.clear();
    terminalMessage("Proses Akses Master Dibatalkan");
    displayMessage("Akses Master", "Dibatalkan", 0);
    tone(BUZZER, 1000);  // Frekuensi 1000 Hz
    delay(250);          // Durasi 250 ms
    noTone(BUZZER);
    delay(250);          // Jeda 250 ms
    tone(BUZZER, 1000);  // Frekuensi 1000 Hz
    delay(250);          // Durasi 250 ms
    noTone(BUZZER);
    delay(500);
    lcd.clear();
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    return;  // Keluar dari fungsi
  }

  if (isAccessCard(newId)) {
    deleteCard(newId);
    aksesMaster++;
    Blynk.virtualWrite(V8, aksesMaster);
    Blynk.virtualWrite(VPIN_MASTER_CARD, 1);  // Kirim status "1" ke Superchart
    timer.setTimeout(500L, []() {
      Blynk.virtualWrite(VPIN_MASTER_CARD, 0);  // Reset status ke "0"
    });
  } else {
    addCard(newId);
    aksesMaster++;
    Blynk.virtualWrite(V8, aksesMaster);
    Blynk.virtualWrite(VPIN_MASTER_CARD, 1);  // Kirim status "1" ke Superchart
    timer.setTimeout(500L, []() {
      Blynk.virtualWrite(VPIN_MASTER_CARD, 0);  // Reset status ke "0"
    });
  }

  // Halt PICC
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
      EEPROM.write(i + id.length(), '\0');
      EEPROM.commit();
      lcd.clear();
      tone(BUZZER, 2000);  // Frekuensi 2000 Hz
      delay(500);          // Durasi 500 ms
      noTone(BUZZER);
      Serial.println(id + " Berhasil ditambahkan");
      terminalMessage(id + " Berhasil ditambahkan");
      displayMessage("UID: " + id, "Ditambahkan", 2000);
      lcd.clear();
      return;
    }
  }
  lcd.clear();
  displayMessage("Memori Penuh", "", 2000);
  terminalMessage("EEPROM penuh, tidak dapat menambahkan UID baru");
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
      tone(BUZZER, 2000);  // Frekuensi 2000 Hz
      delay(500);          // Durasi 500 ms
      noTone(BUZZER);
      Serial.println(id + " Berhasil dihapus");
      terminalMessage(id + " Berhasil dihapus");
      displayMessage("UID: " + id, "Dihapus", 2000);
      lcd.clear();
      return;
    }
  }
  lcd.clear();
  displayMessage("ID Tidak Ditemukan", "", 2000);
  terminalMessage("ID Tidak Ditemukan");
  lcd.clear();
}

void aksesDiterima() {
  lcd.clear();
  aksesditerima++;  // Tambah 1 ke aksesDiterima
  Blynk.virtualWrite(VPIN_AKSES_DITERIMA, 1);
  timer.setTimeout(500L, []() {
    Blynk.virtualWrite(VPIN_AKSES_DITERIMA, 0);
  });
  Blynk.virtualWrite(V6, aksesditerima);  // Kirim ke widget SimpleChart untuk Akses Diterima
  Serial.println("Akses diterima");
  terminalMessage("Akses diterima");
  displayMessage("Akses Diterima", "Pintu Terbuka!!", 0);
  digitalWrite(SOLENOID, HIGH);
  tone(BUZZER, 2000);  // Frekuensi 2000 Hz
  delay(500);          // Durasi 500 ms
  noTone(BUZZER);
  delay(5000);
  digitalWrite(SOLENOID, LOW);
  terminalMessage("Pintu Tertutup!!");
  return;
}

void aksesDitolak() {
  lcd.clear();
  aksesditolak++;                             // Tambah 1 ke aksesDitolak
  Blynk.virtualWrite(VPIN_AKSES_DITOLAK, 1);  // Kirim status "1" ke Superchart
  timer.setTimeout(500L, []() {
    Blynk.virtualWrite(VPIN_AKSES_DITOLAK, 0);  // Reset status ke "0"
  });
  Blynk.virtualWrite(V7, aksesditolak);  // Kirim ke widget SimpleChart untuk Akses Ditolak
  Serial.println("Akses ditolak");
  terminalMessage("Akses ditolak (Mengambil Gambar)");
  displayMessage("Akses Ditolak!!", "Coba Lagi", 0);

  // Mengirim perintah ke ESP32-CAM untuk mengambil foto
  Serial.println("Mengambil Gambar");
  espCamSerial.println("FOTO_TOLAK");

  tone(BUZZER, 1000);  // Frekuensi 1000 Hz
  delay(250);          // Durasi 250 ms
  noTone(BUZZER);
  delay(250);          // Jeda 250 ms
  tone(BUZZER, 1000);  // Frekuensi 1000 Hz
  delay(250);          // Durasi 250 ms
  noTone(BUZZER);
  return;
}

void bellPintu() {
  Serial.println("Bell pintu ditekan");
  terminalMessage("Bell pintu ditekan (Mengambil Gambar)");
  bell_pintu++;
  Blynk.virtualWrite(V10, bell_pintu);

  Serial.println("Mengambil Gambar");
  espCamSerial.println("FOTO_BELL");
  Blynk.virtualWrite(VPIN_BELL_PINTU, 1);  // Kirim status "1" ke Superchart
  timer.setTimeout(500L, []() {
    Blynk.virtualWrite(VPIN_BELL_PINTU, 0);  // Reset status ke "0"
  });

  int melody[] = {262, 330, 392, 523}; // C, E, G, C' (Do, Mi, Sol, Do')
  int noteDurations[] = {500, 500, 500, 1000}; // Durasi masing-masing nada (dalam milidetik)

  // Mainkan nada satu per satu
  for (int i = 0; i < 4; i++) {
    tone(BUZZER_BELL, melody[i]);      // Mainkan nada sesuai frekuensi
    delay(noteDurations[i]);           // Tunggu sesuai durasi nada
    noTone(BUZZER_BELL);               // Matikan nada
    delay(50);                         // Jeda kecil antara nada
  }
}
