#ifndef PROTOCOLO_MENSAGENS_H
#define PROTOCOLO_MENSAGENS_H

#include <Arduino.h>

// Canal padrão de rádio Wi-Fi para o ESP-NOW (deve ser o mesmo em todos os nós)
#define ESPNOW_WIFI_CHANNEL 1

// Tipos de Mensagens
enum TipoMensagem : uint8_t {
    MSG_BROADCAST_BEACON   = 0x01,  // Central -> Broadcast: Anúncio de presença e solicitação de dados
    MSG_DADOS_SENSORES     = 0x02,  // Periférico -> Central (Unicast): Telemetria e bateria
    MSG_ACK_CONFIRMACAO    = 0x03,  // Central -> Periférico: Confirmação de recebimento genérico
    MSG_CONFIG_PARAMETROS  = 0x04,  // Central -> Periférico (Unicast): Envio de configurações (intervalo, modo, etc.)
    MSG_ACK_CONFIG         = 0x05   // Periférico -> Central (Unicast): Confirmação de configuração aplicada
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
    char nodeName[32];             // Ex: "Solo Setor A"
    float sensorUmidade;           // Valor de umidade (0.0 a 100.0%)
    float temperatura;             // Opcional (ex: DHT22, ou temperatura interna)
    uint8_t bateriaPct;            // Nível de bateria (0 a 100%)
    uint16_t bateriaMv;            // Tensão medida em milivolts (ex: 3700 mV)
    uint32_t tempoUptimeSegundos;  // Tempo total ativo acumulado
    uint32_t cicloWakeup;          // Número de vezes que o nó acordou do Deep Sleep
    char modoOperacao[16];         // "DEEP_SLEEP", "LIGHT_SLEEP", "ONLINE", "PAREAMENTO"
    uint8_t pareamentoAtivo;       // 1 se estiver na janela de 1 min pós-hard reset, 0 se ciclo normal
};
#pragma pack(pop)

// ============================================================================
// 3. ESTRUTURA: PACOTE DE CONFIGURAÇÃO (Central -> Periférico Unicast)
// ============================================================================
#pragma pack(push, 1)
struct PacoteConfiguracao {
    uint8_t tipo;                  // MSG_CONFIG_PARAMETROS
    char targetNodeId[12];         // "esp-2", ou "*" para todos os nós
    uint16_t novoIntervaloMin;     // Novo intervalo de leitura em minutos (ex: 15, 30, 60)
    char novoModo[16];             // "DEEP_SLEEP", "LIGHT_SLEEP", "ONLINE"
    char novoNome[32];             // Novo nome amigável do periférico
    uint32_t timestampMillis;      // Timestamp do envio
};
#pragma pack(pop)

// ============================================================================
// 4. ESTRUTURA: PACOTE DE CONFIRMAÇÃO DE CONFIGURAÇÃO (Periférico -> Central)
// ============================================================================
#pragma pack(push, 1)
struct PacoteAckConfig {
    uint8_t tipo;                  // MSG_ACK_CONFIG
    char nodeId[12];               // ID do nó que confirmou
    uint8_t sucesso;               // 1 = Sucesso, 0 = Erro
    uint16_t intervaloAplicadoMin; // Intervalo salvo no periférico
    char modoAplicado[16];         // Modo salvo no periférico
};
#pragma pack(pop)

#endif // PROTOCOLO_MENSAGENS_H
