#ifndef TRAMAS_MICROS3_H
#define TRAMAS_MICROS3_H

#include <stdint.h>

// Número máximo de tareas simultáneas (puede definirse antes de incluir la librería)
#ifndef MAX_TIMED_TASKS
#define MAX_TIMED_TASKS 10
#endif

class SystemTimer {
public:
  static void init();
  static uint32_t getMillis();
  static unsigned long getMicros();
};

class TramasMicros3 {
public:
  bool active;
  unsigned long previous;
  unsigned long interval;
  void (*execute)(void);

  TramasMicros3(unsigned long intervl, void (*function)(void));
  TramasMicros3(unsigned long prev, unsigned long intervl, void (*function)(void), bool enable);

  void reset();
  void disable();
  void enable();
  void check();
  void setInterval(unsigned long intervl);
  bool isDue();

  // Compatibilidad: asignar la función a ejecutar (equivalente a fun() usado en el proyecto)
  void fun(void (*function)(void)) { execute = function; }
};

// Mantener compatibilidad con el nombre antiguo usado en los sketches
using TramaTiempo = TramasMicros3;

class TaskScheduler {
private:
  static TramasMicros3 *tasks[MAX_TIMED_TASKS];
  static uint8_t taskCount;
public:
  static void registerTask(TramasMicros3 *task);
  static bool removeTask(TramasMicros3 *task);  // ¡NUEVO!
  static void checkAll();
};

#endif /* TRAMAS_MICROS3_H */
