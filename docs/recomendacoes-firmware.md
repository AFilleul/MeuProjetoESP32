# Recomendações críticas para o firmware

Avaliação parcial, por leitura do código no commit `eb8cf9a` da `main`. Não foi compilado nem testado em placa. Esta lista traz só os pontos que podem fazer o sistema falhar em campo; melhorias de organização ficam para outra etapa.

## 1. Canal de rádio: nós e Central podem não se encontrar

**O que acontece.** Quando o Central conecta ao roteador, o ESP-NOW passa a usar o canal do roteador (`EspNowCentral.cpp:34-37`, chamado com `configurarWifi = false` em `MeuProjetoESP32.ino:285`). Os nós usam sempre o canal fixo `ESPNOW_WIFI_CHANNEL = 1` (`EspNowPeriferico.cpp:61-63`). O beacon já carrega o canal do Central (`PacoteBroadcastBeacon.canalWifi`), mas o nó só copia o MAC e ignora o canal (`EspNowPeriferico.cpp:312-320`).

**Impacto.** Se o roteador estiver em qualquer canal diferente de 1, nenhum pacote chega e não há mensagem de erro útil. O sistema só funciona no canal 1 ou no modo AP de reserva (`softAP` no canal 1). Um roteador com canal automático pode mudar de canal depois de meses de funcionamento.

**Recomendação.**
- No nó, ao receber um beacon, adotar `beacon.canalWifi` (e gravar na memória RTC junto com o MAC do Central).
- No nó, quando o envio falhar ou o Central não responder, varrer os canais 1 a 13 ouvindo o beacon (o Central emite um a cada 1,5 s) e regravar o canal.
- Alternativa mais simples: fixar o canal do roteador em 1 e documentar isso como requisito de instalação.

**Como verificar.** Colocar o roteador em outro canal (por exemplo 6) e confirmar que um nó que reinicia volta a entregar telemetria.

## 2. Acesso concorrente ao `DeviceManager` sem proteção

**O que acontece.** Os callbacks do ESP-NOW rodam na task de Wi-Fi. O callback de telemetria cadastra e atualiza dispositivos (`MeuProjetoESP32.ino:288-314`, `registerDevice`, `updateTelemetry`). Ao mesmo tempo, `loop()` e os handlers HTTP percorrem e alteram o mesmo `std::vector<Device>` (`handleGetStatus`, `handlePostConfig`, `handleDeleteDevice`, retry em `loop()`). Não há mutex nem fila.

**Impacto.** Uma telemetria que cadastra um nó novo (`emplace_back`, possível realocação) enquanto `/api/status` percorre a lista pode invalidar ponteiros e reiniciar o Central. É um defeito intermitente, difícil de reproduzir e de depurar.

**Recomendação.**
- No callback, só copiar o pacote para uma fila do FreeRTOS (`xQueueSend` com tempo de espera zero) e sair.
- No `loop()`, esvaziar a fila e aplicar o pacote ao `DeviceManager`. Assim só uma task toca nos dados.
- Se preferir manter o desenho atual, proteger todos os acessos ao vetor com um `SemaphoreHandle_t`.
- `getAndClearNotifications()` e `pendingNotifications` têm o mesmo problema.

**Como verificar.** Teste de estresse: vários nós enviando telemetria a cada segundo enquanto a interface consulta `/api/status` em laço, por algumas horas, sem reinício.

## 3. Configuração do usuário só existe na RAM do Central

**O que acontece.** Nome, intervalo, modo, unidade, fonte de energia e capacidade da bateria ficam no `std::vector<Device>` (`DeviceManager.cpp`). Não há uso de NVS, `Preferences`, LittleFS ou EEPROM em nenhum arquivo.

**Impacto.** Queda de energia, brownout ou reset do Central apagam todos os dispositivos e as configurações. Num gateway alimentado por painel solar, isso vai acontecer. Os nós em Deep Sleep voltam a reportar com o nome e o modo guardados neles, mas o Central não sabe mais a configuração desejada pelo usuário, nem os campos que só ele guarda (unidade, fonte, capacidade).

**Recomendação.**
- Gravar no NVS (`Preferences`) os campos que o usuário define: id, nome, intervalo, modo, unidade, fonte de energia, capacidade.
- Gravar quando o dado mudar, não a cada telemetria, para não desgastar a flash.
- Restaurar a lista no `setup()` antes de abrir o ESP-NOW. Marcar os nós restaurados como OFFLINE até o primeiro sinal.

**Como verificar.** Configurar um nó, reiniciar o Central, e confirmar que nome, modo e intervalo continuam na interface sem nova ação do usuário.

## 4. Sem autenticação nem criptografia

**O que acontece.**
- Todos os peers ESP-NOW são criados com `encrypt = false` (`EspNowCentral.cpp:73-83` e as chamadas a `registrarPeer(..., false)`; `EspNowPeriferico.cpp:90-99`).
- Qualquer pacote `MSG_DADOS_SENSORES` com o tamanho certo cadastra um dispositivo novo automaticamente (`MeuProjetoESP32.ino:296-298`).
- A API REST responde com `Access-Control-Allow-Origin: *` e sem nenhuma credencial (`MeuProjetoESP32.ino:33-37`, rotas em `336-341`). Qualquer pessoa na mesma rede pode alterar a configuração de um nó ou apagá-lo.

**Impacto.** Um dispositivo estranho pode injetar leituras falsas ou reconfigurar nós (por exemplo, mudar o intervalo para 1 minuto e esgotar a bateria). Em um sítio com Wi-Fi compartilhado, a API também fica exposta.

**Recomendação.**
- Ativar a criptografia do ESP-NOW (PMK e LMK compartilhados, guardados em `secrets.h`). O ESP-NOW limita a quantidade de pares criptografados (cerca de 6 por padrão; confira na versão do core usada). Se esse limite não servir, assinar cada pacote no nível da aplicação (um HMAC curto com segredo compartilhado, com contador para evitar repetição).
- Cadastrar nós novos só durante a janela de pareamento ou a partir de uma lista de permitidos, e não a qualquer telemetria recebida.
- Exigir um token nas rotas `POST` da API (cabeçalho com valor definido em `secrets.h`) e restringir o CORS à origem da interface.

**Como verificar.** Enviar um pacote forjado de outro ESP32 e confirmar que o Central o descarta; chamar `POST /api/config` sem token e confirmar a recusa.

## Ordem sugerida

1. Canal de rádio (item 1): é o que mais provavelmente impede o sistema de funcionar na instalação.
2. Concorrência (item 2) e persistência (item 3): estabilidade do gateway ao longo do tempo.
3. Segurança (item 4): antes de expor o sistema fora de uma rede isolada.
