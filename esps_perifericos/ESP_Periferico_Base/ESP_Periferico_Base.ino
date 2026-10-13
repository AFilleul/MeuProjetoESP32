#include <Arduino.h>
#include "EspNowPeriferico.h"
#include "config_periferico.h"

// Instância modular do protocolo ESP-NOW para o periférico
EspNowPeriferico espNowNode;

// Variáveis preservadas na memória RTC durante o Deep Sleep
RTC_DATA_ATTR uint32_t bootCount = 0;
RTC_DATA_ATTR uint32_t tempoUptimeAcumuladoSegundos = 0;

// Leitura simulada/real do sensor de umidade de solo (GPIO configurado em config_periferico.h)
float lerUmidadeSolo() {
    #ifdef PIN_SENSOR_VCC
    pinMode(PIN_SENSOR_VCC, OUTPUT);
    digitalWrite(PIN_SENSOR_VCC, HIGH); // Liga o sensor apenas durante a medição
    delay(30);
    #endif

    int leituraAdc = analogRead(PIN_SENSOR_ANALOG);

    #ifdef PIN_SENSOR_VCC
    digitalWrite(PIN_SENSOR_VCC, LOW);  // Desliga o sensor para economizar corrente
    #endif

    // Calibração analógica: 4095 (Seco) -> 1200 (Encharcado)
    float umidade = map(leituraAdc, 4095, 1200, 0, 100);
    return constrain(umidade, 0.0, 100.0);
}

// Estimativa da tensão da bateria Li-ion 18650 em mV e porcentagem
void lerBateria(uint16_t &outMv, uint8_t &outPct) {
    // Exemplo de leitura de divisor de tensão (3.7V - 4.2V típico)
    outMv = 3950; // 3.95V (simulado ou lido de pino ADC dedicado)
    outPct = (uint8_t)map(constrain(outMv, 3300, 4200), 3300, 4200, 0, 100);
}

void setup() {
    Serial.begin(115200);
    delay(300);

    bootCount++;
    unsigned long tempoInicioMs = millis();

    Serial.println("\n========================================================");
    Serial.printf("  ESP32 PERIFÉRICO - NÓ SENSOR [%s]\n", NODE_ID);
    Serial.printf("  Ciclo Wakeup Deep Sleep: #%u\n", bootCount);
    Serial.println("========================================================");

    // 1. Inicializa o rádio ESP-NOW
    if (!espNowNode.iniciar(ESPNOW_WIFI_CHANNEL)) {
        Serial.println("[Periferico] Erro ao iniciar rádio. Abortando para Deep Sleep...");
        espNowNode.entrarDeepSleep(READ_INTERVAL_MIN);
        return;
    }

    // 2. Realiza as medições dos sensores locais
    float umidade = lerUmidadeSolo();
    uint16_t tensaoBateriaMv = 0;
    uint8_t porcentagemBateria = 0;
    lerBateria(tensaoBateriaMv, porcentagemBateria);

    Serial.printf("[Sensores] Umidade do Solo: %.1f%% | Bateria: %d%% (%u mV)\n", 
                  umidade, porcentagemBateria, tensaoBateriaMv);

    // 3. Aguarda o pacote de Broadcast do ESP Central (Auto-Discovery do MAC)
    Serial.println("[Periferico] Aguardando transmissao Broadcast do Central para descoberta de MAC...");
    bool centralDescoberto = espNowNode.aguardarBeaconCentral(7000); // 7 segundos de janela de escuta

    if (centralDescoberto) {
        // 4. Monta o pacote de dados para envio Unicast direto ao MAC descoberto
        PacoteDadosSensores pacote = {};
        pacote.tipo = MSG_DADOS_SENSORES;
        strncpy(pacote.nodeMac, espNowNode.getLocalMacString().c_str(), sizeof(pacote.nodeMac) - 1);
        strncpy(pacote.nodeId, NODE_ID, sizeof(pacote.nodeId) - 1);
        strncpy(pacote.nodeName, NODE_NAME, sizeof(pacote.nodeName) - 1);
        pacote.sensorUmidade = umidade;
        pacote.temperatura = 24.5; // Leitura de temperatura ambiente opcional
        pacote.bateriaPct = porcentagemBateria;
        pacote.bateriaMv = tensaoBateriaMv;
        pacote.cicloWakeup = bootCount;
        pacote.tempoUptimeSegundos = tempoUptimeAcumuladoSegundos + ((millis() - tempoInicioMs) / 1000);
        strncpy(pacote.modoOperacao, OPERATING_MODE, sizeof(pacote.modoOperacao) - 1);

        // 5. Envia via Unicast para o Central descoberto
        Serial.printf("[Periferico] Transmitindo Unicast para o Central (%s)...\n", 
                      espNowNode.getCentralMacString().c_str());
        espNowNode.enviarDadosUnicast(pacote);

        // Pequena pausa para garantir que os pacotes de rádio deixem o buffer do hardware
        delay(100);
    } else {
        Serial.println("[Periferico] Central nao encontrado na janela de tempo.");
    }

    // 6. Atualiza tempo ativo acumulado na memória RTC
    tempoUptimeAcumuladoSegundos += ((millis() - tempoInicioMs) / 1000) + 1;

    // 7. Retorna imediatamente para o modo Deep Sleep para economizar bateria
    Serial.printf("[Periferico] Ciclo concluído em %lu ms. Entrando em Deep Sleep...\n", millis() - tempoInicioMs);
    espNowNode.entrarDeepSleep(READ_INTERVAL_MIN);
}

void loop() {
    // No modo Deep Sleep, o código nunca atinge o loop()
}
