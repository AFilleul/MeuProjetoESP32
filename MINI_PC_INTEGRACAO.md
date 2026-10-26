# Integração ESP32 Central <-> Mini PC (Modo Offline com SQLite)

Esta ramificação (`com-mini-pc`) prepara o firmware do **ESP32 Central** (`MeuProjetoESP32.ino`) para atuar exclusivamente como **coordenador do rádio ESP-NOW** e **cliente HTTP ativo (Push)** para um servidor local executado em um Mini PC na fazenda.

---

## 1. Configuração no `secrets.h`

Copie `secrets.example.h` para `secrets.h` e preencha os dados da rede Wi-Fi local e do Mini PC:

```cpp
const char* const WIFI_SSID = "NOME_DA_SUA_REDE";
const char* const WIFI_PASSWORD = "SUA_SENHA_AQUI";

#define MINI_PC_CONFIGURED 1
const char* const MINI_PC_IP = "192.168.1.100"; // IP fixo do Mini PC na LAN da fazenda
const uint16_t MINI_PC_PORT = 8000;             // Porta HTTP da API no Mini PC
```

- **Timeout máximo de conexão e leitura HTTP**: `1500 ms` (`MINI_PC_HTTP_TIMEOUT_MS`).
- **Não-bloqueante para o rádio**: O callback ESP-NOW enfileira os pacotes em uma fila FreeRTOS (`filaPushMiniPC`) processada por uma task dedicada no Core 1 (`taskDespachoMiniPC`), mantendo a escuta ESP-NOW no Core 0 sempre livre.
- **Reconexão automática**: O `WifiManager` monitora e restabelece a conexão Wi-Fi automaticamente em background sem interromper o rádio ESP-NOW.

---

## 2. Contrato HTTP POST (ESP32 Central -> Mini PC)

Sempre que chega um pacote `MSG_DADOS_SENSORES` de qualquer nó periférico, o ESP32 Central dispara:

- **Método e Endpoint**: `POST http://<MINI_PC_IP>:<MINI_PC_PORT>/api/telemetria`
- **Headers**: `Content-Type: application/json`, `Accept: application/json`
- **Payload JSON enviado**:

```json
{
  "nodeId": "esp-1",
  "nodeName": "Solo Setor A",
  "sensorUmidade": 62.50,
  "bateriaPct": 94,
  "modoOperacao": "DEEP_SLEEP",
  "tempoUptimeSegundos": 3600,
  "macAddr": "AA:BB:CC:DD:EE:FF"
}
```

---

## 3. Contrato da Resposta HTTP 200 (Mini PC -> ESP32 Central -> Periférico)

O Mini PC grava a telemetria no banco SQLite e responde com `HTTP 200 OK` (ou `201 Created`).

### Caso A: Sem comandos pendentes para o nó
```json
{
  "status": "ok",
  "hasPendingConfig": false
}
```
*(Também aceita `{}` ou `{"status": "ok"}`).*

### Caso B: Com comandos/configurações pendentes para o nó
Assim que o ESP32 Central lê qualquer um dos formatos JSON abaixo no corpo da resposta `HTTP 200`, ele monta imediatamente o `PacoteConfiguracao` (`MSG_CONFIG_PARAMETROS`) e transmite via **ESP-NOW Unicast** para o periférico (mantendo reenvio automático até receber o `MSG_ACK_CONFIG`):

#### Formato 1 — Objeto `pendingConfig` ou `config` (Recomendado)
```json
{
  "status": "ok",
  "hasPendingConfig": true,
  "pendingConfig": {
    "nodeId": "esp-1",
    "novoIntervaloMin": 30,
    "novoModo": "DEEP_SLEEP",
    "novoNome": "Estufa Norte"
  }
}
```

#### Formato 2 — Campos diretos na raiz do JSON
```json
{
  "status": "ok",
  "novoIntervaloMin": 15,
  "novoModo": "LIGHT_SLEEP",
  "novoNome": "Sensor Chuva"
}
```
*(Chaves equivalentes também suportadas automaticamente: `interval` / `intervalo` / `intervaloMin`, `mode` / `modo`, e `name` / `nome` dentro de `config`/`pendingConfig`).*

#### Formato 3 — Lista de comandos (`commands`)
```json
{
  "status": "ok",
  "commands": [
    { "field": "interval", "value": 20 },
    { "field": "mode", "value": "ONLINE" },
    { "field": "name", "value": "Setor Sul" }
  ]
}
```

### Valores aceitos para modo de operação
- `"ONLINE"`
- `"LIGHT_SLEEP"` (ou `"LIGHT SLEEP"`)
- `"DEEP_SLEEP"` (ou `"DEEP SLEEP"`)
