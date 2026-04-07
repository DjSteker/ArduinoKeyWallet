/*
  TramasMicros2.h — Ejecutor de tareas periódicas por microsegundos
  Autor original: ivanmv (15/09/2017)
*/
#ifndef TRAMASMICROS2_H_
#define TRAMASMICROS2_H_

#if defined(ARDUINO) && ARDUINO >= 100
#include <Arduino.h>
#else
#include <WProgram.h>
#endif

class TramaTiempo {
public:
  TramaTiempo();
  TramaTiempo(unsigned long interval, void (*function)());
  TramaTiempo(unsigned long prev, unsigned long interval, void (*function)());

  void reset();
  void disable();
  void enable();
  void check();
  void fun(void (*function)());
  void setInterval(unsigned long interval);

private:
  boolean active;
  unsigned long previous;
  unsigned long interval;
  void (*execute)();
};

#endif
