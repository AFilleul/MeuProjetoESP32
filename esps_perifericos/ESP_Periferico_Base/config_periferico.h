#ifndef CONFIG_PERIFERICO_H
#define CONFIG_PERIFERICO_H

// ============================================================================
// CONFIGURAÇÕES DO NÓ ESP32 PERIFÉRICO
// ============================================================================

// Identificação única deste nó na rede do sítio
#define NODE_ID              "esp-2"
#define NODE_NAME            "ESP 2 (Sensor de Umidade)"
#define SENSOR_TYPE          "Umidade do Solo"
#define SENSOR_UNIT          "% Umidade"

// Configuração do pino do sensor analógico (Ex: MH-RD de Umidade de Solo)
#define PIN_SENSOR_ANALOG    34      // GPIO 34 (ADC1, seguro com Wi-Fi ativo)
#define PIN_SENSOR_VCC       23      // GPIO opcional para alimentar o sensor apenas na hora da leitura

// Configuração de energia e fonte de alimentação:
// - "DC"     : Ligado à tomada/fonte contínua 5V (ideal para modo "ONLINE" ininterrupto)
// - "BATTERY": Bateria Li-Ion/18650 ou pilhas (ideal para modo "DEEP_SLEEP")
#define POWER_SOURCE_TYPE    "DC"       // "DC" ou "BATTERY"
#define BATTERY_CAPACITY_MAH 2500       // Capacidade da bateria se aplicável

// Intervalo de leitura e envio (em minutos)
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
#define CENTRAL_SERVER_HOST  "192.168.4.1" // ou IP local do central (ex: 192.168.1.50)
#define CENTRAL_SERVER_PORT  80

#endif // CONFIG_PERIFERICO_H
