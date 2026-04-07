Esta versión **v3.6-alt** 

### 📝 Notas de la Revisión Técnica
1.  **Generación de Contraseñas:** Has implementado correctamente la detección simultánea con un margen de 300ms. Al marcar `longFired = true` en ambos botones, evitas que la contraseña se regenere infinitamente mientras se mantienen pulsados.
2.  **Seguridad de Datos:** El uso de CRC8 asegura que si la memoria EEPROM se corrompe, el usuario verá un mensaje de error en lugar de basura digital.
3.  **Memoria:** Se ha eliminado el selector de Layout (idioma), lo cual simplifica el código y ahorra espacio, aunque ahora depende del idioma configurado en el sistema operativo del PC donde se conecte (asume teclado estándar).
4.  **Eficiencia:** El bus I2C a 400kHz y el refresco por bandera (`refreshNeeded`) garantizan una respuesta inmediata de los botones.

---

# 📖 Manual de Usuario: KeyVault v3.6

KeyVault es un gestor de contraseñas físico basado en hardware que actúa como un teclado USB. Permite almacenar hasta **4 registros** con títulos de 32 caracteres y contenidos (contraseñas/frases) de hasta 128 caracteres.

## 🕹️ Controles Básicos
* **UP (Arriba):** Navegar / Incrementar carácter.
* **DOWN (Abajo):** Navegar / Decrementar carácter.
* **ENTER:** Entrar en modo edición / Siguiente campo o carácter.
* **MANTENER ENTER (2 seg):** Escribir la contraseña en el PC (en modo Vista) o saltar al siguiente campo (en modo Edición).

---

## 🚀 Modos de Operación

### 1. Modo Vista (Pantalla Principal)
Es el modo por defecto al encender el dispositivo.
* **Cambiar de Registro:** Pulsa **UP** o **DOWN**.
* **Enviar Contraseña al PC:** Mantén pulsado **ENTER** durante 2 segundos. El KeyVault "tecleará" automáticamente el contenido del registro activo donde esté el cursor en tu ordenador.
* **Editar Registro:** Pulsa **ENTER** brevemente para entrar al Modo Edición.

### 2. Modo Edición
Permite modificar el Título (TIT) y el Contenido (CON).
* **Modificar carácter:** Pulsa **UP** o **DOWN**. Si los mantienes pulsados, la velocidad aumentará.
* **Siguiente carácter:** Pulsa **ENTER** brevemente.
* **Cambiar de Campo:** Mantén pulsado **ENTER** para pasar rápidamente de Título a Contenido.
* **Generar Contraseña Aleatoria:** 1. Sitúate en el campo de **Contenido (CON)**.
    2. Pulsa **UP y DOWN simultáneamente** durante un instante. 
    3. Se generará automáticamente una contraseña segura de 128 caracteres combinando letras (mayúsculas/minúsculas), números y símbolos.
* **Finalizar:** Al llegar al final del campo de Contenido y pulsar ENTER, pasarás al Modo Confirmación.

### 3. Modo Confirmación e Inactividad
* **Guardar:** Pulsa **UP** para confirmar los cambios. Se guardarán de forma permanente en la memoria EEPROM.
* **Cancelar:** Pulsa **DOWN** para volver a la edición.
* **Aviso "Sin Guardar":** Si el dispositivo entra en reposo por inactividad (30 seg) mientras editabas, al despertar te preguntará si deseas guardar los cambios pendientes o descartarlos.

---

## 💡 Consejos de Seguridad y Uso
* **Reposo:** La pantalla se apagará tras 30 segundos de inactividad para proteger el panel OLED. Pulsa cualquier botón para despertarlo.
* **Símbolo Asterisco (*):** Si aparece un asterisco junto al número de registro, significa que has realizado cambios que aún no han sido guardados.
* **Compatibilidad:** KeyVault funciona como un teclado USB estándar (HID). No necesita drivers ni software adicional en Windows, Mac o Linux.

---
**Nota técnica:** Esta versión utiliza una semilla aleatoria basada en ruido analógico (`analogRead(0)`), lo que garantiza que las contraseñas generadas sean distintas cada vez que se reinicia el dispositivo.
