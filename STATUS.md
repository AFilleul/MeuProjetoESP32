# STATUS — Sítio Online (ESP32)

**Data:** 2026-10-27 · **Branch:** `com-mini-pc` · **Base:** `3bde339` · **Estado:** protótipo funcional, não validado em campo;
começando a **prova de conceito** ESP-NOW → Wi-Fi → servidor

## 1. Onde estamos (verificado por leitura do código)

| Parte | Estado nesta branch | Observação |
|---|---|---|
| Protocolo ESP-NOW (`ProtocoloMensagens.h`) | Funciona | 3 cópias idênticas (raiz, `Base`, `Chuva`); sem número de sequência |
| Central / ponte (`MeuProjetoESP32.ino` + módulos) | Funciona, não testado com servidor | Sem servidor web: envia cada leitura ao PC por HTTP POST (MS-006). Fila de 20 que descarta a mais antiga |
| Nó de chuva (`ESP_Periferico_Chuva`) | Funciona | Bateria e temperatura **simuladas**; espera até 1650 ms por configuração a cada ciclo |
| Nó molde (`ESP_Periferico_Base`) | Funciona | Não é usado na prova de conceito |
| Servidor no PC | **Não existe** | É a MS-011 |
| Interface web (`web/`) | **Não funciona nesta branch** | Consulta `/api/status` no Central, rota que não existe mais |
| Testes automatizados | Não existem | Verificação manual, na placa |

Os fluxos dos firmwares em `docs/index.html` refletem esta branch e marcam cada bloco como **núcleo operacional** ou
**supérfluo** para a prova. A seção "Hoje × proposta" e a visão geral daquela página ainda descrevem a `main`.

## 2. Próximos passos: prova de conceito em 3 ciclos PDCA

Direção completa (cadeia, núcleo × supérfluo, barreiras H1–H7, cenários C1–C6, métricas e stack) em
[`docs/index-poc.html`](docs/index-poc.html). As folhas das missões ficam no app **Tarefas-build**; os IDs abaixo são
provisórios até a emissão.

| Ciclo | Missão | Fase | Entrega principal |
|---|---|---|---|
| 1 · fazer rodar | MS-010 Mapear a arquitetura do firmware da cadeia | P | Mapa e tabela de pontos de risco com `arquivo:linha` |
| 1 | MS-011 Montar servidor mínimo no PC que grava tudo | D | `servidor-poc/` (FastAPI + SQLite) |
| 1 | MS-012 Rodar a cadeia ponta a ponta com o firmware atual (C1) | C | Linha de base e lista de barreiras observadas |
| 2 · medir | MS-013 Numerar e carimbar as leituras | D | `seq` no protocolo, campos novos na ponte, `metricas.sql` |
| 2 | MS-014 Executar a matriz de cenários com HTTP | C | Tabela C1–C6 e hipóteses confirmadas ou refutadas |
| 3 · comparar | MS-015 Repetir os cenários críticos com MQTT | D | Variante MQTT e tabela HTTP × MQTT |
| 3 | MS-016 Consolidar barreiras e recomendação de stack | A | Relatório A3 |

**Substitui** o roteiro anterior: MS-006 mantida (já feita), MS-007 enxugada na MS-011, MS-008 (refatorar `web/`) e
MS-009 (nuvem) adiadas.

**Onde foram parar P1–P5:** P1 (canal) vira a barreira H1, medida no cenário C5; P3 (fila) foi feito em parte na MS-006 e
seu limite é a barreira H3; P2 (configuração no NVS), P4 (bateria real) e P5 (segurança) ficam para depois da prova.

## 3. Trilha de capacitação

A trilha agora são as missões da tabela acima, na ordem. Regras para quem executa:

1. Leia `docs/index-poc.html` até "Núcleo × supérfluo" antes da MS-010.
2. Durante a prova, mexa só no que está marcado como **núcleo**. Precisa tocar em algo supérfluo? Abra uma pendência antes.
3. Cada missão termina com o `CHANGELOG.md` dizendo como foi verificado e este `STATUS.md` atualizado.

## 4. Recomendação de limpeza (redundante ou sem uso para a proposta técnica)

> Nada foi apagado. Decidir item a item **depois da prova de conceito**; cada remoção em um commit `refactor:` ou `chore:` próprio.
> Os itens de firmware vêm da marcação "supérfluo" dos fluxos (`docs/index.html` e `docs/index-poc.html`).

| Item | Situação | Recomendação |
|---|---|---|
| `TecladoMatriz.cpp` / `.h` | Não é incluído por nenhum sketch; a versão com teclado vive na branch `com-teclado-4x4` | **Remover** |
| `.vscode/launch.json`, `.vscode/settings.json`, `.vscode/c_cpp_properties.json` | Configuração de depurador gcc/gdb para PC, com caminhos de outra máquina (`C:/Users/DELL/...`); não serve para Arduino/ESP32 | **Remover** e adicionar `.vscode/` ao `.gitignore` |
| `exemplos/ESP_Central_EspNow/` | Central reduzido que duplica `EspNowCentral` + `DeviceManager` e diverge (beacon a cada 3 s vs 1,5 s) | **Remover** |
| `processarRespostaMiniPC()` (`MeuProjetoESP32.ino:296-481`) | Aceita 3 formatos de resposta e vários nomes por campo; nenhum servidor envia isso ainda | **Reduzir** a um formato combinado com a MS-011 |
| `DeviceManager`: cópia da telemetria, notificações, estimativa de heap, unidade, fonte e capacidade | Servia à API `/api/status`, que não existe nesta branch | **Reduzir** a MAC e configuração pendente |
| `checkScheduleTimeout()` no `loop()` do Central | Marca OFFLINE só na RAM e no Serial; ninguém consulta | **Remover** (o servidor calcula atrasos) |
| Janela de pareamento de 60 s, escuta extra de 150 ms, nome e uptime na RTC (nós) | Não servem à prova; a janela custa 60 s de rádio a cada reset | **Avaliar** depois das medições (MS-014) |
| `web/` (`app.js`, 2.555 linhas) | Não funciona nesta branch (sem `/api/status`) | **Decidir** junto com o painel do sistema final (antiga MS-008) |
| Fluxogramas Mermaid do `README.md` | Repetem o visualizador e o do gateway descreve a `main` (servidor web) | **Reduzir**: deixar só o diagrama de arquitetura e o link para o visualizador |
| Cópias de `ProtocoloMensagens.h` e `EspNowPeriferico.*` em `Base` e `Chuva` | Redundância exigida pelo Arduino IDE (pasta autocontida) | **Manter por enquanto** com a regra 2 do `AGENTS.md` |

**Visualizador:** `docs/index.html` (fluxos dos firmwares) e `docs/index-poc.html` (prova de conceito) usam o mesmo motor
(`docs/visualizador.js` e `docs/visualizador.css`) e a mesma fonte, `docs/data/firmwares.json`. Grafos com `"page": "poc"`
aparecem só na página da prova; blocos com `"role"` aparecem como núcleo ou supérfluo. Depois de editar o JSON, rode
`node tools/gen_firmwares_js.mjs`.
