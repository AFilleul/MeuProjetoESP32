#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>

class WifiManager {
private:
    const char* ssid;
    const char* password;
    unsigned long lastReconnectAttempt;
    unsigned long reconnectInterval;

public:
    // Construtores
    WifiManager();
    WifiManager(const char* ssid, const char* password);

    // Configura credenciais caso não tenham sido passadas no construtor
    void setCredentials(const char* ssid, const char* password);

    // Inicializa e conecta ao Wi-Fi com timeout opcional (padrão 15 segundos)
    bool connect(unsigned long timeoutMs = 15000);
    bool connect(const char* ssid, const char* password, unsigned long timeoutMs = 15000);

    // Verifica se o ESP32 continua conectado à rede
    bool isConnected() const;

    // Monitora a conexão de forma não-bloqueante e tenta reconectar caso caia (chamar no loop)
    void handleReconnect(unsigned long intervalMs = 10000);

    // Métodos utilitários
    IPAddress getLocalIP() const;
    int8_t getRSSI() const;
    void disconnect();
};

#endif // WIFI_MANAGER_H
