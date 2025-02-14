# 
##  ArduinoKeyWallet
Gestor de contraseñas con Arduino y OLED  SSD1306 128x32  

Materiales Necesarios 

Arduino Uno (o similar) 
Pantalla OLED SSD1306 128x32 ,
Tres botones ,
EEPROM integrada en el Arduino ,

# 
# Conexiones
Pantalla OLED SSD1306:
en la placa Arduino Leonardo ETH de VCC a 5V
GND a GND
SCL a A5
SDA a A4
# 
# Botones:  (cortocircuito a masa al presionar)
Botón Arriba: un pin digital (D2)
Botón Abajo: un pin digital (D3)
Botón Enter: un pin digital (D4)

#
# Manejo de botones Enter, Up y Down:
Modo de visualización: Navegar entre registros.
Modo de edición: Modificar caracteres del título o contenido.
Confirmación de guardado: Guardar cambios o cancelar.
Función para incrementar o decrementar caracteres en el título o contenido según la posición del cursor.
Pulsación corta: Enviar contraseña o avanzar cursor en modo edición.
Pulsación larga (3+ segundos): Entrar en modo edición o solicitar confirmación de guardado.

# 
# Librerías

[Adafruit-GFX](https://github.com/adafruit/Adafruit-GFX-Library)
[Adafruit_SSD1306](https://github.com/adafruit/Adafruit_SSD1306)

# 

