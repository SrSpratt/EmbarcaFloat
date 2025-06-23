
# EmbarcaFloat
## Sistema Embarcado de controle de nível, com acionamento de bomba usando a BitdogLab
---
# Objetivo

Sistema embarcado desenvolvido para a plataforma Raspberry Pi Pico W, cujo propósito é monitorar o nível de um reservatório de água utilizando leitura analógica e controlar automaticamente uma bomba hidráulica. O sistema possui:

- Exibição no display OLED (SSD1306)

- Interface Web via servidor TCP

- Conectividade Wi-Fi

- Indicação sonora com buzzer

- Representação visual em matriz de LEDs via PIO

---
# Hardware Utilizado

- Raspberry Pi Pico W 

- Display OLED via I2C

- Buzzer com PWM

- Matriz de LEDs controlada via PIO

- Sensor de nível de água (potenciômetro acoplado a boia)

- Botão físico para modo BOOTSEL

- Módulo Wi-Fi CYW43

---

 # Mapeamento de Pinos


 | Função            | Pino GPIO |
| ----------------- | --------- |
| Leitura ADC       | GPIO 28   |
| Controle da bomba | GPIO 9    |
| Buzzer PWM        | GPIO 21   |
| I2C SDA           | GPIO 14   |
| I2C SCL           | GPIO 15   |
| Botão BOOTSEL     | GPIO 6    |
| LED Matriz (PIO)  | GPIO 7    |

--- 
# Conectividade Wi-Fi

O sistema se conecta automaticamente à rede especificada nas constantes:
```
#define WIFI_SSID "NOME_DA_REDE"
#define WIFI_PASSWORD "SENHA"
```
---

# Interface Web

Servidor TCP integrado que responde a comandos HTTP:

- `GET /level: Retorna o nível de tensão (nível de água).`

- `GET /state: Retorna o estado atual da bomba.`

- `POST /form: Altera os valores de mínimo e máximo do nível via formulário.`

A página HTML exibe o estado da bomba, nível de água e permite ajustar parâmetros via formulário.

---

# Display OLED

O display mostra:

- Estado do Wi-Fi (conectando, conectado, falha)

- Estado da bomba (ligada/desligada)

- Gráfico com o nível de água

- Níveis máximo e mínimo

---

# Tarefas FreeRTOS

O sistema é estruturado em 5 tarefas paralelas, controladas pelo FreeRTOS:

| Tarefa         | Função Principal                                 |
| -------------- | ------------------------------------------------ |
| `vADCReadTask` | Leitura do nível e controle automático da bomba. |
| `vDisplayTask` | Atualização do display OLED.                     |
| `vMatrixTask`  | Controle da matriz de LEDs via PIO.              |
| `vConnectTask` | Conexão à rede Wi-Fi e servidor TCP.             |
| `vBuzzerTask`  | Emissão de alertas sonoros via buzzer.           |

---

# Lógica de Controle da Bomba

A bomba é acionada com base na leitura do ADC:

`
if (adc_reading < LIMITE_INFERIOR) { // liga bomba
    if (!pump_state) set_pump_state();
    pump_state = true;
} else { // desliga bomba
    if (pump_state) set_pump_state();
    pump_state = false;
}

`
--- 
#  Atualização Dinâmica do Nível via Web
O valor lido é atualizado na interface web em tempo real com fetch a cada segundo:
```
setInterval(()=>{
    fetch('/level')
    .then(res => res.text())
    .then(data => {
        document.getElementById("level").innerText = data;
    });
}, 1000);

```
---
# Matriz de LEDs (PIO)
Matriz 5x5 com desenho customizado dependendo do estado do Wi-Fi:

- Conectando: Animação azul

- Conectado: Animação verde

- Falha: Ícone vermelho

---
# Buzzer
- 3 bipes ao ligar a bomba

- 1 bipe ao desligar


