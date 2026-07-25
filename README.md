# A3-8L-TBC — Controlador do Atuador de Marcha Lenta (Audi A3 1.8T 20V 150cv)

Firmware para **ESP32 DevKit** que controla o atuador de marcha lenta integrado ao
corpo de borboleta a cabo do Audi A3 8L 1.8T 20v 150cv.

O atuador é um motor DC acoplado diretamente ao eixo da borboleta. Ele exerce
torque contra a mola de retorno, podendo **abrir** ou **fechar** a borboleta em
relação à posição de repouso (com o motor desligado, a mola leva a borboleta de
volta ao repouso). O controle da abertura é feito modulando o motor por PWM
através de uma ponte H, que permite girar o motor nos dois sentidos.

## Visão geral

```
                          ┌──────────────────────────────┐
  Sinal PWM (alvo %) ────►│                              │────► Ponte H ──► Motor do atuador
  Switch de idle ────────►│           ESP32              │
  Potenciômetro TPS ─────►│  (PID + calibração + WiFi)   │────► Saída analógica (posição 0–100%)
                          │                              │
                          └──────────────┬───────────────┘
                                         │ WiFi AP
                                         ▼
                              Página web de debug/parametrização
```

## Entradas e saídas

| Sinal | Direção | Descrição |
|---|---|---|
| PWM de comando | Entrada | Duty cycle 0–100% representa o percentual de abertura desejado do atuador |
| Switch de idle | Entrada | Fechado quando o pedal do acelerador está **solto** (marcha lenta). Aberto quando o motorista acelera |
| Potenciômetro (TPS) | Entrada analógica | Feedback da posição real da borboleta |
| Ponte H (IN1/IN2) | Saída PWM | Aciona o motor do atuador nos dois sentidos (abrir / fechar) |
| Saída analógica | Saída (DAC) | Posição da borboleta 0–100%, **mascarada** pela lógica de idle (ver abaixo) |

## Semântica de posição: −100% a +100% (0 = repouso)

Posição e setpoint do atuador usam uma escala **com sinal**, ancorada nos três
pontos da calibração:

- **0% = repouso da mola** (motor sem corrente).
- **+100% = abertura máxima calibrada** (motor abrindo em plena carga).
- **−100% = abertura mínima calibrada** (motor fechando no batente).
- A escala é linear por partes: rampas independentes acima e abaixo do repouso
  (o repouso quase nunca é o centro geométrico do curso).
- Leituras do TPS **além** do máx/mín calibrado **saturam em ±100%** (não
  extrapolam a faixa) — vale para a telemetria e para a medição do PID.
- **Corpo cujo repouso da mola é o próprio batente fechado** (caso do 8L): não
  há faixa abaixo do repouso — a calibração aceita mín = repouso e **setpoints
  negativos são tratados como pedido de repouso** (coast; o PID não estola o
  motor contra o batente).

O duty do PWM de comando (0–100%) mapeia linearmente nessa escala:
**0% de duty → −100 (fechar todo), 50% → 0 (repouso), 100% → +100 (abrir
todo)**.

Pedir repouso é pedir **zero absoluto**: com o setpoint dentro da zona morta em
torno de 0, a ponte fica em coast (**nenhuma corrente no motor**) e quem
posiciona a borboleta é a mola — o PID não tenta "segurar" o repouso, então não
há atuação indesejada por erro fracionário de calibração.

> Limitação física do comando: duty 0% verdadeiro é nível estático (sem
> bordas), indistinguível de fio desconectado — ambos caem no failsafe de sinal
> ausente (coast/repouso). Para comandar −100% de fato, o sinal precisa manter
> bordas (duty pequeno, ex.: 1%).

## Lógica de controle

### Malha PID

- O sinal PWM de entrada é medido (duty cycle) e convertido no **setpoint**
  −100..+100% (0 = repouso; ver semântica acima).
- A posição real vem do potenciômetro (TPS), normalizada pela calibração na
  mesma escala com sinal.
- Setpoint dentro da zona morta em torno de 0 → **coast** (motor sem corrente,
  mola posiciona). Fora dela, um PID (derivada na medição, anti-windup por
  integração condicional) calcula o esforço **com sinal**:
  - Esforço positivo → PWM na ponte H no sentido **abrir**.
  - Esforço negativo → PWM na ponte H no sentido **fechar** (abaixo do repouso).
- Zona morta também se aplica ao erro (dentro dela o erro vale zero e o
  integrador segura o último esforço); limite de duty configurável pela web.

### Regra do switch de idle

**Se o motorista estiver acionando o acelerador (switch de idle aberto), o motor
NÃO é acionado** — a ponte H fica desabilitada e o PID é reinicializado
(integrador zerado) para não acumular erro enquanto o pedal comanda a borboleta
pelo cabo.

**Única exceção:** o **modo manual de bancada**, acionado conscientemente pela
página web, comanda o duty diretamente e ignora o idle switch e o TPS (opera em
malha aberta, serve para testar o motor). Por segurança ele **expira sozinho em
3 s** sem keepalive — a página reenvia o comando a cada 1 s enquanto o modo
estiver ligado.

### Saída analógica de posição (mascarada)

A saída no DAC (0–3,3 V) indica a posição da borboleta em 0–100%, com máscara:

| Condição | Saída |
|---|---|
| Em idle (switch de idle acionado / pedal solto) | **0%** |
| Fora de idle (motorista acelerando) | Posição do TPS normalizada pela **faixa da saída** (abaixo) |

O **zero da régua é rebaseado a cada soltura do idle** (`outBaseOnRelease`,
padrão ligado): no flanco de soltura o firmware captura a posição da borboleta
— 0% = ponto em que o pedal assumiu, esteja o atuador onde estiver (a posição
da marcha lenta não "vaza" para o sinal). O **topo é fixo**: `outMaxRaw` ou o
**máximo aprendido** — o firmware observa o maior valor plausível do TPS visto
fora de idle (pedal fundo → WOT), persiste na NVS (com proteção contra
desgaste da flash) e usa esse valor como fundo de escala. Com
`outBaseOnRelease` desligado, vale a régua fixa antiga: de `outMinRaw` (ou o
mín da calibração) ao topo. Assim a
atuação da marcha lenta fica invisível para quem consome o sinal, e fora de
idle a escala cobre o curso todo do pedal. Uma calibração bem-sucedida
re-baseia o aprendido se ele estiver incoerente (abaixo do máximo calibrado ou
fora da faixa plausível). Ambos os limites podem ser fixados manualmente pela
página web.

## Auto calibração

Ao energizar o circuito, se o **switch de idle estiver acionado** (pedal solto),
é executada uma rotina de auto calibração para encontrar os três pontos de
referência do sensor de posição:

1. **Repouso** — motor solto; aguarda a leitura estabilizar e registra.
2. **Abertura máxima** — motor acionado (duty `calDrivePct`, default 100%) no
   sentido abrir; aguarda estabilizar no fim de curso e registra.
3. **Abertura mínima** — idem no sentido fechar; registra.
4. Motor solto (borboleta volta ao repouso); valores validados
   (mín < repouso < máx, faixa mínima configurável) e salvos na NVS.

A rotina **aborta** (mantendo a última calibração válida da NVS) se: o pedal
for acionado no meio, alguma fase não estabilizar dentro do timeout, ou a
validação falhar. Se o idle não estiver acionado na energização, a calibração é
pulada e valem os últimos valores salvos. **Sem nenhuma calibração válida o
firmware entra em `Fault`** (motor desligado) — a página web continua ativa
para diagnosticar e disparar a calibração manualmente (respeitando o idle).

## Modos de operação

| Modo | Comportamento | LED (GPIO2) |
|---|---|---|
| `Boot` | Espera sinais assentarem (~500 ms) e decide calibrar ou não | apagado |
| `Calibrating` | Auto calibração em andamento | pisca 5 Hz |
| `Run` | Em idle: PID atuando no motor | pisca 1 Hz |
| `DriverActive` | Pedal acionado: motor solto, PID zerado, aprende máx do TPS | 2 piscadas curtas/s |
| `Fault` | TPS implausível ou sem calibração: ponte desabilitada | aceso fixo |
| `Manual` | Bancada (via web): duty direto, expira sem keepalive | pisca 10 Hz |

## Segurança / failsafe

- TPS fora da faixa plausível por >100 ms → `Fault` (motor desligado, mola leva
  ao repouso). Recupera sozinho após 500 ms de leitura plausível (se houver
  calibração válida).
- **Perda do sinal PWM de comando** (timeout sem bordas, default 250 ms) →
  **motor solto (coast)**: a mola leva a borboleta ao repouso. Sinal preso em
  nível alto pode, opcionalmente (`cmdStuckHighIs100`, default ligado), valer
  duty 100% (= setpoint +100).
- **Corrente da ponte (IS do IBT-2, opcional — `isenseEnabled`)**: sob drive,
  corrente sustentada acima do limiar de curto ou abaixo do de desconexão →
  `Fault` **retida** ("curto no motor" / "motor desconectado"; vale também nos
  modos Manual e Calibrating). Não há auto-recuperação: limpar pela web (botão
  "Limpar falha do motor") ou reiniciar. O mesmo sensor expõe **fim de curso**
  (stall) como telemetria na página.
- **Watchdog de tarefa** no loop principal (task WDT, ~5 s): travamento do
  firmware → reset, e o boot reinicializa com a ponte H desabilitada.
- Modo manual expira em 3 s sem keepalive.
- Parâmetros são saneados (faixas e relações entre campos) tanto na carga da
  NVS quanto ao salvar pela web.

## Página web (WiFi)

No boot, havendo uma **rede local (STA)** configurada (SSID/senha), o ESP32
tenta conectar nela e a página fica acessível por **`http://a3-tbc.local/`**
(mDNS — funciona em Windows/macOS/iOS/Linux; navegadores Android costumam não
resolver `.local`, use o IP que o roteador atribuir, visível no log serial a
115200). Sem rede configurada — ou se ela não conectar em ~15 s — o ESP sobe
seu **próprio AP** (default: SSID `A3-TBC`, senha `a3tbc123` — **troque**) com
a página em `http://192.168.4.1/`:

- **Monitor ao vivo** (~3 Hz): modo, falha, idle, setpoint, posição, duty
  aplicado, saída analógica, raw do TPS, duty/frequência do comando, termos
  P/I/D, estado e valores da calibração, versão/uptime.
- **Parâmetros** (persistidos na NVS): ganhos Kp/Ki/Kd, zona morta, limite de
  duty, frequência da malha e do PWM da ponte, timeout/semântica do sinal de
  comando, faixa de plausibilidade, filtro (α da EMA e nº de amostras da
  mediana) e sinal invertido do TPS, parâmetros da calibração, mapeamento
  da saída analógica, idle switch, SSID/senha do AP próprio e da rede local
  (STA) (valem após reiniciar).
- **Ações**: disparar calibração, restaurar padrões, modo manual de bancada e
  injeção de setpoint pela web (bancada — testa a malha fechada/PID sem gerador
  de PWM; só atua no modo `Run`, com keepalive de 3 s).

API HTTP (form-encoded/JSON): `GET /api/status`, `GET|POST /api/params`,
`POST /api/cal`, `POST /api/manual`, `POST /api/setpoint`,
`POST /api/faultclear`, `POST /api/defaults`.

## Hardware

Sugestão de pinout, ponte H e circuitos de condicionamento estão documentados em
[`docs/hardware.md`](docs/hardware.md).

## Build e gravação

Projeto [PlatformIO](https://platformio.org/) (framework Arduino, board
`esp32dev`, apenas o core arduino-esp32 — sem bibliotecas externas):

```bash
pio run              # compila
pio run -t upload    # grava via USB
pio device monitor   # serial 115200
pio run -e esp32dev_ota -t upload --upload-port a3-tbc.local   # grava pela rede (OTA)
```

### Atualização OTA (pela rede)

Dois caminhos, ambos só com o core (ArduinoOTA/Update):

- **PlatformIO (espota)**: comando acima, em `a3-tbc.local` (mDNS) ou no IP do
  ESP32 (log serial/roteador). Senha (`--auth` no `platformio.ini`) = senha do
  AP (`apPass`) vigente no boot.
- **Navegador**: card "Atualização de firmware (OTA)" na página — envie o
  `.pio/build/esp32dev/firmware.bin`. Funciona também no AP do veículo (ex.:
  pelo celular).

Cuidados: atualizar **com o motor desligado** — ao iniciar a gravação a ponte H
é desabilitada e o loop fica bloqueado até o fim da transferência; ao final o
ESP32 reinicia (boot em estado seguro). Parâmetros e calibração ficam na NVS.

> Gotcha: a gravação **USB** escreve sempre na partição `app0`, mas o OTA
> alterna entre `app0`/`app1` via `otadata`. Se após usar OTA uma gravação USB
> parecer "não pegar" (volta o firmware antigo), apague o `otadata`:
> `esptool erase_region 0xe000 0x2000` — ou grave de novo por OTA.

## Estrutura do repositório

```
├── README.md            ← este documento
├── CLAUDE.md            ← convenções do projeto (commits, versionamento, docs)
├── CHANGELOG.md         ← histórico de versões (Keep a Changelog / SemVer)
├── docs/
│   └── hardware.md      ← pinout, ligações e condicionamento de sinais
├── platformio.ini
└── src/
    ├── main.cpp         ← setup/loop, watchdog, LED de status
    ├── version.h        ← FW_VERSION (fonte da verdade do SemVer)
    ├── pins.h           ← pinout central (espelho de docs/hardware.md)
    ├── settings.*       ← parâmetros ajustáveis persistidos na NVS
    ├── tps.*            ← leitura filtrada do potenciômetro (mediana + EMA)
    ├── pwm_input.*      ← medição do duty do comando (ISR)
    ├── hbridge.*        ← ponte H via LEDC (duty com sinal)
    ├── pid.*            ← PID com anti-windup e derivada na medição
    ├── calibration.*    ← auto calibração + NVS + máx aprendido do TPS
    ├── analog_out.*     ← saída DAC 0–100%
    ├── control.*        ← máquina de modos, malha, failsafes, máscara
    ├── webui.*          ← AP WiFi + rotas HTTP (inclui POST /update)
    ├── webui_page.h     ← página (HTML/CSS/JS embutidos)
    └── ota.*            ← gravação pela rede (espota/ArduinoOTA)
```

## Roadmap

- [x] Documentação do projeto e do hardware (pinout)
- [x] Esqueleto do projeto PlatformIO
- [x] Leitura do TPS + medição do PWM de entrada + idle switch
- [x] Driver da ponte H (LEDC, dois sentidos)
- [x] Rotina de auto calibração + persistência NVS
- [x] Malha PID + regra do idle switch + failsafes
- [x] Saída analógica mascarada (DAC)
- [x] WiFi AP + página web de debug/parametrização
- [x] Atualização OTA (espota e upload pela página)
- [ ] Compilação verificada com toolchain xtensa (`pio run`) e ajuste de warnings
- [ ] Testes em bancada (motor + potenciômetro reais, sintonia do PID)
- [ ] Testes no veículo
