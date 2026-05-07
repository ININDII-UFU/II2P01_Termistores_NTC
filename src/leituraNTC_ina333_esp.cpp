// Topologia do circuito:
//
//   VCC(3.3V)                         VCC(3.3V)
//      |                                 |
//    R1(10k)                          R3(10k)
//      |                                 |
//   node_REF -----> VIN- (INA333)    node_VIN+ -----> VIN+ (INA333)
//      |                                 |
//    R2(10k)                           NTC(10k @ 25°C)
//      |                                 |
//     GND                               GND
//
//  node_REF --> VREF (INA333) = VCC/2 = 1,65V
//  VOUT (INA333) --> GPIO do ESP32 (ADC)
//
//  Equações:
//    VREF  = VCC / 2                       (divisor R1/R2)
//    VIN-  = VREF  = 1,65 V                (referência fixa)
//    VIN+  = VCC * NTC / (R3 + NTC)        (divisor R3/NTC)
//    VOUT  = G * (VIN+ - VIN-) + VREF
//    VIN+  = (VOUT - VREF) / G + VREF
//    NTC   = VIN+ * R3 / (VCC - VIN+)
//
//  Ganho INA333: G = 1 + 100000 / RG
//  Sem resistor de ganho (RG → ∞): G = 1

#include <Arduino.h>
#include <math.h>

// ──────────────────────────────────────────
//  Parâmetros do circuito
// ──────────────────────────────────────────
#define VCC_MV           3300.0f   // Tensão de alimentação (mV)
#define R_BRIDGE_OHM    10000.0f   // R1, R2 e R3 (Ω)
#define VREF_MV         (VCC_MV / 2.0f)  // 1650 mV

// Ganho do INA333: ajuste RG_OHM conforme o resistor instalado.
// Deixe como 0 (ou comente) para ganho unitário (sem RG).
#define RG_OHM               0.0f
#define INA333_GAIN  ((RG_OHM > 0.0f) ? (1.0f + 100000.0f / RG_OHM) : 1.0f)

// ──────────────────────────────────────────
//  Parâmetros do NTC 10k
// ──────────────────────────────────────────
#define NTC_NOMINAL_OHM  10000.0f  // Resistência nominal do NTC a 25 °C (Ω)
#define TEMP_NOMINAL_C      25.0f  // Temperatura de referência (°C)

// Coeficientes Beta
#define BETA_COEF         3455.0f

// Coeficientes Steinhart-Hart
#define SH_A   0.001129241f
#define SH_B   0.0002341077f
#define SH_C   0.00000008775468f

#define ADC_PIN              34    // GPIO34 — entrada analógica (input-only)

static float lerVoutMV()
{
  return (float)analogReadMilliVolts(ADC_PIN);
}

static float calcResistenciaNTC(float vout_mv)
{
  // VIN+ = (VOUT - VREF) / G + VREF
  float vin_plus_mv = (vout_mv - VREF_MV) / INA333_GAIN + VREF_MV;

  if (vin_plus_mv <= 0.0f || vin_plus_mv >= VCC_MV)
    return -1.0f;

  // NTC = VIN+ * R3 / (VCC - VIN+)
  return (vin_plus_mv * R_BRIDGE_OHM) / (VCC_MV - vin_plus_mv);
}

static float getTempBeta(float ntc_ohm)
{
  if (ntc_ohm <= 0.0f) return -999.0f;
  float kelvin = 1.0f / ((1.0f / (TEMP_NOMINAL_C + 273.15f))
                         + (1.0f / BETA_COEF) * logf(ntc_ohm / NTC_NOMINAL_OHM));
  return kelvin - 273.15f;
}

static float getTempSteinhart(float ntc_ohm)
{
  if (ntc_ohm <= 0.0f) return -999.0f;
  float r = logf(ntc_ohm);
  float kelvin = 1.0f / (SH_A + SH_B * r + SH_C * r * r * r);
  return kelvin - 273.15f;
}

void setup()
{
  Serial.begin(115200);
  analogReadResolution(12);                // 12 bits → 0–4095
  analogSetPinAttenuation(ADC_PIN, ADC_11db); // Faixa 0–3,3 V
  Serial.println("NTC via INA333 + ponte 3R — ESP32 iniciado");
  Serial.printf("Ganho INA333 configurado: %.2f\n", (float)INA333_GAIN);
}

#define INTERVALO_MS  1000
static uint32_t ultimoTempoMS = 0;

void loop()
{
  const uint32_t agora = millis();
  if ((agora - ultimoTempoMS) >= INTERVALO_MS)
  {
    ultimoTempoMS = agora;

    float vout_mv  = lerVoutMV();
    float ntc_ohm  = calcResistenciaNTC(vout_mv);
    float temp_beta = getTempBeta(ntc_ohm);
    float temp_sh   = getTempSteinhart(ntc_ohm);

    Serial.print(">VOUT(mV): ");
    Serial.println(vout_mv, 1);
    Serial.print(">NTC(ohm): ");
    Serial.println(ntc_ohm, 1);
    Serial.print(">Temp Beta(C): ");
    Serial.println(temp_beta, 2);
    Serial.print(">Temp Steinhart(C): ");
    Serial.println(temp_sh, 2);
  }
}
