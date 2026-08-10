# Hardware — pinout e ligações (ESP32 DevKit)

Placa alvo: **ESP32 DevKit V1 (30 pinos, módulo WROOM-32)**.

## Restrições do ESP32 que guiaram a escolha dos pinos

- **ADC2 não pode ser usado com WiFi ativo.** Como o projeto sobe um AP WiFi,
  toda leitura analógica precisa ficar no **ADC1** (GPIO32–39).
- **GPIO34, 35, 36 e 39 são somente entrada** e **não têm pull-up/pull-down
  internos** — ótimos para sinais analógicos e sinais já condicionados.
- **GPIO25 e GPIO26 são os únicos DACs** (8 bits, 0–3,3 V).
- GPIO0, 2, 12 e 15 são pinos de *strapping* (afetam o boot) — evitados para
  sinais externos; GPIO2 fica só com o LED onboard.
- GPIO6–11 são da flash interna — nunca usar.

## Pinout sugerido

| Função | GPIO | Tipo | Observações |
|---|---|---|---|
| TPS (potenciômetro de posição) | **GPIO34** | Entrada analógica (ADC1_CH6) | Somente entrada; alimentar o potenciômetro com **3,3 V** do próprio ESP32 para usar a faixa completa do ADC sem divisor |
| TPS pista 2 — contraposta (opcional) | **GPIO36** — etiqueta **VP** na placa | Entrada analógica (ADC1_CH0) | Somente entrada; verificação cruzada das pistas (ver seção TPS) |
| Sinal PWM de comando (alvo %) | **GPIO35** | Entrada digital (somente entrada) | Passar por condicionamento de nível (ver abaixo) antes de chegar ao pino |
| Switch de idle | **GPIO32** | Entrada digital com pull-up interno | Switch fechando para **GND** quando pedal solto (em idle → nível baixo) |
| Acionamento do motor (PWM, sentido **abrir**) | **GPIO18** | Saída PWM (LEDC) | Um sentido só; fechar é da mola (ver seção do acionamento) |
| *(reservado)* Corrente do motor — shunt futuro | **GPIO33** | Entrada analógica (ADC1_CH5) | Sem uso no momento (`isense` dormente) |
| Saída analógica de posição 0–100% | **GPIO25** | Saída DAC (DAC1) | 0–3,3 V; usar buffer/amplificador se o consumidor esperar 0–5 V (ver abaixo) |
| LED de status | **GPIO2** | Saída | LED onboard do DevKit (heartbeat / erro / calibrando) |

Pinos livres para expansão futura: GPIO16, 17, 19, 21, 22, 23, 26 (DAC2), 27 e
39 (VN — somente entrada, sem pulls).

## Diagrama de ligação

```
                         ESP32 DevKit V1
                       ┌─────────────────┐
   PWM comando ──►[cond.]──► GPIO35      │
   Idle switch ──►[cond.]──► GPIO32      │          ┌────────────────┐   ┌───────────────┐
                       │      GPIO18 ────┼─────────►│ Estágio P-FET  │──►│ Motor do      │
   TPS cursor ────────►│ GPIO34          │          │ high-side (um  │   │ atuador       │
   TPS 3V3 ◄──[100Ω]───┤ 3V3             │          │ sentido)+SS54  │   │ (borboleta)   │
   TPS GND ◄───────────┤ GND             │          └───────┬────────┘   └───────────────┘
   TPS2 cursor ───────►│ GPIO36 (VP)     │                  │ 12 V pós-chave
   Saída 0–100% ◄──[buffer]── GPIO25     │                  │ (fusível + TVS — ver Alimentação)
                       └─────────────────┘
```

## Acionamento do motor (um sentido, high-side)

O motor consome ~0,5–1 A rodando e 2–4 A estolado no batente. **Só o sentido
"abrir" é acionado** — o fechar é sempre passivo, pela mola (curso completo em
~0,3 s, medido em bancada). PWM LEDC de ~20 kHz (inaudível) no GPIO18, duty
0–100%; duty 0 = coast.

### Estágio P-FET high-side (como na placa)

Cadeia de acionamento (esquemático da placa A3-TBC-FULL):

- `MOTOR_ON` (IO18) → gate do **Q3 2N7002** (nível lógico), com **R11 100 kΩ
  para GND** — boot/reset/brownout deixam o estágio desligado.
- Dreno do Q3 = **nó A**, com **R10 2,2 kΩ para +12 V**.
- Nó A → bases do push-pull **Q4 MMBT5551 (NPN) / Q5 MMBT5401 (PNP)**
  (transistores de 160 V); emissores → **R12 100 Ω** → gate do **Q6 AOD4185**
  (P-FET −40 V, source em +12 V).
- Gate do Q6: **R13 10 kΩ para +12 V** (OFF por padrão) + **D5 zener 15 V**
  gate–source (clamp no load dump; ~130 mA por R12 durante o evento).
- Dreno do Q6 → **M+**; **M− → GND**; **D4 SS54** (Schottky 5 A) em
  antiparalelo com o motor (roda-livre). **C13 220 µF (≥ 35 V!) + C14 100 nF**
  locais no 12 V do estágio.

Funcionamento: IO18 alto → Q3 liga → nó A baixo → o PNP puxa o gate →
Vgs ≈ −11,3 V → **FET liga**. IO18 baixo/flutuando → tripla garantia de OFF
(R11 no gate do Q3, R10 no nó A, R13 no gate do Q6). Dinâmica a 20 kHz: o
push-pull faz o swing do gate em ~300 ns e o R13 completa o último 0,6 V após
o corte do seguidor — bordas limpas nos dois flancos.

- **Consistência de tensões**: AOD4185 (−40 V) + XL1509 (40 V) fecham com o
  clamp de ~29 V do **SMBJ18A** (ver Alimentação). Vgs máx ±20 V respeitado
  pelo clamp de 15 V.
- Perdas: stall 4 A × ~9 mΩ ≈ 0,15 W no DPAK + ~0,15 W de chaveamento — sem
  dissipador, o pour resolve.
- **Fail-safe de chicote**: fio do motor roçado no chassi = curto franco → o
  **fusível externo de ~5 A** abre. (Numa chave low-side o mesmo roçado
  ligaria o motor sozinho — por isso high-side.)
- Alternativa integrada (se um dia voltar a precisar de dois sentidos):
  **DRV8871** (6,5–45 V, 3,6 A) — o firmware ≤ 0.12.x o suporta direto.

Alimentação do motor: **nó pós-chave protegido** (relé + TVS + fusível próprio
de ~5 A — topologia na seção Alimentação). GND comum com o ESP32.

### Corrente do motor — sem sensor por enquanto

O sensoriamento de corrente saiu junto com o IBT-2. O firmware mantém o módulo
`isense` **dormente** (`isenseEnabled` desligado) e o **GPIO33 reservado** para
um shunt futuro no retorno de GND do motor (ex.: 20 mΩ 1 W + INA180A1 →
0–5 A vira 0–1,6 V no ADC), reaproveitando os limiares configuráveis da página
(fim de curso / curto / desconexão).

## Condicionamento de sinais

### Sinal PWM de comando (5 V) → GPIO35

O comando vem em **5 V** — não pode entrar direto no ESP32 (GPIO35 é
somente-entrada, **sem diodos de clamp**, abs. máx. 3,6 V). Condicionamento:

```
Sinal 5 V ──[10 kΩ]──┬──► GPIO35
                     ├──[15 kΩ]──► GND      (nível alto = 3,0 V)
                     ├──[1 nF]───► GND      (X7R, junto ao pino)
                     └──[zener 3,6 V]─► GND (BZT52C3V6)
```

- **10 kΩ / 15 kΩ (1%)** → alto = 3,0 V: margem sobre o VIH (~2,5 V) e sobre o
  abs-max mesmo com o rail de 5 V a +5%. O Thevenin do divisor (~6 kΩ) já faz
  o papel de resistor de série.
- **1 nF** → corte ~26 kHz, τ ≈ 6 µs simétrico nas bordas: mata spike de
  chicote sem distorcer a medição de duty (< 0,3% de erro a 1 kHz). Comando
  acima de ~2 kHz → usar 470 pF.
- **Zener 3,6 V** → clampa transientes e protege no modo de falha "resistor de
  baixo aberto" (sem ele o pino veria 5 V diretos).
- ⚠️ Vale para fonte **push-pull**. Se a origem for **coletor aberto**, o
  divisor briga com o pull-up da fonte e o nível alto desaba — nesse caso o
  circuito é outro: **pull-up local de 10 kΩ para 3V3**, sem divisor (mantém o
  1 nF + zener). Confirmar o tipo de saída antes de fechar a placa.
- Sinal em **12 V** (outra aplicação): preferir optoacoplador (PC817 +
  ~2,2 kΩ de base) ou divisor 33 kΩ / 10 kΩ + zener 3,6 V.

### Switch de idle → GPIO32

O switch do corpo de borboleta fecha para o chassi (GND) quando o pedal está
solto. O firmware habilita o **pull-up interno** do GPIO32 (~45 kΩ) — por isso
o switch ficou neste pino: **GPIO34–39 não têm pull-up/pull-down internos**.

- **Bancada**: nada a adicionar — switch (ou jumper) direto entre GPIO32 e GND.
- **Veículo**: o pull-up interno é fraco (~45 kΩ) e deixa o nó de impedância
  alta — sensível a ruído/EMI numa linha longa do chicote. Reforçar com:
  - **pull-up externo de 4,7–10 kΩ para 3V3** (soma em paralelo com o interno,
    sem conflito) — baixa a impedância do nó e acelera a subida ao abrir o
    switch (~1 ms com 10 kΩ + 100 nF, contra ~5 ms só com o interno);
  - resistor de **série de 1 kΩ** e **capacitor de 100 nF para GND** no pino
    (filtro RC contra ruído do chicote).
- Debounce por software (~20 ms, configurável pela web) em ambos os casos.

> Se no veículo o switch chavear +12 V em vez de GND, condicionar como o sinal
> PWM (divisor/opto) e inverter a lógica no firmware.

### TPS (potenciômetro) → GPIO34

- Alimentar as extremidades do potenciômetro com **3V3 e GND do ESP32** — assim
  o cursor entrega 0–3,3 V direto ao ADC, sem divisor.
- **Resistor de proteção no TPS+: 100 Ω / 0,25 W (filme metálico)** em série
  com o 3V3 que alimenta o potenciômetro. Curto do fio TPS+ ao chassi fica
  limitado a ~33 mA (dissipação 0,11 W, dentro do rating): o rail 3V3 —
  **compartilhado com o ESP32** — sobrevive, e o firmware cai em `Fault`
  gracioso ("TPS implausível" → coast) em vez de brownout em loop. A atenuação
  do divisor com as pistas (~5–9% com 1–2 kΩ) é **absorvida pela calibração**
  — recalibrar após instalar. Pré-requisito: faixa de plausibilidade do TPS
  configurada (sem ela, o curto viraria leitura "válida" para o PID).
- Curto do chicote ao **12 V** é outro caso: quem limita é o 1 kΩ em série do
  cursor; blindagem extra opcional = zener de 3,6 V do nó do pino para GND.
- **Pista invertida** (tensão maior fechado — comum nesses TPS de duplo
  elemento): marcar **"Sinal invertido"** nos parâmetros da página — o firmware
  espelha a leitura (4095 − raw). Recalibrar após alterar.

#### Pista 2 contraposta → GPIO36 (opcional — verificação cruzada)

O TPS destes corpos tem **duas pistas contrapostas** (uma cresce abrindo, a
outra decresce). Ligando a segunda, o firmware compara as posições das duas a
cada ciclo: divergência sustentada (trilha gasta, drift, curto — falhas que o
limiar de plausibilidade **não** enxerga, inclusive na região de marcha lenta,
onde a trilha mais desgasta) → **Fault "TPS divergente"**, motor solto.

- Ligação: **somente o cursor da pista 2 → série 1 kΩ → GPIO36** (pino
  etiquetado **VP** no DevKit, logo abaixo do EN), com 100 nF do pino para GND
  (mesmo RC da pista 1). As extremidades de alimentação são as
  mesmas já ligadas (3V3/GND) — não inverter fisicamente; a polaridade se
  ajusta por software (`tps2Invert`).
- Antes de habilitar: medir com multímetro a tensão do cursor 2 fechado e todo
  aberto (na mão). Se a tensão **cresce** ao abrir, deixar "Pista 2 invertida"
  desmarcada; se decresce, marcar. Algumas peças têm a pista 2 com faixa
  parcial/meia inclinação — sem problema: a comparação é feita em **posição
  calibrada**, não em tensão.
- Na página: marcar "Pista 2 (verif. cruzada)" e **recalibrar** (a rotina
  registra as duas pistas juntas). Pista 2 estruturalmente inválida na
  calibração (inversão errada, fio solto) desativa só a verificação cruzada —
  o controle segue normal na pista 1.
- GPIO36 (e o 39) têm glitches esporádicos de ADC com WiFi ativo — a mediana
  do filtro do TPS já os absorve.
- Resistor de série de 1 kΩ e capacitor de 100 nF do pino para GND (filtro
  anti-aliasing/ruído).
- No firmware: média/mediana de várias amostras + atenuação `ADC_11db`.

### Saída analógica → GPIO25 (DAC1)

O DAC entrega 0–3,3 V com pouca capacidade de corrente:

- Se o consumidor aceita 0–3,3 V: apenas um **buffer** (op-amp seguidor, ex.:
  MCP6002 alimentado em 3,3 V, entrada/saída rail-to-rail).
- Se o consumidor espera **0–5 V**: op-amp não inversor com ganho 1,52 —
  preferir **MCP6002 alimentado em 5 V** (rail-to-rail; um LM358 em 12 V
  elevaria o teto de falha visto pelo ESP32), R1 = 10 kΩ para GND,
  Rf = 5,2 kΩ ≈ 5,1 kΩ.

Proteções (o nó exposto ao chicote é a **saída do op-amp**, não o GPIO25):

```
GPIO25 ──[1 kΩ]──┬──► entrada do op-amp     (RC também suaviza os degraus
                 ├──[100 nF]──► GND          de 8 bits do DAC)
                 └──[zener 3,6 V]──► GND    (opcional — pino sem clamp interno)

op-amp ──[330 Ω]──┬──► conector (0–5 V)
                  ├──[zener 6,2 V]──► GND   (curto ao 12 V: nó preso em 6,2 V,
                  └──[100 nF]──► GND         ~17 mA no zener; op-amp vê 3,6 mA
                                             pelos 330 Ω. EMI no conector.)
```

- 330 Ω em série: curto ao GND → ~15 mA (MCP6002 sobrevive); erro no
  consumidor (entrada ≥ 100 kΩ) < 0,4%.
- Grau automotivo pleno para load dump direto no fio do sinal: trocar o zener
  de 6,2 V por um TVS **SMAJ6.0A**.

## Alimentação

Topologia (como na placa): entrada única de **+12 V pós-chave** no conector,
com **fusível externo de ~5 A no chicote (requisito de instalação — a placa
não tem fusível)** e relé pós-chave a montante. **Nada liga direto na
bateria**: o atuador é inútil sem o controlador, e 12 V sempre vivo criaria o
risco de uma falha física acionar o motor com o carro estacionado.

```
12 V pós-chave ──[fusível externo ~5 A]──┬── D6 TVS SMBJ18A → GND
 (relé no chicote)                       │
                        ┌────────────────┴──────────────┐
                        │                               │
          [R1 0,1 Ω]─[D1 SS26]                 12 V do estágio P-FET
                        │                        (direto; bateria invertida →
                XL1509-5.0 (buck 150 kHz)         diodos conduzem → fusível
                        │                         externo abre, sacrificial)
                       5 V ──► MCP6002 · OR-ing com USB (B5819WS)
                        │
                  AMS1117-3.3 → 3,3 V (módulo, TPS, CP2102N)
```

- **TVS SMBJ18A** (clampa em ~29 V): fecha coerente com o AOD4185 (−40 V) e o
  XL1509 (40 V máx). **Limite assumido: jump-start de 24 V queima o TVS**
  (standoff 18 V) — trade-off aceito; capacitores do rail 12 V (C13, C14, C1,
  C2, C3) obrigatoriamente **≥ 35 V**.
- **12 V → 5 V: XL1509-5.0** na placa (40 V máx., 150 kHz, L 68 µH + SS34 +
  220 µF), com **R1 0,1 Ω + D1 SS26 em série** — proteção reversa do ramo da
  eletrônica e amortecimento de inrush. (Regra geral que levou à escolha:
  entrada abs. máx. do regulador **acima** do clamp do TVS — por isso nada de
  MP1584/7805 no veículo.)
- **5 V → 3,3 V**: AMS1117-3.3.
- **USB alimenta a placa em bancada** (OR-ing por Schottky B5819WS no 5 V):
  gravação/depuração sem 12 V — e sem 12 V o motor simplesmente não roda.
- **Sequenciamento é indiferente**: com o ESP32 morto, R11/R10/R13 mantêm o
  estágio desligado; ESP32 vivo com 12 V do estágio morto aciona um FET sem
  energia — nenhuma ordem é perigosa.

### Comportamento no desligamento (desenergizado = estado seguro)

1. ESP32 sem alimentação → GPIO18 em alta impedância.
2. O pull-up de 1 kΩ do nó A (referenciado ao 12 V do estágio) mantém o gate
   igual ao source → **P-FET desligado por construção**.
3. Motor sem corrente — **coast, não freio** (a fcem não passa do diodo de
   roda-livre): motor solto de verdade.
4. **A mola leva a borboleta ao batente mecânico de marcha lenta** — o carro
   volta a se comportar como corpo a cabo "burro": marcha lenta fixa no ajuste
   mecânico, dirigível normalmente.
5. O DAC vai a 0 V = 0%, coerente com a saída mascarada (em idle já é 0%).

O mesmo vale nos transientes: *brownout* do ESP32 solta o GPIO18 (→ pull-up do
nó A → FET desligado) e o boot inicializa com o PWM em 0 (`motor::begin`);
GPIO18 não é pino de *strapping* com glitch alto no reset. Ligando, desligando
ou morrendo no meio de um movimento, todo caminho termina em **saída do motor
desligada + mola no repouso**.

## Placa própria (módulo WROOM-32E-N4): boot, reset e gravação

Em operação normal nada disso é usado (atualização por **OTA**, reset por
ciclo de chave). A placa traz **USB completo onboard** para primeira gravação,
socorro e bancada:

- **USB micro + CP2102N-A02** (self-powered: VDD=VREGIN=3,3 V; pino VBUS por
  divisor 22 kΩ/47 kΩ; RSTb com pull-up 2 kΩ), **ESD LESD5D5.0CT1G** em
  VBUS/D+/D− e **auto-reset canônico** com 2× MMBT3904 cruzados (DTR/RTS →
  EN/IO0) — `esptool` grava sem botões. TXD0/RXD0 passam por **0 Ω em série**
  (R6/R7): pontos de corte/tap se algum dia o UART for reaproveitado.
- O USB **alimenta a placa** (OR-ing no 5 V): gravação e web em bancada sem
  12 V — e sem 12 V o motor não roda, o que é um ótimo interlock natural.
- **EN**: pull-up 10 kΩ + **1 µF para GND** (power-on-reset).
- **IO0**: pull-up 10 kΩ para 3V3 — o interno é fraco; um glitch baixo no IO0
  durante um reset (brownout/ruído automotivo) jogaria o chip em modo download
  até o próximo ciclo de chave (o auto-reset só atua com USB plugado).
- **Strapping**: GPIO12 **solto** (não rotear — alto no boot muda a tensão da
  flash); GPIO2 só com o **LED (KT-0603R + 1 kΩ) → GND** (pull-down fraco,
  compatível com o modo download); GPIO15 pode ficar solto.
- Sem botões BOOT/RESET: o auto-reset via USB cobre gravação; reset de campo é
  ciclo de chave.
- Área sob a antena do módulo **sem cobre** (keepout do datasheet); variante
  **-32UE** (U.FL) se a caixa for metálica.

## Conector da placa (CN1 — DLL-5569-12AW, 12 vias)

| Pino | Sinal | Pino | Sinal |
|---|---|---|---|
| 1 | **M−** (motor, retorno) | 2 | **M+** (motor, comutado) |
| 3 | **GND** (sensores/sinais) | 4 | — (NC) |
| 5 | **PWM_IN** (comando 5 V) | 6 | **+12 V** (pós-chave, fusível externo ~5 A) |
| 7 | **TPS_OUT** (saída analógica 0–5 V) | 8 | — (NC) |
| 9 | **TPS1** (cursor pista 1) | 10 | **IDLE_SW** (switch p/ GND) |
| 11 | **TPS2** (cursor pista 2) | 12 | **TPS+** (3V3 via 100 Ω) |

Par do motor junto (1–2) e sinais de sensor agrupados; GND do pino 3 é a
referência dos sensores — no layout, encontrar o GND de potência num ponto só.

## Conector do corpo de borboleta (referência)

O corpo de borboleta a cabo do 1.8T 20v (AGN/APG/AGU família 06A) possui no
conector: motor do atuador (2 fios), potenciômetro (3 fios: 5V/ref, cursor,
GND) e switch de idle (2 fios ou compartilhando GND). **Confirmar a pinagem
real do seu corpo com multímetro antes de ligar** (medir resistência do
potenciômetro entre extremidades ~1–5 kΩ, cursor variando ao mover a borboleta;
motor com resistência baixa ~1–10 Ω; switch mudando de estado no repouso).
