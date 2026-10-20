# Changelog

Mudanças relevantes do projeto, da mais nova para a mais antiga.
Formato baseado em [Keep a Changelog](https://keepachangelog.com/pt-BR/1.0.0/).
Cada entrada diz **o que mudou** e **como foi verificado**.

## [Não lançado]

### Adicionado
- Governança mínima: `AGENTS.md` (regras), `STATUS.md` (estado e próximos passos) e este `CHANGELOG.md`.
  Verificação: revisão de leitura; nenhum código alterado.
- Visualizador (`docs/firmwares.html`): abertura com visão geral do projeto (cita os 3 documentos de governança) e
  seção "Hoje × proposta": tabela P1–P5, uso do sistema hoje e proposto, topologia proposta e recursos do ESP32.

### Alterado
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
