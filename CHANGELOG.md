# Changelog

Formato baseado em [Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/);
versionamento segue [SemVer](https://semver.org/lang/pt-BR/).

## [Não lançado]

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
