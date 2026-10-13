# 🌧️ Módulo de Sensor de Chuva (MH-RD) com Proteção contra Oxidação e Economia de Energia

Este módulo implementa a leitura otimizada do sensor de chuva **MH-RD** (ou similares baseados no comparador LM393 e placa fenólica com trilhas entrelaçadas de cobre) em microcontroladores **ESP32**.

---

## ⚡ 1. O Problema da Eletrólise e Oxidação das Trilhas

Nos sensores de chuva convencionais mantidos energizados de forma contínua:
1. Uma tensão contínua (DC) permanente é aplicada entre as trilhas de cobre expostas ao ar livre.
2. Na presença de água da chuva ou orvalho (que contêm minerais e atuam como eletrólito condutor), ocorre **eletrólise**:
   - O cobre do eletrodo positivo (ânodo) sofre oxidação forçada e se dissolve rapidamente no meio aquoso.
   - Ocorre deposição de óxido esverdeado/escuro, rompendo as trilhas condutivas e destruindo o sensor em poucos dias ou semanas.
3. Além disso, o sensor dissipa corrente desnecessária 24 horas por dia (cerca de 5 a 15 mA contínuos), inviabilizando a operação por bateria.

### 🛡️ A Solução: Chaveamento Rápido com MOSFET Canal N (Low-Side Switch)
Ao utilizar um transistor MOSFET Canal N (como o **2N7000**, **BS170**, **IRLML2502** ou **AO3400**) para comutar o polo negativo (GND) do sensor:
- O sensor permanece **totalmente desligado e isolado da terra** por padrão (corrente de 0 µA).
- O ESP32 fecha o circuito ativando o Gate do MOSFET por apenas **50 ms** a cada ciclo de amostragem.
- **Resultado:** O sensor opera com um ciclo de trabalho (*duty cycle*) de menos de **0,1%**. A oxidação por eletrólise é praticamente eliminada, aumentando a vida útil das trilhas em **mais de 200 a 1000 vezes** e reduzindo drasticamente o consumo energético.

---

## 🔌 2. Esquema de Ligação Elétrica

```
                        ESP32 (3.3V)
                             │
                             ├─── [VCC] ────> Pino VCC do Módulo MH-RD (3.3V)
                             │
 [GPIO 23] ──[ 330Ω ]──┬─────┴──────────────> Gate (G) do MOSFET Canal N
                       │
                    [ 10kΩ ] (Pull-Down para manter desligado no boot/sleep)
                       │
                      GND
                       │
                      Source (S) do MOSFET
                       │
                      GND Comum do ESP32

 Módulo MH-RD:
      Pino GND ─────────────────────────────> Drain (D) do MOSFET Canal N
      Pino AO  ─────────────────────────────> GPIO 34 do ESP32 (ADC1_CH6)
      Pino DO  ─────────────────────────────> GPIO 35 do ESP32 (Entrada Digital)
```

### 📋 Tabela de Conexões de Pinos

| Componente | Terminal | Conexão no ESP32 / Circuito | Observação Importante |
| :--- | :--- | :--- | :--- |
| **Módulo MH-RD** | `VCC` | Pino `3V3` do ESP32 | **NUNCA conecte em 5V!** O ESP32 tolera no máximo 3.3V no ADC. |
| **Módulo MH-RD** | `GND` | **Drain (D)** do MOSFET | O negativo do sensor passa pelo chaveamento do transistor. |
| **Módulo MH-RD** | `AO` | **GPIO 34** do ESP32 | ADC1 canal 6 (somente entrada, livre de ruídos de rádio Wi-Fi). |
| **Módulo MH-RD** | `DO` | **GPIO 35** do ESP32 | Pino digital do comparador LM393 com ajuste por trimpot. |
| **MOSFET Canal N** | `Gate (G)` | **GPIO 23** (via resistor 330Ω) | Resistor pull-down de 10kΩ do Gate para o GND do ESP32. |
| **MOSFET Canal N** | `Drain (D)` | Pino `GND` do MH-RD | Chaveia o polo negativo do circuito. |
| **MOSFET Canal N** | `Source (S)` | **GND** do ESP32 | Referência de terra comum. |

---

## 📊 3. Interpretação dos Dados de Leitura (ADC 12 Bits: 0 a 4095)

No módulo sensor de chuva com comparador LM393:
- **Trilhas Secas (Resistência Infinita):** A tensão na saída AO fica próxima de 3.3V (`ADC ≈ 3200 a 4095`).
- **Garoa / Gotículas de Chuva:** A água fecha o circuito entre as trilhas reduzindo a resistência (`ADC ≈ 1800 a 3200`).
- **Chuva Moderada / Forte (Lâmina d'água):** A condução atinge o pico e a tensão cai para perto de 0V (`ADC < 1800`).

| Faixa ADC (12 bits) | Tensão no AO (V) | Intensidade (%) | Saída Digital (DO) | Classificação |
| :--- | :--- | :--- | :--- | :--- |
| **3200 a 4095** | 2.58 V a 3.30 V | 0% | HIGH (Desativado) | ☀️ **Tempo Seco** |
| **1800 a 3199** | 1.45 V a 2.57 V | 1% a 55% | Varia c/ trimpot | 🌦️ **Chuva Leve / Garoa** |
| **0 a 1799** | 0.00 V a 1.44 V | 56% a 100% | LOW (Ativado) | 🌧️ **Chuva Moderada a Forte** |

---

## 🚀 4. Como Integrar no Nó Periférico com ESP-NOW e Deep Sleep

Para utilizar este sensor no sketch [`ESP_Periferico_Base.ino`](../ESP_Periferico_Base/ESP_Periferico_Base.ino):

1. Inclua o cabeçalho:
   ```cpp
   #include "../ESP_Periferico_Chuva/SensorChuvaMHRD.h"
   ```
2. Crie a instância global:
   ```cpp
   SensorChuvaMHRD sensorChuva(23, 34, 35);
   ```
3. No `setup()` logo antes do envio ESP-NOW:
   ```cpp
   sensorChuva.begin();
   LeituraSensorChuva dadosChuva = sensorChuva.realizarLeitura();
   
   // Preenche o pacote de telemetria ESP-NOW
   pacote.sensorUmidade = dadosChuva.porcentagemIntensidade;
   ```
4. Ao entrar em Deep Sleep com `esp_deep_sleep_start()`, o pino GPIO 23 é desenergizado e o MOSFET permanece em corte, garantindo **consumo zero** durante o sono profundo.
