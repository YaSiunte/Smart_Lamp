#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

const char* default_SSID = "Wokwi-GUEST";
const char* default_PASSWORD = "";
const char* default_BROKER_MQTT = "54.80.122.222";
const int default_BROKER_PORT = 1883;

const char* default_TOPICO_SUBSCRIBE = "/TEF/lamp001/cmd";
const char* default_TOPICO_PUBLISH_1 = "/TEF/lamp001/attrs";
const char* default_TOPICO_PUBLISH_2 = "/TEF/lamp001/attrs/l";
const char* default_TOPICO_PUBLISH_3 = "/TEF/lamp001/attrs/rgb";
const char* TOPICO_CMDEXE = "/TEF/lamp001/cmdexe";

const char* default_ID_MQTT = "fiware_001";
const int default_D4 = 2;

// LED RGB catodo comum: R=32, G=5, B=18 (comum no GND)
const int PINO_R = 32;
const int PINO_G = 5;
const int PINO_B = 18;

const int CANAL_R = 0;
const int CANAL_G = 1;
const int CANAL_B = 2;

const char* topicPrefix = "lamp001";

// --- Limiar de luminosidade (0 a 100) abaixo do qual o RGB apaga sozinho ---
const int LUMINOSITY_THRESHOLD = 20;

char* SSID = const_cast<char*>(default_SSID);
char* PASSWORD = const_cast<char*>(default_PASSWORD);
char* BROKER_MQTT = const_cast<char*>(default_BROKER_MQTT);
int BROKER_PORT = default_BROKER_PORT;
char* TOPICO_SUBSCRIBE = const_cast<char*>(default_TOPICO_SUBSCRIBE);
char* TOPICO_PUBLISH_1 = const_cast<char*>(default_TOPICO_PUBLISH_1);
char* TOPICO_PUBLISH_2 = const_cast<char*>(default_TOPICO_PUBLISH_2);
char* TOPICO_PUBLISH_3 = const_cast<char*>(default_TOPICO_PUBLISH_3);
char* ID_MQTT = const_cast<char*>(default_ID_MQTT);
int D4 = default_D4;

WiFiClient espClient;
PubSubClient MQTT(espClient);
char EstadoSaida = '0';

// Cor escolhida pelo usuario (mantida mesmo com a lampada desligada)
int corEscolhidaR = 255;
int corEscolhidaG = 255;
int corEscolhidaB = 255;

// Cor que esta fisicamente acesa no LED agora
int corAtualR = 0;
int corAtualG = 0;
int corAtualB = 0;

// Flag: verdadeiro quando o RGB foi apagado automaticamente por pouca luz
bool apagadoPorLuminosidade = false;

// --- Prototipos das funcoes (necessario em .cpp, diferente do .ino) ---
void initSerial();
void initWiFi();
void initMQTT();
void reconectWiFi();
void escreveRGB(int r, int g, int b);
void aplicaEstadoRGB();
void mqtt_callback(char* topic, byte* payload, unsigned int length);
void VerificaConexoesWiFIEMQTT();
void EnviaEstadoOutputMQTT();
void InitOutput();
void reconnectMQTT();
void handleLuminosity();

void initSerial() {
    Serial.begin(115200);
}

void initWiFi() {
    delay(10);
    Serial.println("------Conexao WI-FI------");
    Serial.print("Conectando-se na rede: ");
    Serial.println(SSID);
    Serial.println("Aguarde");
    reconectWiFi();
}

void initMQTT() {
    MQTT.setServer(BROKER_MQTT, BROKER_PORT);
    MQTT.setCallback(mqtt_callback);
}

void setup() {
    InitOutput();
    initSerial();
    initWiFi();
    initMQTT();
    delay(5000);
    MQTT.publish(TOPICO_PUBLISH_1, "s|on");
}

void loop() {
    VerificaConexoesWiFIEMQTT();
    EnviaEstadoOutputMQTT();
    handleLuminosity();
    MQTT.loop();
}

void reconectWiFi() {
    if (WiFi.status() == WL_CONNECTED)
        return;
    WiFi.begin(SSID, PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(100);
        Serial.print(".");
    }
    Serial.println();
    Serial.println("Conectado com sucesso na rede ");
    Serial.print(SSID);
    Serial.println("IP obtido: ");
    Serial.println(WiFi.localIP());
    digitalWrite(D4, LOW);
}

void escreveRGB(int r, int g, int b) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(PINO_R, r);
    ledcWrite(PINO_G, g);
    ledcWrite(PINO_B, b);
#else
    ledcWrite(CANAL_R, r);
    ledcWrite(CANAL_G, g);
    ledcWrite(CANAL_B, b);
#endif
    corAtualR = r;
    corAtualG = g;
    corAtualB = b;
}

// Acende o RGB com a cor escolhida apenas se a lampada estiver ligada
// E somente se a luminosidade ambiente nao estiver baixa (ver handleLuminosity)
void aplicaEstadoRGB() {
    if (EstadoSaida == '1' && !apagadoPorLuminosidade) {
        escreveRGB(corEscolhidaR, corEscolhidaG, corEscolhidaB);
    } else {
        escreveRGB(0, 0, 0);
    }
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    String msg;
    for (int i = 0; i < length; i++) {
        msg += (char)payload[i];
    }
    Serial.print("- Mensagem recebida: ");
    Serial.println(msg);

    String onTopic = String(topicPrefix) + "@on|";
    String offTopic = String(topicPrefix) + "@off|";

    if (msg.startsWith(onTopic)) {
        digitalWrite(D4, HIGH);
        EstadoSaida = '1';
        aplicaEstadoRGB();
        MQTT.publish(TOPICO_CMDEXE, "lamp001@on|OK");
    }

    if (msg.startsWith(offTopic)) {
        digitalWrite(D4, LOW);
        EstadoSaida = '0';
        aplicaEstadoRGB();
        MQTT.publish(TOPICO_CMDEXE, "lamp001@off|OK");
    }

    // Comando de cor: lamp001@rgb|R,G,B
    String rgbPrefix = String(topicPrefix) + "@rgb|";
    if (msg.startsWith(rgbPrefix)) {
        String valores = msg.substring(rgbPrefix.length());
        valores.replace("|", "");
        valores.trim();

        int primeiraVirgula = valores.indexOf(',');
        int segundaVirgula = valores.indexOf(',', primeiraVirgula + 1);

        if (primeiraVirgula > 0 && segundaVirgula > primeiraVirgula) {
            corEscolhidaR = constrain(valores.substring(0, primeiraVirgula).toInt(), 0, 255);
            corEscolhidaG = constrain(valores.substring(primeiraVirgula + 1, segundaVirgula).toInt(), 0, 255);
            corEscolhidaB = constrain(valores.substring(segundaVirgula + 1).toInt(), 0, 255);

            aplicaEstadoRGB();
            MQTT.publish(TOPICO_CMDEXE, "lamp001@rgb|OK");
            Serial.printf("- LED RGB atualizado para: R=%d G=%d B=%d\n", corEscolhidaR, corEscolhidaG, corEscolhidaB);
        } else {
            Serial.println("- Comando de cor recebido em formato invalido.");
            MQTT.publish(TOPICO_CMDEXE, "lamp001@rgb|ERRO");
        }
    }
}

void VerificaConexoesWiFIEMQTT() {
    if (!MQTT.connected())
        reconnectMQTT();
    reconectWiFi();
}

void EnviaEstadoOutputMQTT() {
    if (EstadoSaida == '1') {
        MQTT.publish(TOPICO_PUBLISH_1, "s|on");
        Serial.println("- Led Ligado");
    }
    if (EstadoSaida == '0') {
        MQTT.publish(TOPICO_PUBLISH_1, "s|off");
        Serial.println("- Led Desligado");
    }

    String corMsg = String(corAtualR) + "-" + String(corAtualG) + "-" + String(corAtualB);
    MQTT.publish(TOPICO_PUBLISH_3, corMsg.c_str());

    Serial.println("- Estados enviados ao broker!");
    delay(1000);
}

void InitOutput() {
    pinMode(D4, OUTPUT);
    digitalWrite(D4, HIGH);
    boolean toggle = false;
    for (int i = 0; i <= 10; i++) {
        toggle = !toggle;
        digitalWrite(D4, toggle);
        delay(200);
    }

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttach(PINO_R, 5000, 8);
    ledcAttach(PINO_G, 5000, 8);
    ledcAttach(PINO_B, 5000, 8);
#else
    ledcSetup(CANAL_R, 5000, 8);
    ledcSetup(CANAL_G, 5000, 8);
    ledcSetup(CANAL_B, 5000, 8);
    ledcAttachPin(PINO_R, CANAL_R);
    ledcAttachPin(PINO_G, CANAL_G);
    ledcAttachPin(PINO_B, CANAL_B);
#endif

    escreveRGB(0, 0, 0);
}

void reconnectMQTT() {
    while (!MQTT.connected()) {
        Serial.print("* Tentando se conectar ao Broker MQTT: ");
        Serial.println(BROKER_MQTT);
        if (MQTT.connect(ID_MQTT)) {
            Serial.println("Conectado com sucesso ao broker MQTT!");
            if (MQTT.subscribe(TOPICO_SUBSCRIBE)) {
                Serial.print("- Inscrito no topico: ");
                Serial.println(TOPICO_SUBSCRIBE);
            } else {
                Serial.println("- Falha ao se inscrever no topico MQTT.");
            }
        } else {
            Serial.println("Falha ao reconectar no broker.");
            Serial.println("Havera nova tentativa de conexao em 2s");
            delay(2000);
        }
    }
}

void handleLuminosity() {
    const int potPin = 34;
    int sensorValue = analogRead(potPin);
    int luminosity = map(sensorValue, 0, 4095, 0, 100);
    String mensagem = String(luminosity);
    Serial.print("Valor da luminosidade: ");
    Serial.println(mensagem.c_str());
    MQTT.publish(TOPICO_PUBLISH_2, mensagem.c_str());

    // --- Apaga o RGB automaticamente quando a luminosidade esta baixa ---
    if (luminosity < LUMINOSITY_THRESHOLD) {
        if (!apagadoPorLuminosidade) {
            apagadoPorLuminosidade = true;
            aplicaEstadoRGB(); // forca off, mas preserva corEscolhida para depois
            Serial.println("- Luminosidade baixa: RGB apagado automaticamente.");
        }
    } else {
        if (apagadoPorLuminosidade) {
            apagadoPorLuminosidade = false;
            aplicaEstadoRGB(); // restaura a cor escolhida, se a lampada estiver 'on'
            Serial.println("- Luminosidade normalizada: RGB restaurado (se lampada ligada).");
        }
    }
}
