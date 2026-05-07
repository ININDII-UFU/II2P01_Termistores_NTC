# Leitura de NTC 10k com INA333 + Ponte de 3 Resistores — ESP32

## Visão Geral

Este projeto mede a temperatura de um termistor NTC 10k usando um **amplificador de instrumentação INA333** como interface entre a ponte resistiva e o ADC interno do **ESP32**. O INA333 amplifica a diferença de tensão entre o ponto médio da ponte (que varia com o NTC) e uma referência fixa, melhorando a relação sinal/ruído em relação à leitura direta do ADC.

---

## Topologia do Circuito

```
         3,3 V
           |
         R1 (10k)              R3 (10k)
           |                      |
    node_REF ────┬──> VIN−      node_VIN+ ──> VIN+
           |     └──> VREF               |
         R2 (10k)      (INA333)        NTC (10k @ 25°C)
           |                              |
          GND                            GND

                          VOUT ──> GPIO34 (ESP32 ADC)
```

| Pino INA333 | Conexão                          | Valor típico     |
|-------------|----------------------------------|------------------|
| `VREF`      | Ponto médio R1–R2                | 1,65 V (VCC/2)   |
| `VIN−`      | Mesmo ponto médio R1–R2          | 1,65 V (fixo)    |
| `VIN+`      | Ponto médio R3–NTC               | varia com T      |
| `VOUT`      | GPIO34 do ESP32                  | 0 – 3,3 V        |
| `GND`       | GND do sistema                   | 0 V              |
| `VCC`       | 3,3 V                            | 3,3 V            |
| `RG`        | Resistor externo de ganho (opcional) | sem RG → G = 1 |

### Por que usar o INA333?

Sem o amplificador, o ADC do ESP32 leria apenas a tensão do divisor R3/NTC, que cobre apenas parte da faixa do conversor e possui baixa relação sinal/ruído. O INA333:

- Centraliza a saída em `VREF = 1,65 V` quando o NTC está na temperatura de referência
- Amplifica a variação diferencial com ganho configurável via `RG`
- Rejeita modo comum com alto CMRR (≥ 90 dB típico)
- Opera com alimentação simples de 3,3 V

---

## Equações do Circuito

### Tensões na ponte

$$V_{REF} = \frac{V_{CC}}{2} = 1{,}65\,\text{V}$$

$$V_{IN+} = V_{CC} \cdot \frac{R_{NTC}}{R_3 + R_{NTC}}$$

### Saída do INA333

$$V_{OUT} = G \cdot (V_{IN+} - V_{IN-}) + V_{REF}$$

onde $V_{IN-} = V_{REF}$.

### Inversão — recuperar $R_{NTC}$ a partir de $V_{OUT}$

$$V_{IN+} = \frac{V_{OUT} - V_{REF}}{G} + V_{REF}$$

$$R_{NTC} = \frac{V_{IN+} \cdot R_3}{V_{CC} - V_{IN+}}$$

### Ganho do INA333

$$G = 1 + \frac{100\,000}{R_G} \quad (\Omega)$$

Sem resistor de ganho ($R_G \to \infty$): $G = 1$.

**Exemplos de ganho:**

| $R_G$ (Ω) | Ganho G |
|-----------|---------|
| sem RG    | 1       |
| 10 000    | 11      |
| 1 000     | 101     |
| 100       | 1001    |

---

## Modelos de Temperatura

### Modelo Beta (B-parameter)

$$T\,[K] = \frac{1}{\dfrac{1}{T_0} + \dfrac{1}{B}\ln\!\left(\dfrac{R_{NTC}}{R_0}\right)}$$

| Parâmetro | Valor           |
|-----------|-----------------|
| $R_0$     | 10 000 Ω        |
| $T_0$     | 25 °C (298,15 K)|
| $B$       | 3455 K          |

### Equação de Steinhart-Hart

$$T\,[K] = \frac{1}{A + B\ln R + C(\ln R)^3}$$

| Coeficiente | Valor (código) |
|-------------|----------------|
| A           | 1,129241 × 10⁻³ |
| B           | 2,341077 × 10⁻⁴ |
| C           | 8,775468 × 10⁻⁸ |

> Os coeficientes acima são valores de referência padrão para NTC 10k genérico. Coeficientes calibrados para o termistor específico do laboratório estão em `assets/ntc_coeffs.json`.

---

## Estrutura de Arquivos

```
EININDII01_Termistores_NTC/
├── platformio.ini               # Ambientes de build (uno, esp32_ina333)
├── README_ina333.md             # Este arquivo
├── assets/
│   └── ntc_coeffs.json          # Coeficientes calibrados (Steinhart-Hart e Beta)
├── include/
│   └── ads1115_c.h              # Classe wrapper para ADS1115 (outro experimento)
├── src/
│   ├── leituraNTC_uno.cpp       # Versão Arduino UNO (ADC direto, sem INA333)
│   ├── leituraNTC_esp.cpp       # Versão ESP32 com iikit_lib
│   └── leituraNTC_ina333_esp.cpp  # ← Versão ESP32 + INA333 (este projeto)
├── python/
│   ├── ntc_calibrator.py        # Calibração dos coeficientes NTC
│   └── linear_system.py         # Resolução do sistema linear para Steinhart-Hart
└── simulIDE/
    ├── NTC_ApInst.sim1          # Simulação com amplificador de instrumentação
    └── NTC_DivRes.sim1          # Simulação com divisor resistivo simples
```

---

## Configuração PlatformIO

Arquivo `platformio.ini`, ambiente `[env:esp32_ina333]`:

```ini
[env:esp32_ina333]
platform = espressif32
board = az-delivery-devkit-v4
framework = arduino
build_src_filter = +<leituraNTC_ina333_esp.cpp>
upload_protocol = esptool
monitor_speed = 115200
```

---

## Conexões Físicas (ESP32 DevKit v4)

| Componente          | Pino ESP32 | Observação                              |
|---------------------|------------|-----------------------------------------|
| `VOUT` do INA333    | GPIO34     | Input-only, sem pull-up interno         |
| `VCC` do INA333     | 3V3        |                                         |
| `GND` do INA333     | GND        |                                         |
| `VREF` do INA333    | node\_REF  | Ponto médio R1–R2 (não conectar ao ESP) |

> **Atenção:** o GPIO34 é *input-only* no ESP32. Não conectar saídas digitais a ele.

---

## Como Compilar e Gravar

```bash
# Compilar
pio run -e esp32_ina333

# Compilar e gravar
pio run -e esp32_ina333 -t upload

# Abrir monitor serial
pio device monitor -e esp32_ina333
```

---

## Saída Serial Esperada

```
NTC via INA333 + ponte 3R — ESP32 iniciado
Ganho INA333 configurado: 1.00
>VOUT(mV): 1652.3
>NTC(ohm): 10138.5
>Temp Beta(C): 24.65
>Temp Steinhart(C): 24.71
```

O prefixo `>` é compatível com o **Serial Plotter** do PlatformIO/Arduino IDE para plotagem em tempo real.

---

## Ajuste de Ganho

Para melhorar a resolução em faixas menores de temperatura, instale um resistor `RG` entre os pinos **RG1** e **RG2** do INA333 e atualize a constante no código:

```cpp
// src/leituraNTC_ina333_esp.cpp
#define RG_OHM  1000.0f   // G ≈ 101 — faixa de saída ampliada ~100×
```

> Com $G > 1$, verifique se $V_{OUT}$ não satura nas temperaturas extremas esperadas. A faixa útil de $V_{OUT}$ é $0$ a $3{,}3\,\text{V}$.

---

## Referências

- [INA333 Datasheet — Texas Instruments](https://www.ti.com/lit/ds/symlink/ina333.pdf)
- [Steinhart-Hart Equation — Wikipedia](https://en.wikipedia.org/wiki/Steinhart%E2%80%93Hart_equation)
- [ESP32 ADC — Espressif Docs](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc.html)
- [PlatformIO — Espressif32 Platform](https://registry.platformio.org/platforms/platformio/espressif32)
