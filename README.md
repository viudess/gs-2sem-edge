# 🧠 Pragma Focus Pod – Projeto IoT

Este repositório contém o módulo de hardware e software do **Pragma Focus Pod**, uma extensão física da plataforma **Pragma – Otimizador de Rotina**, projetada para monitorar foco, presença, ambiente e sessões de trabalho/estudo.

---

## 👥 Participantes do Grupo

- **Eduardo Viudes** – RM: 564075
- **Frederico de Paula** – RM: 562109
- **Victor Tadashi** – RM: 563582

---

## 🎯 Objetivo

Criar um dispositivo inteligente que:

- Detecta presença via sensor ultrassônico  
- Monitora temperatura e umidade  
- Gerencia sessões de foco  
- Simula o nível de foco cognitivo  
- Envia dados ao FIWARE (Orion + IoT-Agent)  
- Fornece feedback por LED, OLED e buzzer  
- Funciona com **Dashboard Web** e **MyMQTT**

---

## 🧩 Tecnologias utilizadas

- ESP32
- Sensor DHT22
- Ultrassônico HC-SR04
- Display OLED SSD1306
- NeoPixel RGB
- Buzzer
- MQTT (Ultralight 2.0)
- FIWARE IoT-Agent JSON/UL
- Orion Context Broker
- HTML, CSS e JavaScript

---

# 🔧 Projetos no Wokwi

<img width="570" height="491" alt="image" src="https://github.com/user-attachments/assets/1389ca44-b46e-42c3-a018-24454805b3ba" />

Cada projeto funciona com um sistema específico:

### ▶ Projeto funcional com Dashboard (Interface Web)
[![Dashboard Funcionando](https://img.shields.io/badge/Projeto%20funcional%20com%20Dashboard-0A84FF?style=for-the-badge&logo=wokwi&logoColor=white)](https://wokwi.com/projects/448160488841329665)

### ▶ Projeto funcional com MyMQTT (Start/End funcionando)
[![MyMQTT Funcionando](https://img.shields.io/badge/Projeto%20funcional%20com%20MyMQTT-C0392B?style=for-the-badge&logo=wokwi&logoColor=white)](https://wokwi.com/projects/448058082408030209)

---

# 📡 Como usar o projeto IoT no ESP32

### 1. Monte o hardware
O esquema está disponível no Wokwi acima.

### 2. Configure o código do ESP32
No início do código, ajustar:
```
WIFI, broker, client ID e tópicos FIWARE:

const char* WIFI_SSID = "SeuWifi";
const char* WIFI_PASS = "Senha";
const char* MQTT_BROKER = "44.223.43.74";

const char* TOPIC_SUB = "TEF/device014/cmd";
const char* TOPIC_ATTR_T = "TEF/device014/attrs/t";
const char* TOPIC_ATTR_H = "TEF/device014/attrs/h";
const char* TOPIC_ATTR_D = "TEF/device014/attrs/d";
const char* TOPIC_ATTR_F = "TEF/device014/attrs/f";
const char* TOPIC_ATTR_ST = "TEF/device014/attrs/s";

const char* CLIENT_ID = "fiware_014";

O ESP32:

📤 Publica temperatura, umidade, presença, foco, distância e status  
📥 Recebe comandos: **start**, **stop**, **pause**, **resume**
```

---

# 🖥 Parte 1 — Dashboard Web (Interface)

<img width="545" height="646" alt="image" src="https://github.com/user-attachments/assets/ab811f74-8b93-4bd4-a0d6-2be6ef391b3a" />

A interface lê dados diretamente do Orion:

GET http://44.223.43.74:1026/v2/entities/urn:ngsi-ld:device:014

E filtra valores UL2.0 do tipo:

```
t|23.5  
h|40.2  
d|31.0  
f|78  
st|active
```

### ✔ Como fazer o Dashboard funcionar

1. Abra este projeto (azul):  
   https://wokwi.com/projects/448160488841329665

2. Rode o ESP32 → verifique no Serial:  
   - Conexão WiFi  
   - Conexão ao MQTT  
   - Publicação dos atributos

3. Abra o Dashboard em um navegador  
4. Confira se os dados aparecem:

- Temperatura  
- Umidade  
- Distância  
- Presença (distância < 40 cm → SIM)  
- Foco  
- Status  

---

# 📱 Parte 2 — Funcionamento com MyMQTT

Este outro projeto funciona com UL2.0 direto no MQTT:

▶ Projeto vermelho:  
https://wokwi.com/projects/448058082408030209

### ✔ Como testar no MyMQTT

1. App MyMQTT → Adicionar conexão:

Broker: test.mosquitto.org  
Porta: 1883  

2. Inscrever no tópico:

- pragma/focuspod/env/user1
- pragma/focuspod/focus_level/user1
- pragma/focuspod/presence/user1
- pragma/focuspod/status/user1

3. Publicar: 

- Tópico:
   - pragma/focuspod/cmd/user1
- Mensagens:
   - {"action":"start_session","type":"focus","duration":25}
   - {"action":"end_session"}

O dispositivo responde imediatamente.

---

# 📄 Como funciona o sistema completo

### ESP32 faz:
- Lê sensores (DHT22 + HC-SR04)  
- Calcula presença pela distância (< 40 cm)  
- Atualiza nível de foco  
- Publica via UL2.0  
- Recebe start/stop do IoT-Agent  

### Dashboard faz:
- Consulta Orion a cada 3s  
- Ajuda na sua rotina de estudos
- Tradução de distância → presença  

### MyMQTT faz:
- Envia mensagens diretas  
- Testa rápido o funcionamento do fluxo MQTT
- Envia comandos para o ESP32

---

# ✔ Conclusão

Este repositório contém:

- Hardware ESP32 totalmente funcional  
- Versão compatível com Dashboard Web  
- Versão compatível com MyMQTT  
- Integração FIWARE completa  
- Suporte para sensores, foco cognitivo e sessões  

---
