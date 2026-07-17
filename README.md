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

## Lógica de controle

### Malha PID

- O sinal PWM de entrada é medido (duty cycle) e convertido no **setpoint** de
  abertura (0–100% da faixa calibrada do atuador).
- A posição real vem do potenciômetro (TPS), normalizada pela calibração.
- Um PID básico calcula um esforço de controle **com sinal**:
  - Esforço positivo → PWM na ponte H no sentido **abrir**.
  - Esforço negativo → PWM na ponte H no sentido **fechar** (abaixo do repouso).
  - Esforço zero / motor desligado → a mola leva a borboleta à posição de repouso.
- Saturação do integrador (anti-windup), zona morta configurável e limite de
  duty configurável pela página web.

### Regra do switch de idle

**Se o motorista estiver acionando o acelerador (switch de idle aberto), o motor
NÃO é acionado** — a ponte H fica desabilitada/livre e o PID é reinicializado
(integrador zerado) para não acumular erro enquanto o pedal comanda a borboleta
pelo cabo.

### Saída analógica de posição (mascarada)

A saída analógica indica a posição da borboleta em 0–100%, com a seguinte
máscara:

| Condição | Saída |
|---|---|
| Em idle (switch de idle acionado / pedal solto) | **0%** |
| Fora de idle (motorista acelerando) | Replica o valor lido do sensor de posição (0–100% da faixa calibrada) |

Isso faz com que a atuação da marcha lenta fique "invisível" para quem consome
esse sinal, que só enxerga a abertura provocada pelo pedal.

## Auto calibração

Ao energizar o circuito, se o **switch de idle estiver acionado** (pedal solto),
é executada uma rotina de auto calibração para encontrar os três pontos de
referência do sensor de posição:

1. **Repouso** — motor desligado; aguarda estabilizar e registra a leitura do TPS.
2. **Abertura máxima** — motor acionado em 100% no sentido abrir; aguarda a
   leitura estabilizar (fim de curso mecânico) e registra.
3. **Abertura mínima** — motor acionado em 100% no sentido fechar; aguarda
   estabilizar e registra.
4. Motor desligado, borboleta volta ao repouso; valores são validados
   (mín < repouso < máx, faixa mínima plausível) e salvos na NVS (flash).

Se o switch de idle **não** estiver acionado na energização (ou a validação
falhar), a calibração é pulada e são usados os **últimos valores salvos** na NVS.
A calibração também pode ser disparada manualmente pela página web (respeitando
a condição de idle).

## Segurança / failsafe

- Leitura do TPS fora da faixa plausível (sensor desconectado/curto) → motor
  desligado (mola leva ao repouso) e erro sinalizado na página web.
- Perda do sinal PWM de comando (timeout sem bordas) → setpoint tratado como 0%
  (repouso) — comportamento configurável.
- Watchdog de software; qualquer travamento resulta em motor desligado
  (ponte H desabilitada), estado seguro garantido pela mola de retorno.

## Página web (WiFi AP)

O ESP32 sobe um **Access Point WiFi** (ex.: SSID `A3-TBC`, IP `192.168.4.1`)
servindo uma página para debug e parametrização:

- **Monitor ao vivo**: setpoint, posição, duty aplicado, estado do idle switch,
  erro do PID, valores brutos do ADC, estado da calibração.
- **Parâmetros ajustáveis** (persistidos na NVS): ganhos Kp/Ki/Kd, zona morta,
  limites de duty, frequência do PWM de saída, timeout do sinal de comando,
  parâmetros da calibração.
- **Ações**: disparar auto calibração, modo manual (comandar duty diretamente
  para testes de bancada), salvar/restaurar padrões.

## Hardware

Sugestão de pinout, ponte H e circuitos de condicionamento estão documentados em
[`docs/hardware.md`](docs/hardware.md).

## Estrutura planejada do repositório

```
├── README.md            ← este documento
├── docs/
│   └── hardware.md      ← pinout, ligações e condicionamento de sinais
├── platformio.ini       ← projeto PlatformIO (Arduino framework)
└── src/                 ← firmware
    ├── main.cpp
    ├── pins.h           ← definição central do pinout
    ├── pid.*            ← controlador PID
    ├── calibration.*    ← rotina de auto calibração + NVS
    ├── pwm_input.*      ← medição do duty do sinal de comando
    ├── hbridge.*        ← driver da ponte H (LEDC)
    └── webui.*          ← AP WiFi + página de debug/parametrização
```

## Roadmap

- [x] Documentação do projeto e do hardware (pinout)
- [ ] Esqueleto do projeto PlatformIO
- [ ] Leitura do TPS + medição do PWM de entrada + idle switch
- [ ] Driver da ponte H (LEDC, dois sentidos)
- [ ] Rotina de auto calibração + persistência NVS
- [ ] Malha PID + regra do idle switch + failsafes
- [ ] Saída analógica mascarada (DAC)
- [ ] WiFi AP + página web de debug/parametrização
- [ ] Testes em bancada e no veículo
