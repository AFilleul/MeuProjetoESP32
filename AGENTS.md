# AGENTS.md — Regras do projeto (leia antes de alterar código)

> **Sítio Online**: monitoramento agrícola com ESP32. Nós sensores (bateria, Deep Sleep) enviam leituras por
> **ESP-NOW** a um **Central**, que mostra os dados em uma **interface web** pela API REST.
> Este projeto é, ao mesmo tempo, **desenvolvimento e capacitação**: cada mudança deve ser entendível por quem
> está aprendendo.

## 1. Os 3 arquivos de governança (e só eles)

| Arquivo | Para que serve | Quando atualizar |
|---|---|---|
| `AGENTS.md` | Regras permanentes (este arquivo) | Raramente; só quando uma regra mudar |
| `STATUS.md` | Onde estamos, o que vem a seguir, o que limpar | **Ao fim de toda sessão de trabalho** |
| `CHANGELOG.md` | O que mudou, em linguagem simples | **A cada commit que muda comportamento** |

O **visualizador de fluxos** (`docs/abrir-firmwares.bat` → `docs/index.html`) mostra o sistema hoje e a proposta de
melhoria (P1–P5), além do fluxo de cada firmware. A fonte é `docs/data/firmwares.json`. Depois de mudar o
comportamento de um firmware, atualize o JSON e rode `node tools/gen_firmwares_js.mjs`.
Na branch `com-mini-pc`, a página `docs/index-poc.html` (mesmo motor e mesma fonte) mostra o plano da prova de conceito.
O `README.md` é a porta de entrada, com a arquitetura resumida e o link para o visualizador. Não criar outros documentos de
controle sem necessidade comprovada.

## 2. Núcleo estratégico (o que sustenta a proposta)

1. **Protocolo** — `ProtocoloMensagens.h`: o "contrato" entre nós e Central. Mudou aqui, muda em todos os lados.
2. **Central** — `MeuProjetoESP32.ino`, `EspNowCentral.*`, `DeviceManager.*`, `WifiManager.*`.
3. **Nós periféricos** — `esps_perifericos/ESP_Periferico_Chuva/` (concreto) e `ESP_Periferico_Base/` (molde).
4. **Interface web** — `web/` (consome `/api/status`, `/api/config`, `/api/device/delete`).

Na branch `com-mini-pc` (prova de conceito), vale o **núcleo operacional** marcado nos fluxos: `web/` e `ESP_Periferico_Base/`
ficam fora dele, porque a interface não tem servidor nesta branch e a prova usa só o nó de chuva.

Todo o resto é apoio. Se um arquivo não serve a um desses quatro, ele é candidato a sair (ver `STATUS.md` §4).

## 3. Regras (poucas, obrigatórias)

1. **Uma mudança por vez.** Um commit = um objetivo, com mensagem `tipo: descrição` (`feat`, `fix`, `docs`, `refactor`).
2. **Protocolo é sagrado.** Alterou `ProtocoloMensagens.h`? Copie o mesmo arquivo para as 3 pastas
   (raiz, `Base`, `Chuva`) no mesmo commit e confira com `diff`.
3. **Teste na placa antes de dizer "pronto".** Registre no `CHANGELOG.md` *como* foi verificado
   (ex.: "Central reiniciado, nó voltou a reportar em 2 min").
4. **Segredos nunca no Git.** Wi-Fi e chaves ficam em `secrets.h` (ignorado). Só `secrets.example.h` é versionado.
5. **Explique o porquê.** Comentário curto em pt-BR quando a razão não for óbvia (ex.: tempo de 50 ms do BJT).
6. **Não fazer push** sem pedido explícito do dono do repositório.

## 4. Para quem está aprendendo

Siga a trilha em `STATUS.md` §3. Um passo de cada vez; cada passo termina com algo funcionando na placa.
