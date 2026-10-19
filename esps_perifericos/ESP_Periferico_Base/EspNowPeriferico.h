#ifndef ESP_NOW_PERIFERICO_H
#define ESP_NOW_PERIFERICO_H

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_system.h>
#include <functional>
#if __has_include("ProtocoloMensagens.h")
#include "ProtocoloMensagens.h"
#else
#include "../../ProtocoloMensagens.h"
#endif

class EspNowPeriferico {
public:
    using ConfigCallback = std::function<void(const PacoteConfiguracao& config)>;

    EspNowPeriferico();
    ~EspNowPeriferico();

    // Inicializa o rádio Wi-Fi e ESP-NOW no canal desejado
    bool iniciar(uint8_t canal = ESPNOW_WIFI_CHANNEL);

    // Identificação do tipo de reinicialização (Hard Reset vs Retorno de Deep Sleep)
    static bool isHardReset();
    static String getMotivoResetString();

    // Aguarda ativamente o recebimento do pacote de Broadcast do Central até timeoutMs
    bool aguardarBeaconCentral(unsigned long timeoutMs = 8000);

    // Restaura o MAC do Central salvo na memória RTC (evita perda de conexão pós-Deep Sleep)
    bool restaurarCentralMac(const uint8_t* macSalvo);

    // Janela prioritária de escuta de configurações (tem prioridade sobre envio e sobre entrada em sleep)
    bool aguardarPrioridadeConfiguracao(unsigned long janelaMs = 400);

    // Envia os dados do nó via Unicast diretamente para o Central descoberto
    bool enviarDadosUnicast(const PacoteDadosSensores& dados);

    // Envia confirmação (ACK) de configuração aplicada de volta ao Central
    bool enviarAckConfiguracao(const char* nodeId, bool sucesso, uint16_t intervalo, const char* modo);

    // Mantém o nó online por 1 minuto (60.000 ms) pós-hard reset para pareamento e recebimento de configs
    void executarJanelaPareamento(unsigned long duracaoMs, 
                                  std::function<PacoteDadosSensores()> geradorTelemetria,
                                  std::function<void(const PacoteConfiguracao&)> onConfigRecebida = nullptr);

    // Registra callback acionado quando o Central enviar um pacote de configuração
    void definirCallbackConfiguracao(ConfigCallback callback);

    // Coloca o ESP32 em Deep Sleep por N minutos
    void entrarDeepSleep(uint32_t minutos);

    // Métodos auxiliares
    bool centralDescoberto() const;
    void getCentralMac(uint8_t* outMac) const;
    String getCentralMacString() const;
    String getLocalMacString() const;
    bool temConfiguracaoRecebida() const;
    void limparFlagConfiguracao();

private:
    uint8_t canalWifi;
    bool centralEncontrado;
    uint8_t macCentral[6];
    volatile bool beaconRecebidoFlag;
    volatile bool configRecebidaFlag;
    volatile unsigned long ultimoConfigRecebidoMs;
    PacoteConfiguracao ultimaConfiguracao;

    ConfigCallback onConfigCallback;

    // Registra um peer na tabela ESP-NOW
    bool adicionarPeer(const uint8_t* macAddr, uint8_t canal = ESPNOW_WIFI_CHANNEL, bool encrypt = false);

    // Callbacks C nativos do ESP-NOW
    #if defined(ESP_IDF_VERSION_MAJOR) && (ESP_IDF_VERSION_MAJOR >= 5)
    static void onDataReceive(const esp_now_recv_info_t* recvInfo, const uint8_t* data, int dataLen);
    static void onDataSend(const wifi_tx_info_t* tx_info, esp_now_send_status_t status);
    #else
    static void onDataReceive(const uint8_t* mac_addr, const uint8_t* data, int dataLen);
    static void onDataSend(const uint8_t* mac_addr, esp_now_send_status_t status);
    #endif

    static EspNowPeriferico* instancia;
};

#endif // ESP_NOW_PERIFERICO_H
