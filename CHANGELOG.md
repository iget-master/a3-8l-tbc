# Changelog

Formato baseado em [Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/);
versionamento segue [SemVer](https://semver.org/lang/pt-BR/).

## [Não lançado]

## [0.11.0] - 2026-07-26

### Adicionado

- **Pista 2 do TPS (contraposta) com verificação cruzada** — opcional
  (`tps2Enabled`, padrão desligado), no **GPIO36**: canal filtrado próprio
  (mesma mediana+EMA), inversão independente (`tps2Invert`), calibração das
  duas pistas na mesma rotina (NVS) e comparação em posição a cada ciclo —
  divergência acima de `tps2DivergePct` (padrão 10%) sustentada → `Fault`
  **"TPS divergente"** (mesmas persistências de queda/recuperação do TPS
  implausível). Detecta trilha gasta/drift/curto que o limiar de
  plausibilidade não enxerga. Pista 2 inválida na calibração desativa só a
  verificação cruzada (controle segue na pista 1). Telemetria `rawTps2`/`pos2`
  no status e no monitor da página; `docs/hardware.md` com a ligação
  (cursor 2 → RC → GPIO36).

## [0.10.1] - 2026-07-26

### Corrigido

- **Vibração/ciclo-limite com setpoint fixo**: a zona morta era dura (erro
  inteiro voltava de uma vez ao cruzar a borda — o P saltava a cada excursão
  de ruído do TPS, com direito a excursões grandes ocasionais). Agora é
  **subtrativa (suave)**: fora da banda conta só o excedente do erro, contínuo
  na borda.

## [0.10.0] - 2026-07-26

### Adicionado

- **Saída analógica com zero rebaseado na soltura do idle**
  (`outBaseOnRelease`, padrão ligado): 0% = posição da borboleta no instante em
  que o pedal assume (capturada no flanco cru do switch e confirmada pelo
  debounce) — a posição do atuador não vaza mais para o sinal (antes: soltar
  com o atuador estendido partia de ~24%). 100% continua fixo no máx
  aprendido/WOT. Sem span útil até o máx, ou antes da primeira soltura, vale a
  régua fixa antiga (mín da calibração).

## [0.9.1] - 2026-07-25

### Corrigido

- **Caçada ao voltar do pedal para idle com setpoint alto**: a transição
  DriverActive → Run reengatava o PID de uma vez com o erro inteiro (chute de
  duty com a borboleta ainda em movimento). Agora o reengate é suave: PID
  zerado e rampa do setpoint partindo da posição atual.

## [0.9.0] - 2026-07-25

### Adicionado

- **Rampa do setpoint** (`spSlewPctPerS`, %/s; padrão 250; 0 = desligada):
  limita a variação do setpoint que o PID segue — retorno rápido ao repouso
  vira descida controlada (sem pancada no batente nem o quique que o PID
  caçava). Engate suave: ao entrar em modo normal a rampa parte da posição
  atual. A telemetria (`setpoint`) mostra o valor pós-rampa.

### Alterado

- Página: **barra de posição/setpoint fixa no topo** (sticky — sempre visível
  ao rolar); sliders do modo manual e do setpoint de bancada agora **só
  positivos** (0..+100) — coerente com o corpo do 8L, sem faixa abaixo do
  repouso.

## [0.8.3] - 2026-07-25

### Corrigido

- **Calibração abortando na fase de fechamento** no corpo do 8L: o switch de
  idle fecha pelo contato alavanca↔pino do atuador — recolher o pino
  (drive negativo) descola a alavanca, o switch abre e a rotina abortava com
  "idle solto". Novo parâmetro `calMeasureClose` (padrão **desligado**): a
  fase de fechamento é pulada e `mín = repouso`. Marcar apenas em hardware com
  curso real abaixo do repouso.

## [0.8.2] - 2026-07-24

### Adicionado

- **Motivo da falha da calibração**: cada caminho de reprovação registra o
  porquê com os valores medidos (faixa insuficiente com rep/máx/Δ, timeout de
  estabilidade com a banda vigente, idle solto na fase X, aborto externo) —
  visível na página (linha "Calibração (raw)"), no `/api/status`
  (`cal.fail`) e no log serial (fases e candidatos também são logados).

## [0.8.1] - 2026-07-24

### Corrigido

- **Calibração em corpo cujo repouso da mola é o batente fechado** (caso do
  8L): a validação exigia `mín < repouso` — uma faixa abaixo do repouso que
  esse hardware não tem — e reprovava sempre. Agora aceita mín = repouso
  (clampa a diferença de ruído) e exige a faixa mínima só do lado da abertura.
  Motor com fios trocados continua sendo reprovado (a "abertura" não sai do
  repouso).
- Com calibração de mín = repouso, **setpoints negativos viram pedido de
  repouso** (coast) — o PID não fica estolando o motor contra o batente
  fechado.

## [0.8.0] - 2026-07-24

### Adicionado

- **Suporte a TPS com pista invertida** (tensão maior fechado, caso do sensor
  real do corpo): novo parâmetro "Sinal invertido" (`tpsInvert`) espelha a
  leitura na fonte (`4095 − raw`) — calibração, PID, saída analógica, máximo
  aprendido e plausibilidade funcionam sem mudança. Recalibrar após alterar.
  Sem o flag, a calibração de um TPS invertido reprova na validação
  (`mín < repouso < máx`) e restaura a calibração anterior da NVS.

## [0.7.0] - 2026-07-23

### Adicionado

- **Atualização de firmware pela rede (OTA)**, só com bibliotecas do core:
  - **espota/ArduinoOTA** (módulo `ota`, porta 3232):
    `pio run -e esp32dev_ota -t upload --upload-port a3-tbc.local` — novo env
    no `platformio.ini`; senha = senha do AP (`apPass`) vigente no boot.
  - **mDNS**: o aparelho atende por **`http://a3-tbc.local/`** (página, API e
    espota; serviço `_http._tcp` anunciado) — sem precisar saber o IP.
    Navegadores Android costumam não resolver `.local` — usar o IP.
  - **Upload pelo navegador**: card "Atualização de firmware (OTA)" na página
    envia o `firmware.bin` para `POST /update` (funciona no AP do veículo).
  - Segurança: ao iniciar a gravação a ponte H é desabilitada (a transferência
    bloqueia o loop) e o task WDT é alimentado durante a escrita; ao final o
    ESP32 reinicia no boot seguro. A tabela de partições padrão já tem
    `app0`/`app1` + `otadata` — sem mudança de layout.

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
