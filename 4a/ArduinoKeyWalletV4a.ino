
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>
#include <Keyboard.h>

// Configuración de la pantalla OLED
#define OLED_WIDTH 128
#define OLED_HEIGHT 32
#define OLED_RESET    -1
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);

// Configuración de los botones
#define BTN_UP 10
#define BTN_DOWN 16
#define BTN_ENTER 14

// Constantes
#define TOTAL_RECORDS 4
#define MAX_TITLE_LENGTH 32
#define MAX_CONTENT_LENGTH 128
#define RECORD_SIZE (MAX_TITLE_LENGTH + MAX_CONTENT_LENGTH + 2) // +2 para caracteres nulos
#define BLOCK_SIZE (RECORD_SIZE + sizeof(unsigned long)) // Tamaño de bloque con timestamp
#define TOTAL_BLOCKS (EEPROM.length() / BLOCK_SIZE)

// Modos del menú
#define MODE_VIEW 0
#define MODE_EDIT 1
#define MODE_CONFIRM_SAVE 2
#define MODE_TIMEOUT_CONFIRM 3
#define MODE_SAVED_MESSAGE 4

// Estructura para almacenar los datos
struct Data {
  char header[MAX_TITLE_LENGTH + 1]; // +1 para el carácter nulo
  char details[MAX_CONTENT_LENGTH + 1]; // +1 para el carácter nulo
};

Data dataset[TOTAL_RECORDS];
Data tempData; // Para almacenar cambios temporales durante edición
int currentRecord = 0;
int menuMode = MODE_VIEW;
bool editingTitle = true;
int cursorPosition = 0;
unsigned long lastInteraction = 0;
unsigned long enterPressTime = 0;
bool isEnterPressed = false;
unsigned long msgDisplayStartTime = 0;
const unsigned long LONG_PRESS_DURATION = 3000;
const unsigned long TIMEOUT_DURATION = 30000;
const unsigned long MESSAGE_DURATION = 1500;

// Inicialización
void setup() {
  delay(500);
  // Inicialización de los botones
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_ENTER, INPUT_PULLUP);

  // Inicialización de la pantalla OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for(;;); // Detener si la pantalla no se inicializa
  }
  display.clearDisplay();
  display.display();
  
  // Inicialización del teclado
  Keyboard.begin();

  // Inicialización de la EEPROM
  if (!isEEPROMInitialized()) {
    initializeEEPROM();
  }
  loadFromEEPROM();
}

void loop() {
  handleButtonEvents();
  checkForTimeout();
  refreshDisplay();
  
  // Actualizar estado de mensajes temporales
  if (menuMode == MODE_SAVED_MESSAGE && millis() - msgDisplayStartTime > MESSAGE_DURATION) {
    menuMode = MODE_VIEW;
  }
}

void handleButtonEvents() {
  bool currentUpState = (digitalRead(BTN_UP) == LOW);
  bool currentDownState = (digitalRead(BTN_DOWN) == LOW);
  bool currentEnterState = (digitalRead(BTN_ENTER) == LOW);
  
  // No procesar botones si estamos mostrando un mensaje temporal
  if (menuMode == MODE_SAVED_MESSAGE) {
    return;
  }
  
  // Generar contraseña aleatoria si se pulsan UP y DOWN juntos
  if (currentUpState && currentDownState && menuMode == MODE_EDIT) {
    generateRandomPassword();
    delay(500); // Debounce
    return;
  }

  // Botón UP
  if (currentUpState) {
    lastInteraction = millis();
    
    switch (menuMode) {
      case MODE_VIEW:
        currentRecord = (currentRecord + 1) % TOTAL_RECORDS;
        break;
      case MODE_EDIT:
        if (editingTitle) {
          incrementChar(tempData.header[cursorPosition]);
        } else {
          incrementChar(tempData.details[cursorPosition]);
        }
        break;
      case MODE_CONFIRM_SAVE:
        saveChanges();
        showMessage("Cambios guardados!");
        break;
      case MODE_TIMEOUT_CONFIRM:
        // Continuar editando
        menuMode = MODE_EDIT;
        break;
    }
    delay(150); // Debounce
  }
  
  // Botón DOWN
  if (currentDownState) {
    lastInteraction = millis();
    
    switch (menuMode) {
      case MODE_VIEW:
        currentRecord = (currentRecord - 1 + TOTAL_RECORDS) % TOTAL_RECORDS;
        break;
      case MODE_EDIT:
        if (editingTitle) {
          decrementChar(tempData.header[cursorPosition]);
        } else {
          decrementChar(tempData.details[cursorPosition]);
        }
        break;
      case MODE_CONFIRM_SAVE:
        menuMode = MODE_EDIT;
        break;
      case MODE_TIMEOUT_CONFIRM:
        // Descartar cambios
        menuMode = MODE_VIEW;
        break;
    }
    delay(150); // Debounce
  }
  
  // Botón ENTER
  if (currentEnterState) {
    if (!isEnterPressed) {
      enterPressTime = millis();
      isEnterPressed = true;
    } else if (millis() - enterPressTime >= LONG_PRESS_DURATION) {
      // Pulsación larga detectada
      lastInteraction = millis();
      
      switch (menuMode) {
        case MODE_VIEW:
          // Enviar contraseña al teclado
          Keyboard.print(dataset[currentRecord].details);
          break;
        case MODE_EDIT:
          if (editingTitle) {
            editingTitle = false;
            cursorPosition = 0;
          } else {
            menuMode = MODE_CONFIRM_SAVE;
          }
          break;
      }
      isEnterPressed = false;
    }
  } else {
    // Botón liberado
    if (isEnterPressed) {
      lastInteraction = millis();
      
      // Solo procesar si fue una pulsación corta
      if (millis() - enterPressTime < LONG_PRESS_DURATION) {
        switch (menuMode) {
          case MODE_VIEW:
            // Entrar en modo edición
            menuMode = MODE_EDIT;
            editingTitle = true;
            cursorPosition = 0;
            // Copiar datos actuales a datos temporales
            strcpy(tempData.header, dataset[currentRecord].header);
            strcpy(tempData.details, dataset[currentRecord].details);
            break;
          case MODE_EDIT:
            // Mover cursor o cambiar entre título y contenido
            if (editingTitle) {
              cursorPosition++;
              if (cursorPosition >= strlen(tempData.header) || 
                  cursorPosition >= MAX_TITLE_LENGTH) {
                // Fin del título, pasar a editar contenido
                editingTitle = false;
                cursorPosition = 0;
              }
            } else {
              cursorPosition++;
              if (cursorPosition >= strlen(tempData.details) || 
                  cursorPosition >= MAX_CONTENT_LENGTH) {
                // Fin del contenido, ir a confirmación
                menuMode = MODE_CONFIRM_SAVE;
              }
            }
            break;
        }
      }
      isEnterPressed = false;
    }
  }
}

void incrementChar(char &c) {
  if (c < 32) c = 32; // Iniciar en espacio si es un carácter de control
  c = (c < 126) ? c + 1 : 32; // Ciclar entre espacio y '~'
  // Validar caracteres no válidos
  if (c == '\0' || c == '\n' || c == '\r') {
    c = 32; // Reemplazar con espacio
  }
}

void decrementChar(char &c) {
  if (c < 32) c = 32; // Iniciar en espacio si es un carácter de control
  c = (c > 32) ? c - 1 : 126; // Ciclar entre espacio y '~'
  // Validar caracteres no válidos
  if (c == '\0' || c == '\n' || c == '\r') {
    c = 126; // Reemplazar con '~'
  }
}

void generateRandomPassword() {
  const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*()";
  const int passwordLength = 12; // Longitud de la contraseña generada
  for (int i = 0; i < passwordLength; i++) {
    tempData.details[i] = charset[random(0, strlen(charset))];
  }
  tempData.details[passwordLength] = '\0'; // Terminar la cadena
  cursorPosition = 0; // Reiniciar la posición del cursor
  showMessage("Contraseña generada!");
}

void checkForTimeout() {
  if (menuMode == MODE_EDIT && millis() - lastInteraction > TIMEOUT_DURATION) {
    // En lugar de volver directamente a vista, mostrar confirmación
    menuMode = MODE_TIMEOUT_CONFIRM;
  }
}

void refreshDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  switch (menuMode) {
    case MODE_VIEW:
      displayViewMode();
      break;
    case MODE_EDIT:
      displayEditMode();
      break;
    case MODE_CONFIRM_SAVE:
      displayConfirmSave();
      break;
    case MODE_TIMEOUT_CONFIRM:
      displayTimeoutConfirm();
      break;
    case MODE_SAVED_MESSAGE:
      displaySavedMessage();
      break;
  }

  display.display();
}

void displayViewMode() {
  display.println("Registro: " + String(currentRecord + 1) + "/" + String(TOTAL_RECORDS));
  display.println(dataset[currentRecord].header);
  
  // Mostrar contenido con puntos si es largo
  char contentPreview[21]; // 20 caracteres + nulo
  strncpy(contentPreview, dataset[currentRecord].details, 20);
  contentPreview[20] = '\0';
  if (strlen(dataset[currentRecord].details) > 20) {
    strcat(contentPreview, "...");
  }
  display.println(contentPreview);
  
  display.println("^v:Nav | E:Edit | E-largo:Enviar");
}

void displayEditMode() {
  display.println(editingTitle ? "Editando Titulo:" : "Editando Contenido:");
  
  // Mostrar título y contenido
  display.println(tempData.header);
  display.println(tempData.details);
  
  // Mostrar cursor
  int cursorY = editingTitle ? 8 : 16; // Ajustar según tamaño del texto
  display.fillRect(cursorPosition * 6, cursorY, 6, 8, SSD1306_INVERSE);
  
  // Mostrar indicador de pulsación larga si corresponde
  if (isEnterPressed && (millis() - enterPressTime) < LONG_PRESS_DURATION) {
    int progressWidth = map(millis() - enterPressTime, 0, LONG_PRESS_DURATION, 0, OLED_WIDTH);
    display.fillRect(0, OLED_HEIGHT - 2, progressWidth, 2, SSD1306_WHITE);
  }
}

void displayConfirmSave() {
  display.println("Guardar cambios?");
  display.println("");
  display.println("^ SI    v NO");
}

void displayTimeoutConfirm() {
  display.println("Tiempo agotado!");
  display.println("Continuar editando?");
  display.println("");
  display.println("^ SI    v NO (descartar)");
}

void displaySavedMessage() {
  display.println(tempData.header);
  display.println("");
  display.println("Cambios guardados!");
}

bool isEEPROMInitialized() {
  // Usar una marca específica para determinar si la EEPROM está inicializada
  return EEPROM.read(0) == 'P' && EEPROM.read(1) == 'W' && EEPROM.read(2) == 'D';
}

void initializeEEPROM() {
  // Escribir marca de inicialización
  EEPROM.write(0, 'P');
  EEPROM.write(1, 'W');
  EEPROM.write(2, 'D');
  
  // Crear registros de ejemplo y guardarlos
  for (int i = 0; i < TOTAL_RECORDS; i++) {
    sprintf(dataset[i].header, "Cuenta %d", i + 1);
    sprintf(dataset[i].details, "Password%d", i + 1);
    dataset[i].header[MAX_TITLE_LENGTH] = '\0';
    dataset[i].details[MAX_CONTENT_LENGTH] = '\0';
    saveRecordToEEPROM(i);
  }
}

void loadFromEEPROM() {
  for (int i = 0; i < TOTAL_RECORDS; i++) {
    loadRecordFromEEPROM(i);
  }
}

void saveRecordToEEPROM(int index) {
  // Calcular dirección base: 3 bytes para la marca + índice * tamaño de registro
  int baseAddress = 3 + (index * BLOCK_SIZE);
  
  // Guardar título
  for (int i = 0; i < MAX_TITLE_LENGTH + 1; i++) {
    EEPROM.write(baseAddress + i, dataset[index].header[i]);
  }
  
  // Guardar contenido
  for (int i = 0; i < MAX_CONTENT_LENGTH + 1; i++) {
    EEPROM.write(baseAddress + MAX_TITLE_LENGTH + 1 + i, dataset[index].details[i]);
  }
  
  // Guardar timestamp
  EEPROM.put(baseAddress + MAX_TITLE_LENGTH + MAX_CONTENT_LENGTH + 2, millis());
}

void loadRecordFromEEPROM(int index) {
  // Calcular dirección base
  int baseAddress = 3 + (index * BLOCK_SIZE);
  
  // Cargar título
  for (int i = 0; i < MAX_TITLE_LENGTH + 1; i++) {
    dataset[index].header[i] = EEPROM.read(baseAddress + i);
  }
  
  // Asegurar terminación con nulo
  dataset[index].header[MAX_TITLE_LENGTH] = '\0';
  
  // Cargar contenido
  for (int i = 0; i < MAX_CONTENT_LENGTH + 1; i++) {
    dataset[index].details[i] = EEPROM.read(baseAddress + MAX_TITLE_LENGTH + 1 + i);
  }
  
  // Asegurar terminación con nulo
  dataset[index].details[MAX_CONTENT_LENGTH] = '\0';
}

void saveChanges() {
  // Copiar datos temporales a dataset
  strcpy(dataset[currentRecord].header, tempData.header);
  strcpy(dataset[currentRecord].details, tempData.details);
  
  // Guardar en EEPROM
  saveRecordToEEPROM(currentRecord);
}

void showMessage(const char* message) {
  menuMode = MODE_SAVED_MESSAGE;
  msgDisplayStartTime = millis();
}
