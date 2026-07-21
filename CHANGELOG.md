# Changelog

Formato baseado em [Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/);
versionamento segue [SemVer](https://semver.org/lang/pt-BR/).

## [Não lançado]

## [0.6.0] - 2026-07-21

### Adicionado

- **Sensor de corrente da ponte (IS do IBT-2/BTS7960)** no GPIO33 (ADC1):
  módulo `isense` lê R_IS+L_IS filtrado e normaliza pela duty aplicada
  (estimativa @100%). Detecta, com limiares e persistências configuráveis pela
  web: **fim de curso** (stall — telemetria na página), **curto no motor** e
  **motor desconectado** — os dois últimos derrubam para `Fault` **retida**
  (sem auto-recuperação; vale também em Manual/Calibrating), com limpeza pelo
  botão "Limpar falha do motor" (`POST /api/faultclear`). Desabilitado por
  padrão (`isenseEnabled`) — sem o circuito, o pino flutua.
- Página: tiles "Corrente ponte (raw)" e "Fim de curso" no monitor; fieldset
  "Corrente do motor" nos parâmetros; campos `mCur`/`mCurEst`/`stall`/`mLatch`
  no `/api/status`.
- Hardware: `docs/hardware.md` ganha a seção do IBT-2 (mapeamento de pinos,
  rede do IS com 680 Ω + RC, nota sobre o resistor onboard) e o GPIO33 no
  pinout (`PIN_ISENSE`).

## [0.5.1] - 2026-07-19

### Corrigido

- **Posição satura em ±100%**: leitura do TPS acima do máx (ou abaixo do mín)
  calibrado não extrapola mais além de +100%/−100%. Vale para a telemetria e
  para a medição que alimenta o PID (`calibration::positionPct`).

## [0.5.0] - 2026-07-19

### Adicionado

- **Filtro do TPS configurável pela web**: `α` da EMA e o nº de amostras da
  mediana (ímpar, 1–15) agora são parâmetros na página, persistidos na NVS —
  permite ajustar o compromisso ruído × lag (relevante para o termo derivativo
  do PID) sem recompilar.

### Alterado

- Persistência da NVS agora faz **migração append-only**: campos novos entram
  no fim da struct de `Settings` e um blob de versão anterior é carregado sobre
  os padrões, **preservando a configuração salva** (inclusive credenciais WiFi)
  em atualizações de firmware. `SETTINGS_VERSION` 2 → 3.

## [0.4.0] - 2026-07-19

### Adicionado

- **Override de setpoint pela web (bancada)**: slider na página que injeta o
  setpoint no lugar do PWM de comando, permitindo testar a malha fechada (PID)
  sem gerador de sinal. Só atua no modo `Run` (idle ativo + calibração válida) e
  conta como "comando presente"; keepalive de 3 s (a página reenvia a cada 1 s),
  como o modo manual. Endpoint `POST /api/setpoint` e campo `spOvr` no
  `/api/status`.

## [0.3.0] - 2026-07-18

### Adicionado

- **Modo estação (STA) com fallback para AP**: no boot, havendo uma rede local
  configurada (SSID/senha), o ESP tenta conectar nela; se não achar ou não
  conectar em ~15 s, sobe o próprio AP (`A3-TBC`) como antes. Permite acessar a
  página pela rede local (LAN), útil para depuração.
- Campos de SSID/senha da rede local (STA) na página web, persistidos na NVS
  (separados das credenciais do AP próprio).
- Logs de rede na serial (115200): estado do AP, tentativa de STA e IP obtido.

### Alterado

- Layout de configuração na NVS ganhou os campos de STA (`SETTINGS_VERSION`
  1 → 2); parâmetros salvos anteriormente voltam aos padrões neste upgrade.

## [0.2.0] - 2026-07-17

### Alterado

- **Semântica de posição/setpoint: agora −100..+100%, com 0 = repouso da mola**
  (antes 0–100% com o repouso num % intermediário). +100 = abertura máxima
  calibrada, −100 = mínima; normalização linear por partes em torno do repouso.
- Duty do PWM de comando mapeia 0/50/100% → setpoint −100/0/+100.
- Pedido de repouso (setpoint dentro da zona morta em torno de 0) → coast:
  nenhuma corrente no motor, a mola posiciona — elimina atuação indesejada
  quando o pedido é repouso.
- Página web: barra de posição/setpoint com escala −100..+100 e marca do
  repouso no centro.

## [0.1.0] - 2026-07-17

### Adicionado

- Firmware inicial (ESP32 DevKit, PlatformIO/Arduino):
  - Leitura filtrada do TPS (ADC1) e medição do duty do PWM de comando (ISR).
  - Driver da ponte H via LEDC com duty com sinal (abrir/fechar/coast).
  - PID com anti-windup, derivada na medição e zona morta configurável.
  - Auto calibração no boot (repouso/máx/mín) condicionada ao idle switch,
    com persistência na NVS e fallback para a última calibração válida.
  - Malha de controle com modos Boot/Calibrando/Run/Motorista/Falha/Manual,
    regra do idle switch e failsafes (TPS implausível, perda do sinal de
    comando → coast, watchdog de tarefa no loop).
  - Saída analógica mascarada no DAC (0% em idle; TPS normalizado fora de idle),
    com aprendizado do máximo do TPS.
  - AP WiFi com página web de debug e parametrização (ganhos, limites,
    calibração, modo manual de bancada com expiração).
- Documentação: README, hardware/pinout (`docs/hardware.md`), convenções
  (`CLAUDE.md`) e este changelog.
