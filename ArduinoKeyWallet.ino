#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define BUTTON_UP 2
#define BUTTON_DOWN 3
#define BUTTON_ENTER 4

#define NUM_RECORDS 4
#define TITLE_SIZE 32
#define CONTENT_SIZE 128

struct Record {
  char title[TITLE_SIZE];
  char content[CONTENT_SIZE];
};

Record records[NUM_RECORDS];
int currentRecord = 0;
int currentMode = 0; // 0: View, 1: Edit
int editCursor = 0;
bool editingTitle = true;
unsigned long enterPressTime = 0;

void setup() {
  pinMode(BUTTON_UP, INPUT_PULLDOWN);
  pinMode(BUTTON_DOWN, INPUT_PULLDOWN);
  pinMode(BUTTON_ENTER, INPUT_PULLDOWN);

  Serial.begin(9600);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.display();
  delay(2000);
  display.clearDisplay();

  // Inicializar registros si la EEPROM está vacía
  if (EEPROM.read(0) == 255) { // 255 indica EEPROM sin inicializar
    writeRecordsToEEPROM();
  }

  // Cargar registros desde EEPROM
  readRecordsFromEEPROM();
}

void loop() {
  if (digitalRead(BUTTON_UP) == HIGH) {
    if (currentMode == 1) {
      if (editingTitle) {
        records[currentRecord].title[editCursor]++;
      } else {
        records[currentRecord].content[editCursor]++;
      }
    } else {
      currentRecord = (currentRecord + 1) % NUM_RECORDS;
    }
    delay(200);
  }
  if (digitalRead(BUTTON_DOWN) == HIGH) {
    if (currentMode == 1) {
      if (editingTitle) {
        records[currentRecord].title[editCursor]--;
      } else {
        records[currentRecord].content[editCursor]--;
      }
    } else {
      currentRecord = (currentRecord - 1 + NUM_RECORDS) % NUM_RECORDS;
    }
    delay(200);
  }
  if (digitalRead(BUTTON_ENTER) == HIGH) {
    if (enterPressTime == 0) {
      enterPressTime = millis();
    } else if (millis() - enterPressTime >= 3000) {
      // Guardar cambios después de 3 segundos
      currentMode = 0;
      editCursor = 0;
      editingTitle = true;

      // Encriptar y guardar en EEPROM
      encrypt(records[currentRecord].title, TITLE_SIZE);
      encrypt(records[currentRecord].content, CONTENT_SIZE);
      EEPROM.put(currentRecord * sizeof(Record), records[currentRecord]);

      // Desencriptar para mostrar
      decrypt(records[currentRecord].title, TITLE_SIZE);
      decrypt(records[currentRecord].content, CONTENT_SIZE);

      enterPressTime = 0;
    }
  } else {
    if (enterPressTime != 0 && millis() - enterPressTime < 3000) {
      // Cambiar modo si el botón Enter se soltó antes de 3 segundos
      currentMode = !currentMode;
      if (currentMode == 1) {
        editCursor = 0;
        editingTitle = true;
      }
      enterPressTime = 0;
    }
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Record ");
  display.print(currentRecord + 1);
  display.print("/");
  display.print(NUM_RECORDS);
  display.print(currentMode == 1 ? " (Edit)" : "");
  display.setCursor(0, 10);
  display.print("Title: ");
  display.print(records[currentRecord].title);
  display.setCursor(0, 20);
  display.print("Content: ");
  display.print(records[currentRecord].content);
  
  if (currentMode == 1) {
    // Mostrar indicador de edición
    display.fillRect(editCursor * 6, editingTitle ? 10 : 20, 6, 8, SSD1306_INVERSE);
    
    if (digitalRead(BUTTON_ENTER) == HIGH) {
      editCursor++;
      if (editingTitle && editCursor >= TITLE_SIZE) {
        editingTitle = false;
        editCursor = 0;
      } else if (!editingTitle && editCursor >= CONTENT_SIZE) {
        // Finalizar edición
        currentMode = 0;
        editCursor = 0;
        editingTitle = true;

        // Encriptar y guardar en EEPROM
        encrypt(records[currentRecord].title, TITLE_SIZE);
        encrypt(records[currentRecord].content, CONTENT_SIZE);
        EEPROM.put(currentRecord * sizeof(Record), records[currentRecord]);

        // Desencriptar para mostrar
        decrypt(records[currentRecord].title, TITLE_SIZE);
        decrypt(records[currentRecord].content, CONTENT_SIZE);
      }
      delay(200);
    }
  }

  display.display();
}

void writeRecordsToEEPROM() {
  Record records[4] = {
    {"Title 1", "Content 1"},
    {"Title 2", "Content 2"},
    {"Title 3", "Content 3"},
    {"Title 4", "Content 4"}
  };

  for (int i = 0; i < 4; i++) {
    // Encriptar antes de guardar
    encrypt(records[i].title, TITLE_SIZE);
    encrypt(records[i].content, CONTENT_SIZE);

    EEPROM.put(i * sizeof(Record), records[i]);

    // Desencriptar para mantener la consistencia en RAM
    decrypt(records[i].title, TITLE_SIZE);
    decrypt(records[i].content, CONTENT_SIZE);
  }
}

void readRecordsFromEEPROM() {
  for (int i = 0; i < NUM_RECORDS; i++) {
    EEPROM.get(i * sizeof(Record), records[i]);

    // Desencriptar después de leer
    decrypt(records[i].title, TITLE_SIZE);
    decrypt(records[i].content, CONTENT_SIZE);
  }
}

void encrypt(char* data, int len) {
  for (int i = 0; i < len; i++) {
    data[i] = data[i] ^ 0xAA; // XOR encryption
  }
}

void decrypt(char* data, int len) {
  for (int i = 0; i < len; i++) {
    data[i] = data[i] ^ 0xAA; // XOR decryption
  }
}
