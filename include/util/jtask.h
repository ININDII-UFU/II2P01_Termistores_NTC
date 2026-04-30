#ifndef UTIL_JTASK_H
#define UTIL_JTASK_H

#include <Arduino.h>
#include "util/jqueue.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <freertos/FreeRTOS.h>
#elif defined(ARDUINO_ARCH_AVR)
#include <avr/interrupt.h>
#include <avr/io.h>
#endif

#ifndef NUMTASKS
#define NUMTASKS 2
#endif

typedef void (*jtask_callback_t)();

struct jtask_counter_config_t {
  uint16_t counter;
  uint16_t limit;
  jtask_callback_t task;
};

inline volatile uint8_t &jtaskIndexRef()
{
  static volatile uint8_t index = 0;
  return index;
}

inline jtask_counter_config_t (&jtaskStructRef())[NUMTASKS]
{
  static jtask_counter_config_t tasks[NUMTASKS] = {};
  return tasks;
}

inline jQueue_t &jtaskQueueRef()
{
  static jQueue_t queue;
  return queue;
}

#if defined(ARDUINO_ARCH_ESP32)
inline hw_timer_t *&jtaskTimerRef()
{
  static hw_timer_t *timerHandle = nullptr;
  return timerHandle;
}

inline portMUX_TYPE &jtaskMuxRef()
{
  static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
  return mux;
}
#endif

inline void jtaskTick()
{
  volatile uint8_t &jtaskIndex = jtaskIndexRef();
  jtask_counter_config_t (&jtaskStruct)[NUMTASKS] = jtaskStructRef();

#if defined(ARDUINO_ARCH_ESP32)
  portENTER_CRITICAL_ISR(&jtaskMuxRef());
#endif

  const uint8_t taskCount = jtaskIndex;
  for (uint8_t i = 0; i < taskCount; ++i) {
    if (++jtaskStruct[i].counter >= jtaskStruct[i].limit) {
      if (jQueueSendFromISR(&jtaskQueueRef(), jtaskStruct[i].task)) {
        jtaskStruct[i].counter = 0;
      } else {
        jtaskStruct[i].counter = jtaskStruct[i].limit - 1;
      }
    }
  }

#if defined(ARDUINO_ARCH_ESP32)
  portEXIT_CRITICAL_ISR(&jtaskMuxRef());
#endif
}

#if defined(ARDUINO_ARCH_ESP32)
inline void ARDUINO_ISR_ATTR jtaskTimerCallback()
{
  jtaskTick();
}
#elif defined(ARDUINO_ARCH_AVR)
ISR(TIMER2_COMPA_vect)
{
  jtaskTick();
}
#endif

#if defined(ARDUINO_ARCH_AVR)
inline bool jtaskSetupAvrTimer2(uint32_t frequency)
{
  struct PrescalerOption {
    uint16_t divisor;
    uint8_t controlBits;
  };

  static const PrescalerOption kPrescalers[] = {
      {1, 0b00000001},
      {8, 0b00000010},
      {32, 0b00000011},
      {64, 0b00000100},
      {128, 0b00000101},
      {256, 0b00000110},
      {1024, 0b00000111},
  };

  if (frequency == 0) {
    return false;
  }

  for (const PrescalerOption &option : kPrescalers) {
    const uint32_t denominator = static_cast<uint32_t>(option.divisor) * frequency;
    if (denominator == 0) {
      continue;
    }

    const uint32_t counts = (F_CPU + (denominator / 2UL)) / denominator;
    if (counts == 0 || counts > 256UL) {
      continue;
    }

    const uint8_t compareValue = static_cast<uint8_t>(counts - 1UL);
    const uint8_t sreg = SREG;
    cli();

    TCCR2A = _BV(WGM21);
    TCCR2B = option.controlBits;
    TCNT2 = 0;
    OCR2A = compareValue;
    TIFR2 = _BV(OCF2A);
    TIMSK2 = _BV(OCIE2A);

    SREG = sreg;
    return true;
  }

  return false;
}
#endif

inline bool jtaskSetup(uint32_t frequency)
{
  if (frequency == 0) {
    return false;
  }

  if (!jQueueCreate(&jtaskQueueRef())) {
    return false;
  }

  volatile uint8_t &jtaskIndex = jtaskIndexRef();
  jtask_counter_config_t (&jtaskStruct)[NUMTASKS] = jtaskStructRef();
  for (uint8_t i = 0; i < jtaskIndex; ++i) {
    jtaskStruct[i].counter = 0;
  }

#if defined(ARDUINO_ARCH_ESP32)
  hw_timer_t *&timerHandle = jtaskTimerRef();
  if (timerHandle != nullptr) {
    timerEnd(timerHandle);
    timerHandle = nullptr;
  }

  timerHandle = timerBegin(frequency);
  if (timerHandle == nullptr) {
    return false;
  }

  timerAttachInterrupt(timerHandle, &jtaskTimerCallback);
  timerAlarm(timerHandle, 1, true, 0);
  return true;
#elif defined(ARDUINO_ARCH_AVR)
  return jtaskSetupAvrTimer2(frequency);
#else
  return false;
#endif
}

inline bool jtaskAttachFunc(jtask_callback_t task, uint16_t limit)
{
  if (task == nullptr || limit == 0) {
    return false;
  }

  volatile uint8_t &jtaskIndex = jtaskIndexRef();
  jtask_counter_config_t (&jtaskStruct)[NUMTASKS] = jtaskStructRef();

#if defined(ARDUINO_ARCH_ESP32)
  portENTER_CRITICAL(&jtaskMuxRef());
#elif defined(ARDUINO_ARCH_AVR)
  const uint8_t sreg = SREG;
  cli();
#else
  noInterrupts();
#endif

  bool result = false;
  if (jtaskIndex < NUMTASKS) {
    jtaskStruct[jtaskIndex] = {0, limit, task};
    ++jtaskIndex;
    result = true;
  }

#if defined(ARDUINO_ARCH_ESP32)
  portEXIT_CRITICAL(&jtaskMuxRef());
#elif defined(ARDUINO_ARCH_AVR)
  SREG = sreg;
#else
  interrupts();
#endif

  return result;
}

inline void jtaskLoop()
{
  jtask_callback_t taskMessage = nullptr;
  while (jQueueReceive(&jtaskQueueRef(), &taskMessage)) {
    if (taskMessage != nullptr) {
      taskMessage();
    }
  }
}

#endif
