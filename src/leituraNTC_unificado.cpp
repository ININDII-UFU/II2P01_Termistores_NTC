// Arquivo unificado para leitura de NTC em ESP32 e Arduino Uno
// Usa jtask para agendamento simples e compara métodos Beta e Steinhart
// Circuito: Vcc---NTC---ADC---SERIES_RESISTOR---GND

#include <Arduino.h>
#include "util/jtask.h"
#include "wserial_c.h"
#include "display_c.h"

#if defined(ARDUINO_ARCH_ESP32)
  #define ADC_RESOLUTION 4095
  #define NTC_PIN 34 // ou defina conforme seu hardware
#elif defined(ARDUINO_ARCH_AVR)
  #define ADC_RESOLUTION 1023
  #define NTC_PIN A5
#else
  #error "Plataforma não suportada"
#endif

#define TEMPERATURENOMINAL 25

WSerial_c WSerial;
Display_c disp;

// ---------- FILTROS ----------
static int readStableADC(int pin) {
  const int N = 9;
  const int DISCARD = 2;
  int v[N];
  for (int i = 0; i < N; ++i) v[i] = analogRead(pin);
  for (int i = 0; i < N; ++i)
    for (int j = i + 1; j < N; ++j)
      if (v[j] < v[i]) { int t = v[i]; v[i] = v[j]; v[j] = t; }
  long sum = 0;
  for (int i = DISCARD; i < N - DISCARD; ++i) sum += v[i];
  return (int)(sum / (N - 2*DISCARD));
}

static float iir(float y_prev, float x, float alpha) {
  return y_prev + alpha * (x - y_prev);
}
// ----------------------------

double getTempTermistorNTCBeta(const uint16_t analogValue,
                               const uint16_t serialResistance,
                               const uint16_t bCoefficient,
                               const uint16_t nominalResistance)
{
  float resistance, temp;
  resistance = (serialResistance / ((float)analogValue)) * ADC_RESOLUTION - serialResistance;
  temp = 1.0 / ((1.0 / (TEMPERATURENOMINAL + 273.15)) +
                (1.0 / bCoefficient) * log(resistance / nominalResistance));
  return (temp - 273.15);
}

double getTempTermistorNTCSteinhart(const uint16_t analogValue,
                                    const uint16_t serialResistance,
                                    const float a, const float b, const float c)
{
  float resistance, temp;
  resistance = (serialResistance / ((float)analogValue)) * ADC_RESOLUTION - serialResistance;
  resistance = log(resistance);
  temp = 1.0 / (a + b * resistance + c * resistance * resistance * resistance);
  return (temp - 273.15);
}

// Estados do filtro IIR
float tBetaFilt      = NAN;
float tSteinhartFilt = NAN;
const float IIR_ALPHA = 0.2f;

void tarefaNTC() {
  uint16_t adc = (uint16_t)readStableADC(NTC_PIN);
  float temperature1 = getTempTermistorNTCBeta(adc, 10000, 3455, 10000);
  float temperature2 = getTempTermistorNTCSteinhart(adc, 10000, 0.001129241f, 0.0002341077f, 0.00000008775468f);
  if (isnan(tBetaFilt))      tBetaFilt      = temperature1; else tBetaFilt      = iir(tBetaFilt,      temperature1, IIR_ALPHA);
  if (isnan(tSteinhartFilt)) tSteinhartFilt = temperature2; else tSteinhartFilt = iir(tSteinhartFilt, temperature2, IIR_ALPHA);
  WSerial.plot("Temp Beta", millis(), tBetaFilt);
  WSerial.plot("Temp Steinhart", millis(), tSteinhartFilt);
  disp.setText(2, (String("TB:") + String(tBetaFilt, 2)).c_str());
  disp.setText(3, (String("TS:") + String(tSteinhartFilt, 2)).c_str());
}

void setup() {
  WSerial.start(0, 115200);
  disp.start();
#if defined(ARDUINO_ARCH_ESP32)
  analogReadResolution(12);
#endif
  jtaskSetup(1); // 1 Hz
  jtaskAttachFunc(tarefaNTC, 1); // Executa a cada tick
}

void loop() {
  jtaskLoop();
}
