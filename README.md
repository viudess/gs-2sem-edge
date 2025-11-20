# 🧠 Pragma Focus Pod – Projeto IoT

Esse repositório corresponde ao módulo de hardware e software do **Pragma Focus Pod**, uma extensão física da plataforma **Pragma – Otimizador de Rotina**, projetada para monitorar foco, presença, ambiente e sessões de trabalho/estudo.

---

## 👥 Participantes do Grupo

- **Eduardo Viudes** – RM: 564075  
- **Frederico de Paula** – RM: 562109 
- **Victor Tadashi** – RM: 563582  

---

## 🎯 Objetivo

Criar um dispositivo de mesa inteligente que:

- Detecta presença do usuário via sensor ultrassônico  
- Monitora qualidade do ambiente (temperatura, umidade)  
- Gerencia sessões (foco / pausa) automaticamente  
- Simula nível de foco cognitivo  
- Comunica-se via MQTT/FIWARE-IoT-Agent para um dashboard  
- Fornece feedback visual (OLED + Neopixel) e sonoro (buzzer)

---

## 🧩 Tecnologias utilizadas

- ESP32  
- DHT22  
- HC-SR04  
- SSD1306 OLED 128×64  
- NeoPixel  
- Buzzer  
- MQTT (Ultralight 2.0 ou JSON MQTT)  
- FIWARE IoT-Agent + Orion  
- Dashboard em HTML/JS

---

## 🔧 Projeto no Wokwi

<img width="579" height="533" alt="image" src="https://github.com/user-attachments/assets/fd336787-b91c-4648-b411-a111c46af3ec" />

[![Abrir no Wokwi](https://img.shields.io/badge/▶%20Abrir%20no%20Wokwi-0A84FF?style=for-the-badge&logo=wokwi&logoColor=white)](https://wokwi.com/projects/448160488841329665)

---

## 💾 Como usar o projeto

### 1. Monte o hardware
Conecte todos os sensores conforme o esquema acima.  
Ligue o ESP32 no USB do computador.

---

## 2. Configure o código do ESP32

No início do código, defina:

- Rede WiFi  
- Broker MQTT  
- Tópicos (segundo a estrutura do FIWARE ou Mosquitto)

Exemplo:

const char* WIFI_SSID = "SeuWifi";  
const char* WIFI_PASS = "Senha";  
const char* MQTT_BROKER = "44.223.43.74";  
const char* TOPIC_PUB = "TEF/device014/attrs/b";  
const char* TOPIC_SUB = "TEF/device014/cmd";  
const char* CLIENT_ID = "fiware_014";

O ESP32:

- **Publica** temperatura, umidade, presença, nível de foco  
- **Recebe** comandos como start_session, end_session, pause, resume

---

## 3. Executando o sistema

1. Abra o **Arduino IDE**  
2. Carregue o código no ESP32  
3. Abra o **Serial Monitor**  
4. Verifique:  
   - WiFi conectando  
   - MQTT conectando  
   - Publicação dos dados  
5. Abra o dashboard / interface web  
6. Veja em tempo real:  
   - Temperatura  
   - Umidade  
   - Presença  
   - Nível de foco  
   - Status da sessão

---

## 4. Dashboard / Interface Web

A interface utiliza:

- HTML  
- CSS  
- JavaScript  
- fetch() para receber dados do FIWARE / Orion  
- Botões para enviar comandos (start, end)

Exemplo de chamada:

fetch("http://44.223.43.74:1026/v2/entities/urn:ngsi-ld:device:014")

---

## 📸 Imagens do Projeto

As imagens devem ser adicionadas na pasta `/assets` no repositório.  
Exemplos sugeridos:

- Foto do dispositivo montado  
- Foto da tela OLED com as informações  
- Tela da interface web funcionando  
- Fluxo do MQTT (diagrama)  
- Esquema elétrico real do circuito  

---
