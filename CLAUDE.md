# CLAUDE.md — convenções do projeto

Controlador de marcha lenta (ESP32) para o corpo de borboleta a cabo do Audi A3
8L 1.8T 20v. Visão geral em `README.md`; hardware/pinout em `docs/hardware.md`.

## Idioma

- Respostas no chat, documentação, mensagens de commit e textos da página web: **pt-BR**.
- Identificadores de código (variáveis, funções, arquivos): **inglês**.
- Comentários de código: curtos, em pt-BR, apenas quando o código não se explica.

## Commits — Conventional Commits

Formato: `tipo(escopo): descrição no imperativo em pt-BR`.

- Tipos: `feat`, `fix`, `docs`, `chore`, `refactor`, `build`, `test`, `perf`.
- Escopos usuais: `pid`, `cal`, `web`, `hw`, `io`, `control`, `core`.
- Exemplos: `feat(cal): persiste calibração na NVS`, `fix(pid): corrige anti-windup na saturação`.
- Um commit por mudança lógica; não misturar refactor com feature.

## Versionamento — SemVer

- Fonte da verdade: `src/version.h` (`FW_VERSION`). Manter `CHANGELOG.md` (formato Keep a Changelog) sincronizado.
- Em 0.x: feature → bump **minor**; correção → bump **patch**. Após 1.0: quebra de comportamento/protocolo (API web, semântica dos sinais) → **major**.
- Bump de versão + entrada no changelog em um commit `chore(release): versão X.Y.Z`. Tags `vX.Y.Z` só na `main` (ao mesclar).

## Regra de documentação (sempre verificar antes de commitar)

- Mudou comportamento/lógica de controle → atualizar `README.md`.
- Mudou pino/hardware → atualizar `docs/hardware.md` **e** `src/pins.h` (devem estar sempre espelhados).
- Mudança relevante para o usuário → entrada no `CHANGELOG.md`.

## Arquitetura do firmware

Modelo de execução: **tudo roda na task do `loop()` do Arduino** (single-thread);
a única exceção é a ISR do `pwm_input`. Sem primitivas de RTOS, sem bibliotecas
externas (apenas o core arduino-esp32; web com `WebServer.h` síncrono, JSON
montado à mão).

| Módulo | Responsabilidade |
|---|---|
| `pins.h` | Pinout (espelho de `docs/hardware.md`) |
| `settings` | Parâmetros ajustáveis persistidos na NVS |
| `tps` | Leitura filtrada do potenciômetro de posição (ADC1) — pistas 1 e 2 (verificação cruzada opcional) |
| `pwm_input` | Medição do duty do sinal PWM de comando (ISR) |
| `motor` | Acionamento do motor via LEDC — um sentido (abrir; duty 0 = coast, mola fecha) |
| `isense` | Corrente do motor (shunt futuro — dormente) — fim de curso e falha de motor |
| `pid` | PID com anti-windup e derivada na medição |
| `calibration` | Auto calibração (repouso/máx/mín) + NVS + máx aprendido do TPS |
| `analog_out` | DAC (GPIO25) 0–100% → 0–3,3 V |
| `control` | Máquina de modos, malha PID, failsafes, saída mascarada |
| `webui` | AP WiFi + página de debug/parametrização (inclui `POST /update`) |
| `ota` | Gravação pela rede (espota/ArduinoOTA, senha = `apPass`) |

## Regras de segurança (invariantes)

- Estado seguro = saída do motor **desligada** (mola leva a borboleta ao repouso). Todo caminho de falha deve terminar aí.
- **Nunca** acionar o motor com o pedal acionado (idle switch solto) — exceto modo manual de bancada, que expira sem keepalive.
- ADC2 não pode ser usado (WiFi ativo): entradas analógicas só em ADC1 (GPIO32–39).

## Decisões de projeto

- Semântica de posição/setpoint do atuador: **−100..+100%, com 0 = repouso da mola** (+100 = abertura máxima calibrada, −100 = mínima; rampas independentes acima/abaixo do repouso). Duty do comando 0/50/100% → −100/0/+100. Pedido dentro da zona morta em torno de 0 → coast (nenhuma corrente no motor).
- Saída analógica mascarada: 0% em idle; fora de idle replica o TPS normalizado com **zero rebaseado na soltura do idle** (posição da borboleta quando o pedal assume → máximo aprendido/WOT fixo; `outBaseOnRelease` desligado volta à régua fixa do mín da calibração).
- Sem calibração válida → modo `Fault` (motor desligado); web continua ativa para diagnosticar/calibrar.

## Build e gravação

```bash
pio run              # compila
pio run -t upload    # grava via USB
pio device monitor   # serial 115200
pio run -e esp32dev_ota -t upload --upload-port <IP>  # grava pela rede (OTA)
```
