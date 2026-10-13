#ifndef ESP_NOW_PERIFERICO_H
#define ESP_NOW_PERIFERICO_H

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <functional>
#include "../../ProtocoloMensagens.h"

class EspNowPeriferico {
public:
    EspNowPeriferico();
    ~EspNowPeriferico();

    // Inicializa o rádio Wi-Fi e ESP-NOW no canal desejado
    bool iniciar(uint8_t canal = ESPNOW_WIFI_CHANNEL);

    // Aguarda ativamente o recebimento do pacote de Broadcast do Central até timeoutMs
    // Retorna true se o Broadcast foi recebido e o MAC do Central foi descoberto
    bool aguardarBeaconCentral(unsigned long timeoutMs = 8000);

    // Envia os dados do nó via Unicast diretamente para o Central descoberto
    bool enviarDadosUnicast(const PacoteDadosSensores& dados);

    // Coloca o ESP32 em Deep Sleep por N minutos
    void entrarDeepSleep(uint32_t minutos);

    // Métodos auxiliares
    bool centralDescoberto() const;
    void getCentralMac(uint8_t* outMac) const;
    String getCentralMacString() const;
    String getLocalMacString() const;

private:
    uint8_t canalWifi;
    bool centralEncontrado;
    uint8_t macCentral[6];
    volatile bool beaconRecebidoFlag;

    // Registra um peer na tabela ESP-NOW
    bool adicionarPeer(const uint8_t* macAddr, uint8_t canal = ESPNOW_WIFI_CHANNEL, bool encrypt = false);

    // Callbacks C nativos do ESP-NOW
    #if defined(ESP_IDF_VERSION_MAJOR) && (ESP_IDF_VERSION_MAJOR >= 5)
    static void onDataReceive(const esp_now_recv_info_t* recvInfo, const uint8_t* data, int dataLen);
    #else
    static void onDataReceive(const uint8_t* mac_addr, const uint8_t* data, int dataLen);
    #endif

    static void onDataSend(const uint8_t* mac_addr, esp_now_send_status_t status);

    static EspNowPeriferico* instancia;
};

#endif // ESP_NOW_PERIFERICO_H
