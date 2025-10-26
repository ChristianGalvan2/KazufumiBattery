#include <GxEPD2_BW.h>
#include <EEPROM.h>
#include "Courier_Thin10ttf8pt7b.h"
#include "inverterCAN.h"

/*to do 
add other inverter functions, bms protocoll implementation, HM-interface design, PCB design
*/

// Waveshare 2.13" V4 (UC8151D)
GxEPD2_BW<GxEPD2_213_B74, GxEPD2_213_B74::HEIGHT> display(
  GxEPD2_213_B74(/*CS=*/5, /*DC=*/21, /*RST=*/22, /*BUSY=*/4)
);

InverterCAN inverter;

// -------------------- Inputs --------------------
#define POT_PIN 34
#define BTN_PIN 15

// -------------------- EEPROM --------------------
#define EEPROM_SIZE 64
struct Settings {
  int freq;
  int acVolt;
  int acProt;
  int dcProt;
} settings;

// -------------------- Menus --------------------
const char* mainMenuItems[] PROGMEM = { "Inverter", "BMS", "About", "Exit" };
const int mainMenuSize = sizeof(mainMenuItems) / sizeof(mainMenuItems[0]);

const char* inverterMenuItems[] PROGMEM = {
  "Power ON", "Power OFF", "Reset",
  "Set Freq", "Set AC Volt", "Set AC Prot", "Set DC Prot",
  "Back"
};
const int inverterMenuSize = sizeof(inverterMenuItems) / sizeof(inverterMenuItems[0]);

// -------------------- State --------------------
enum MenuLevel { STARTUP, MAIN, INVERTER, EDITING, ERROR_SCREEN };
MenuLevel menuLevel = STARTUP;

int selectedIndex = 0;
int windowStart = 0;

bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 200;

const int visibleCount = 4;
const int lineHeight = 25;
const int startY = 30;
const int highlightHeight = 25;

String lastFaultMessage = "";

// -------------------- Rhino Bitmap --------------------
const unsigned char rhinoBitmap[] PROGMEM = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0xff, 0xff, 
	0xff, 0xbf, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xbf, 0xc0, 
	0x00, 0x07, 0xff, 0xc0, 0x00, 0x07, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xbf, 0x80, 0x00, 0x07, 0xff, 
	0xc0, 0x00, 0x03, 0xff, 0xf0, 0xff, 0xff, 0xff, 0x3f, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x01, 
	0xbf, 0xf0, 0xff, 0xff, 0xff, 0x06, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x07, 0xff, 0xf0, 0xff, 
	0xff, 0xfe, 0x06, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x07, 0xef, 0xf0, 0xf7, 0xff, 0xfc, 0x06, 
	0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x07, 0xef, 0xf0, 0xf7, 0xff, 0xf8, 0x06, 0x00, 0x00, 0x00, 
	0x03, 0x00, 0x00, 0x07, 0xef, 0xf0, 0xf3, 0xef, 0xf0, 0x06, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 
	0x07, 0xef, 0xf0, 0xf3, 0xcf, 0xc0, 0x06, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x07, 0xef, 0xf0, 
	0xf3, 0xcf, 0x80, 0x06, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x07, 0xef, 0xf0, 0xf1, 0xc7, 0x00, 
	0x06, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x07, 0xef, 0xf0, 0xf1, 0xc0, 0x00, 0x06, 0x00, 0x00, 
	0x00, 0x03, 0x00, 0x00, 0x07, 0xff, 0xf0, 0xf0, 0xc0, 0x00, 0x06, 0x00, 0x00, 0x00, 0x03, 0x00, 
	0x00, 0x07, 0xff, 0xf0, 0xf0, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x07, 0xff, 
	0xf0, 0xf0, 0x00, 0x00, 0x1f, 0x80, 0x00, 0x00, 0x03, 0x00, 0x00, 0x07, 0xff, 0xf0, 0xf0, 0x00, 
	0x00, 0x3f, 0x80, 0x00, 0x00, 0x03, 0x00, 0x00, 0x07, 0xff, 0xf0, 0xe0, 0x00, 0x00, 0x7f, 0x80, 
	0x00, 0x00, 0x03, 0x00, 0x00, 0x07, 0xff, 0xf0, 0xc0, 0x00, 0x00, 0x7f, 0x80, 0x00, 0x00, 0x03, 
	0x00, 0x00, 0x07, 0xff, 0xf0, 0xc0, 0x00, 0x00, 0xff, 0x80, 0x00, 0x00, 0x03, 0x00, 0x00, 0x07, 
	0xff, 0xf0, 0xc0, 0x00, 0x01, 0xff, 0x80, 0x00, 0x00, 0x03, 0x00, 0x00, 0x07, 0xff, 0xf0, 0xc0, 
	0x00, 0x01, 0xff, 0x80, 0x00, 0x00, 0x03, 0x00, 0x00, 0x07, 0xff, 0xf0, 0xc0, 0x00, 0x03, 0xff, 
	0x80, 0x00, 0x00, 0x03, 0x00, 0x00, 0x07, 0xff, 0xf0, 0xc0, 0x00, 0x03, 0xff, 0x80, 0x00, 0x00, 
	0x03, 0x00, 0x00, 0x07, 0xff, 0xf0, 0xe0, 0x00, 0x07, 0xff, 0x80, 0x00, 0x00, 0x03, 0x00, 0x00, 
	0x07, 0xff, 0xf0, 0xf3, 0x00, 0x7f, 0xff, 0x80, 0x00, 0x03, 0xff, 0x80, 0x00, 0x07, 0xff, 0xf0, 
	0xff, 0xff, 0xff, 0xff, 0x80, 0x00, 0x07, 0xff, 0xc0, 0x00, 0x07, 0xff, 0xf0, 0xff, 0xff, 0xff, 
	0xff, 0xe0, 0x60, 0x3f, 0xff, 0xf0, 0x30, 0x3f, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xe0, 0xf0, 
	0x7f, 0xff, 0xf0, 0x78, 0x3f, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xe0, 0xf0, 0x7f, 0xff, 0xf0, 
	0x78, 0x3f, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xe0, 0xf0, 0x7f, 0xff, 0xf0, 0x78, 0x3f, 0xff, 
	0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xe0, 
	0xf0, 0x7f, 0xff, 0xf0, 0x78, 0x3f, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xe0, 0xe0, 0x7f, 0xff, 
	0xf0, 0x70, 0x3f, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xe0, 0x60, 0x7f, 0xff, 0xf0, 0x70, 0x3f, 
	0xff, 0xf0, 0xff, 0xff, 0xff, 0xff, 0x80, 0x00, 0x7f, 0xff, 0xc0, 0x20, 0x3f, 0xff, 0xf0, 0xff, 
	0xff, 0xff, 0xff, 0x80, 0x00, 0x7f, 0xff, 0xc0, 0x00, 0x3f, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0
};
const int rhinoW = 100;
const int rhinoH = 50;

// -------------------- Edit Mode --------------------
int editValue = 0;
String editLabel = "";
bool editExitMode = false;

// relative pot handling
int potCenter = 0;
int baseValue = 0;

// -------------------- Helpers --------------------
static inline int clampInt(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

void saveSettings() {
  EEPROM.put(0, settings);
  EEPROM.commit();
}

void loadSettings() {
  EEPROM.get(0, settings);
  if (settings.freq < 40 || settings.freq > 70) settings.freq = 50;
  if (settings.acVolt < 200 || settings.acVolt > 260) settings.acVolt = 230;
  if (settings.acProt < 200 || settings.acProt > 260) settings.acProt = 230;
  if (settings.dcProt < 40 || settings.dcProt > 70) settings.dcProt = 50;
}

// -------------------- Startup Screen --------------------
void drawStartupScreen() {
  display.setPartialWindow(0, 0, display.width(), display.height());
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setFont(&Courier_Thin10ttf8pt7b);
    display.setTextColor(GxEPD_BLACK);

    display.setCursor(5, 15);
    display.print(F("Kazufumi"));

    // Rhino bitmap
    int rhinoX = 10;
    int rhinoY = (display.height() - rhinoH) / 2 - 5;
    display.drawBitmap(rhinoX, rhinoY, rhinoBitmap, rhinoW, rhinoH, GxEPD_WHITE, GxEPD_BLACK);

    display.setCursor(rhinoX + rhinoW, 40);
    display.print(F("IN"));
    display.setCursor(rhinoX + rhinoW, 60);
    display.print(F("[A]: "));
    display.setCursor(rhinoX + rhinoW, 80);
    display.print(F("[V]: "));

    display.setCursor(rhinoX + rhinoW + 70, 40);
    display.print(F("OUT"));
    display.setCursor(rhinoX + rhinoW + 70, 60);
    display.print(F("[A]: "));
    display.setCursor(rhinoX + rhinoW + 70, 80);
    display.print(F("[V]: "));

    display.setCursor(10, 100);
    display.print(F("[C]:"));

    display.setCursor(display.width() - 65, display.height() - 5);
    display.print(F("MENU"));
  } while (display.nextPage());
}

// -------------------- Generic Menu Drawing --------------------
void drawMenu(const char** items, int size) {
  display.setPartialWindow(0, 0, display.width(), display.height());
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setFont(&Courier_Thin10ttf8pt7b);
    for (int i = 0; i < visibleCount; i++) {
      int itemIndex = windowStart + i;
      if (itemIndex >= size) break;
      int y = startY + i * lineHeight;
      if (itemIndex == selectedIndex) {
        display.fillRect(0, y - highlightHeight + 10, display.width(), highlightHeight, GxEPD_BLACK);
        display.setTextColor(GxEPD_WHITE);
      } else {
        display.setTextColor(GxEPD_BLACK);
      }
      display.setCursor(5, y);
      display.print(items[itemIndex]);
    }
  } while (display.nextPage());
}

// -------------------- Edit Screen --------------------
void drawEditScreen(const char* label, int value, const char* unit, bool exitMode) {
  display.setPartialWindow(0, 0, display.width(), display.height());
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setFont(&Courier_Thin10ttf8pt7b);
    display.setTextColor(GxEPD_BLACK);

    display.setCursor(10, 30);
    display.print(label);

    display.setCursor(10, 70);
    display.print(value);
    display.print(" ");
    display.print(unit);

    display.setCursor(10, 110);
    if (exitMode) display.print("[Exit] Press");
    else display.print("[OK] Press");
  } while (display.nextPage());
}

// -------------------- Navigation --------------------
void handleNavigation(const char** items, int size) {
  int potValue = analogRead(POT_PIN);
  int mapped = map(potValue, 0, 4095, 0, size - 1);
  int newIndex = clampInt(mapped, 0, size - 1);

  if (newIndex != selectedIndex) {
    selectedIndex = newIndex;
    if (selectedIndex < windowStart) windowStart = selectedIndex;
    else if (selectedIndex >= windowStart + visibleCount) windowStart = selectedIndex - visibleCount + 1;
    drawMenu(items, size);
  }
}

// -------------------- Setup --------------------
void setup() {
  Serial.begin(115200);
  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(POT_PIN, INPUT);

  EEPROM.begin(EEPROM_SIZE);
  loadSettings();

  display.init();
  display.setRotation(1);

  inverter.begin();
  drawStartupScreen();
  menuLevel = STARTUP;
}

// -------------------- Loop --------------------
void loop() {
  inverter.listen();

  int reading = digitalRead(BTN_PIN);

// --- Edit Mode ---
if (menuLevel == EDITING) {
  int potNow = analogRead(POT_PIN);

  if (editLabel == "Frequency") {
    // Only two options: 0=50Hz, 1=60Hz
    int mapped = map(potNow, 0, 4095, 0, 1);
    editValue = clampInt(mapped, 0, 1);
    drawEditScreen("Frequency", (editValue == 0 ? 50 : 60), "Hz", editExitMode);
  }
  else if (editLabel == "AC Volt") {
    // Only two options: 0=220V, 1=110V
    int mapped = map(potNow, 0, 4095, 0, 1);
    editValue = clampInt(mapped, 0, 1);
    drawEditScreen("AC Volt", (editValue == 0 ? 220 : 110), "V", editExitMode);
  }
  else {
    // Relative handling for protections
    int delta = (potNow - potCenter) / 50;
    editValue = clampInt(baseValue + delta,
                        (editLabel == "DC Prot" ? 40 : 200),
                        (editLabel == "DC Prot" ? 70 : 260));
    drawEditScreen(editLabel.c_str(), editValue, "V", editExitMode);
  }

  // --- Button press logic ---
  if (reading == LOW && lastButtonState == HIGH && (millis() - lastDebounceTime > debounceDelay)) {
    lastDebounceTime = millis();

    if (!editExitMode) {
      // First press → send + save
      if (editLabel == "Frequency") {
        inverter.setFrequency(1, editValue);   // 0=50Hz, 1=60Hz
        settings.freq = (editValue == 0 ? 50 : 60);
      }
      else if (editLabel == "AC Volt") {
        inverter.setACVoltageType(1, editValue);  // 0=220V, 1=110V
        settings.acVolt = (editValue == 0 ? 220 : 110);
      }
      else if (editLabel == "AC Prot") {
        inverter.setACProt(1, editValue, 180);
        settings.acProt = editValue;
      }
      else if (editLabel == "DC Prot") {
        inverter.setDCProt(1, editValue, 40);
        settings.dcProt = editValue;
      }

      saveSettings();

      // Switch to exit mode
      editExitMode = true;
      if (editLabel == "Frequency")
        drawEditScreen("Frequency", (editValue == 0 ? 50 : 60), "Hz", true);
      else if (editLabel == "AC Volt")
        drawEditScreen("AC Volt", (editValue == 0 ? 220 : 110), "V", true);
      else
        drawEditScreen(editLabel.c_str(), editValue, "V", true);
    } else {
      // Second press → return to inverter menu
      menuLevel = INVERTER;
      drawMenu(inverterMenuItems, inverterMenuSize);
      editExitMode = false;
    }
  }

  lastButtonState = reading;
  return;
}



  // --- Normal Menu ---
  if (reading == LOW && lastButtonState == HIGH && (millis() - lastDebounceTime > debounceDelay)) {
    lastDebounceTime = millis();
    if (menuLevel == STARTUP) {
      menuLevel = MAIN;
      selectedIndex = 0;
      windowStart = 0;
      drawMenu(mainMenuItems, mainMenuSize);
    } else if (menuLevel == MAIN) {
      String choice = mainMenuItems[selectedIndex];
      if (choice == "Inverter") {
        menuLevel = INVERTER;
        selectedIndex = 0;
        windowStart = 0;
        drawMenu(inverterMenuItems, inverterMenuSize);
      } else if (choice == "Exit") {
        menuLevel = STARTUP;
        drawStartupScreen();
      }
    } else if (menuLevel == INVERTER) {
      String choice = inverterMenuItems[selectedIndex];
      if (choice == "Back") {
        menuLevel = MAIN;
        drawMenu(mainMenuItems, mainMenuSize);
      } else if (choice == "Power ON") inverter.powerOn(1);
      else if (choice == "Power OFF") inverter.powerOff(1);
      else if (choice == "Reset") inverter.powerReset(1);
      else if (choice == "Set Freq") { 
        menuLevel = EDITING; 
        editLabel = "Frequency"; 
        editValue = settings.freq; 
        baseValue = settings.freq; 
        potCenter = analogRead(POT_PIN); 
        drawEditScreen("Frequency", editValue, "Hz", false); 
      }
      else if (choice == "Set AC Volt") { 
        menuLevel = EDITING; 
        editLabel = "AC Volt"; 
        editValue = settings.acVolt; 
        baseValue = settings.acVolt; 
        potCenter = analogRead(POT_PIN); 
        drawEditScreen("AC Volt", editValue, "V", false); 
      }
      else if (choice == "Set AC Prot") { 
        menuLevel = EDITING; 
        editLabel = "AC Prot"; 
        editValue = settings.acProt; 
        baseValue = settings.acProt; 
        potCenter = analogRead(POT_PIN); 
        drawEditScreen("AC Prot", editValue, "V", false); 
      }
      else if (choice == "Set DC Prot") { 
        menuLevel = EDITING; 
        editLabel = "DC Prot"; 
        editValue = settings.dcProt; 
        baseValue = settings.dcProt; 
        potCenter = analogRead(POT_PIN); 
        drawEditScreen("DC Prot", editValue, "V", false); 
      }
    }
  }
  lastButtonState = reading;

  if (menuLevel == MAIN) handleNavigation(mainMenuItems, mainMenuSize);
  else if (menuLevel == INVERTER) handleNavigation(inverterMenuItems, inverterMenuSize);
}
