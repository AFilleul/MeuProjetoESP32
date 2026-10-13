#ifndef PROTOCOLO_MENSAGENS_H
#define PROTOCOLO_MENSAGENS_H

#include <Arduino.h>

// Canal padrão de rádio Wi-Fi para o ESP-NOW (deve ser o mesmo em todos os nós)
#define ESPNOW_WIFI_CHANNEL 1

// Tipos de Mensagens
enum TipoMensagem : uint8_t {
    MSG_BROADCAST_BEACON   = 0x01,  // Central -> Broadcast: Anúncio de presença e solicitação de dados
    MSG_DADOS_SENSORES     = 0x02,  // Periférico -> Central (Unicast): Telemetria e bateria
    MSG_ACK_CONFIRMACAO    = 0x03   // Central -> Periférico: Confirmação de recebimento (opcional)
};

// ============================================================================
// 1. ESTRUTURA: PACOTE DE BROADCAST (Central -> Periféricos)
// ============================================================================
#pragma pack(push, 1)
struct PacoteBroadcastBeacon {
    uint8_t tipo;                  // MSG_BROADCAST_BEACON
    uint32_t timestampMillis;      // Timestamp ou millis do Central
    uint8_t canalWifi;             // Canal Wi-Fi sugerido
    char identificadorCentral[16]; // Ex: "CENTRAL_SITIO"
};
#pragma pack(pop)

// ============================================================================
// 2. ESTRUTURA: PACOTE DE TELEMETRIA (Periférico -> Central Unicast)
// ============================================================================
#pragma pack(push, 1)
struct PacoteDadosSensores {
    uint8_t tipo;                  // MSG_DADOS_SENSORES
    char nodeMac[18];              // MAC em formato texto "XX:XX:XX:XX:XX:XX"
    char nodeId[12];               // Ex: "esp-2", "esp-3"
    char nodeName[24];             // Ex: "Solo Setor A"
    float sensorUmidade;           // Valor de umidade (0.0 a 100.0%)
    float temperatura;             // Opcional (ex: DHT22, ou temperatura interna)
    uint8_t bateriaPct;            // Nível de bateria (0 a 100%)
    uint16_t bateriaMv;            // Tensão medida em milivolts (ex: 3700 mV)
    uint32_t tempoUptimeSegundos;  // Tempo total ativo acumulado
    uint32_t cicloWakeup;          // Número de vezes que o nó acordou do Deep Sleep
    char modoOperacao[12];         // "DEEP_SLEEP", "LIGHT_SLEEP", "ONLINE"
};
#pragma pack(pop)

#endif // PROTOCOLO_MENSAGENS_H
