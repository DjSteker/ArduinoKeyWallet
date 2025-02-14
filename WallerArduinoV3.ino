

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>
#include <Keyboard.h>

// ... (resto de definiciones igual)

void processEnterButton(unsigned long pressDuration) {
  if (pressDuration < 3000) {  // Pulsación corta
    if (menuMode == 0) {
      // Enviar contraseña
      Keyboard.print(dataset[activeRecord].details);
    } else if (menuMode == 1) {
      // Avanzar cursor en modo edición
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
        }
      }
    } else if (menuMode == 2) {
      // Cancelar guardado
      menuMode = 0;
    }
  } else {  // Pulsación larga (3+ segundos)
    if (menuMode == 0) {
      // Entrar en modo edición
      menuMode = 1;
      cursorPosition = 0;
      isEditingTitle = true;
      displayMessage("Modo Edicion", 1000);
    } else if (menuMode == 1) {
      // Solicitar confirmación de guardado
      menuMode = 2;
      displayMessage("Guardar?", 1000);
    }
  }
}

void displayMessage(const char* message, int duration) {
  oled.clearDisplay();
  oled.setCursor(0, 0);
  oled.println(message);
  oled.display();
  delay(duration);
}

// Modificar processUpButton y processDownButton
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

void modifyCharacter(String &text, bool increment) {
  if (cursorPosition < text.length()) {
    char c = text[cursorPosition];
    if (increment) {
      c = (c < 126) ? c + 1 : 32;
    } else {
      c = (c > 32) ? c - 1 : 126;
    }
    text[cursorPosition] = c;
  } else if (cursorPosition < (isEditingTitle ? MAX_TITLE_LENGTH : MAX_CONTENT_LENGTH) - 1) {
    text += ' ';
  }
}
