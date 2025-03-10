#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>
#include <Keyboard.h>

#define OLED_WIDTH 128
#define OLED_HEIGHT 32
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire);

// Configuración de pines
const int BTN_UP = 10;
const int BTN_DOWN = 16;
const int BTN_ENTER = 14;

// Constantes del sistema
const int TOTAL_RECORDS = 4;
const int MAX_TITLE_LENGTH = 32;
const int MAX_CONTENT_LENGTH = 128;
const int RECORD_SIZE = MAX_TITLE_LENGTH + MAX_CONTENT_LENGTH + 2; // +2 para checksums

struct DataRecord {
  char header[MAX_TITLE_LENGTH + 1];
  char details[MAX_CONTENT_LENGTH + 1];
};

DataRecord dataset[TOTAL_RECORDS];
int activeRecord = 0;
uint8_t menuMode = 0; // 0: Vista, 1: Edición, 2: Confirmación
uint8_t editMode = 0; // 0: Título, 1: Contenido
int cursorPos = 0;
unsigned long lastInteraction = 0;
unsigned long pressStartTime = 0;
bool isEnterPressed = false;
const unsigned long LONG_PRESS_DURATION = 3000;
bool showSavedMessage = false;
unsigned long savedMessageTime = 0;
const unsigned long SAVED_MESSAGE_DURATION = 1500;

void setup() {
  delay(500);
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_ENTER, INPUT_PULLUP);
  
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  Keyboard.begin();
  
  if (!checkEEPROMInitialized()) {
    initializeEEPROM();
  }
  loadRecord(activeRecord);
}

void loop() {
  handleButtonEvents();
  updateDisplay();
  checkTimeout();
  
  // Manejador de mensaje de guardado
  if (showSavedMessage && millis() - savedMessageTime > SAVED_MESSAGE_DURATION) {
    showSavedMessage = false;
  }
}

// Funciones de manejo de EEPROM
bool checkEEPROMInitialized() {
  return EEPROM.read(0) != 255;
}

void initializeEEPROM() {
  for (int i = 0; i < TOTAL_RECORDS; i++) {
    sprintf(dataset[i].header, "Header %d", i+1);
    sprintf(dataset[i].details, "Details %d", i+1);
    saveRecord(i);
  }
}

void loadRecord(int index) {
  int address = index * RECORD_SIZE;
  for (int i = 0; i < MAX_TITLE_LENGTH; i++) {
    dataset[index].header[i] = EEPROM.read(address + i);
  }
  dataset[index].header[MAX_TITLE_LENGTH] = '\0';  // Garantizar terminación
  
  for (int i = 0; i < MAX_CONTENT_LENGTH; i++) {
    dataset[index].details[i] = EEPROM.read(address + MAX_TITLE_LENGTH + 1 + i);
  }
  dataset[index].details[MAX_CONTENT_LENGTH] = '\0';  // Garantizar terminación
}

void saveRecord(int index) {
  int address = index * RECORD_SIZE;
  for (int i = 0; i < MAX_TITLE_LENGTH + 1; i++) {
    EEPROM.write(address + i, dataset[index].header[i]);
  }
  for (int i = 0; i < MAX_CONTENT_LENGTH + 1; i++) {
    EEPROM.write(address + MAX_TITLE_LENGTH + 1 + i, dataset[index].details[i]);
  }
}

void handleButtonEvents() {
  bool currentUpState = digitalRead(BTN_UP) == LOW;
  bool currentDownState = digitalRead(BTN_DOWN) == LOW;
  bool currentEnterState = digitalRead(BTN_ENTER) == LOW;

  // Si se está mostrando el mensaje de guardado, no procesar botones
  if (showSavedMessage) {
    return;
  }

  if (currentUpState) {
    lastInteraction = millis();
    if (menuMode == 0) {
      activeRecord = (activeRecord + 1) % TOTAL_RECORDS;
      loadRecord(activeRecord);
    } else if (menuMode == 1) {
      if (editMode == 0) {
        incrementChar(dataset[activeRecord].header[cursorPos]);
      } else {
        incrementChar(dataset[activeRecord].details[cursorPos]);
      }
    } else if (menuMode == 2) {
      saveChanges();
    }
    delay(200); // Pequeño debounce para evitar múltiples pulsaciones
  }

  if (currentDownState) {
    lastInteraction = millis();
    if (menuMode == 0) {
      activeRecord = (activeRecord - 1 + TOTAL_RECORDS) % TOTAL_RECORDS;
      loadRecord(activeRecord);
    } else if (menuMode == 1) {
      if (editMode == 0) {
        decrementChar(dataset[activeRecord].header[cursorPos]);
      } else {
        decrementChar(dataset[activeRecord].details[cursorPos]);
      }
    } else if (menuMode == 2) {
      menuMode = 1;
    }
    delay(200); // Pequeño debounce para evitar múltiples pulsaciones
  }

  if (currentEnterState) {
    if (!isEnterPressed) {
      pressStartTime = millis();
      isEnterPressed = true;
    } else if (isEnterPressed && millis() - pressStartTime >= LONG_PRESS_DURATION) {
      lastInteraction = millis();
      if (menuMode == 0) {
        // En modo vista, pulsación larga envía el contenido al teclado
        Keyboard.print(dataset[activeRecord].details);
      } else if (menuMode == 1) {
        if (editMode == 0) {
          editMode = 1;
          cursorPos = 0;
        } else {
          menuMode = 2;
        }
      }
      isEnterPressed = false;
    }
  } else {
    if (isEnterPressed) {
      lastInteraction = millis();
      if (millis() - pressStartTime < LONG_PRESS_DURATION) {
        // Pulsación corta
        if (menuMode == 0) {
          menuMode = 1;
          cursorPos = 0;
          editMode = 0;
        } else if (menuMode == 1) {
          // Navegación mejorada
          if (editMode == 0) {
            if (cursorPos >= strlen(dataset[activeRecord].header) || cursorPos >= MAX_TITLE_LENGTH - 1) {
              editMode = 1;
              cursorPos = 0;
            } else {
              cursorPos++;
            }
          } else { // editMode == 1
            if (cursorPos >= strlen(dataset[activeRecord].details) || cursorPos >= MAX_CONTENT_LENGTH - 1) {
              menuMode = 2;
            } else {
              cursorPos++;
            }
          }
        }
      }
      isEnterPressed = false;
    }
  }
}

void incrementChar(char &c) {
  if (c < 126) c++; else c = 32;
}

void decrementChar(char &c) {
  if (c > 32) c--; else c = 126;
}

void updateDisplay() {
  display.clearDisplay();
  display.setCursor(0, 0);

  if (showSavedMessage) {
    displaySavedMessage();
  } else if (menuMode == 0) {
    displayViewMode();
  } else if (menuMode == 1) {
    displayEditMode();
  } else if (menuMode == 2) {
    displaySaveConfirmation();
  }

  display.display();
}

void displayViewMode() {
  display.print("Registro ");
  display.print(activeRecord + 1);
  display.print("/");
  display.println(TOTAL_RECORDS);

  display.println(dataset[activeRecord].header);
  display.println(dataset[activeRecord].details);

  display.println("UP/DN:Nav ENT:Edit HOLD:Send");
}

void displayEditMode() {
  display.print("Editando ");
  display.println(editMode == 0 ? "Titulo" : "Contenido");

  display.println(dataset[activeRecord].header);
  display.println(dataset[activeRecord].details);

  int cursorY = editMode == 0 ? 8 : 16;
  display.fillRect(cursorPos * 6, cursorY, 6, 8, SSD1306_INVERSE);
  
  // Añadir indicador de pulsación larga
  if (isEnterPressed) {
    int progress = map(millis() - pressStartTime, 0, LONG_PRESS_DURATION, 0, 128);
    display.fillRect(0, 31, progress, 1, SSD1306_WHITE);
  }
}

void displaySaveConfirmation() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Guardar cambios?");
  display.println("UP: Si");
  display.println("DOWN: No");
}

void displaySavedMessage() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("cambios guardados!");
  display.println("");
  display.println("Volviendo al modo de visualización...");
}

void checkTimeout() {
  const unsigned long TIMEOUT_DURATION = 30000;
  if (menuMode != 0 && millis() - lastInteraction > TIMEOUT_DURATION) {
    menuMode = 0;
  }
}

void saveChanges() {
  saveRecord(activeRecord);
  showSavedMessage = true;
  savedMessageTime = millis();
  menuMode = 0;
}
