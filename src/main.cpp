#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <addons/RTDBHelper.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SparkFun_APDS9960.h>
#include <SparkFunHTU21D.h>

// ════════════════════════════════════════════════════════════════
//   CONFIGURAÇÕES — edite aqui
// ════════════════════════════════════════════════════════════════
#define WIFI_SSID       "uaifai-brum"
#define WIFI_PASSWORD   "bemvindoaocesar"
#define DATABASE_URL    "https://mpes-g2-default-rtdb.firebaseio.com/"
#define DATABASE_SECRET "3NdS9ZSZAu0QdbfRMbQO8OGrBNBPnilEoltWuoeE"

// Limites de temperatura (°C)
#define TEMP_COLD    18.0f   // abaixo: alarme frio   (LED azul)
#define TEMP_WARN    27.0f   // acima:  alarme quente (LED amarelo)
#define TEMP_CRIT    35.0f   // acima:  CRÍTICO       (LED vermelho + bip)

// Buzzer — false = somente LED; true = buzzer em D33
#define USE_BUZZER   false
#define BUZZER_PIN   33

// Intervalo de envio ao Firebase (ms)
#define INTERVALO_FIREBASE 5000UL
// Intervalo de leitura do sensor (ms)
#define INTERVALO_SENSOR   3000UL
// Troca automática de tela OLED (ms)
#define INTERVALO_TELA    10000UL

// ════════════════════════════════════════════════════════════════
//   PINOS
// ════════════════════════════════════════════════════════════════
#define LED_R   19
#define LED_G   23
#define LED_B   18
#define BTN_PIN 27

// ════════════════════════════════════════════════════════════════
//   OBJETOS
// ════════════════════════════════════════════════════════════════
Adafruit_SSD1306  display(128, 64, &Wire, -1);
SparkFun_APDS9960 apds;
HTU21D            htu21d;

FirebaseData   fbdo;
FirebaseAuth   auth;
FirebaseConfig config;

// ════════════════════════════════════════════════════════════════
//   ESTADO GLOBAL
// ════════════════════════════════════════════════════════════════
// Ambiente
float g_temp = 0, g_umid = 0;

// Pessoas
int g_entradas = 0, g_saidas = 0;
inline int g_total() { return g_entradas - g_saidas; }

// Alarme
enum EstadoAlarme { AL_NORMAL = 0, AL_FRIO, AL_QUENTE, AL_CRITICO };
EstadoAlarme g_alarme         = AL_NORMAL;
bool         g_alarmeAtivo    = false;
bool         g_alarmeLogado   = false;
unsigned long g_tInicioAlarme = 0;

// Display
enum Tela { TELA_TEMP = 0, TELA_PESSOAS, TELA_STATUS, TELA_COUNT };
Tela g_tela = TELA_TEMP;

// Temporização
unsigned long t_sensor   = 0;
unsigned long t_firebase = 0;
unsigned long t_tela     = 0;
unsigned long t_pisca    = 0;
bool piscaEstado         = false;
bool btnAnterior         = HIGH;

// ════════════════════════════════════════════════════════════════
//   HELPERS — LED RGB
// ════════════════════════════════════════════════════════════════
void setRGB(bool r, bool g, bool b) {
    digitalWrite(LED_R, r ? LOW : HIGH);
    digitalWrite(LED_G, g ? LOW : HIGH);
    digitalWrite(LED_B, b ? LOW : HIGH);
}

void ledStatus() {
    // LED fixo reflete estado (chamado quando não está piscando)
    switch (g_alarme) {
        case AL_NORMAL:  setRGB(false, true,  false); break;  // verde
        case AL_FRIO:    setRGB(false, false, true);  break;  // azul
        case AL_QUENTE:  setRGB(true,  true,  false); break;  // amarelo
        case AL_CRITICO: setRGB(true,  false, false); break;  // vermelho
    }
}

// ════════════════════════════════════════════════════════════════
//   HELPERS — BUZZER
// ════════════════════════════════════════════════════════════════
void bipa(int freq, int durMs) {
#if USE_BUZZER
    tone(BUZZER_PIN, freq, durMs);
#endif
    delay(durMs);
#if USE_BUZZER
    noTone(BUZZER_PIN);
#endif
}

void padraoAlarme() {
    bipa(1000, 100); delay(60);
    bipa(1000, 100); delay(60);
    bipa(1000, 100); delay(60);
    bipa(800,  500);
}

// ════════════════════════════════════════════════════════════════
//   HELPERS — DISPLAY OLED
// ════════════════════════════════════════════════════════════════
void oledTelaTemp() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("  TEMPERATURA/UMID  ");
    display.drawLine(0, 9, 127, 9, SSD1306_WHITE);

    // Temperatura grande
    display.setTextSize(3);
    display.setCursor(0, 16);
    display.printf("%.1f", g_temp);
    display.setTextSize(1);
    display.setCursor(88, 18);
    display.println("o");
    display.setTextSize(2);
    display.setCursor(93, 22);
    display.println("C");

    // Umidade
    display.setTextSize(2);
    display.setCursor(0, 46);
    display.printf("%.0f%%", g_umid);
    display.setTextSize(1);
    display.setCursor(46, 52);
    display.println("Umidade");

    // Indicador de alarme
    if (g_alarme != AL_NORMAL) {
        const char* alertas[] = {"", "FRIO", "QUENTE", "CRITICO"};
        display.setTextSize(1);
        display.setCursor(80, 46);
        display.println(alertas[g_alarme]);
    }
    display.display();
}

void oledTelaPessoas() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("  OCUPACAO DA SALA  ");
    display.drawLine(0, 9, 127, 9, SSD1306_WHITE);

    int total = g_total();
    display.setTextSize(4);
    int digitos = (total < 10) ? 1 : (total < 100 ? 2 : 3);
    display.setCursor((128 - digitos * 24) / 2, 14);
    display.printf("%d", total);

    display.setTextSize(1);
    display.setCursor(0, 54);
    display.printf("Ent:%d  Sai:%d  Sala:%d",
                   g_entradas, g_saidas, total);
    display.display();
}

void oledTelaStatus() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("   STATUS SISTEMA   ");
    display.drawLine(0, 9, 127, 9, SSD1306_WHITE);

    display.setCursor(0, 12);
    display.print("Wi-Fi: ");
    display.println(WiFi.status() == WL_CONNECTED ? "CONECTADO" : "OFFLINE");

    display.setCursor(0, 22);
    display.print("IP: ");
    display.println(WiFi.localIP().toString());

    display.setCursor(0, 32);
    display.print("RSSI: ");
    display.print(WiFi.RSSI());
    display.println(" dBm");

    display.setCursor(0, 42);
    display.printf("T:%.1fC  U:%.0f%%  P:%d",
                   g_temp, g_umid, g_total());

    const char* estadoStr[] = {"NORMAL", "FRIO", "QUENTE", "CRITICO"};
    display.setCursor(0, 54);
    display.print("Alarme: ");
    display.println(estadoStr[g_alarme]);

    display.display();
}

void oledAlarmeCritico() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    // Cabeçalho invertido (pisca)
    if (piscaEstado) {
        display.fillRect(0, 0, 128, 12, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
    }
    display.setTextSize(1);
    display.setCursor(4, 2);
    display.println("!!! TEMPERATURA CRITICA !!!");
    display.setTextColor(SSD1306_WHITE);

    display.setTextSize(3);
    display.setCursor(5, 16);
    display.printf("%.1f", g_temp);
    display.setTextSize(1);
    display.setCursor(86, 18);
    display.println("o");
    display.setTextSize(2);
    display.setCursor(92, 22);
    display.println("C");

    display.setTextSize(1);
    display.setCursor(0, 46);
    display.printf("Lim: %.0fC  Umid: %.0f%%", TEMP_CRIT, g_umid);

    display.setCursor(0, 56);
    unsigned long dec = (millis() - g_tInicioAlarme) / 1000;
    display.printf("Alarme ha: %lus", dec);

    display.display();
}

void atualizaDisplay() {
    if (g_alarme == AL_CRITICO) {
        oledAlarmeCritico();
        return;
    }
    switch (g_tela) {
        case TELA_TEMP:    oledTelaTemp();    break;
        case TELA_PESSOAS: oledTelaPessoas(); break;
        case TELA_STATUS:  oledTelaStatus();  break;
        default: break;
    }
}

// ════════════════════════════════════════════════════════════════
//   FIREBASE — Envio completo
// ════════════════════════════════════════════════════════════════
void enviaFirebase() {
    if (!Firebase.ready()) return;

    // Ambiente
    Firebase.setFloat(fbdo, "/ambiente/temperatura", g_temp);
    Firebase.setFloat(fbdo, "/ambiente/umidade",     g_umid);

    // Pessoas
    Firebase.setInt(fbdo, "/pessoas/entradas", g_entradas);
    Firebase.setInt(fbdo, "/pessoas/saidas",   g_saidas);
    Firebase.setInt(fbdo, "/pessoas/total",    g_total());

    // Alarme
    const char* estadoStr[] = {"normal", "frio", "quente", "critico"};
    Firebase.setString(fbdo, "/alarme/estado",      estadoStr[g_alarme]);
    Firebase.setBool  (fbdo, "/alarme/ativo",        g_alarmeAtivo);
    Firebase.setFloat (fbdo, "/alarme/temp_alarme",  g_temp);

    Serial.printf("[Firebase] T:%.1f U:%.1f P:%d Alarme:%s\n",
                  g_temp, g_umid, g_total(), estadoStr[g_alarme]);
}

void registraHistoricoAlarme() {
    if (!Firebase.ready() || g_alarmeLogado) return;
    String path = "/alarme/historico/";
    path += millis();
    FirebaseJson json;
    json.set("temperatura", g_temp);
    json.set("umidade",     g_umid);
    json.set("pessoas",     g_total());
    json.set("timestamp",   (int)(millis() / 1000));
    Firebase.updateNode(fbdo, path, json);
    g_alarmeLogado = true;
    Serial.println("[Firebase] Alarme crítico registrado no histórico.");
}

// ════════════════════════════════════════════════════════════════
//   LEITURA DE SENSORES
// ════════════════════════════════════════════════════════════════
void atualizaAlarme() {
    EstadoAlarme novoEstado;
    if      (g_temp >= TEMP_CRIT) novoEstado = AL_CRITICO;
    else if (g_temp >  TEMP_WARN) novoEstado = AL_QUENTE;
    else if (g_temp <  TEMP_COLD) novoEstado = AL_FRIO;
    else                           novoEstado = AL_NORMAL;

    if (novoEstado != g_alarme) {
        Serial.printf("[Alarme] Mudança: %d -> %d (%.1f°C)\n",
                      (int)g_alarme, (int)novoEstado, g_temp);
        g_alarme = novoEstado;
    }

    bool deveAlarmar = (g_alarme == AL_CRITICO);

    if (deveAlarmar && !g_alarmeAtivo) {
        g_alarmeAtivo   = true;
        g_tInicioAlarme = millis();
        g_alarmeLogado  = false;
        Serial.printf("[ALARME CRÍTICO] %.1f°C!\n", g_temp);
        padraoAlarme();
        registraHistoricoAlarme();
    } else if (!deveAlarmar && g_alarmeAtivo) {
        g_alarmeAtivo = false;
        Serial.println("[Alarme] Temperatura normalizada.");
    }
}

void leSensores() {
    float t = htu21d.readTemperature();
    float u = htu21d.readHumidity();

    if (t != ERROR_I2C_TIMEOUT && u != ERROR_I2C_TIMEOUT) {
        g_temp = t;
        g_umid = u;
        Serial.printf("[Sensor] T:%.1f°C U:%.1f%%\n", g_temp, g_umid);
        atualizaAlarme();
    } else {
        Serial.println("[Sensor] Erro HTU21D");
    }
}

void processaGestos() {
    if (!apds.isGestureAvailable()) return;

    int gesto = apds.readGesture();
    switch (gesto) {
        case DIR_RIGHT:
            g_entradas++;
            Serial.printf("[Gesto] ENTRADA | Ent:%d Sai:%d Total:%d\n",
                          g_entradas, g_saidas, g_total());
            // Feedback rápido no LED
            setRGB(false, true, false); delay(200); setRGB(false, false, false);
            break;

        case DIR_LEFT:
            g_saidas++;
            if (g_saidas > g_entradas) g_saidas = g_entradas;
            Serial.printf("[Gesto] SAIDA   | Ent:%d Sai:%d Total:%d\n",
                          g_entradas, g_saidas, g_total());
            setRGB(false, false, true); delay(200); setRGB(false, false, false);
            break;

        default:
            break;
    }
}

// ════════════════════════════════════════════════════════════════
//   SETUP
// ════════════════════════════════════════════════════════════════
void setup() {
    Serial.begin(9600);
    Wire.begin(21, 22);   // SDA=21, SCL=22

    // Pinos de saída
    pinMode(LED_R,   OUTPUT);
    pinMode(LED_G,   OUTPUT);
    pinMode(LED_B,   OUTPUT);
    pinMode(BTN_PIN, INPUT_PULLUP);
    setRGB(false, false, false);

#if USE_BUZZER
    pinMode(BUZZER_PIN, OUTPUT);
#endif

    // ── OLED ─────────────────────────────────────────────────────
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("ERRO: OLED não encontrado!");
        while (1);
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 22);
    display.println("DATACENTER MONITOR");
    display.setCursor(20, 34);
    display.println("Inicializando...");
    display.display();
    delay(1500);

    // ── HTU21D ───────────────────────────────────────────────────
    htu21d.begin();
    float testTemp = htu21d.readTemperature();
    if (testTemp == ERROR_I2C_TIMEOUT) {
        Serial.println("ERRO: HTU21D não encontrado!");
        display.clearDisplay();
        display.setCursor(0, 24);
        display.println("ERRO: HTU21D!");
        display.display();
        while (1);
    }
    Serial.println("HTU21D OK");

    // ── APDS-9960 ────────────────────────────────────────────────
    if (!apds.init() || !apds.enableGestureSensor(true)) {
        Serial.println("AVISO: APDS-9960 não inicializou — gestos desabilitados.");
        // Não trava — o projeto continua sem gestos
    } else {
        Serial.println("APDS-9960 OK");
    }

    // ── Wi-Fi ────────────────────────────────────────────────────
    display.clearDisplay();
    display.setCursor(0, 24);
    display.println("Conectando Wi-Fi...");
    display.display();

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Wi-Fi");
    int t = 0;
    while (WiFi.status() != WL_CONNECTED && t++ < 30) {
        delay(500); Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("\nWi-Fi OK: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\nWi-Fi FALHOU — modo offline");
    }

    // ── Firebase ─────────────────────────────────────────────────
    config.database_url               = DATABASE_URL;
    config.signer.tokens.legacy_token = DATABASE_SECRET;
    Firebase.reconnectNetwork(true);
    fbdo.setBSSLBufferSize(4096, 1024);
    Firebase.begin(&config, &auth);

    // Carrega contadores anteriores
    if (Firebase.getInt(fbdo, "/pessoas/entradas")) g_entradas = fbdo.intData();
    if (Firebase.getInt(fbdo, "/pessoas/saidas"))   g_saidas   = fbdo.intData();
    Firebase.setBool(fbdo, "/alarme/ativo", false);

    // Publica limites no Firebase para referência
    Firebase.setFloat(fbdo, "/config/temp_cold", TEMP_COLD);
    Firebase.setFloat(fbdo, "/config/temp_warn", TEMP_WARN);
    Firebase.setFloat(fbdo, "/config/temp_crit", TEMP_CRIT);

    Serial.println("Sistema completo pronto!");
    setRGB(false, true, false);
    delay(800);
    setRGB(false, false, false);
}

// ════════════════════════════════════════════════════════════════
//   LOOP
// ════════════════════════════════════════════════════════════════
void loop() {
    unsigned long agora = millis();

    // ── 1. Gestos ─────────────────────────────────────────────────
    processaGestos();

    // ── 2. Botão: troca de tela ───────────────────────────────────
    bool btnAtual = digitalRead(BTN_PIN);
    if (btnAnterior == HIGH && btnAtual == LOW) {
        g_tela = (Tela)((g_tela + 1) % TELA_COUNT);
        t_tela = agora;
        Serial.printf("[Display] Tela manual: %d\n", (int)g_tela);
    }
    btnAnterior = btnAtual;

    // ── 3. Troca automática de tela ───────────────────────────────
    if (g_alarme != AL_CRITICO && agora - t_tela >= INTERVALO_TELA) {
        g_tela = (Tela)((g_tela + 1) % TELA_COUNT);
        t_tela = agora;
    }

    // ── 4. Leitura de sensores ────────────────────────────────────
    if (agora - t_sensor >= INTERVALO_SENSOR) {
        t_sensor = agora;
        leSensores();
    }

    // ── 5. Envio Firebase ─────────────────────────────────────────
    if (agora - t_firebase >= INTERVALO_FIREBASE) {
        t_firebase = agora;
        enviaFirebase();
    }

    // ── 6. LED e display (non-blocking) ───────────────────────────
    if (agora - t_pisca >= (g_alarme == AL_CRITICO ? 200UL : 400UL)) {
        t_pisca     = agora;
        piscaEstado = !piscaEstado;

        if (g_alarmeAtivo) {
            // Pisca vermelho no crítico + bip ocasional
            if (piscaEstado) {
                setRGB(true, false, false);
#if USE_BUZZER
                tone(BUZZER_PIN, 1200, 80);
#endif
            } else {
                setRGB(false, false, false);
            }
        } else if (g_alarme == AL_FRIO || g_alarme == AL_QUENTE) {
            // Pisca a cor de aviso
            if (piscaEstado) ledStatus();
            else setRGB(false, false, false);
        } else {
            // Normal: verde fixo (atualiza só quando necessário)
            ledStatus();
        }
    }

    // ── 7. Atualiza display ───────────────────────────────────────
    atualizaDisplay();

    delay(50);   // pequeno yield para o watchdog
}