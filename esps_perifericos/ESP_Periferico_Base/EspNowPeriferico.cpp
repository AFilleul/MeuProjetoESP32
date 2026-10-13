#include "EspNowPeriferico.h"
#include <esp_wifi.h>

EspNowPeriferico* EspNowPeriferico::instancia = nullptr;

EspNowPeriferico::EspNowPeriferico() 
    : canalWifi(ESPNOW_WIFI_CHANNEL), 
      centralEncontrado(false), 
      beaconRecebidoFlag(false) {
    instancia = this;
    memset(macCentral, 0, sizeof(macCentral));
}

EspNowPeriferico::~EspNowPeriferico() {
    esp_now_deinit();
    if (instancia == this) {
        instancia = nullptr;
    }
}

bool EspNowPeriferico::iniciar(uint8_t canal) {
    this->canalWifi = canal;
    this->centralEncontrado = false;
    this->beaconRecebidoFlag = false;

    // 1. Configura rádio Wi-Fi em Station e desconecta
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    // Fixa o canal de rádio
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(canalWifi, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);

    Serial.printf("[ESP-NOW Periferico] Rádio ativado no Canal %d.\n", canalWifi);
    Serial.printf("[ESP-NOW Periferico] MAC do Periférico: %s\n", WiFi.macAddress().c_str());

    // 2. Inicializa o protocolo ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESP-NOW Periferico] ERRO FATAL: Falha ao iniciar ESP-NOW!");
        return false;
    }

    // 3. Registra os callbacks nativos
    #if defined(ESP_IDF_VERSION_MAJOR) && (ESP_IDF_VERSION_MAJOR >= 5)
    esp_now_register_recv_cb(EspNowPeriferico::onDataReceive);
    #else
    esp_now_register_recv_cb(reinterpret_cast<esp_now_recv_cb_t>(EspNowPeriferico::onDataReceive));
    #endif

    esp_now_register_send_cb(EspNowPeriferico::onDataSend);

    Serial.println("[ESP-NOW Periferico] Aguardando transmissao Broadcast do Central...");
    return true;
}

bool EspNowPeriferico::adicionarPeer(const uint8_t* macAddr, uint8_t canal, bool encrypt) {
    if (esp_now_is_peer_exist(macAddr)) {
        return true; // Já registrado anteriormente
    }

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, macAddr, 6);
    peerInfo.channel = canal;
    peerInfo.encrypt = encrypt;
    peerInfo.ifidx = WIFI_IF_STA;

    esp_err_t res = esp_now_add_peer(&peerInfo);
    if (res != ESP_OK) {
        Serial.printf("[ESP-NOW Periferico] Falha ao adicionar peer: 0x%02X\n", res);
        return false;
    }

    Serial.printf("[ESP-NOW Periferico] Peer [%02X:%02X:%02X:%02X:%02X:%02X] adicionado com sucesso!\n",
                  macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
    return true;
}

bool EspNowPeriferico::aguardarBeaconCentral(unsigned long timeoutMs) {
    unsigned long inicio = millis();
    Serial.printf("[ESP-NOW Periferico] Escutando o ar (timeout: %lu ms)...\n", timeoutMs);

    while (!beaconRecebidoFlag && (millis() - inicio < timeoutMs)) {
        delay(10); // Permite processamento de interrupções de rádio
    }

    if (beaconRecebidoFlag && centralEncontrado) {
        Serial.printf("[ESP-NOW Periferico] Central DESCOBERTO: %s\n", getCentralMacString().c_str());
        return true;
    }

    Serial.println("[ESP-NOW Periferico] Timeout: Central nao respondeu a tempo.");
    return false;
}

bool EspNowPeriferico::enviarDadosUnicast(const PacoteDadosSensores& dados) {
    if (!centralEncontrado) {
        Serial.println("[ESP-NOW Periferico] Erro: Tentativa de envio Unicast sem MAC do Central!");
        return false;
    }

    esp_err_t res = esp_now_send(macCentral, reinterpret_cast<const uint8_t*>(&dados), sizeof(dados));
    if (res == ESP_OK) {
        Serial.printf("[ESP-NOW Periferico] Dados Unicast despachados com sucesso para %s!\n", getCentralMacString().c_str());
        return true;
    } else {
        Serial.printf("[ESP-NOW Periferico] Falha ao despachar dados Unicast: 0x%02X\n", res);
        return false;
    }
}

void EspNowPeriferico::entrarDeepSleep(uint32_t minutos) {
    uint64_t tempoMicros = (uint64_t)minutos * 60ULL * 1000000ULL;
    Serial.printf("[ESP-NOW Periferico] Entrando em Deep Sleep por %u minuto(s)...\n", minutos);
    Serial.flush();
    esp_sleep_enable_timer_wakeup(tempoMicros);
    esp_deep_sleep_start();
}

bool EspNowPeriferico::centralDescoberto() const {
    return centralEncontrado;
}

void EspNowPeriferico::getCentralMac(uint8_t* outMac) const {
    memcpy(outMac, macCentral, 6);
}

String EspNowPeriferico::getCentralMacString() const {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             macCentral[0], macCentral[1], macCentral[2], macCentral[3], macCentral[4], macCentral[5]);
    return String(buf);
}

String EspNowPeriferico::getLocalMacString() const {
    return WiFi.macAddress();
}

// ============================================================================
// CALLBACKS DO SISTEMA ESP-NOW (C -> C++)
// ============================================================================

#if defined(ESP_IDF_VERSION_MAJOR) && (ESP_IDF_VERSION_MAJOR >= 5)
void EspNowPeriferico::onDataReceive(const esp_now_recv_info_t* recvInfo, const uint8_t* data, int dataLen) {
    const uint8_t* mac = recvInfo->src_addr;
#else
void EspNowPeriferico::onDataReceive(const uint8_t* mac_addr, const uint8_t* data, int dataLen) {
    const uint8_t* mac = mac_addr;
#endif

    if (!instancia || !data || dataLen <= 0) return;

    uint8_t tipo = data[0];

    // Se for o pacote de Broadcast de anúncio do Central (Beacon)
    if (tipo == MSG_BROADCAST_BEACON && dataLen == sizeof(PacoteBroadcastBeacon)) {
        PacoteBroadcastBeacon beacon;
        memcpy(&beacon, data, sizeof(beacon));

        Serial.printf("\n[ESP-NOW Periferico] Beacon recebido de \"%s\" via Broadcast!\n", beacon.identificadorCentral);
        Serial.printf("[ESP-NOW Periferico] Auto-Discovery: MAC do Central extraido = %02X:%02X:%02X:%02X:%02X:%02X\n",
                      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        // 1. Armazena o MAC do Central
        memcpy(instancia->macCentral, mac, 6);
        instancia->centralEncontrado = true;

        // 2. Registra o Central imediatamente como peer para poder enviar Unicast
        instancia->adicionarPeer(mac, instancia->canalWifi, false);

        // 3. Marca flag de recebimento para liberar o loop de espera
        instancia->beaconRecebidoFlag = true;
    }
}

void EspNowPeriferico::onDataSend(const uint8_t* mac_addr, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        Serial.println("[ESP-NOW Periferico] ACK de entrega Unicast CONFIRMADO pelo Central!");
    } else {
        Serial.println("[ESP-NOW Periferico] Alerta: Envio Unicast falhou ou sem confirmacao.");
    }
}
