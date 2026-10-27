# Changelog

Mudanças relevantes do projeto, da mais nova para a mais antiga.
Formato baseado em [Keep a Changelog](https://keepachangelog.com/pt-BR/1.0.0/).
Cada entrada diz **o que mudou** e **como foi verificado**.

## [Não lançado]

### Adicionado
- Prova de conceito ESP-NOW → Wi-Fi → servidor em `docs/index-poc.html`, no mesmo protocolo do visualizador: grafos da
  cadeia, do mapa núcleo × supérfluo e dos ciclos PDCA (MS-010 a MS-016) em `firmwares.json` com `"page": "poc"`;
  barreiras H1–H7, cenários C1–C6, métricas e stack em tabelas. `STATUS.md` passa a seguir esse plano.
  Verificação: `node tools/gen_firmwares_js.mjs --check` = ok; as duas páginas abertas por servidor local e por duplo
  clique, com todos os grafos renderizados e sem erros de script.
- Campos opcionais no `firmwares.json`: `role` (`nucleo` ou `superfluo`) por bloco e `page` (`index` ou `poc`) por grafo,
  validados por `tools/gen_firmwares_js.mjs`.
- Governança mínima: `AGENTS.md` (regras), `STATUS.md` (estado e próximos passos) e este `CHANGELOG.md`.
  Verificação: revisão de leitura; nenhum código alterado.
- Visualizador (`docs/firmwares.html`): abertura com visão geral do projeto (cita os 3 documentos de governança) e
  seção "Hoje × proposta": tabela P1–P5, uso do sistema hoje e proposto, topologia proposta e recursos do ESP32.

### Alterado
- Fluxos do Central (`g-gw-*`) e topologia redesenhados para a branch `com-mini-pc`: sem servidor web, com fila, task HTTP
  no core 1 e push ao PC; o grafo da API REST deu lugar a "Envio ao PC". Fluxos do nó atualizados para a janela de
  1650 ms. Todos os blocos de firmware marcados como núcleo operacional ou supérfluo para a prova.
  Verificação: referências `arquivo:linha` conferidas no commit `3bde339`.
- Motor e estilo do visualizador extraídos do `index.html` para `docs/visualizador.js` e `docs/visualizador.css`,
  compartilhados pelas duas páginas.
- Visualizador renomeado para `docs/index.html` (+ `docs/.nojekyll`) para publicação pelo GitHub Pages;
  `servidor.ps1`, README e AGENTS ajustados. Verificação: `abrir-firmwares.bat` servindo a página localmente.
- Os 8 grafos de firmware ganharam uma nota "Conceito:" explicando o recurso do ESP32 envolvido.
  Verificação: `node tools/gen_firmwares_js.mjs --check` = ok; página servida localmente com os 13 grafos
  renderizados e console sem erros.

## Histórico anterior (resumo do Git até `76dc838`)

- Interface web operando 100% pelo computador; versão com teclado 4x4 preservada na branch `com-teclado-4x4`.
- Arquitetura ESP-NOW: Central com beacon por broadcast e nós com auto-discovery, unicast e Deep Sleep.
- Modos `ONLINE`, `DEEP_SLEEP` e `LIGHT_SLEEP` nos periféricos; janela de pareamento de 60 s após hard reset.
- Sensor de chuva MH-RD com chaveamento por BJT NPN (GPIO 14) contra eletrólise.
- Exclusão de periféricos pela interface; status OFFLINE quando o nó atrasa.
- README principal, visualizador de fluxos e `docs/recomendacoes-firmware.md`.
