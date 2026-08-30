#ifndef F_CPU
#if defined(__AVR_ATmega2560__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega328P__) || defined(__AVR_ATmega32U4__)
#define F_CPU 16000000UL  // 16 MHz (Standard Arduino Uno, Mega, Leonardo)
#elif defined(__AVR_ATmega328__) || defined(__AVR_ATmega8__) || defined(__AVR_ATtiny85__)
#define F_CPU 8000000UL  // 8 MHz (Versiones a 8MHz o cristales internos)
#elif defined(__AVR_ATmega168__)
#define F_CPU 16000000UL  // O 8000000UL según versión, por defecto 16MHz
#else
#define F_CPU 16000000UL  // Valor por defecto seguro para la mayoría
#endif
#endif

#include "TramasMicros3.h"
#include <avr/interrupt.h>
#include <avr/io.h>

// ============================================================================
// SystemTimer - Implementación (Timer1, CTC, prescaler configurable)
// ============================================================================

// Prescalers VÁLIDOS para Timer1 (16 bits): 1, 8, 64, 256, 1024
#ifndef TIMER_PRESCALER
#define TIMER_PRESCALER 1
#endif

#if TIMER_PRESCALER == 1
#define TIMER1_CS_BITS ((1 << CS10))
#elif TIMER_PRESCALER == 8
#define TIMER1_CS_BITS ((1 << CS11))
#elif TIMER_PRESCALER == 64
#define TIMER1_CS_BITS ((1 << CS11) | (1 << CS10))
#elif TIMER_PRESCALER == 256
#define TIMER1_CS_BITS ((1 << CS12))
#elif TIMER_PRESCALER == 1024
#define TIMER1_CS_BITS ((1 << CS12) | (1 << CS10))
#else
#error "TIMER_PRESCALER invalido para Timer1. Usa 1, 8, 64, 256 o 1024."
#endif

#define TICKS_PER_MICROSECOND (F_CPU / 1000000UL / TIMER_PRESCALER)

#if TICKS_PER_MICROSECOND == 0
#error "TIMER_PRESCALER demasiado alto para F_CPU: TICKS_PER_MICROSECOND=0 (division por cero en runtime)."
#endif

#define OCR1A_VALUE ((F_CPU / 1000UL / TIMER_PRESCALER) - 1)

#if ((F_CPU / 1000UL) % TIMER_PRESCALER) != 0
#warning "F_CPU/1000 no es multiplo exacto de TIMER_PRESCALER: el periodo de 1ms tendra error de redondeo."
#endif

static volatile uint32_t g_micros_counter = 0;

ISR(TIMER1_COMPA_vect) {
  g_micros_counter += 1000; // ISR cada 1 ms
}

void SystemTimer::init() {
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;

  TCCR1B = (1 << WGM12) | TIMER1_CS_BITS;
  OCR1A = OCR1A_VALUE;

  TIMSK1 = (1 << OCIE1A);
  sei();
}

uint32_t SystemTimer::getMicros() {
  uint32_t base;
  uint16_t tcnt;
  uint8_t oldSREG = SREG;

  cli();
  base = g_micros_counter;
  tcnt = TCNT1;

  if ((TIFR1 & (1 << OCF1A)) && (tcnt < (OCR1A / 2))) {
    base += 1000;
  }
  SREG = oldSREG;

  uint32_t micros_fraction = ((uint32_t)tcnt * TIMER_PRESCALER) / (F_CPU / 1000000UL);

  return base + micros_fraction;
}

uint32_t SystemTimer::getMillis() {
  return SystemTimer::getMicros() / 1000;
}

// ============================================================================
// TramasMicros3 - Implementación
// ============================================================================

TramasMicros3::TramasMicros3(unsigned long intervl, void (*function)(void))
  : active(true), previous(0), interval(intervl), execute(function) {
}

TramasMicros3::TramasMicros3(unsigned long prev, unsigned long intervl, void (*function)(void), bool enable)
  : active(enable), previous(prev), interval(intervl), execute(function) {
}

void TramasMicros3::reset() {
  previous = SystemTimer::getMicros();
}

void TramasMicros3::disable() {
  active = false;
}

void TramasMicros3::enable() {
  active = true;
}

void TramasMicros3::setInterval(unsigned long intervl) {
  interval = intervl;
}

void TramasMicros3::check() {
  if (!active) return;
  unsigned long now = SystemTimer::getMicros();
  if ((now - previous) >= interval) {
    previous = now;
    if (execute) execute();
  } else if (now < previous) {
    unsigned long elapsed = (0xFFFFFFFFUL - previous) + now + 1UL;
    if (elapsed >= interval) {
      previous = now;
      if (execute) execute();
    }
  }
}

bool TramasMicros3::isDue() {
  if (!active) return false;
  unsigned long now = SystemTimer::getMicros();
  if (now - previous >= interval) {
    previous = now;
    return true;
  }
  if (now < previous) {
    unsigned long elapsed = (0xFFFFFFFFUL - previous) + now + 1UL;
    if (elapsed >= interval) {
      previous = now;
      return true;
    }
  }
  return false;
}

// ============================================================================
// TaskScheduler - Implementación
// ============================================================================

TramasMicros3 *TaskScheduler::tasks[MAX_TIMED_TASKS] = { nullptr };
uint8_t TaskScheduler::taskCount = 0;

void TaskScheduler::registerTask(TramasMicros3 *task) {
  if (taskCount < MAX_TIMED_TASKS) {
    tasks[taskCount++] = task;
  }
}

bool TaskScheduler::removeTask(TramasMicros3 *task) {
  for (uint8_t i = 0; i < taskCount; i++) {
    if (tasks[i] == task) {
      tasks[i] = nullptr;
      return true;
    }
  }
  return false;
}

void TaskScheduler::checkAll() {
  for (uint8_t i = 0; i < taskCount; i++) {
    if (tasks[i]) {
      tasks[i]->check();
    }
  }
}
