# ESPs Periféricos (Nós Sensores / Clientes)

Esta pasta contém o firmware modular dos **microcontroladores ESP32 secundários (nós periféricos)** que se comunicam via **ESP-NOW** diretamente com o **ESP32 Central**.

---

## ⚡ Modos de Operação dos Periféricos

Os nós periféricos **não funcionam apenas em modo Sleep**. Eles são flexíveis e suportam três modos operacionais, configuráveis no código ou dinamicamente pela Interface Web:

| Modo de Operação | Fonte de Energia Recomendada | Comportamento do Periférico | Vantagens |
| :--- | :--- | :--- | :--- |
| **`ONLINE` (Contínuo)** | **Fonte DC / Tomada (5V)** | O ESP32 permanece **100% acordado**, com CPU e rádio Wi-Fi/ESP-NOW ativos continuamente no `loop()`. | Telemetria contínua/rápida, monitoramento em tempo real e aplicação **instantânea** de configurações enviadas pela Web (sem esperar acordar). |
| **`DEEP_SLEEP`** | **Bateria Li-Ion / 18650** | Acorda periodicamente pelo timer RTC, lê os sensores, envia via ESP-NOW Unicast e **desliga CPU e rádio**. | Máxima economia de energia, permitindo meses ou anos de autonomia em locais remotos. |
| **`LIGHT_SLEEP`** | **Bateria ou Painel Solar** | Suspende o processador mantendo a memória RAM e periféricos internos em espera de baixo consumo. | Retomada instantânea sem perder o estado de execução. |

---

## 📡 Arquitetura de Comunicação (ESP-NOW Sem Roteador)

```text
[ Sensores (Solo / Chuva MH-RD / DHT) ]
         │ (GPIO / ADC Seguro)
         ▼
[ ESP32 Periférico ]
   ├── Modo ONLINE (Contínuo no loop) OU Modo Sleep (Timer RTC)
   ├── Auto-Discovery dinâmico do MAC do Central via Broadcast
   └── Envio Unicast direto para o ESP32 Central ──▶ [ ESP32 Central ]
```

---

## 📁 Estrutura dos Módulos Periféricos

* **[`ESP_Periferico_Base/`](ESP_Periferico_Base/) (Template Genérico):**
  * Molde modular reutilizável para implementar novos sensores (Solo, Temperatura, Luz, etc.).
  * [`config_periferico.h`](ESP_Periferico_Base/config_periferico.h): Configuração padrão de ID, fonte e modo de operação.
  * [`EspNowPeriferico.h`](ESP_Periferico_Base/EspNowPeriferico.h) / [`EspNowPeriferico.cpp`](ESP_Periferico_Base/EspNowPeriferico.cpp): Pilha de comunicação ESP-NOW com auto-discovery e pareamento.
  * [`ProtocoloMensagens.h`](ESP_Periferico_Base/ProtocoloMensagens.h): Definição de structs de rede compartilhadas.
  * [`ESP_Periferico_Base.ino`](ESP_Periferico_Base/ESP_Periferico_Base.ino): Sketch template com suporte a loop contínuo (`ONLINE`) ou ciclo de *Deep Sleep*.

* **[`ESP_Periferico_Chuva/`](ESP_Periferico_Chuva/) (Firmware Concreto do Sensor de Chuva):**
  * Projeto completo e **100% autocontido** para compilação direta no Arduino IDE.
  * [`config_periferico.h`](ESP_Periferico_Chuva/config_periferico.h): Pré-configurado para o sensor de chuva (`NODE_ID "esp-chuva-1"`, GPIO 14 na Base BJT, GPIO 34 no AO, GPIO 35 no DO).
  * [`SensorChuvaMHRD.h`](ESP_Periferico_Chuva/SensorChuvaMHRD.h) / [`SensorChuvaMHRD.cpp`](ESP_Periferico_Chuva/SensorChuvaMHRD.cpp): Driver com chaveamento Low-Side por BJT NPN (50 ms) contra oxidação.
  * [`EspNowPeriferico.h`](ESP_Periferico_Chuva/EspNowPeriferico.h) / [`EspNowPeriferico.cpp`](ESP_Periferico_Chuva/EspNowPeriferico.cpp) e [`ProtocoloMensagens.h`](ESP_Periferico_Chuva/ProtocoloMensagens.h): Comunicação ESP-NOW integrada.
  * [`ESP_Periferico_Chuva.ino`](ESP_Periferico_Chuva/ESP_Periferico_Chuva.ino): Firmware final pronto para gravar na placa (gravação via Arduino IDE).
