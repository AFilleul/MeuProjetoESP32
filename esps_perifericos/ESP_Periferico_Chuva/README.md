# 🌧️ Módulo de Sensor de Chuva (MH-RD) com Chaveamento por Transistor BJT NPN

Este módulo implementa a leitura otimizada do sensor de chuva **MH-RD** (ou similares com comparador LM393 e placa fenólica de trilhas de cobre expostas) utilizando um microcontrolador **ESP32** e comutação do polo negativo (GND) através de um **transistor bipolar de junção NPN (BJT)**.

---

## ⚡ 1. O Problema da Eletrólise e a Solução com Transistor BJT NPN

### O Problema:
Quando o sensor de chuva é mantido energizado de forma contínua:
1. Uma diferença de potencial (DC) permanente é mantida entre as trilhas de cobre.
2. Com a água da chuva ou sereno (que contêm minerais condutores), ocorre **eletrólise**: o cobre do ânodo corrói e se dissolve rapidamente, destruindo o sensor em poucos dias.
3. Há desperdício contínuo de energia (~5 mA a 15 mA 24h por dia).

### A Solução com BJT NPN (ex: BC547, BC548, 2N2222, 2N3904, BC337):
- O transistor atua como uma chave eletrônica no polo negativo (**Low-Side Switch**).
- **Em repouso (Corte):** A GPIO do ESP32 envia 0V para a Base ($I_B = 0 \text{ mA}$). O circuito do sensor fica aberto e isolado do terra ($I_C = 0 \text{ µA}$). Corrosão galvânica é nula.
- **Na leitura (Saturação):** A GPIO do ESP32 envia 3.3V para a Base através de um resistor limitador ($R_B = 1\text{ k}\Omega$). O transistor satura ($V_{CE(sat)} \approx 0{,}1\text{ V}$), aterrando o sensor por apenas **50 ms**.
- **Resultado:** O sensor opera com *duty cycle* inferior a **0,1%**, economizando mais de **99,8% de energia** e multiplicando a durabilidade da placa em centenas de vezes.

---

## 🔌 2. Esquema Elétrico: BJT NPN (Low-Side Switch)

```
                            ESP32 (3.3V)
                                 │
                                 ├─── [3V3] ───> Pino VCC do Módulo MH-RD (3.3V)
                                 │
     [GPIO 23] ──[ 1kΩ (Rb) ]────┼─────────────> Base (B) do Transistor BJT NPN
                                 │
                              [ 10kΩ ] (Pull-Down para garantir corte no boot / deep sleep)
                                 │
                                GND
                                 │
                              Emissor (E) do BJT NPN
                                 │
                                GND Comum do ESP32

     Módulo Sensor MH-RD (LM393):
          Pino GND ─────────────────────────────> Coletor (C) do Transistor BJT NPN
          Pino AO  ─────────────────────────────> GPIO 34 do ESP32 (ADC1_CH6 - Seguro)
          Pino DO  ─────────────────────────────> GPIO 35 do ESP32 (Entrada Digital)
```

---

## 📋 3. Tabela de Conexões e Especificação dos Componentes

| Componente | Terminal | Conexão no ESP32 / Circuito | Função e Detalhes |
| :--- | :--- | :--- | :--- |
| **Módulo MH-RD** | `VCC` | Pino `3V3` do ESP32 | **Use 3.3V!** Protege as entradas analógicas do ESP32 contra sobretensão. |
| **Módulo MH-RD** | `GND` | **Coletor (C)** do BJT NPN | O polo negativo é aterrado apenas quando o BJT satura. |
| **Módulo MH-RD** | `AO` | **GPIO 34** do ESP32 | Entrada analógica ADC1 canal 6 (sem interferência de Wi-Fi/ESP-NOW). |
| **Módulo MH-RD** | `DO` | **GPIO 35** do ESP32 | Saída digital do comparador LM393 (com ajuste por potenciômetro). |
| **BJT NPN (BC547/2N2222)** | `Base (B)` | **GPIO 23** via Resistor $R_B$ de 1kΩ | Corrente de base calculada: $I_B \approx \frac{3{,}3\text{V} - 0{,}7\text{V}}{1\text{k}\Omega} \approx 2{,}6\text{ mA}$. |
| **BJT NPN** | `Coletor (C)` | Pino `GND` do MH-RD | Chaveia a corrente do sensor. |
| **BJT NPN** | `Emissor (E)` | **GND** do ESP32 | Referência comum do circuito. |
| **Resistor Pull-Down** | 10kΩ | Entre Base (B) e GND | Garante corte total durante resets, bootloader ou Deep Sleep. |

---

## 📊 4. Interpretação dos Dados de Leitura (ADC 12 Bits: 0 a 4095)

* **Trilhas Secas (Resistência Alta):** Tensão no AO próxima de 3.3V ($\text{ADC} \approx 3200 \dots 4095$).
* **Garoa / Gotículas de Chuva:** Resistência decai ($\text{ADC} \approx 1800 \dots 3200$).
* **Chuva Moderada / Forte (Lâmina d'água):** Tensão cai próximo a zero ($\text{ADC} < 1800$).

| Faixa ADC (12 bits) | Tensão no AO | Intensidade (%) | Saída Digital DO | Diagnóstico |
| :--- | :--- | :--- | :--- | :--- |
| **3200 a 4095** | 2.58 V a 3.30 V | 0.0% | `HIGH` (Inativo) | ☀️ **Tempo Seco** |
| **1800 a 3199** | 1.45 V a 2.57 V | 1% a 55% | Conforme ajuste do trimpot | 🌦️ **Chuva Leve / Garoa** |
| **0 a 1799** | 0.00 V a 1.44 V | 56% a 100% | `LOW` (Ativado) | 🌧️ **Chuva Moderada a Forte** |

---

## 🚀 5. Integração com ESP-NOW e Deep Sleep

```cpp
#include "../ESP_Periferico_Chuva/SensorChuvaMHRD.h"

// Inicializa com controle BJT NPN na GPIO 23, AO na GPIO 34 e DO na GPIO 35
SensorChuvaMHRD sensorChuva(23, 34, 35);

void setup() {
    sensorChuva.begin(); // Garante o BJT NPN em corte imediatamente
    
    // Acorda do sono, realiza a leitura transitória e coleta os dados
    LeituraSensorChuva leitura = sensorChuva.realizarLeitura();
    
    // Preenche o payload e transmite via ESP-NOW
    pacote.sensorUmidade = leitura.porcentagemIntensidade;
    // ...
    // Entra novamente em Deep Sleep (a GPIO fica em alta impedância,
    // e o resistor pull-down de 10k mantém o BJT em corte com consumo zero)
}
```
