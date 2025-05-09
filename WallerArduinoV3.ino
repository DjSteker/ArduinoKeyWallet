#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>
#include <Keyboard.h>

#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 32
#define OLED_RESET_PIN -1
Adafruit_SSD1306 oled(DISPLAY_WIDTH, DISPLAY_HEIGHT, &Wire, OLED_RESET_PIN);

#define BTN_UP 10
#define BTN_DOWN 16
#define BTN_ENTER 14

#define TOTAL_RECORDS 4
#define MAX_TITLE_LENGTH 32
#define MAX_CONTENT_LENGTH 128

struct Data {
  String header;
  String details;
};

Data dataset[TOTAL_RECORDS];
int activeRecord = 0;
int menuMode = 0;  // 0: View, 1: Edit, 2: Confirm Save
int cursorPosition = 0;
bool isEditingTitle = true;
unsigned long pressStartTime = 0;
bool isEnterPressed = false;
unsigned long lastInteractionTime = 0;
const unsigned long IDLE_TIMEOUT = 30000;

bool saveConfirmation = false;

bool prevUpState = LOW;
bool prevDownState = LOW;
bool prevEnterState = LOW;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_ENTER, INPUT_PULLUP);
  Serial.begin(9600);
  Keyboard.begin();

  if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }
  oled.display();
  delay(2000);
  oled.clearDisplay();

  if (EEPROM.read(0) == 255) {
    initializeEEPROM();
  }

  loadFromEEPROM();
}

void loop() {
  handleButtonEvents();
  checkForTimeout();
  refreshDisplay();
}

void handleButtonEvents() {
  bool currentUpState = digitalRead(BTN_UP);
  bool currentDownState = digitalRead(BTN_DOWN);
  bool currentEnterState = digitalRead(BTN_ENTER);

  if (currentUpState == LOW && prevUpState == HIGH) {
    lastInteractionTime = millis();
    processUpButton();
  }

  if (currentDownState == LOW && prevDownState == HIGH) {
    lastInteractionTime = millis();
    processDownButton();
  }

  if (currentEnterState == LOW && prevEnterState == HIGH) {
    lastInteractionTime = millis();
    pressStartTime = millis();
    isEnterPressed = true;
  } else if (currentEnterState == HIGH && prevEnterState == LOW) {
    if (isEnterPressed) {
      unsigned long pressDuration = millis() - pressStartTime;
      processEnterButton(pressDuration);
      isEnterPressed = false;
    }
  } else if (isEnterPressed && millis() - pressStartTime >= 3000) {
    menuMode = 1;  // Entrar en modo edición
    cursorPosition = 0;
    isEditingTitle = true;
    isEnterPressed = false;
    displayMessage("Modo Edicion", 1000);
  }

  prevUpState = currentUpState;
  prevDownState = currentDownState;
  prevEnterState = currentEnterState;

  delay(50);
}

void processUpButton() {
  if (menuMode == 0) {
    activeRecord = (activeRecord + 1) % TOTAL_RECORDS;
  } else if (menuMode == 1) {
    if (isEditingTitle) {
      modifyCharacter(dataset[activeRecord].header, true);
    } else {
      modifyCharacter(dataset[activeRecord].details, true);
    }
  } else if (menuMode == 2) {
    saveChanges();
    displayMessage("Guardado!", 1000);
    menuMode = 0;
  }
}

void processDownButton() {
  if (menuMode == 0) {
    activeRecord = (activeRecord - 1 + TOTAL_RECORDS) % TOTAL_RECORDS;
  } else if (menuMode == 1) {
    if (isEditingTitle) {
      modifyCharacter(dataset[activeRecord].header, false);
    } else {
      modifyCharacter(dataset[activeRecord].details, false);
    }
  } else if (menuMode == 2) {
    menuMode = 0;
    displayMessage("Cancelado", 1000);
  }
}

void processEnterButton(unsigned long pressDuration) {
  if (pressDuration < 3000) {  // Pulsación corta
    if (menuMode == 0) {
      Keyboard.print(dataset[activeRecord].details);
    } else if (menuMode == 1) {
      if (isEditingTitle) {
        if (cursorPosition < dataset[activeRecord].header.length()) {
          cursorPosition++;
        } else {
          isEditingTitle = false;
          cursorPosition = 0;
        }
      } else {
        if (cursorPosition < dataset[activeRecord].details.length()) {
          cursorPosition++;
        } else {
          menuMode = 2;  // Confirmación de guardado
          displayMessage("Guardar?", 1000);
        }
      }
    } else if (menuMode == 2) {
      menuMode = 0;
    }
  } else {  // Pulsación larga
    if (menuMode == 0) {
      menuMode = 1;  // Modo edición
      cursorPosition = 0;
      isEditingTitle = true;
      displayMessage("Modo Edicion", 1000);
    }
  }
}

void modifyCharacter(String& text, bool increment) {
  if (cursorPosition < text.length()) {
    char c = text[cursorPosition];
    c = (increment) ? (c < 126) ? c + 1 : 32 : (c > 32) ? c - 1
                                                        : 126;
    text[cursorPosition] = c;
  } else if (cursorPosition < (isEditingTitle ? MAX_TITLE_LENGTH : MAX_CONTENT_LENGTH) - 1) {
    text += ' ';
  }
}

void checkForTimeout() {
  if (menuMode != 0 && millis() - lastInteractionTime > IDLE_TIMEOUT) {
    menuMode = 0;
    cursorPosition = 0;
    isEditingTitle = true;
    saveConfirmation = false;
  }
}

void refreshDisplay() {
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0, 0);

  if (menuMode == 0) {
    displayViewMode();
  } else if (menuMode == 1) {
    displayEditMode();
  } else if (menuMode == 2) {
    displaySaveConfirmation();
  }

  oled.display();
}

void displayViewMode() {
  oled.print("Registro ");
  oled.print(activeRecord + 1);
  oled.print("/");
  oled.println(TOTAL_RECORDS);

  oled.println(dataset[activeRecord].header);
  oled.println(dataset[activeRecord].details);

  oled.println("UP/DOWN: Navega");
  oled.println("ENTER: Selecciona");
}

void displayEditMode() {
  oled.print("Editando ");
  oled.println(isEditingTitle ? "Titulo" : "Contenido");

  oled.println(dataset[activeRecord].header);
  oled.println(dataset[activeRecord].details);

  int cursorY = isEditingTitle ? 8 : 16;
  oled.fillRect(cursorPosition * 6, cursorY, 6, 8, SSD1306_INVERSE);
}

void displaySaveConfirmation() {
  oled.println("Guardar cambios?");
  oled.println("UP: Si");
  oled.println("DOWN: No");
}

void initializeEEPROM() {
  for (int i = 0; i < TOTAL_RECORDS; i++) {
    dataset[i].header = "Header " + String(i + 1);
    dataset[i].details = "Details " + String(i + 1);
    saveToEEPROM(i);
  }
}

void loadFromEEPROM() {
  for (int i = 0; i < TOTAL_RECORDS; i++) {
    readFromEEPROM(i);
  }
}

void saveToEEPROM(int index) {
  int address = index * (MAX_TITLE_LENGTH + MAX_CONTENT_LENGTH);
  for (int i = 0; i < MAX_TITLE_LENGTH; i++) {
    EEPROM.write(address + i, dataset[index].header[i] ^ 0xAA);
  }
  address += MAX_TITLE_LENGTH;
  for (int i = 0; i < MAX_CONTENT_LENGTH; i++) {
    EEPROM.write(address + i, dataset[index].details[i] ^ 0xAA);
  }
}

void readFromEEPROM(int index) {
  int address = index * (MAX_TITLE_LENGTH + MAX_CONTENT_LENGTH);
  dataset[index].header = "";
  for (int i = 0; i < MAX_TITLE_LENGTH; i++) {
    char c = EEPROM.read(address + i) ^ 0xAA;
    if (c == 0) break;
    dataset[index].header += c;
  }
  address += MAX_TITLE_LENGTH;
  dataset[index].details = "";
  for (int i = 0; i < MAX_CONTENT_LENGTH; i++) {
    char c = EEPROM.read(address + i) ^ 0xAA;
    if (c == 0) break;
    dataset[index].details += c;
  }
}

void saveChanges() {
  saveToEEPROM(activeRecord);
  saveConfirmation = false;
}

void displayMessage(const char* message, int duration) {
  oled.clearDisplay();
  oled.setCursor(0, 0);
  oled.println(message);
  oled.display();
  delay(duration);
}
