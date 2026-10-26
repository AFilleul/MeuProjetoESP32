#ifndef SECRETS_H
#define SECRETS_H

// Renomeie este arquivo para "secrets.h" e coloque suas credenciais reais abaixo
const char* const WIFI_SSID = "NOME_DA_SUA_REDE";
const char* const WIFI_PASSWORD = "SUA_SENHA_AQUI";

// Configuracao do Servidor Local no Mini PC da Fazenda (Modo Offline / SQLite)
#define MINI_PC_CONFIGURED 1
const char* const MINI_PC_IP = "192.168.1.100"; // Substitua pelo IP fixo do Mini PC na rede local
const uint16_t MINI_PC_PORT = 8000;             // Porta HTTP do servidor no Mini PC (ex.: 8000)

#endif // SECRETS_H
