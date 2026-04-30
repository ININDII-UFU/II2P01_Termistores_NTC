#ifndef UTIL_JQUEUE_H
#define UTIL_JQUEUE_H

#include <Arduino.h>

#if defined(ARDUINO_ARCH_ESP32)
#include <freertos/FreeRTOS.h>
#elif defined(ARDUINO_ARCH_AVR)
#include <avr/interrupt.h>
#endif

#ifndef MAXLENGTHJQUEUE
#define MAXLENGTHJQUEUE 5
#endif

typedef struct {
    void (*buffer[MAXLENGTHJQUEUE])();
    uint8_t head;
    uint8_t tail;
    uint8_t count;
#if defined(ARDUINO_ARCH_ESP32)
    portMUX_TYPE mux;
#endif
} jQueue_t;

typedef jQueue_t* jQueueHandle_t;

static inline bool jQueueCreate(jQueueHandle_t queue) {
    if (queue == nullptr) return false;
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
#if defined(ARDUINO_ARCH_ESP32)
    queue->mux = portMUX_INITIALIZER_UNLOCKED;
#endif
    return true;
}

static inline bool jQueueSendFromISR(jQueueHandle_t xQueue, void (*pvItemToQueue)()) {
    if (xQueue == nullptr || pvItemToQueue == nullptr) return false;

#if defined(ARDUINO_ARCH_ESP32)
    portENTER_CRITICAL_ISR(&xQueue->mux);
#elif defined(ARDUINO_ARCH_AVR)
    uint8_t sreg = SREG;
    cli();
#else
    noInterrupts();
#endif

    bool result = false;
    if (xQueue->count < MAXLENGTHJQUEUE) {
        xQueue->buffer[xQueue->tail] = pvItemToQueue;
        xQueue->tail = (xQueue->tail + 1) % MAXLENGTHJQUEUE;
        xQueue->count++;
        result = true;
    }

#if defined(ARDUINO_ARCH_ESP32)
    portEXIT_CRITICAL_ISR(&xQueue->mux);
#elif defined(ARDUINO_ARCH_AVR)
    SREG = sreg;
#else
    interrupts();
#endif

    return result;
}

static inline bool jQueueReceive(jQueueHandle_t xQueue, void (**pvBuffer)()) {
    if (xQueue == nullptr || pvBuffer == nullptr) return false;

#if defined(ARDUINO_ARCH_ESP32)
    portENTER_CRITICAL(&xQueue->mux);
#elif defined(ARDUINO_ARCH_AVR)
    uint8_t sreg = SREG;
    cli();
#else
    noInterrupts();
#endif

    bool result = false;
    if (xQueue->count > 0) {
        *pvBuffer = xQueue->buffer[xQueue->head];
        xQueue->head = (xQueue->head + 1) % MAXLENGTHJQUEUE;
        xQueue->count--;
        result = true;
    }

#if defined(ARDUINO_ARCH_ESP32)
    portEXIT_CRITICAL(&xQueue->mux);
#elif defined(ARDUINO_ARCH_AVR)
    SREG = sreg;
#else
    interrupts();
#endif

    return result;
}

#endif
