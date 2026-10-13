#ifndef CONFIG_PERIFERICO_H
#define CONFIG_PERIFERICO_H

// ============================================================================
// CONFIGURAÇÃO DO NÓ PERIFÉRICO - SENSOR DE CHUVA (MH-RD)
// ============================================================================

// Identificação única do Periférico na rede IoT
#define NODE_ID              "esp-chuva-1"
#define NODE_NAME            "Sensor de Chuva 01"

// Canal Wi-Fi para o protocolo ESP-NOW (deve coincidir com o canal do ESP Central)
#define ESPNOW_WIFI_CHANNEL  1

// Pinos de hardware do Sensor de Chuva MH-RD com chaveamento BJT NPN (Low-Side Switch):
// Circuito: GPIO 14 -> Resistor 1k -> Base BJT | Coletor -> GND do MH-RD | Emissor -> GND ESP32
#define PIN_BJT_BASE         14      // GPIO 14: Aciona saturação/corte do BJT NPN (Low-Side Switch)
#define PIN_SENSOR_AO        34      // GPIO 34: ADC1 canal 6 (Leitura analógica 0-4095)
#define PIN_SENSOR_DO        35      // GPIO 35: Entrada digital (Saída do comparador LM393)

// Configuração de energia e fonte de alimentação:
// - "DC"     : Ligado à tomada/fonte contínua 5V (ideal para modo "ONLINE" ininterrupto)
// - "BATTERY": Bateria Li-Ion/18650 ou pilhas (ideal para modo "DEEP_SLEEP")
#define POWER_SOURCE_TYPE    "DC"       // "DC" ou "BATTERY"
#define BATTERY_CAPACITY_MAH 2500       // Capacidade da bateria se aplicável

// Intervalo de leitura e envio periódico (em minutos)
#define READ_INTERVAL_MIN    5

// Modo de operação do microcontrolador periférico:
// - "ONLINE"     : O nó permanece continuamente LIGADO e CONECTADO (rádio e CPU 100% ativos).
//                  Ideal para alimentação DC na tomada ou quando se deseja respostas instantâneas,
//                  telemetria contínua e aplicação imediata de configurações enviadas pela interface web.
// - "DEEP_SLEEP" : O nó acorda, transmite a telemetria em milissegundos e desliga CPU/rádio.
//                  Modo de máxima conservação para nós operados a bateria.
// - "LIGHT_SLEEP": O nó suspende temporariamente o processador mantendo a memória RAM preservada.
#define OPERATING_MODE       "ONLINE"

// Endereço IP e porta do ESP32 Central (servidor REST)
#define CENTRAL_SERVER_HOST  "192.168.4.1"
#define CENTRAL_SERVER_PORT  80

#endif // CONFIG_PERIFERICO_H
