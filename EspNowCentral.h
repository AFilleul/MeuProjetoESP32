#ifndef ESP_NOW_CENTRAL_H
#define ESP_NOW_CENTRAL_H

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <functional>
#include "ProtocoloMensagens.h"

class EspNowCentral {
public:
    // Callbacks C++
    using SensorDataCallback = std::function<void(const uint8_t* mac, const PacoteDadosSensores& dados)>;
    using AckConfigCallback  = std::function<void(const uint8_t* mac, const PacoteAckConfig& ack)>;

    EspNowCentral();
    ~EspNowCentral();

    // Inicializa Wi-Fi e configura o ESP-NOW (permite usar Wi-Fi pré-existente)
    bool iniciar(uint8_t canal = ESPNOW_WIFI_CHANNEL, bool configurarWifi = true);

    // Envia um pacote de Broadcast (FF:FF:FF:FF:FF:FF) anunciando o Central
    bool enviarBroadcastBeacon(const char* identificador = "CENTRAL_SITIO");

    // Envia uma configuração diretamente (Unicast) para um periférico
    bool enviarConfiguracao(const uint8_t* macPeriferico, const PacoteConfiguracao& config);

    // Registra callbacks do usuário
    void definirCallbackRecepcao(SensorDataCallback callback);
    void definirCallbackAckConfig(AckConfigCallback callback);

    // Adiciona ou atualiza um peer no ESP-NOW
    bool registrarPeer(const uint8_t* macAddr, uint8_t canal = ESPNOW_WIFI_CHANNEL, bool encrypt = false);

    // Retorna endereço MAC do Central formatado em String
    String getMacAddressString() const;

private:
    uint8_t canalWifi;
    uint8_t broadcastMac[6];
    SensorDataCallback onSensorData;
    AckConfigCallback onAckConfig;

    // Callbacks C nativos do ESP-NOW
    #if defined(ESP_IDF_VERSION_MAJOR) && (ESP_IDF_VERSION_MAJOR >= 5)
    static void onDataReceive(const esp_now_recv_info_t* recvInfo, const uint8_t* data, int dataLen);
    #else
    static void onDataReceive(const uint8_t* mac_addr, const uint8_t* data, int dataLen);
    #endif

    static void onDataSend(const uint8_t* mac_addr, esp_now_send_status_t status);

    // Ponteiro estático para a instância única (Singleton pattern para callbacks C)
    static EspNowCentral* instancia;
};

#endif // ESP_NOW_CENTRAL_H
