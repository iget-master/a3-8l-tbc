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
| Sinal PWM de comando (alvo %) | **GPIO35** | Entrada digital (somente entrada) | Passar por condicionamento de nível (ver abaixo) antes de chegar ao pino |
| Switch de idle | **GPIO32** | Entrada digital com pull-up interno | Switch fechando para **GND** quando pedal solto (em idle → nível baixo) |
| Ponte H — IN1 (sentido **abrir**) | **GPIO18** | Saída PWM (LEDC) | |
| Ponte H — IN2 (sentido **fechar**) | **GPIO19** | Saída PWM (LEDC) | |
| Ponte H — Enable/Sleep (se o driver tiver) | **GPIO21** | Saída digital | Manter em nível seguro (desabilitado) no boot |
| Corrente da ponte (R_IS+L_IS do IBT-2) | **GPIO33** | Entrada analógica (ADC1_CH5) | Rede resistiva + RC — ver seção do IBT-2 abaixo |
| Saída analógica de posição 0–100% | **GPIO25** | Saída DAC (DAC1) | 0–3,3 V; usar buffer/amplificador se o consumidor esperar 0–5 V (ver abaixo) |
| LED de status | **GPIO2** | Saída | LED onboard do DevKit (heartbeat / erro / calibrando) |

Pinos livres para expansão futura: GPIO16, 17, 22, 23, 26 (DAC2), 27.

## Diagrama de ligação

```
                         ESP32 DevKit V1
                       ┌─────────────────┐
   PWM comando ──►[cond.]──► GPIO35      │
   Idle switch ──►[cond.]──► GPIO32      │          ┌────────────┐   ┌───────────────┐
                       │      GPIO18 ────┼─────────►│ RPWM (IN1) │   │ Motor do      │
   TPS cursor ────────►│ GPIO34          │          │  Ponte H   ├──►│ atuador       │
   TPS 3V3 ◄───────────┤ 3V3             │          │  (IBT-2)   │   │ (borboleta)   │
   TPS GND ◄───────────┤ GND  GPIO19 ────┼─────────►│ LPWM (IN2) │   └───────────────┘
                       │      GPIO21 ────┼─────────►│ R_EN+L_EN  │
                       │      GPIO33 ◄───┼─[680Ω+RC]┤ R_IS+L_IS  │
                       │                 │          └─────┬──────┘
   Saída 0–100% ◄──[buffer]── GPIO25     │                │ VMOT = 12 V pós-chave
                       │                 │                │ (fusível + TVS — ver Alimentação)
                       └─────────────────┘
```

## Ponte H

O motor de marcha lenta desses corpos de borboleta consome tipicamente 1–3 A.
Sugestões de driver:

| Driver | Corrente | Observações |
|---|---|---|
| **DRV8871** (recomendado) | 3,6 A pico | 2 pinos (IN1/IN2), limite de corrente ajustável por resistor, proteção térmica/curto |
| BTS7960 (módulo IBT-2) | 43 A | Superdimensionado, mas robusto e barato |
| L298N | 2 A | Evitar: queda de tensão alta e baixa eficiência |

Esquema de acionamento com IN1/IN2 (LEDC ~20 kHz para ficar inaudível):

| Ação | IN1 | IN2 |
|---|---|---|
| Abrir (proporcional) | PWM | 0 |
| Fechar (proporcional) | 0 | PWM |
| Livre (coast) — mola leva ao repouso | 0 | 0 |

A alimentação do motor (VMOT) vem do **nó pós-chave protegido** (relé + TVS +
fusível próprio de ~5 A — topologia na seção Alimentação). GND do driver comum
com o GND do ESP32.

### Módulo IBT-2 (2× BTS7960) — ligação e sensor de corrente

Mapeamento do header do IBT-2 para o pinout acima:

| IBT-2 | Liga em | Observação |
|---|---|---|
| RPWM | GPIO18 (IN1) | PWM do sentido **abrir** |
| LPWM | GPIO19 (IN2) | PWM do sentido **fechar** |
| R_EN + L_EN (juntos) | GPIO21 (EN) | Enable único — baixo = ponte dormindo |
| VCC | 3V3 | Lógica em 3,3 V funciona no BTS7960 |
| GND | GND | Comum com o ESP32 |
| B+ / B− | 12 V protegido / GND | Fusível + TVS |
| M+ / M− | Motor do atuador | |
| **R_IS + L_IS (juntos)** | nó **ISENSE** | Sensor de corrente — rede abaixo |

O acionamento é sign-magnitude (PWM em um lado por vez), então **só o lado
ativo injeta corrente no IS** — os dois pinos podem ser somados num nó único.

**Rede do ISENSE → GPIO33** (o IS espelha a corrente do motor:
`I_IS = I_motor / 8500`; em falha interna do driver injeta ~4,5 mA fixos):

```
R_IS ─┬─ L_IS
      │
      ├── R_IS→GND: 680 Ω (ver nota do resistor onboard)
      ├── série 1 kΩ ──► GPIO33
      └─ no GPIO33: 100 nF → GND   (RC ≈ 100 µs — média o PWM)
```

- **680 Ω** põe a assinatura de falha do driver (4,5 mA) em ~3,06 V — dentro do
  ADC sem clamp — e o stall do motor (2–4 A) em 160–320 mV.
- **Nota do resistor onboard**: muitas revisões do IBT-2 já trazem resistor de
  IS para GND (1 kΩ ou 10 kΩ, varia por lote). **Medir com multímetro
  (desenergizado) de R_IS para GND** e compor o paralelo para ~680 Ω: onboard
  1 kΩ → externo 2,2 kΩ; onboard 10 kΩ → externo 750 Ω; sem onboard → 680 Ω.
- Como o IS só conduz com o high-side ligado, a tensão média escala com o duty;
  o firmware normaliza para 100% de duty antes de comparar os limiares
  (parâmetros `is*` na página web). Referência com 680 Ω: counts ≈ 1,24/mV →
  1 A ≈ 100 counts; stall ≈ 200–400; curto ≈ saturado (>3000); desconectado ≈ 0.

## Condicionamento de sinais

### Sinal PWM de comando → GPIO35

Se o sinal vier em 5 V ou 12 V, **não pode** entrar direto no ESP32 (máx. 3,3 V):

- **5 V:** divisor resistivo 10 kΩ / 20 kΩ (5 V → ~3,3 V), com resistor de série.
- **12 V:** preferir **optoacoplador** (ex.: PC817 + resistor de base ~2,2 kΩ) ou
  divisor 33 kΩ / 10 kΩ + diodo zener 3,3 V de proteção. O optoacoplador também
  isola o circuito e ignora transientes do chicote.

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
- Resistor de série de 1 kΩ e capacitor de 100 nF do pino para GND (filtro
  anti-aliasing/ruído).
- No firmware: média/mediana de várias amostras + atenuação `ADC_11db`.

### Saída analógica → GPIO25 (DAC1)

O DAC entrega 0–3,3 V com pouca capacidade de corrente:

- Se o consumidor aceita 0–3,3 V: apenas um **buffer** (op-amp seguidor, ex.:
  MCP6002 alimentado em 3,3 V, entrada/saída rail-to-rail).
- Se o consumidor espera **0–5 V**: op-amp não inversor com ganho 1,52
  (ex.: MCP6002/LM358 alimentado em 5 V ou 12 V, R1 = 10 kΩ para GND,
  Rf = 5,2 kΩ ≈ 5,1 kΩ).

## Alimentação

Topologia: **um relé pós-chave** alimenta um nó de 12 V protegido, do qual saem
**dois ramos com fusíveis separados** — eletrônica e ponte H. **Nada liga
direto na bateria**, inclusive o B+ do IBT-2: a ponte é inútil sem o
controlador (que precisa ser pós-chave pelo consumo de ~100 mA), e B+ sempre
vivo criaria o risco de uma falha física (ex.: chicote roçado curtando RPWM ao
12 V) acionar o motor com o carro estacionado e sem supervisão.

```
12 V bateria ──[relé pós-chave 10 A]──┬── TVS SMBJ24A → GND
                                      │
                     ┌────────────────┴────────────────┐
               [fusível ~1 A]                    [fusível ~5 A]
                     │                                 │
        [diodo reverso] → buck LM2596-5.0        B+ / B− do IBT-2
                     │        │                  (sem diodo série: 3–4 A
        op-amp do DAC ◄───────┼─ 5 V              dissiparia ~3 W; usar
        (se versão 5 V)       ▼                   conector com chaveamento
                        VIN/5V do DevKit          mecânico contra inversão)
                              │
                        AMS1117-3.3 onboard → 3,3 V (ESP32, TPS, VCC do IBT-2)
```

- **12 V → 5 V: buck LM2596S-5.0** (módulo pronto; em placa própria, TPS5430).
  Entrada absoluta de 40–45 V fica **acima do clamp do TVS** — o SMBJ24A deixa
  passar até ~39 V num load dump. Por isso **não usar MP1584** (28 V abs. máx.,
  abaixo do clamp) **nem 7805 no veículo**: além dos 35 V abs. máx. (também
  abaixo do clamp), um linear dissiparia (14 − 5) V × 0,3–0,5 A = **2,7–4,5 W**
  num cofre a 60–80 °C. Em **bancada** (fonte limpa, ambiente frio), um 7805
  com dissipador serve.
- **5 V → 3,3 V**: o **AMS1117-3.3 onboard do DevKit** (alimentando pelo pino
  VIN/5V). Em placa própria sem DevKit, usar o mesmo AMS1117-3.3.
- **Sequenciamento é indiferente**: os pull-downs do IBT-2 mantêm a ponte
  dormindo com o ESP32 morto; ESP32 vivo com B+ morto aciona uma ponte sem
  energia — nenhuma ordem é perigosa.

### Comportamento no desligamento (desenergizado = estado seguro)

1. ESP32 sem alimentação → GPIO18/19/21 em alta impedância.
2. Os pull-downs do IBT-2 seguram RPWM/LPWM/EN em nível baixo → BTS7960 com os
   quatro chaveamentos abertos.
3. Saída da ponte em alta impedância — **coast, não freio** (a fcem do motor
   não vence os diodos de corpo): motor solto de verdade.
4. **A mola leva a borboleta ao batente mecânico de marcha lenta** — o carro
   volta a se comportar como corpo a cabo "burro": marcha lenta fixa no ajuste
   mecânico, dirigível normalmente.
5. O DAC vai a 0 V = 0%, coerente com a saída mascarada (em idle já é 0%).

O mesmo vale nos transientes: *brownout* do ESP32 solta os GPIOs (→ pull-downs
→ ponte desabilitada) e o boot inicializa com EN em nível baixo antes de
qualquer PWM (`hbridge::begin`); GPIO18/19/21 não são pinos de *strapping* com
glitch alto no reset. Ligando, desligando ou morrendo no meio de um movimento,
todo caminho termina em **ponte desabilitada + mola no repouso**.

## Conector do corpo de borboleta (referência)

O corpo de borboleta a cabo do 1.8T 20v (AGN/APG/AGU família 06A) possui no
conector: motor do atuador (2 fios), potenciômetro (3 fios: 5V/ref, cursor,
GND) e switch de idle (2 fios ou compartilhando GND). **Confirmar a pinagem
real do seu corpo com multímetro antes de ligar** (medir resistência do
potenciômetro entre extremidades ~1–5 kΩ, cursor variando ao mover a borboleta;
motor com resistência baixa ~1–10 Ω; switch mudando de estado no repouso).
