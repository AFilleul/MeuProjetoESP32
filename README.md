# MeuProjetoESP32 — Sítio Online

Monitoramento agrícola com ESP32 usando **ESP-NOW** (sem roteador entre os nós) e uma **API REST + interface web** no Central.

> **Fluxos interativos:** abra [`docs/index.html`](docs/index.html) (publicado em https://br-tavares.github.io/MeuProjetoESP32/) no navegador para ver os grafos de cada firmware (vis.js, funciona offline). Clique em um bloco para ver o que faz e a linha do código. Os diagramas Mermaid abaixo são a versão renderizada pelo GitHub.
>
> **Prova de conceito (branch `com-mini-pc`):** o plano de trabalho atual, com o que é núcleo e o que é supérfluo no firmware, está em [`docs/index-poc.html`](docs/index-poc.html). Os Mermaid abaixo ainda descrevem o Central da `main`, com servidor web.

### Abrindo e atualizando os fluxos

**Abrir (Windows):** dê duplo clique em [`docs/abrir-firmwares.bat`](docs/abrir-firmwares.bat). Ele sobe um servidor local (só PowerShell, sem instalar nada) e abre a página no navegador. Feche a janela do console para parar.

**Atualizar:** os grafos vêm de [`docs/data/firmwares.json`](docs/data/firmwares.json) (nós, arestas, texto de cada bloco). Edite o JSON e atualize o navegador (F5). Um grafo novo em `graphs` aparece sozinho na seção "Outros grafos".

**Abrir sem servidor:** navegadores bloqueiam `fetch()` de arquivos locais, então um duplo clique direto em `index.html` usa a cópia `docs/data/firmwares.js`. Para regenerá-la depois de editar o JSON, rode `node tools/gen_firmwares_js.mjs` (Node 18+), que também valida o JSON: ids repetidos, arestas para nós inexistentes, `kind` inválido. Com `--check` ele só confere se o `.js` está em dia. Quem usa o `.bat` não precisa disso.

## Visão geral dos firmwares

O repositório tem 4 sketches `.ino` principais, em dois papéis:

| Papel | Sketch | Energia | Comportamento |
| :--- | :--- | :--- | :--- |
| **Gateway / Central** | [`MeuProjetoESP32.ino`](MeuProjetoESP32.ino) | Sempre ligado (rádio ativo, servidor HTTP) | Recebe telemetria via ESP-NOW, serve a API REST, despacha configurações |
| **Nó edge (sensor de chuva)** | [`esps_perifericos/ESP_Periferico_Chuva/`](esps_perifericos/ESP_Periferico_Chuva/ESP_Periferico_Chuva.ino) | Bateria, Deep Sleep | Acorda, lê o sensor, envia, dorme |
| **Nó edge (template genérico)** | [`esps_perifericos/ESP_Periferico_Base/`](esps_perifericos/ESP_Periferico_Base/ESP_Periferico_Base.ino) | Bateria, Deep Sleep | Mesmo ciclo; lê umidade do solo (ADC) |
| Exemplo | [`exemplos/ESP_Central_EspNow/`](exemplos/ESP_Central_EspNow/ESP_Central_EspNow.ino) | — | Central mínimo, só ESP-NOW (sem HTTP) |

Os dois sketches edge têm o mesmo fluxo; só mudam a leitura do sensor e a função de bateria.

## Arquitetura

```mermaid
flowchart LR
    subgraph EDGE["Nós edge (bateria, Deep Sleep)"]
        N1["ESP_Periferico_Chuva<br/>sensor MH-RD"]
        N2["ESP_Periferico_Base<br/>umidade do solo"]
    end
    subgraph GW["Gateway (MeuProjetoESP32.ino)"]
        C["ESP32 Central<br/>ESP-NOW + WebServer"]
    end
    W["Interface web<br/>web/index.html"]

    N1 -- "MSG_DADOS_SENSORES (unicast)" --> C
    N2 -- "MSG_DADOS_SENSORES (unicast)" --> C
    C -- "MSG_BROADCAST_BEACON (a cada 1,5 s)" --> N1
    C -- "MSG_CONFIG_PARAMETROS" --> N1
    N1 -- "MSG_ACK_CONFIG" --> C
    W -- "HTTP: /api/status, /api/config, /api/device/delete" --> C
```

## Fluxograma: nó edge (`ESP_Periferico_Chuva.ino` e `ESP_Periferico_Base.ino`)

Estado persistente em memória RTC (sobrevive ao Deep Sleep): `bootCount`, `tempoUptimeAcumuladoSegundos`, `rtcIntervaloMin`, `rtcModoOperacao`, `rtcNodeName`, `rtcMacCentral`, `rtcCentralConhecido`.

```mermaid
flowchart TD
    A([Boot / acorda do Deep Sleep]) --> B["setup: Serial, sensor begin (só Chuva)"]
    B --> C{"espNowNode.iniciar OK?"}
    C -- não --> C1["entrarDeepSleep(intervalo)"]
    C -- sim --> D["Registra callback de configuração"]
    D --> E{"MAC do Central salvo na RTC?"}
    E -- sim --> E1["restaurarCentralMac"]
    E -- não --> F
    E1 --> F{"isHardReset?"}

    %% Caso A
    F -- "sim (liga na energia / botão reset)" --> A1["bootCount = 1"]
    A1 --> A2["aguardarBeaconCentral 8 s"]
    A2 --> A3["executarJanelaPareamento 60 s<br/>envia telemetria e recebe configuração"]
    A3 --> A4["acumula uptime"]
    A4 --> A5{"modo = ONLINE?"}
    A5 -- sim --> LOOP
    A5 -- não --> A6["aguardarPrioridadeConfiguracao 250 ms"]
    A6 --> SLEEP

    %% Caso B
    F -- "não (despertar programado)" --> B1["bootCount++"]
    B1 --> B2{"Central conhecido?"}
    B2 -- não --> B3["aguardarBeaconCentral 4 s"]
    B2 -- sim --> B4
    B3 --> B3a{"achou?"}
    B3a -- não --> B9["Central indisponível neste ciclo"]
    B3a -- sim --> B4["salva MAC na RTC"]
    B4 --> B5["escuta configuração 150 ms"]
    B5 --> B6["lê sensor + bateria<br/>enviarDadosUnicast"]
    B6 --> B7{"recebeu configuração<br/>em 600 ms?"}
    B7 -- sim --> B8["mede de novo e reenvia telemetria<br/>com o estado novo"]
    B7 -- não --> B10
    B8 --> B10["acumula uptime"]
    B9 --> B10
    B10 --> B11{"modo = ONLINE?"}
    B11 -- sim --> LOOP
    B11 -- não --> SLEEP

    SLEEP["entrarDeepSleep(rtcIntervaloMin)"] --> A

    %% Loop
    subgraph LOOP["loop(): modo ONLINE contínuo / LIGHT_SLEEP"]
        L1["descobre Central se necessário"] --> L2["escuta configuração 50 ms<br/>se aplicou: envia telemetria de confirmação"]
        L2 --> L3{"passou o intervalo<br/>ou primeiro envio?"}
        L3 -- sim --> L4["lê sensor, envia telemetria,<br/>janela de 500 ms para configuração"]
        L3 -- não --> L5
        L4 --> L5{"modo atual?"}
        L5 -- DEEP_SLEEP --> L6["aguarda 200 ms e vai ao SLEEP"]
        L5 -- LIGHT_SLEEP --> L7["timer wakeup + esp_light_sleep_start<br/>ultimoEnvio = 0"]
        L5 -- ONLINE --> L8["delay 20 ms"]
        L7 --> L1
        L8 --> L1
    end
    L6 --> SLEEP

    subgraph CB["Callback aplicarNovaConfiguracao"]
        K1["atualiza intervalo, modo e nome na RTC"] --> K2["enviarAckConfiguracao ao Central"]
    end
```

Particularidades do `ESP_Periferico_Chuva`: antes de ler, o BJT NPN liga o sensor por 50 ms (média de 8 amostras ADC) e desliga em seguida, contra oxidação das trilhas. O `ESP_Periferico_Base` liga o sensor por um pino VCC (`PIN_SENSOR_VCC`, se definido), aguarda 30 ms, lê o ADC e desliga.

## Fluxograma: gateway (`MeuProjetoESP32.ino`)

```mermaid
flowchart TD
    S([Boot]) --> S1["Serial 115200"]
    S1 --> S2["wifiManager.connect 15 s"]
    S2 --> S3{"Wi-Fi OK?"}
    S3 -- sim --> S4["Imprime IP"]
    S3 -- não --> S5["WIFI_AP_STA + softAP<br/>SitioOnline_ESP32, canal ESP-NOW"]
    S4 --> S6
    S5 --> S6["espNowCentral.iniciar"]
    S6 --> S7["Registra callback de telemetria<br/>e callback de ACK de configuração"]
    S7 --> S8["Registra rotas REST e server.begin"]
    S8 --> LP

    subgraph LP["loop() a cada ~10 ms"]
        P1["server.handleClient"] --> P2{"a cada 250 ms:<br/>algum nó com config pendente,<br/>MAC conhecido e visto há menos de 15 s?"}
        P2 -- sim --> P3["reenvia configuração (mín. 450 ms entre tentativas)"]
        P2 -- não --> P4
        P3 --> P4{"a cada 1,5 s"}
        P4 --> P5["enviarBroadcastBeacon CENTRAL_SITIO"]
        P5 --> P6["wifiManager.handleReconnect"]
        P6 --> P7{"a cada 2 s"}
        P7 --> P8["checkScheduleTimeout 30 s<br/>marca nós atrasados como OFFLINE"]
        P8 --> P1
    end

    subgraph RX["Callback: telemetria recebida"]
        R1["registra nó se novo, guarda MAC"] --> R2["updateTelemetry"]
        R2 --> R3{"há configuração pendente?"}
        R3 -- sim --> R4["enviarConfiguracao imediato"]
        R3 -- não --> R5([fim])
        R4 --> R5
    end

    subgraph ACK["Callback: ACK de configuração"]
        K0{"sucesso?"} -- sim --> K3["clearPendingConfig + notificação"]
        K0 -- não --> K4([mantém pendente])
    end

    subgraph API["REST API"]
        H1["GET /api/status → lista de nós, notificações, janela de recepção"]
        H2["POST /api/config → unit / powerSource / batteryCapacity ficam no Central;<br/>interval / mode / name viram configuração via ESP-NOW"]
        H3["POST /api/device/delete → removeDevice"]
    end
```

## Observações sobre o estado atual do código

Pontos observados durante a leitura, úteis para a próxima etapa do projeto:

1. **O gateway não tem gestão de energia solar.** `MeuProjetoESP32.ino` roda sempre ligado (Wi-Fi/AP + WebServer + beacon a cada 1,5 s). Para operar com painel solar e bateria, faltam leitura de tensão da bateria/painel, política de sono ou de duty-cycle do rádio e persistência dos dados, o que ainda não existe no código.
2. **Janela de recepção só informativa.** `CICLO_RECEPCAO_MS` (60 s) e `DURACAO_JANELA_MS` (10 s) são usados só para exibir o cronograma em `/api/status`. O rádio do Central nunca é desligado fora da janela.
3. **Bateria simulada nos nós.** `lerBateria()` devolve valor fixo (3980 mV), então a porcentagem de bateria ainda não reflete leitura real de ADC. A temperatura também é constante (25,0 / 24,8).
4. **Código duplicado.** Os dois sketches edge (`Chuva` e `Base`) diferem apenas no sensor, e `EspNowPeriferico` e `ProtocoloMensagens.h` existem em três cópias (raiz, `Base`, `Chuva`). Uma biblioteca compartilhada evitaria divergência.
5. **`secrets.h`** não está no repositório (só `secrets.example.h`). Copie e preencha antes de compilar o Central.

## Como compilar

1. Central: copie `secrets.example.h` para `secrets.h`, preencha o Wi-Fi e grave `MeuProjetoESP32.ino`.
2. Nós edge: abra a pasta do periférico no Arduino IDE, ajuste `config_periferico.h` (`NODE_ID`, modo, fonte) e grave.
3. Todos os nós usam o canal Wi-Fi definido em `ESPNOW_WIFI_CHANNEL` (1).
