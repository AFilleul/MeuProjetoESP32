# STATUS — Sítio Online (ESP32)

**Data:** 2026-10-20 · **Último commit:** `76dc838` · **Estado:** protótipo funcional, não validado em campo

## 1. Onde estamos (verificado por leitura do código)

| Parte do núcleo | Estado | Observação |
|---|---|---|
| Protocolo ESP-NOW (`ProtocoloMensagens.h`) | Funciona | 3 cópias idênticas hoje (raiz, `Base`, `Chuva`) |
| Central (`MeuProjetoESP32.ino` + módulos) | Funciona | Configuração só na RAM; sem autenticação |
| Nó de chuva (`ESP_Periferico_Chuva`) | Funciona | Bateria e temperatura **simuladas** (valores fixos) |
| Nó molde (`ESP_Periferico_Base`) | Funciona | Leitura de umidade do solo via ADC |
| Interface web (`web/`) | Funciona | Tem modo simulador para estudar sem placa |
| Testes automatizados | Não existem | Verificação é manual, na placa |

Riscos críticos já descritos em [`docs/recomendacoes-firmware.md`](docs/recomendacoes-firmware.md):
canal de rádio fixo em 1, acesso concorrente ao `DeviceManager`, configuração perdida no reset, ausência de
criptografia/autenticação.

## 2. Próximos passos (em ordem de prioridade)

- [ ] **P1** Canal de rádio: nó adota `beacon.canalWifi` (recomendação 1). *Sem isso, o sistema pode não funcionar fora do canal 1.*
- [ ] **P2** Persistir configuração do Central no NVS com `Preferences` (recomendação 3).
- [ ] **P3** Fila FreeRTOS entre callback ESP-NOW e `loop()` (recomendação 2).
- [ ] **P4** Leitura real de bateria por ADC nos nós.
- [ ] **P5** Token nas rotas `POST` e criptografia ESP-NOW (recomendação 4).

## 3. Trilha de capacitação (um passo por vez)

| Passo | Aprende | Entrega na placa |
|---|---|---|
| 1 | Abrir o visualizador (`docs/abrir-firmwares.bat`) e percorrer o fluxo do nó | Explicar com suas palavras o ciclo acorda → envia → dorme |
| 2 | `ProtocoloMensagens.h` | Listar as 5 mensagens e quem envia cada uma |
| 3 | Gravar Central + nó de chuva | Ver a leitura chegar na interface web |
| 4 | Fazer o **P1** | Trocar o canal do roteador e o nó continuar entregando |
| 5 | Fazer o **P2** | Reiniciar o Central sem perder o nome do nó |
| 6 | Registrar no `CHANGELOG.md` e atualizar este `STATUS.md` | Hábito de governança consolidado |

Os passos P3 a P5 entram depois que os passos 1–6 estiverem confortáveis.

## 4. Recomendação de limpeza (redundante ou sem uso para a proposta técnica)

> Nada foi apagado. Decidir item a item; cada remoção em um commit `refactor:` ou `chore:` próprio.

| Item | Situação | Recomendação |
|---|---|---|
| `TecladoMatriz.cpp` / `.h` | Não é incluído por nenhum sketch; a versão com teclado vive na branch `com-teclado-4x4` | **Remover** da `main` |
| `.vscode/launch.json`, `.vscode/settings.json`, `.vscode/c_cpp_properties.json` | Configuração de depurador gcc/gdb para PC, com caminhos de outra máquina (`C:/Users/DELL/...`); não serve para Arduino/ESP32 | **Remover** e adicionar `.vscode/` ao `.gitignore` |
| `exemplos/ESP_Central_EspNow/` | Central reduzido que duplica `EspNowCentral` + `DeviceManager` e diverge (beacon a cada 3 s vs 1,5 s) | **Remover**, ou manter só se for usado como exercício da trilha (passo 2) — nesse caso, dizer isso no README |
| Fluxogramas Mermaid do `README.md` | Repetem o conteúdo do visualizador (`docs/firmwares.html` + `docs/data/firmwares.json`); duas fontes para manter em sincronia | **Reduzir**: o visualizador é a interface principal de estudo. No README, deixar só o diagrama de arquitetura e um link para o visualizador. Os fluxos detalhados passam a ter uma fonte única: `firmwares.json` |
| Cópias de `ProtocoloMensagens.h` e `EspNowPeriferico.*` em `Base` e `Chuva` | Redundância exigida pelo Arduino IDE (pasta autocontida) | **Manter por enquanto** com a regra 2 do `AGENTS.md`; mais tarde, avaliar biblioteca em `libraries/` |
| `web/app.js` — modo simulador e dados de demonstração | Útil para aprender sem placa | **Manter** (sustenta a capacitação) |

Depois da limpeza, o núcleo fica visível de relance: **protocolo → Central → nós → web**, mais `README`,
`docs/recomendacoes-firmware.md`, o visualizador de fluxos e os 3 arquivos de governança.

**Visualizador:** feito em 2026-10-20. Abre com a visão geral e a seção "Hoje × proposta" (uso atual,
uso proposto, topologia proposta). Ao concluir um passo P, ele passa de "proposto" para "hoje": atualize
`g-uso-atual`, `g-topo` e `g-capacidades`.
