#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Adafruit_Fingerprint.h>
#include <Keypad.h>
#include <Preferences.h>
#include <ArduinoJson.h>

// ================== CONFIGURATION ==================
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";
const char* GATEWAY_URL = "http://YOUR_GATEWAY_IP:8000/ingest/biometric";

#define I2C_ADDRESS     0x3C
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64

// Hardware Interfaces
Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
HardwareSerial mySerial(2); // RX=16, TX=17
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);
Preferences prefs;

// Keypad
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte rowPins[ROWS] = {32, 33, 25, 26}; 
byte colPins[COLS] = {27, 14, 12, 13}; 
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// State Management
enum DeviceMode { MENU, ENROLL, USE, REINIT, CLEAR_ID, CLEAR_ALL };
DeviceMode currentMode = MENU;
int menuIndex = 0;
const char* menuOptions[] = {"Enrollment", "Use Mode", "ReInit", "Clear ID", "Clear All"};
const int menuCount = 5;

// Persistent Data placeholders
int currentYear = 0;
String currentSection = "";

// ================== SETUP & MAIN LOOP ==================

void showMessage(String msg, int delayTime = 1000);
void drawMenu();
void handleRegistration();
void handleVerification();
void handleReInit();
void handleClearID();
void handleClearAll();
void setupPersistence();
bool ensureWiFi();

void setup() {
  Serial.begin(115200);
  delay(100);

  // OLED Init
  Wire.begin(21, 22);
  if(!display.begin(I2C_ADDRESS, true)) {
    Serial.println("OLED Failed");
    while(1);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.display();

  // Fingerprint Init
  mySerial.begin(57600, SERIAL_8N1, 16, 17);
  if (finger.verifyPassword()) {
    Serial.println("Sensor Found");
  } else {
    showMessage("Sensor Error!", 0);
    while(1);
  }

  setupPersistence();
  
  if (currentYear == 0 || currentSection == "") {
    handleReInit();
  }
}

void loop() {
  switch (currentMode) {
    case MENU:
      drawMenu();
      char key = keypad.getKey();
      if (key == 'A') { // UP
        menuIndex = (menuIndex - 1 + menuCount) % menuCount;
      } else if (key == 'B') { // DOWN
        menuIndex = (menuIndex + 1) % menuCount;
      } else if (key == '#') { // SELECT
        if (menuIndex == 0) currentMode = ENROLL;
        else if (menuIndex == 1) currentMode = USE;
        else if (menuIndex == 2) currentMode = REINIT;
        else if (menuIndex == 3) currentMode = CLEAR_ID;
        else if (menuIndex == 4) currentMode = CLEAR_ALL;
      }
      break;

    case ENROLL:
      handleRegistration();
      currentMode = MENU;
      break;

    case USE:
      handleVerification();
      // handleVerification returns to MENU if '*' is pressed
      break;

    case REINIT:
      handleReInit();
      currentMode = MENU;
      break;

    case CLEAR_ID:
      handleClearID();
      currentMode = MENU;
      break;

    case CLEAR_ALL:
      handleClearAll();
      currentMode = MENU;
      break;
  }
}

// ================== FUNCTIONS ==================

void setupPersistence() {
  prefs.begin("touchify", false);
  currentYear = prefs.getInt("year", 0);
  currentSection = prefs.getString("section", "");
}

void showMessage(String msg, int delayTime) {
  display.clearDisplay();
  display.setCursor(0, 20);
  display.println(msg);
  display.display();
  if (delayTime > 0) delay(delayTime);
}

void drawMenu() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("--- TOUCHIFY MENU ---");
  for (int i = 0; i < menuCount; i++) {
    if (i == menuIndex) display.print("> ");
    else display.print("  ");
    display.println(menuOptions[i]);
  }
  display.setCursor(0, 55);
  display.printf("Y:%d S:%s", currentYear, currentSection.c_str());
  display.display();
}

String getInput(String prompt, bool numericOnly = true) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(prompt);
  display.display();
  
  String input = "";
  while (true) {
    char key = keypad.getKey();
    if (key) {
      if (key == '#') return input;
      if (key == '*') return ""; // Cancel
      if (numericOnly && (key < '0' || key > '9')) continue;
      
      input += key;
      display.print(key);
      display.display();
    }
  }
}

void handleReInit() {
  String yr = getInput("Enter Year (1-4):");
  if (yr == "") return;
  currentYear = yr.toInt();
  
  currentSection = getInput("Enter Sec (e.g. H2):", false);
  if (currentSection == "") return;

  prefs.putInt("year", currentYear);
  prefs.putString("section", currentSection);
  showMessage("Saved!", 1500);
}

void handleRegistration() {
  String studentID = getInput("Student ID:");
  if (studentID == "") return;

  // Find next free slot (1-1000)
  int slot = -1;
  for (int i = 1; i <= 1000; i++) {
    if (finger.loadModel(i) != FINGERPRINT_OK) {
      slot = i;
      break;
    }
  }
  
  if (slot == -1) {
    showMessage("Sensor Full!", 2000);
    return;
  }

  showMessage("Place Finger...", 500);
  int p = -1;
  while (p != FINGERPRINT_OK) p = finger.getImage();
  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) { showMessage("Error!"); return; }

  showMessage("Remove Finger", 2000);
  while (finger.getImage() != FINGERPRINT_NOFINGER);

  showMessage("Place Again...", 500);
  p = -1;
  while (p != FINGERPRINT_OK) p = finger.getImage();
  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) { showMessage("Error!"); return; }

  if (finger.createModel() != FINGERPRINT_OK) { showMessage("Mismatch!"); return; }
  
  if (finger.storeModel(slot) == FINGERPRINT_OK) {
    // Save mapping to Preferences
    String key = "id_" + String(slot);
    prefs.putString(key.c_str(), studentID);
    showMessage("Enrolled Slot " + String(slot), 2000);
  } else {
    showMessage("Save Failed");
  }
}

void handleVerification() {
  showMessage("Scanning Mode\nPress * back", 0);
  
  while (true) {
    char k = keypad.getKey();
    if (k == '*') {
      currentMode = MENU;
      return;
    }

    int p = finger.getImage();
    if (p == FINGERPRINT_NOFINGER) continue;
    if (p != FINGERPRINT_OK) continue;

    p = finger.image2Tz();
    if (p != FINGERPRINT_OK) continue;

    p = finger.fingerSearch();
    if (p == FINGERPRINT_OK) {
      String key = "id_" + String(finger.fingerID);
      String studentID = prefs.getString(key.c_str(), "UNKNOWN");
      
      display.clearDisplay();
      display.setCursor(0, 10);
      display.setTextSize(2);
      display.println("MATCH!");
      display.setTextSize(1);
      display.println("ID: " + studentID);
      display.println("Conf: " + String(finger.confidence));
      display.display();

      // Send to Gateway
      if (ensureWiFi()) {
        HTTPClient http;
        http.begin(GATEWAY_URL);
        http.addHeader("Content-Type", "application/json");

        JsonDocument doc;
        doc["year"] = currentYear;
        doc["section_id"] = currentSection;
        doc["finger_id"] = studentID;
        doc["confidence"] = finger.confidence;
        
        String json;
        serializeJson(doc, json);
        int httpResponseCode = http.POST(json);
        http.end();
      }
      delay(3000);
      showMessage("Scanning Mode\nPress * back", 0);
    }
  }
}

void handleClearID() {
  // Try by scan first
  showMessage("Scan to Delete\n# for Manual ID", 0);
  while (true) {
    char k = keypad.getKey();
    if (k == '*') return;
    if (k == '#') {
      String sid = getInput("Enter ID to Clear:");
      // Search index
      for(int i=1; i<=1000; i++) {
        String key = "id_" + String(i);
        if (prefs.getString(key.c_str(), "") == sid) {
          finger.deleteModel(i);
          prefs.remove(key.c_str());
          showMessage("Deleted!", 1500);
          return;
        }
      }
      showMessage("Not Found", 1500);
      return;
    }

    if (finger.getImage() == FINGERPRINT_OK) {
       finger.image2Tz();
       if (finger.fingerSearch() == FINGERPRINT_OK) {
         String key = "id_" + String(finger.fingerID);
         finger.deleteModel(finger.fingerID);
         prefs.remove(key.c_str());
         showMessage("Deleted Match!", 1500);
         return;
       }
    }
  }
}

void handleClearAll() {
  String confirm = getInput("Clear ALL?\n1 to OK, * back");
  if (confirm == "1") {
    finger.emptyDatabase();
    // Clear all mapping keys - crude but simple for Preferences
    prefs.clear(); 
    // Restore year/section
    prefs.putInt("year", currentYear);
    prefs.putString("section", currentSection);
    showMessage("Wiped Clean!", 2000);
  }
}

bool ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 10) {
    delay(500);
    retry++;
  }
  return WiFi.status() == WL_CONNECTED;
}