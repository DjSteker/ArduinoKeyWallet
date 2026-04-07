/*
  TramasMicros2.cpp — Ejecutor de tareas periódicas por microsegundos
  Autor original: ivanmv (15/09/2017)
*/
#include "TramasMicros2.h"

TramaTiempo::TramaTiempo() {
  active = true;
  previous = 0;
  interval = 1;
}
TramaTiempo::TramaTiempo(unsigned long intervl, void (*function)()) {
  active = true;
  previous = 0;
  interval = intervl;
  execute = function;
}
TramaTiempo::TramaTiempo(unsigned long prev, unsigned long intervl, void (*function)()) {
  active = true;
  previous = prev;
  interval = intervl;
  execute = function;
}
void TramaTiempo::fun(void (*function)()) {
  execute = function;
}
void TramaTiempo::reset() {
  previous = micros();
}
void TramaTiempo::disable() {
  active = false;
}
void TramaTiempo::enable() {
  active = true;
}
void TramaTiempo::setInterval(unsigned long intervl) {
  interval = intervl;
}

void TramaTiempo::check() {
  if (active && (micros() - previous >= interval)) {
    previous = micros();
    execute();
  } else if (active && micros() < previous) {
    unsigned long TMr = (4294967295UL - previous);
    if (TMr < interval) {
      previous = interval - TMr;
    } else {
      previous = micros();
      execute();
    }
  }
}
