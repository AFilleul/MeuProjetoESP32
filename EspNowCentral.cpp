#include "EspNowCentral.h"
#include <esp_wifi.h>

EspNowCentral* EspNowCentral::instancia = nullptr;

EspNowCentral::EspNowCentral() 
    : canalWifi(ESPNOW_WIFI_CHANNEL), 
      onSensorData(nullptr), 
      onAckConfig(nullptr) {
    instancia = this;
    memset(broadcastMac, 0xFF, sizeof(broadcastMac)); // FF:FF:FF:FF:FF:FF
}

EspNowCentral::~EspNowCentral() {
    esp_now_deinit();
    if (instancia == this) {
        instancia = nullptr;
    }
}

bool EspNowCentral::iniciar(uint8_t canal, bool configurarWifi) {
    this->canalWifi = canal;

    if (configurarWifi) {
        // 1. Configura rádio Wi-Fi em modo STA sem conectar a roteador
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();

        // Define o canal do Wi-Fi explicitamente
        esp_wifi_set_promiscuous(true);
        esp_wifi_set_channel(canalWifi, WIFI_SECOND_CHAN_NONE);
        esp_wifi_set_promiscuous(false);
    } else {
        uint8_t currentCh = WiFi.channel();
        if (currentCh > 0) {
            this->canalWifi = currentCh;
        }
    }

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

    #if defined(ESP_IDF_VERSION_MAJOR) && (ESP_IDF_VERSION_MAJOR >= 5)
    esp_now_register_send_cb(EspNowCentral::onDataSend);
    #else
    esp_now_register_send_cb(reinterpret_cast<esp_now_send_cb_t>(EspNowCentral::onDataSend));
    #endif

    // 4. Registra o endereço de Broadcast padrão na lista de peers
    if (!registrarPeer(broadcastMac, 0, false)) {
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
    // Usa channel = 0 para utilizar automaticamente o canal Wi-Fi ativo da interface STA
    peerInfo.channel = 0;
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
        return true;
    } else {
        Serial.printf("[ESP-NOW Central] Falha ao enviar Broadcast: 0x%02X\n", res);
        return false;
    }
}

bool EspNowCentral::enviarConfiguracao(const uint8_t* macPeriferico, const PacoteConfiguracao& config) {
    // Garante que o periférico está registrado como peer
    if (!registrarPeer(macPeriferico, canalWifi, false)) {
        Serial.println("[ESP-NOW Central] Erro: Impossivel registrar peer para envio de configuracao.");
        return false;
    }

    esp_err_t res = esp_now_send(macPeriferico, reinterpret_cast<const uint8_t*>(&config), sizeof(config));
    if (res == ESP_OK) {
        Serial.printf("[ESP-NOW Central] Pacote de Configuracao enviado com sucesso para %02X:%02X:%02X:%02X:%02X:%02X!\n",
                      macPeriferico[0], macPeriferico[1], macPeriferico[2], macPeriferico[3], macPeriferico[4], macPeriferico[5]);
        return true;
    } else {
        Serial.printf("[ESP-NOW Central] Falha ao despachar configuracao: 0x%02X\n", res);
        return false;
    }
}

void EspNowCentral::definirCallbackRecepcao(SensorDataCallback callback) {
    this->onSensorData = callback;
}

void EspNowCentral::definirCallbackAckConfig(AckConfigCallback callback) {
    this->onAckConfig = callback;
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

    // 1. PACOTE DE TELEMETRIA DOS SENSORES (UNICAST)
    if (tipo == MSG_DADOS_SENSORES && dataLen == sizeof(PacoteDadosSensores)) {
        PacoteDadosSensores dados;
        memcpy(&dados, data, sizeof(PacoteDadosSensores));

        // Registra automaticamente o periférico como peer no Central
        instancia->registrarPeer(mac, instancia->canalWifi, false);

        Serial.printf("\n[ESP-NOW Central] Telemetria recebida de [%s]:\n", dados.nodeMac);
        Serial.printf("  Nó: %s (%s) | Modo: %s | Ciclo: #%u\n", 
                      dados.nodeId, dados.nodeName, dados.modoOperacao, dados.cicloWakeup);
        Serial.printf("  Sensor Umidade: %.1f%% | Bateria: %d%% (%u mV)\n", 
                      dados.sensorUmidade, dados.bateriaPct, dados.bateriaMv);

        if (dados.pareamentoAtivo) {
            Serial.println("  *** [PAREAMENTO ATIVO] Este nó realizou Hard Reset e está ONLINE aguardando configurações! ***");
        }

        if (instancia->onSensorData) {
            instancia->onSensorData(mac, dados);
        }
    }
    // 2. PACOTE DE ACK DE CONFIGURAÇÃO (UNICAST)
    else if (tipo == MSG_ACK_CONFIG && dataLen == sizeof(PacoteAckConfig)) {
        PacoteAckConfig ack;
        memcpy(&ack, data, sizeof(PacoteAckConfig));

        Serial.printf("\n[ESP-NOW Central] ACK de Configuracao recebido do nó [%s]:\n", ack.nodeId);
        Serial.printf("  Status: %s | Intervalo Salvo: %d min | Modo Salvo: %s\n",
                      ack.sucesso ? "SUCESSO" : "ERRO", ack.intervaloAplicadoMin, ack.modoAplicado);

        if (instancia->onAckConfig) {
            instancia->onAckConfig(mac, ack);
        }
    }
}

#if defined(ESP_IDF_VERSION_MAJOR) && (ESP_IDF_VERSION_MAJOR >= 5)
void EspNowCentral::onDataSend(const wifi_tx_info_t* tx_info, esp_now_send_status_t status) {
    if (status != ESP_NOW_SEND_SUCCESS) {
        Serial.println("[ESP-NOW Central] Alerta: Falha na confirmacao de entrega do pacote.");
    }
}
#else
void EspNowCentral::onDataSend(const uint8_t* mac_addr, esp_now_send_status_t status) {
    if (status != ESP_NOW_SEND_SUCCESS) {
        Serial.println("[ESP-NOW Central] Alerta: Falha na confirmacao de entrega do pacote.");
    }
}
#endif
