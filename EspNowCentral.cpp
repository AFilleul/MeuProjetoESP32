#include "EspNowCentral.h"
#include <esp_wifi.h>

EspNowCentral* EspNowCentral::instancia = nullptr;

EspNowCentral::EspNowCentral() : canalWifi(ESPNOW_WIFI_CHANNEL), onSensorData(nullptr) {
    instancia = this;
    memset(broadcastMac, 0xFF, sizeof(broadcastMac)); // FF:FF:FF:FF:FF:FF
}

EspNowCentral::~EspNowCentral() {
    esp_now_deinit();
    if (instancia == this) {
        instancia = nullptr;
    }
}

bool EspNowCentral::iniciar(uint8_t canal) {
    this->canalWifi = canal;

    // 1. Configura rádio Wi-Fi em modo STA sem conectar a roteador
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    // Define o canal do Wi-Fi explicitamente
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(canalWifi, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);

    Serial.printf("[ESP-NOW Central] Inicializando no Canal Wi-Fi %d...\n", canalWifi);
    Serial.printf("[ESP-NOW Central] Endereco MAC local: %s\n", WiFi.macAddress().c_str());

    // 2. Inicializa o subsistema ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESP-NOW Central] ERRO FATAL: Falha ao inicializar o ESP-NOW!");
        return false;
    }

    // 3. Registra os callbacks de envio e recebimento
    #if defined(ESP_IDF_VERSION_MAJOR) && (ESP_IDF_VERSION_MAJOR >= 5)
    esp_now_register_recv_cb(EspNowCentral::onDataReceive);
    #else
    esp_now_register_recv_cb(reinterpret_cast<esp_now_recv_cb_t>(EspNowCentral::onDataReceive));
    #endif

    esp_now_register_send_cb(EspNowCentral::onDataSend);

    // 4. Registra o endereço de Broadcast padrão na lista de peers
    if (!registrarPeer(broadcastMac, canalWifi, false)) {
        Serial.println("[ESP-NOW Central] Aviso: Falha ao cadastrar peer Broadcast.");
    } else {
        Serial.println("[ESP-NOW Central] Peer Broadcast (FF:FF:FF:FF:FF:FF) configurado com sucesso.");
    }

    Serial.println("[ESP-NOW Central] Pronto para receber mensagens Unicast e transmitir Broadcast.");
    return true;
}

bool EspNowCentral::registrarPeer(const uint8_t* macAddr, uint8_t canal, bool encrypt) {
    if (esp_now_is_peer_exist(macAddr)) {
        return true; // Já registrado
    }

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, macAddr, 6);
    peerInfo.channel = canal;
    peerInfo.encrypt = encrypt;
    peerInfo.ifidx = WIFI_IF_STA;

    esp_err_t result = esp_now_add_peer(&peerInfo);
    if (result != ESP_OK) {
        Serial.printf("[ESP-NOW Central] Erro ao adicionar peer: 0x%02X\n", result);
        return false;
    }
    return true;
}

bool EspNowCentral::enviarBroadcastBeacon(const char* identificador) {
    PacoteBroadcastBeacon beacon;
    beacon.tipo = MSG_BROADCAST_BEACON;
    beacon.timestampMillis = millis();
    beacon.canalWifi = canalWifi;
    strncpy(beacon.identificadorCentral, identificador, sizeof(beacon.identificadorCentral) - 1);
    beacon.identificadorCentral[sizeof(beacon.identificadorCentral) - 1] = '\0';

    esp_err_t res = esp_now_send(broadcastMac, reinterpret_cast<const uint8_t*>(&beacon), sizeof(beacon));
    if (res == ESP_OK) {
        Serial.printf("[ESP-NOW Central] Broadcast Beacon enviado (\"%s\").\n", identificador);
        return true;
    } else {
        Serial.printf("[ESP-NOW Central] Falha ao enviar Broadcast: 0x%02X\n", res);
        return false;
    }
}

void EspNowCentral::definirCallbackRecepcao(SensorDataCallback callback) {
    this->onSensorData = callback;
}

String EspNowCentral::getMacAddressString() const {
    return WiFi.macAddress();
}

// ============================================================================
// CALLBACKS DO SISTEMA ESP-NOW (C -> C++)
// ============================================================================

#if defined(ESP_IDF_VERSION_MAJOR) && (ESP_IDF_VERSION_MAJOR >= 5)
void EspNowCentral::onDataReceive(const esp_now_recv_info_t* recvInfo, const uint8_t* data, int dataLen) {
    const uint8_t* mac = recvInfo->src_addr;
#else
void EspNowCentral::onDataReceive(const uint8_t* mac_addr, const uint8_t* data, int dataLen) {
    const uint8_t* mac = mac_addr;
#endif

    if (!instancia || !data || dataLen <= 0) return;

    uint8_t tipo = data[0];

    // Se for pacote de telemetria dos sensores enviado por um periférico
    if (tipo == MSG_DADOS_SENSORES && dataLen == sizeof(PacoteDadosSensores)) {
        PacoteDadosSensores dados;
        memcpy(&dados, data, sizeof(PacoteDadosSensores));

        Serial.printf("\n[ESP-NOW Central] Pacote Unicast recebido de [%s]:\n", dados.nodeMac);
        Serial.printf("  Nó: %s (%s) | Modo: %s | Ciclo Wakeup: #%u\n", dados.nodeId, dados.nodeName, dados.modoOperacao, dados.cicloWakeup);
        Serial.printf("  Sensor Umidade: %.1f%% | Bateria: %d%% (%u mV)\n", dados.sensorUmidade, dados.bateriaPct, dados.bateriaMv);

        // Dispara o callback do usuário se configurado
        if (instancia->onSensorData) {
            instancia->onSensorData(mac, dados);
        }
    } else {
        Serial.printf("[ESP-NOW Central] Pacote recebido com tamanho %d bytes (Tipo 0x%02X desconhecido).\n", dataLen, tipo);
    }
}

void EspNowCentral::onDataSend(const uint8_t* mac_addr, esp_now_send_status_t status) {
    // Status do envio (Broadcast sempre retorna Delivery Success no ESP-NOW por não exigir ACK)
    if (status != ESP_NOW_SEND_SUCCESS) {
        Serial.println("[ESP-NOW Central] Alerta: Falha na entrega de pacote.");
    }
}
