# ESPs Periféricos (Nós Sensores / Clientes)

Esta pasta contém o firmware dos **microcontroladores ESP32 secundários (nós periféricos)** que se comunicam via rede Wi-Fi com o **ESP32 Central (Servidor REST)**.

## Arquitetura de Comunicação

```text
[ Sensor (ex: MH-RD) ]
        │ (GPIO)
        ▼
[ ESP32 Periférico / Nó ] ──(HTTP POST /api/data)──▶ [ ESP32 Central (Servidor REST) ]
   - Lê sensores periodicamente
   - Opera em Deep Sleep ou Light Sleep para poupar bateria
   - Registra seu ID, tipo de sensor e nível de bateria
```

## Estrutura dos Módulos Periféricos

- `ESP_Periferico_Base/`: Template modular básico para nós com suporte a leitura analógica/digital e modos de baixo consumo (`Deep Sleep`, `Light Sleep`, `Online`).
- `config_periferico.h`: Definições de ID do nó, pino do sensor, intervalo e endereço do ESP Central.
