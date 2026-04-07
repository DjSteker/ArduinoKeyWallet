#define COMPILATION_NAME    "KeyVault"
#define COMPILATION_DATE    __DATE__
#define COMPILATION_TIME    __TIME__
#define COMPILATION_VERSION  "v3.6"

#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>
#include <Keyboard.h>
#include <avr/pgmspace.h>

#define OLED_W 128
#define OLED_H 32
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(OLED_W, OLED_H, &Wire);

// Pines
static const uint8_t PIN_UP    = 10;
static const uint8_t PIN_DOWN  = 16;
static const uint8_t PIN_ENTER = 14;

// Configuración general
static const uint8_t TOTAL_RECORDS     = 4;
static const uint8_t MAX_TITLE_LEN     = 32;
static const uint8_t MAX_CONTENT_LEN   = 128;

// Estados de menú
enum MenuMode { MODE_VIEW, MODE_EDIT, MODE_CONFIRM, MODE_CONFIG, MODE_UNSAVED };
static uint8_t menuMode = MODE_VIEW;

// Campos de edición
static const uint8_t FIELD_TITLE   = 0;
static const uint8_t FIELD_CONTENT  = 1;

// Tiempos
static const unsigned long T_LONG_PRESS   = 2000;
static const unsigned long T_TIMEOUT      = 30000;
static const unsigned long T_SAVED_MSG     = 1200;
static const unsigned long T_DEBOUNCE      = 30;
static const unsigned long T_REPT_START    = 400;
static const unsigned long T_REPT_FAST     = 40;

// Layout teclado
static const uint8_t KB_EN = 0;
static const uint8_t KB_ES = 1;
static const uint8_t KB_COUNT = 2;

// EEPROM Layout
static const uint8_t EE_MAGIC0 = 0;
static const uint8_t EE_MAGIC1 = 1;
static const uint8_t EE_MAGIC2 = 2;
static const uint8_t EE_MAGIC3 = 3;
static const uint8_t EE_VER    = 4;
static const uint8_t EE_LAYOUT  = 5;
static const uint8_t EE_ACTIVE  = 6;
static const uint8_t EE_STATE_CRC = 7;

static const uint8_t EE_MAGIC_VAL[4] = {'K', 'V', '3', '4'};
static const uint8_t EE_VER_VAL  = 1;
static const uint16_t EE_DATA_START = 16;
static const uint16_t RECORD_EE_SIZE = (MAX_TITLE_LEN + 1) + (MAX_CONTENT_LEN + 1) + 1;

static const char CHAR_MIN = 32;
static const char CHAR_MAX = 126;

// -----------------------------------------------------------------------------
// Estructuras
// -----------------------------------------------------------------------------
struct DataRecord {
  char header[MAX_TITLE_LEN + 1];
  char details[MAX_CONTENT_LEN + 1];
  bool dirty;
};

struct Button {
  bool raw = false;
  bool state = false;
  bool pressed = false;
  bool released = false;
  bool longFired = false;
  unsigned long lastChange = 0;
  unsigned long pressTime = 0;
  unsigned long lastRepeat = 0;
};

struct CharMap {
  char ch;
  uint8_t mod;
  uint8_t key;
};

static const CharMap ES_MAP[] PROGMEM = {
  { '@', KEY_RIGHT_ALT, '2' }, { '#', KEY_RIGHT_ALT, '3' },
  { '~', KEY_RIGHT_ALT, '4' }, { '[', KEY_RIGHT_ALT, '[' },
  { ']', KEY_RIGHT_ALT, ']' }, { '{', KEY_RIGHT_ALT, '\'' },
  { '}', KEY_RIGHT_ALT, '\\' }, { '\\', KEY_RIGHT_ALT, '`' },
  { '|', KEY_RIGHT_ALT, '1' }
};
static const uint8_t ES_MAP_LEN = sizeof(ES_MAP) / sizeof(ES_MAP[0]);

// -----------------------------------------------------------------------------
// Variables globales
// -----------------------------------------------------------------------------
DataRecord dataset[TOTAL_RECORDS];
uint8_t activeRecord = 0;
uint8_t editField = FIELD_TITLE;
uint8_t cursorPos = 0;
uint8_t kbLayout = KB_EN;
uint8_t kbLayoutTmp = KB_EN;

unsigned long lastInteraction = 0;
unsigned long savedMsgTime = 0;
bool showSavedMsg = false;
bool displaySleeping = false;
bool refreshNeeded = true;

Button bUp, bDown, bEnter;

// -----------------------------------------------------------------------------
// Funciones Auxiliares
// -----------------------------------------------------------------------------
static uint8_t crc8(const uint8_t *data, uint16_t len) {
  uint8_t crc = 0xFF;
  for (uint16_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t b = 0; b < 8; b++) {
      if (crc & 0x80) {
        crc = (crc << 1) ^ 0x31;
      } else {
        crc = (crc << 1);
      }
    }
  }
  return crc;
}

static uint8_t stateCRC(uint8_t ar, uint8_t kb) {
  uint8_t buf[2] = { ar, kb };
  return crc8(buf, 2);
}

static uint8_t recordCRC(uint8_t idx) {
  return crc8((uint8_t*)&dataset[idx], (MAX_TITLE_LEN + 1 + MAX_CONTENT_LEN + 1));
}

static uint16_t recordAddr(uint8_t idx) {
  return EE_DATA_START + (uint16_t)idx * RECORD_EE_SIZE;
}

static uint8_t activeFieldMax() { 
  return (editField == FIELD_TITLE) ? MAX_TITLE_LEN : MAX_CONTENT_LEN; 
}

static char &activeChar() { 
  if (editField == FIELD_TITLE) {
    return dataset[activeRecord].header[cursorPos];
  } else {
    return dataset[activeRecord].details[cursorPos];
  }
}

// -----------------------------------------------------------------------------
// EEPROM
// -----------------------------------------------------------------------------
static void writeStateToEEPROM() {
  for(uint8_t i = 0; i < 4; i++) {
    EEPROM.update(EE_MAGIC0 + i, EE_MAGIC_VAL[i]);
  }
  EEPROM.update(EE_VER, EE_VER_VAL);
  EEPROM.update(EE_LAYOUT, kbLayout);
  EEPROM.update(EE_ACTIVE, activeRecord);
  EEPROM.update(EE_STATE_CRC, stateCRC(activeRecord, kbLayout));
}

static void loadRecord(uint8_t idx) {
  uint16_t addr = recordAddr(idx);
  for (uint8_t i = 0; i <= MAX_TITLE_LEN; i++) {
    dataset[idx].header[i] = (char)EEPROM.read(addr + i);
  }
  uint16_t cAddr = addr + MAX_TITLE_LEN + 1;
  for (uint8_t i = 0; i <= MAX_CONTENT_LEN; i++) {
    dataset[idx].details[i] = (char)EEPROM.read(cAddr + i);
  }
  
  uint8_t storedCRC = EEPROM.read(cAddr + MAX_CONTENT_LEN + 1);
  if (storedCRC != recordCRC(idx)) {
    strcpy(dataset[idx].header, "Error Datos");
    strcpy(dataset[idx].details, "Vacio");
  }
  dataset[idx].dirty = false;
}

static void saveRecord(uint8_t idx) {
  uint16_t addr = recordAddr(idx);
  for (uint8_t i = 0; i <= MAX_TITLE_LEN; i++) {
    EEPROM.update(addr + i, dataset[idx].header[i]);
  }
  uint16_t cAddr = addr + MAX_TITLE_LEN + 1;
  for (uint8_t i = 0; i <= MAX_CONTENT_LEN; i++) {
    EEPROM.update(cAddr + i, dataset[idx].details[i]);
  }
  EEPROM.update(cAddr + MAX_CONTENT_LEN + 1, recordCRC(idx));
  dataset[idx].dirty = false;
}

// -----------------------------------------------------------------------------
// Keyboard
// -----------------------------------------------------------------------------
static void sendChar(char c) {
  if (kbLayout == KB_ES) {
    CharMap t;
    for (uint8_t i = 0; i < ES_MAP_LEN; i++) {
      memcpy_P(&t, &ES_MAP[i], sizeof(CharMap));
      if (t.ch == c) {
        Keyboard.press(t.mod); 
        Keyboard.press(t.key);
        delay(15); 
        Keyboard.releaseAll();
        return;
      }
    }
  }
  Keyboard.print(c);
}

static void sendRecord(uint8_t idx) {
  const char *s = dataset[idx].details;
  for (uint16_t i = 0; s[i] != '\0'; i++) {
    sendChar(s[i]);
    delay(10);
  }
}

// -----------------------------------------------------------------------------
// Input
// -----------------------------------------------------------------------------
static void pollButton(Button &b, uint8_t pin) {
  bool raw = (digitalRead(pin) == LOW);
  unsigned long now = millis();
  b.pressed = false;
  b.released = false;

  if (raw != b.raw) { 
    b.raw = raw; 
    b.lastChange = now; 
  }

  if ((now - b.lastChange) >= T_DEBOUNCE && raw != b.state) {
    b.state = raw;
    if (raw) { 
      b.pressed = true; 
      b.pressTime = now; 
      b.longFired = false; 
      b.lastRepeat = now; 
    } else {
      b.released = true;
    }
    refreshNeeded = true;
  }
}

static bool checkRepeat(Button &b) {
  if (!b.state) {
    return false;
  }
  unsigned long now = millis();
  unsigned long held = now - b.pressTime;
  if (held < T_REPT_START) {
    return false;
  }

  uint16_t interval = (held < 2000) ? 120 : T_REPT_FAST;
  if (now - b.lastRepeat >= interval) {
    b.lastRepeat = now;
    refreshNeeded = true;
    return true;
  }
  return false;
}

// -----------------------------------------------------------------------------
// Display
// -----------------------------------------------------------------------------
static void printWindowed(const char *str, uint8_t cursorP, bool showCursor) {
  uint8_t charsPerLine = OLED_W / 6;
  uint8_t winStart = 0;
  if (showCursor) {
    winStart = (cursorP / charsPerLine) * charsPerLine;
  }
  
  for (uint8_t i = 0; i < charsPerLine; i++) {
    uint8_t pos = winStart + i;
    if (str[pos] != '\0') {
      display.write(str[pos]);
    } else {
      if (showCursor && pos < activeFieldMax()) {
        display.write(' ');
      } else {
        display.write('\0');
      }
    }
  }
}

static void updateDisplay() {
  if (displaySleeping || !refreshNeeded) {
    return;
  }

  display.clearDisplay();
  display.setCursor(0, 0);

  if (showSavedMsg) {
    display.println(F("\n >> Guardado! <<"));
  } else {
    switch (menuMode) {
      case MODE_VIEW:
        display.print(F("Reg ")); 
        display.print(activeRecord + 1);
        display.print(kbLayout == KB_EN ? F(" [EN]") : F(" [ES]"));
        if (dataset[activeRecord].dirty) {
          display.print('*');
        }
        display.println();
        printWindowed(dataset[activeRecord].header, 0, false); 
        display.println();
        printWindowed(dataset[activeRecord].details, 0, false); 
        display.println();
        display.print(F("U/D:Nav ENT:Edit"));
        break;

      case MODE_EDIT: {
        display.print(editField == FIELD_TITLE ? F("TIT[") : F("CON["));
        display.print(cursorPos + 1); 
        display.println(']');
        
        if (editField == FIELD_TITLE) {
          printWindowed(dataset[activeRecord].header, cursorPos, true);
        } else {
          printWindowed(dataset[activeRecord].header, 0, false);
        }
        
        display.setCursor(0, 16);
        if (editField == FIELD_CONTENT) {
          printWindowed(dataset[activeRecord].details, cursorPos, true);
        } else {
          printWindowed(dataset[activeRecord].details, 0, false);
        }

        uint8_t cx = (cursorPos % (OLED_W/6)) * 6;
        uint8_t cy = (editField == FIELD_TITLE) ? 8 : 16;
        display.fillRect(cx, cy, 6, 8, SSD1306_INVERSE);
        break;
      }

      case MODE_CONFIRM:
        display.println(F("Guardar cambios?"));
        display.println(F(" UP : SI"));
        display.println(F(" DN : NO"));
        break;

      case MODE_CONFIG:
        display.println(F("== Idioma =="));
        display.print(kbLayoutTmp == KB_EN ? F("> ENGLISH") : F("  ENGLISH")); 
        display.println();
        display.print(kbLayoutTmp == KB_ES ? F("> ESPANOL") : F("  ESPANOL"));
        break;

      case MODE_UNSAVED:
        display.println(F("! Datos sin guardar !"));
        display.println(F(" UP : Guardar ahora"));
        display.println(F(" DN : Descartar"));
        break;
    }
  }

  display.display();
  refreshNeeded = false;
}

// -----------------------------------------------------------------------------
// Lógica Principal
// -----------------------------------------------------------------------------
void wakeDisplay() {
  if (displaySleeping) {
    display.ssd1306_command(SSD1306_DISPLAYON);
    displaySleeping = false;
    refreshNeeded = true;
  }
  lastInteraction = millis();
}

void handleInput() {
  if (showSavedMsg) {
    return;
  }
  unsigned long now = millis();

  if ((bUp.pressed || bDown.pressed || bEnter.pressed) && displaySleeping) {
    wakeDisplay(); 
    return;
  }

  if (bUp.pressed || bDown.pressed || bEnter.pressed) {
    lastInteraction = now;
  }

  switch (menuMode) {
    case MODE_VIEW:
      if (bUp.pressed) { 
        activeRecord = (activeRecord + 1) % TOTAL_RECORDS; 
        loadRecord(activeRecord); 
      }
      if (bDown.pressed) { 
        activeRecord = (activeRecord > 0) ? activeRecord - 1 : TOTAL_RECORDS - 1; 
        loadRecord(activeRecord); 
      }
      if (bEnter.released && !bEnter.longFired) { 
        menuMode = MODE_EDIT; 
        editField = FIELD_TITLE; 
        cursorPos = 0; 
      }
      if (bEnter.state && !bEnter.longFired && (now - bEnter.pressTime > T_LONG_PRESS)) {
        bEnter.longFired = true; 
        sendRecord(activeRecord);
      }
      if (bDown.state && !bDown.longFired && (now - bDown.pressTime > T_LONG_PRESS)) {
        bDown.longFired = true; 
        kbLayoutTmp = kbLayout; 
        menuMode = MODE_CONFIG;
      }
      break;

    case MODE_EDIT:
      if (bUp.pressed || checkRepeat(bUp)) { 
        if (activeChar() < CHAR_MAX) {
          activeChar()++;
        } else {
          activeChar() = CHAR_MIN;
        }
        dataset[activeRecord].dirty = true; 
      }
      if (bDown.pressed || checkRepeat(bDown)) { 
        if (activeChar() > CHAR_MIN) {
          activeChar()--;
        } else {
          activeChar() = CHAR_MAX;
        }
        dataset[activeRecord].dirty = true; 
      }
      if (bEnter.released && !bEnter.longFired) {
        if (cursorPos < activeFieldMax() - 1) {
          cursorPos++;
        } else if (editField == FIELD_TITLE) { 
          editField = FIELD_CONTENT; 
          cursorPos = 0; 
        } else {
          menuMode = MODE_CONFIRM; 
        }
      }
      if (bEnter.state && !bEnter.longFired && (now - bEnter.pressTime > T_LONG_PRESS)) {
        bEnter.longFired = true;
        if (editField == FIELD_TITLE) { 
          editField = FIELD_CONTENT; 
          cursorPos = 0; 
        } else {
          menuMode = MODE_CONFIRM; 
        }
      }
      break;

    case MODE_CONFIRM:
      if (bUp.pressed) { 
        saveRecord(activeRecord); 
        showSavedMsg = true; 
        savedMsgTime = now; 
        menuMode = MODE_VIEW; 
        writeStateToEEPROM(); 
      }
      if (bDown.pressed) {
        menuMode = MODE_EDIT;
      }
      break;

    case MODE_CONFIG:
      if (bUp.pressed || bDown.pressed) {
        kbLayoutTmp = (kbLayoutTmp + 1) % KB_COUNT;
      }
      if (bEnter.pressed) { 
        kbLayout = kbLayoutTmp; 
        menuMode = MODE_VIEW; 
        writeStateToEEPROM(); 
      }
      break;

    case MODE_UNSAVED:
      if (bUp.pressed) { 
        saveRecord(activeRecord); 
        menuMode = MODE_VIEW; 
        writeStateToEEPROM(); 
      }
      if (bDown.pressed) { 
        loadRecord(activeRecord); 
        menuMode = MODE_VIEW; 
      }
      break;
  }
}

void setup() {
  pinMode(PIN_UP, INPUT_PULLUP); 
  pinMode(PIN_DOWN, INPUT_PULLUP); 
  pinMode(PIN_ENTER, INPUT_PULLUP);
  
  Wire.begin();
  Wire.setClock(400000); 

  if (display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    display.println(F(COMPILATION_NAME));
    display.println(F(COMPILATION_VERSION));
    display.display();
    delay(1500);
  }

  Keyboard.begin();

  bool valid = true;
  for(uint8_t i = 0; i < 4; i++) {
    if(EEPROM.read(EE_MAGIC0 + i) != EE_MAGIC_VAL[i]) {
      valid = false;
    }
  }
  
  if (!valid) {
    kbLayout = KB_EN; 
    activeRecord = 0;
    writeStateToEEPROM();
    for(uint8_t i = 0; i < TOTAL_RECORDS; i++) {
      sprintf(dataset[i].header, "Record %d", i+1);
      sprintf(dataset[i].details, "Empty");
      saveRecord(i);
    }
  } else {
    kbLayout = EEPROM.read(EE_LAYOUT);
    activeRecord = EEPROM.read(EE_ACTIVE);
    if (activeRecord >= TOTAL_RECORDS) {
      activeRecord = 0;
    }
    loadRecord(activeRecord);
  }
  
  lastInteraction = millis();
}

void loop() {
  pollButton(bUp, PIN_UP);
  pollButton(bDown, PIN_DOWN);
  pollButton(bEnter, PIN_ENTER);

  handleInput();

  unsigned long elapsed = millis() - lastInteraction;
  if (!displaySleeping && elapsed > T_TIMEOUT) {
    if (menuMode != MODE_VIEW) {
      if (dataset[activeRecord].dirty && menuMode != MODE_UNSAVED) {
        menuMode = MODE_UNSAVED;
        lastInteraction = millis();
        refreshNeeded = true;
      } else {
        loadRecord(activeRecord);
        menuMode = MODE_VIEW;
        display.ssd1306_command(SSD1306_DISPLAYOFF);
        displaySleeping = true;
      }
    } else {
      display.ssd1306_command(SSD1306_DISPLAYOFF);
      displaySleeping = true;
    }
  }

  if (showSavedMsg && (millis() - savedMsgTime > T_SAVED_MSG)) {
    showSavedMsg = false;
    refreshNeeded = true;
  }

  updateDisplay();
}
