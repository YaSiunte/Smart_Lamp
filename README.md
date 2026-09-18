# Smart Lamp - CP4 Edge Computing (FIWARE)

Projeto de Check Point 4 da disciplina de Edge Computing (FIAP), utilizando o **FIWARE Descomplicado** como back-end de uma solução de lâmpada inteligente (Smart Lamp), baseada em ESP32.

## Descrição

O projeto simula/implementa uma lâmpada inteligente com:
- **Sensor de luminosidade (LDR)**, publicando o valor lido para o FIWARE via MQTT
- **LED onboard do ESP32**, controlável remotamente (ligar/desligar) via comando enviado pelo FIWARE
- **LED RGB (módulo tipo KY-016)**, que acende (branco) e apaga junto com o LED onboard

## Arquitetura

```
ESP32 (LDR + LED + LED RGB)
      |  MQTT
      v
Broker MQTT (Mosquitto, rodando em EC2 na AWS)
      |
      v
IoT Agent (UltraLight) ---> Orion Context Broker (FIWARE)
      ^
      |  HTTP (PATCH/GET)
   Postman
```

- **Edge → Cloud**: o ESP32 publica a luminosidade lida no LDR, que chega até o Orion Context Broker via MQTT e IoT Agent.
- **Cloud → Edge**: comandos enviados via Postman (PATCH no Orion) são repassados pelo IoT Agent ao ESP32 via MQTT, ligando/desligando o LED onboard e o LED RGB.

## Hardware / Pinagem

| Componente | Pino ESP32 |
|---|---|
| LDR (sensor de luminosidade) | D34 (analógico) |
| LED onboard | D2 |
| LED RGB - R | D32 |
| LED RGB - G | D5 |
| LED RGB - B | D18 |
| LED RGB - COM (GND) | GND |

## Tópicos MQTT

- `/TEF/lamp001/attrs` — estado do LED onboard (`s|on` / `s|off`)
- `/TEF/lamp001/attrs/l` — valor de luminosidade
- `/TEF/lamp001/cmd` — comandos recebidos (`lamp001@on|`, `lamp001@off|`)

## Arquivos

- `sketch.ino` — código do ESP32
- `diagram.json` — circuito do projeto no Wokwi (ESP32 + LDR + LED RGB)
- `wokwi.toml` — configuração do projeto Wokwi

## Como testar

1. Provisionar o FIWARE (Service Group + Device `lamp001`) usando a collection do Postman do FIWARE Descomplicado
2. Rodar a simulação no Wokwi (ou o hardware físico) com o `sketch.ino`
3. Enviar comandos via Postman:
   - `PATCH /v2/entities/urn:ngsi-ld:Lamp:001/attrs` com body `{"on": {"type": "command", "value": ""}}` para ligar
   - Trocar `"on"` por `"off"` para desligar
4. Consultar `GET /v2/entities/urn:ngsi-ld:Lamp:001/attrs/luminosity` para ver o valor de luminosidade

## Links

- Simulação Wokwi: https://wokwi.com/projects/475462423486770177
- Vídeo demonstrativo: https://youtube.com/shorts/xzs3gjGSRzM?feature=share
