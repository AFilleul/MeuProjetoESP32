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

// Configuração de energia
#define POWER_SOURCE_TYPE    "BATTERY"  // "BATTERY" ou "DC"
#define BATTERY_CAPACITY_MAH 2500       // Capacidade da bateria se aplicável

// Intervalo de leitura e envio (em minutos)
#define READ_INTERVAL_MIN    30

// Modo de operação ("DEEP_SLEEP", "LIGHT_SLEEP", "ONLINE")
#define OPERATING_MODE       "DEEP_SLEEP"

// Endereço IP e porta do ESP32 Central (servidor REST)
#define CENTRAL_SERVER_HOST  "192.168.4.1" // ou IP local do central (ex: 192.168.1.50)
#define CENTRAL_SERVER_PORT  80

#endif // CONFIG_PERIFERICO_H
