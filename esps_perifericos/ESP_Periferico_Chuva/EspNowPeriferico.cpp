#include "EspNowPeriferico.h"
#include <esp_wifi.h>

EspNowPeriferico* EspNowPeriferico::instancia = nullptr;

EspNowPeriferico::EspNowPeriferico() 
    : canalWifi(ESPNOW_WIFI_CHANNEL), 
      centralEncontrado(false), 
      beaconRecebidoFlag(false),
      configRecebidaFlag(false),
      onConfigCallback(nullptr) {
    instancia = this;
    memset(macCentral, 0, sizeof(macCentral));
    memset(&ultimaConfiguracao, 0, sizeof(ultimaConfiguracao));
}

EspNowPeriferico::~EspNowPeriferico() {
    esp_now_deinit();
    if (instancia == this) {
        instancia = nullptr;
    }
}

bool EspNowPeriferico::isHardReset() {
    esp_reset_reason_t motivo = esp_reset_reason();
    // Qualquer reinicialização que NÃO seja o retorno do Deep Sleep é um Hard Reset
    return (motivo != ESP_RST_DEEPSLEEP);
}

String EspNowPeriferico::getMotivoResetString() {
    esp_reset_reason_t motivo = esp_reset_reason();
    switch (motivo) {
        case ESP_RST_POWERON:
            return "POWERON (Energizacao a frio / Cabo conectado)";
        case ESP_RST_EXT:
            return "EXTERNAL_PIN (Botao fisico EN / RESET pressionado)";
        case ESP_RST_SW:
            return "SOFTWARE_RESET (Reinicializacao via software)";
        case ESP_RST_DEEPSLEEP:
            return "DEEP_SLEEP (Retorno programado de sono profundo)";
        case ESP_RST_BROWNOUT:
            return "BROWNOUT (Instabilidade/queda de tensao)";
        default:
            return "OUTRO (" + String((int)motivo) + ")";
    }
}

bool EspNowPeriferico::iniciar(uint8_t canal) {
    this->canalWifi = canal;
    this->centralEncontrado = false;
    this->beaconRecebidoFlag = false;
    this->configRecebidaFlag = false;

    // 1. Configura rádio Wi-Fi em Station e desconecta
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    // Fixa o canal de rádio Wi-Fi
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(canalWifi, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);

    Serial.printf("[ESP-NOW Periferico] Rádio ativado no Canal %d.\n", canalWifi);
    Serial.printf("[ESP-NOW Periferico] Endereco MAC local: %s\n", WiFi.macAddress().c_str());

    // 2. Inicializa o protocolo ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESP-NOW Periferico] ERRO FATAL: Falha ao iniciar ESP-NOW!");
        return false;
    }

    // 3. Registra callbacks de recebimento e envio
    #if defined(ESP_IDF_VERSION_MAJOR) && (ESP_IDF_VERSION_MAJOR >= 5)
    esp_now_register_recv_cb(EspNowPeriferico::onDataReceive);
    #else
    esp_now_register_recv_cb(reinterpret_cast<esp_now_recv_cb_t>(EspNowPeriferico::onDataReceive));
    #endif

    #if defined(ESP_IDF_VERSION_MAJOR) && (ESP_IDF_VERSION_MAJOR >= 5)
    esp_now_register_send_cb(EspNowPeriferico::onDataSend);
    #else
    esp_now_register_send_cb(reinterpret_cast<esp_now_send_cb_t>(EspNowPeriferico::onDataSend));
    #endif

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

    Serial.printf("[ESP-NOW Periferico] Central adicionado como Peer: [%02X:%02X:%02X:%02X:%02X:%02X]\n",
                  macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
    return true;
}

bool EspNowPeriferico::aguardarBeaconCentral(unsigned long timeoutMs) {
    unsigned long inicio = millis();
    Serial.printf("[ESP-NOW Periferico] Escutando o ar por Broadcast (timeout: %lu ms)...\n", timeoutMs);

    while (!beaconRecebidoFlag && (millis() - inicio < timeoutMs)) {
        delay(10);
    }

    if (beaconRecebidoFlag && centralEncontrado) {
        Serial.printf("[ESP-NOW Periferico] Central DESCOBERTO: %s\n", getCentralMacString().c_str());
        return true;
    }

    Serial.println("[ESP-NOW Periferico] Timeout: Beacon do Central nao capturado.");
    return false;
}

bool EspNowPeriferico::enviarDadosUnicast(const PacoteDadosSensores& dados) {
    if (!centralEncontrado) {
        Serial.println("[ESP-NOW Periferico] Erro: Impossivel enviar Unicast sem MAC do Central!");
        return false;
    }

    esp_err_t res = esp_now_send(macCentral, reinterpret_cast<const uint8_t*>(&dados), sizeof(dados));
    if (res == ESP_OK) {
        Serial.printf("[ESP-NOW Periferico] Telemetria despachada via Unicast para %s.\n", getCentralMacString().c_str());
        return true;
    } else {
        Serial.printf("[ESP-NOW Periferico] Falha ao despachar telemetria Unicast: 0x%02X\n", res);
        return false;
    }
}

bool EspNowPeriferico::enviarAckConfiguracao(const char* nodeId, bool sucesso, uint16_t intervalo, const char* modo) {
    if (!centralEncontrado) return false;

    PacoteAckConfig ack = {};
    ack.tipo = MSG_ACK_CONFIG;
    strncpy(ack.nodeId, nodeId, sizeof(ack.nodeId) - 1);
    ack.sucesso = sucesso ? 1 : 0;
    ack.intervaloAplicadoMin = intervalo;
    strncpy(ack.modoAplicado, modo, sizeof(ack.modoAplicado) - 1);

    esp_err_t res = esp_now_send(macCentral, reinterpret_cast<const uint8_t*>(&ack), sizeof(ack));
    if (res == ESP_OK) {
        Serial.println("[ESP-NOW Periferico] ACK de configuracao enviado de volta ao Central com sucesso.");
        return true;
    }
    return false;
}

void EspNowPeriferico::executarJanelaPareamento(unsigned long duracaoMs, 
                                                std::function<PacoteDadosSensores()> geradorTelemetria,
                                                std::function<void(const PacoteConfiguracao&)> onConfigRecebida) {
    unsigned long inicio = millis();
    unsigned long ultimoEnvioPresenca = 0;
    unsigned long ultimoLogContagem = 0;

    Serial.println("\n****************************************************************");
    Serial.println("  >>> MODO PAREAMENTO ATIVO (POS HARD-RESET) <<<");
    Serial.printf("  Permanecendo ONLINE por %lu segundos para receber configs...\n", duracaoMs / 1000);
    Serial.println("****************************************************************\n");

    while (millis() - inicio < duracaoMs) {
        unsigned long decorrido = millis() - inicio;
        unsigned long restanteSegundos = (duracaoMs - decorrido) / 1000;

        // Log da contagem regressiva a cada 10 segundos
        if (millis() - ultimoLogContagem >= 10000) {
            ultimoLogContagem = millis();
            Serial.printf("[Pareamento] Nó online. Tempo restante na janela de pareamento: %lu segundos...\n", restanteSegundos);
        }

        // Se ainda não descobriu o Central, continua aguardando o beacon
        if (!centralEncontrado) {
            delay(50);
            continue;
        }

        // A cada 5 segundos, envia pacote de telemetria / anúncio de presença
        if (millis() - ultimoEnvioPresenca >= 5000) {
            ultimoEnvioPresenca = millis();
            if (geradorTelemetria) {
                PacoteDadosSensores pacote = geradorTelemetria();
                pacote.pareamentoAtivo = 1;
                strncpy(pacote.modoOperacao, "PAREAMENTO", sizeof(pacote.modoOperacao) - 1);
                enviarDadosUnicast(pacote);
            }
        }

        // Se uma configuração foi recebida do Central durante a janela
        if (configRecebidaFlag) {
            configRecebidaFlag = false;
            Serial.println("[Pareamento] Nova configuracao recebida do Central!");
            if (onConfigRecebida) {
                onConfigRecebida(ultimaConfiguracao);
            }
        }

        delay(50);
    }

    Serial.println("\n[Pareamento] Janela de 1 minuto finalizada. Prosseguindo com o ciclo operacional normal.\n");
}

void EspNowPeriferico::definirCallbackConfiguracao(ConfigCallback callback) {
    this->onConfigCallback = callback;
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

bool EspNowPeriferico::temConfiguracaoRecebida() const {
    return configRecebidaFlag;
}

// ============================================================================
// CALLBACKS NATIVOS DO ESP-NOW (C -> C++)
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

    // 1. BEACON BROADCAST DO CENTRAL (DESCOBERTA DINÂMICA DE MAC)
    if (tipo == MSG_BROADCAST_BEACON && dataLen == sizeof(PacoteBroadcastBeacon)) {
        PacoteBroadcastBeacon beacon;
        memcpy(&beacon, data, sizeof(beacon));

        Serial.printf("\n[ESP-NOW Periferico] Beacon recebido de \"%s\" via Broadcast!\n", beacon.identificadorCentral);
        Serial.printf("[ESP-NOW Periferico] Auto-Discovery: MAC do Central obtido = %02X:%02X:%02X:%02X:%02X:%02X\n",
                      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        memcpy(instancia->macCentral, mac, 6);
        instancia->centralEncontrado = true;
        instancia->adicionarPeer(mac, instancia->canalWifi, false);
        instancia->beaconRecebidoFlag = true;
    }
    // 2. PACOTE DE CONFIGURAÇÃO ENVIADO PELO CENTRAL (UNICAST)
    else if (tipo == MSG_CONFIG_PARAMETROS && dataLen == sizeof(PacoteConfiguracao)) {
        PacoteConfiguracao cfg;
        memcpy(&cfg, data, sizeof(cfg));

        Serial.printf("\n[ESP-NOW Periferico] Pacote de Configuracao recebido do Central!\n");
        Serial.printf("  Alvo: \"%s\" | Novo Intervalo: %d min | Novo Modo: \"%s\"\n", 
                      cfg.targetNodeId, cfg.novoIntervaloMin, cfg.novoModo);

        memcpy(&instancia->ultimaConfiguracao, &cfg, sizeof(cfg));
        instancia->configRecebidaFlag = true;

        if (instancia->onConfigCallback) {
            instancia->onConfigCallback(cfg);
        }
    }
}

#if defined(ESP_IDF_VERSION_MAJOR) && (ESP_IDF_VERSION_MAJOR >= 5)
void EspNowPeriferico::onDataSend(const wifi_tx_info_t* tx_info, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        Serial.println("[ESP-NOW Periferico] ACK de envio confirmado com sucesso.");
    } else {
        Serial.println("[ESP-NOW Periferico] Alerta: Envio falhou.");
    }
}
#else
void EspNowPeriferico::onDataSend(const uint8_t* mac_addr, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        Serial.println("[ESP-NOW Periferico] ACK de envio confirmado com sucesso.");
    } else {
        Serial.println("[ESP-NOW Periferico] Alerta: Envio falhou.");
    }
}
#endif
