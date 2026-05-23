#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <addons/RTDBHelper.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SparkFun_APDS9960.h>
#include <SparkFunHTU21D.h>
#include <time.h>

// ════════════════════════════════════════════════════════════════
//   CONFIGURAÇÕES — edite aqui
// ════════════════════════════════════════════════════════════════
#define WIFI_SSID       "uaifai-brum"
#define WIFI_PASSWORD   "bemvindoaocesar"
#define DATABASE_URL    "https://mpes-g2-default-rtdb.firebaseio.com/"
#define DATABASE_SECRET "3NdS9ZSZAu0QdbfRMbQO8OGrBNBPnilEoltWuoeE"

// ════════════════════════════════════════════════════════════════
//   NTP
// ════════════════════════════════════════════════════════════════
#define NTP_SERVER   "pool.ntp.org"
#define GMT_OFFSET   -3 * 3600   // UTC-3 Brasília
#define DST_OFFSET   0

// ════════════════════════════════════════════════════════════════
//   LIMITES — ASHRAE A1 para TI
// ════════════════════════════════════════════════════════════════
#define TEMP_COLD    18.0f
#define TEMP_WARN    27.0f
#define TEMP_CRIT    35.0f
#define UMID_MIN     40.0f
#define UMID_MAX     55.0f

// ════════════════════════════════════════════════════════════════
//   LIMITE DE OCUPAÇÃO
// ════════════════════════════════════════════════════════════════
#define PESSOAS_MAX  5

// ════════════════════════════════════════════════════════════════
//   BUZZER
// ════════════════════════════════════════════════════════════════
#define USE_BUZZER   true
#define BUZZER_PIN   25   


// ════════════════════════════════════════════════════════════════
//   INTERVALOS
// ════════════════════════════════════════════════════════════════
#define INTERVALO_FIREBASE   5000UL
#define INTERVALO_SENSOR     3000UL
#define INTERVALO_TELA       3000UL

// ════════════════════════════════════════════════════════════════
//   PINOS
// ════════════════════════════════════════════════════════════════
#define LED_R    19
#define LED_G    23
#define LED_B    18
#define BTN_PIN  27

#define BAR_1    13
#define BAR_2     4
#define BAR_3    16
#define BAR_4    17

#define BAR_ON   HIGH
#define BAR_OFF  LOW

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
float g_temp = 0, g_umid = 0;
int   g_entradas = 0, g_saidas = 0;
inline int g_total() { return g_entradas - g_saidas; }

enum EstadoTemp  { TEMP_OK = 0, TEMP_FRIO, TEMP_QUENTE, TEMP_CRITICO };
enum EstadoUmid  { UMID_OK = 0, UMID_SECO, UMID_UMIDO };
enum NivelAlarme { AL_NORMAL = 0, AL_AVISO, AL_CRITICO };

EstadoTemp  g_estadoTemp  = TEMP_OK;
EstadoUmid  g_estadoUmid  = UMID_OK;
NivelAlarme g_nivelAlarme = AL_NORMAL;

bool          g_alarmeAtivo      = false;
bool          g_alarmeLogado     = false;
bool          g_alertaPessoas    = false;
bool          g_pessoasLogado    = false;
bool          g_alarmeCombinado  = false;
bool          g_combinadoLogado  = false;
unsigned long g_tInicioAlarme    = 0;

// Subtipo do alarme combinado — para mensagem e tom distintos
enum TipoCombinado {
    COMB_NENHUM    = 0,
    COMB_ALTO_ALTO,    // temp alta  + umid alta
    COMB_ALTO_BAIXO,   // temp alta  + umid baixa
    COMB_BAIXO_ALTO,   // temp baixa + umid alta
    COMB_BAIXO_BAIXO   // temp baixa + umid baixa
};
TipoCombinado g_tipoCombinado = COMB_NENHUM;

enum Tela { TELA_TEMP = 0, TELA_PESSOAS, TELA_STATUS, TELA_COUNT };
Tela g_tela = TELA_TEMP;

unsigned long t_sensor   = 0;
unsigned long t_firebase = 0;
unsigned long t_tela     = 0;
unsigned long t_pisca    = 0;
bool piscaEstado = false;
bool btnAnterior = HIGH;

// ── Timestamps ───────────────────────────────────────────────────
String g_dtUltimoSensor   = "--/-- --:--:--";
String g_dtUltimoGesto    = "--/-- --:--:--";
String g_dtUltimoFirebase = "--/-- --:--:--";

// ════════════════════════════════════════════════════════════════
//   FORWARD DECLARATIONS
// ════════════════════════════════════════════════════════════════
void registraHistoricoAlarme();
void registraHistoricoPessoas();
void registraHistoricoCombinado();
void verificaPessoas();

// ════════════════════════════════════════════════════════════════
//   HELPERS — DATA / HORA (NTP)
// ════════════════════════════════════════════════════════════════

// "DD/MM/AAAA HH:MM:SS" — para Firebase e Serial
String dataHoraAtual() {
    struct tm info;
    if (!getLocalTime(&info)) return "--/--/---- --:--:--";
    char buf[20];
    strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M:%S", &info);
    return String(buf);
}

// "DD/MM HH:MM:SS" — compacto para o display OLED
String dataHoraResumida() {
    struct tm info;
    if (!getLocalTime(&info)) return "--/-- --:--:--";
    char buf[15];
    strftime(buf, sizeof(buf), "%d/%m %H:%M:%S", &info);
    return String(buf);
}

// ════════════════════════════════════════════════════════════════
//   HELPERS — LED RGB
// ════════════════════════════════════════════════════════════════
void setRGB(bool r, bool g, bool b) {
    digitalWrite(LED_R, r ? LOW : HIGH);
    digitalWrite(LED_G, g ? LOW : HIGH);
    digitalWrite(LED_B, b ? LOW : HIGH);
}

void ledStatus() {
    if (g_alertaPessoas)   { setRGB(true,  true,  true);  return; } // branco
    if (g_alarmeCombinado) { setRGB(true,  false, true);  return; } // magenta

    switch (g_estadoTemp) {
        case TEMP_CRITICO: setRGB(true,  false, false); return; // vermelho
        case TEMP_QUENTE:  setRGB(true,  true,  false); return; // amarelo
        case TEMP_FRIO:    setRGB(false, false, true);  return; // azul
        default: break;
    }
    switch (g_estadoUmid) {
        case UMID_SECO:  setRGB(false, true,  true);  return;   // ciano
        case UMID_UMIDO: setRGB(true,  false, true);  return;   // magenta
        default: break;
    }
    setRGB(false, true, false); // verde: OK
}

// ════════════════════════════════════════════════════════════════
//   HELPERS — BAR GRAPH
// ════════════════════════════════════════════════════════════════
void setBarGraph(bool l1, bool l2, bool l3, bool l4) {
    digitalWrite(BAR_1, l1 ? BAR_ON : BAR_OFF);
    digitalWrite(BAR_2, l2 ? BAR_ON : BAR_OFF);
    digitalWrite(BAR_3, l3 ? BAR_ON : BAR_OFF);
    digitalWrite(BAR_4, l4 ? BAR_ON : BAR_OFF);
}

void atualizaBarGraph() {
    // 1 — Temperatura crítica
    if (g_nivelAlarme == AL_CRITICO) {
        if (piscaEstado) setBarGraph(true,  true,  true,  true);
        else             setBarGraph(false, false, false, false);
        return;
    }

    // 2 — Alarme combinado: pisca alternado 1/3 e 2/4
    if (g_alarmeCombinado) {
        if (piscaEstado) setBarGraph(true,  false, true,  false);
        else             setBarGraph(false, true,  false, true);
        return;
    }

    // 3 — Lotação excedida
    if (g_alertaPessoas) {
        if (piscaEstado) setBarGraph(true,  true,  true,  true);
        else             setBarGraph(false, false, false, false);
        return;
    }

    // 4 — Aviso de temperatura ou umidade
    if (g_nivelAlarme == AL_AVISO) {
        if (!piscaEstado) { setBarGraph(false, false, false, false); return; }

        bool tempFora = (g_estadoTemp != TEMP_OK);
        bool umidFora = (g_estadoUmid != UMID_OK);
        bool tempAlta = (g_estadoTemp == TEMP_QUENTE);

        if      (tempFora && umidFora) setBarGraph(true, true, true,  false);
        else if (tempAlta || umidFora) setBarGraph(true, true, false, false);
        else                           setBarGraph(true, false,false, false);
        return;
    }

    // 5 — Normal: medidor proporcional 1–4 LEDs
    float pct = (g_temp - TEMP_COLD) / (TEMP_WARN - TEMP_COLD);
    pct       = constrain(pct, 0.0f, 1.0f);
    int n     = (int)(pct * 3.99f) + 1;
    setBarGraph(n >= 1, n >= 2, n >= 3, n >= 4);
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

// Temperatura crítica: três bipes rápidos + tom longo
void padraoAlarme() {
    bipa(1000, 100); delay(60);
    bipa(1000, 100); delay(60);
    bipa(1000, 100); delay(60);
    bipa(800,  500);
}

// Temp alta + Umid alta: tons alternados ascendentes (risco de condensação)
void padraoCombinadoAltoAlto() {
    bipa(900,  150); delay(50);
    bipa(1100, 150); delay(50);
    bipa(900,  150); delay(50);
    bipa(1100, 150); delay(50);
    bipa(1300, 400);
}

// Temp alta + Umid baixa: dois tons agudos (equipamento ressecando)
void padraoCombinadoAltoBaixo() {
    bipa(1400, 120); delay(60);
    bipa(1400, 120); delay(60);
    bipa(1000, 300); delay(60);
    bipa(1400, 120); delay(60);
    bipa(1400, 400);
}

// Temp baixa + Umid alta: tons graves e lentos (frio e úmido)
void padraoCombinadoBaixoAlto() {
    bipa(500, 250); delay(100);
    bipa(700, 250); delay(100);
    bipa(500, 250); delay(100);
    bipa(700, 500);
}

// Temp baixa + Umid baixa: tons graves curtos (frio e seco)
void padraoCombinadoBaixoBaixo() {
    bipa(500, 150); delay(60);
    bipa(500, 150); delay(60);
    bipa(500, 150); delay(60);
    bipa(400, 500);
}

// Lotação
void padraoAlarmelotacao() {
    bipa(600, 200); delay(80);
    bipa(600, 200); delay(80);
    bipa(900, 400);
}

// Despacha o tom correto conforme o tipo combinado
void soaCombinado(TipoCombinado tipo) {
    switch (tipo) {
        case COMB_ALTO_ALTO:   padraoCombinadoAltoAlto();   break;
        case COMB_ALTO_BAIXO:  padraoCombinadoAltoBaixo();  break;
        case COMB_BAIXO_ALTO:  padraoCombinadoBaixoAlto();  break;
        case COMB_BAIXO_BAIXO: padraoCombinadoBaixoBaixo(); break;
        default: break;
    }
}

// ════════════════════════════════════════════════════════════════
//   HELPERS — DISPLAY OLED
// ════════════════════════════════════════════════════════════════
void barraHoriz(int x, int y, int w, int h, float pct) {
    display.drawRect(x, y, w, h, SSD1306_WHITE);
    int fill = (int)(constrain(pct, 0.0f, 1.0f) * (w - 2));
    if (fill > 0)
        display.fillRect(x + 1, y + 1, fill, h - 2, SSD1306_WHITE);
}

void badge(int x, int y, const char* txt, bool alerta) {
    int w = strlen(txt) * 6 + 4;
    if (alerta) {
        display.fillRect(x, y - 1, w, 9, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
    } else {
        display.drawRect(x, y - 1, w, 9, SSD1306_WHITE);
    }
    display.setCursor(x + 2, y);
    display.print(txt);
    display.setTextColor(SSD1306_WHITE);
}

// ── Tela 0: Temperatura / Umidade ────────────────────────────────
void oledTelaTemp() {
    const char* bT[] = {"  OK  ", " FRIO ", "QUENTE", " CRIT "};
    const char* bU[] = {" OK ", "SECO", "UMID"};

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);

    display.setCursor(4,  1); display.print("TEMPERATURA");
    display.setCursor(70, 1); display.print("UMIDADE");
    display.drawLine(0,  9, 127,  9, SSD1306_WHITE);
    display.drawLine(63, 0,  63, 63, SSD1306_WHITE);

    // Coluna esquerda
    display.setCursor(2, 12);
    display.printf("Temp: %.1f C", g_temp);

    float pT = (g_temp - TEMP_COLD) / (TEMP_CRIT - TEMP_COLD);
    barraHoriz(2, 23, 59, 6, pT);
    int xW = 3 + (int)((TEMP_WARN - TEMP_COLD) / (TEMP_CRIT - TEMP_COLD) * 57);
    display.drawLine(xW, 21, xW, 31, SSD1306_WHITE);
    display.setCursor(2,    33); display.print("18");
    display.setCursor(xW-3, 33); display.print("27");
    display.setCursor(50,   33); display.print("35");

    badge(2, 45, bT[g_estadoTemp], g_estadoTemp != TEMP_OK);
    display.setCursor(2, 56); display.print("ok:18-27C");

    // Coluna direita
    display.setCursor(66, 12);
    display.printf("Umid: %.0f %%", g_umid);

    float pU = (g_umid - 20.0f) / 60.0f;
    barraHoriz(66, 23, 60, 6, pU);
    int xMn = 67 + (int)((UMID_MIN - 20.0f) / 60.0f * 58);
    int xMx = 67 + (int)((UMID_MAX - 20.0f) / 60.0f * 58);
    display.drawLine(xMn, 21, xMn, 31, SSD1306_WHITE);
    display.drawLine(xMx, 21, xMx, 31, SSD1306_WHITE);
    display.setCursor(66,     33); display.print("20");
    display.setCursor(xMn-3,  33); display.print("40");
    display.setCursor(xMx-3,  33); display.print("55");

    badge(66, 45, bU[g_estadoUmid], g_estadoUmid != UMID_OK);
    display.setCursor(66, 56); display.print("ok:40-55%");

    display.display();
}

// ── Tela 1: Ocupação ──────────────────────────────────────────────
void oledTelaPessoas() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);

    if (g_alertaPessoas && piscaEstado) {
        display.fillRect(0, 0, 128, 11, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
        display.setCursor(10, 2);
        display.print("!!! SALA LOTADA !!!");
        display.setTextColor(SSD1306_WHITE);
    } else {
        display.setCursor(0, 2);
        display.println("  OCUPACAO DA SALA  ");
    }
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    int total   = g_total();
    int digitos = (total < 10) ? 1 : (total < 100 ? 2 : 3);
    display.setTextSize(3);
    display.setCursor((128 - digitos * 18) / 2, 13);
    display.printf("%d", total);

    display.setTextSize(1);
    float pP = (float)total / (float)PESSOAS_MAX;
    barraHoriz(2, 37, 124, 8, pP);
    display.setCursor(105, 28);
    display.printf("max:%d", PESSOAS_MAX);

    display.setCursor(0, 54);
    display.printf("Ent:%d  Sai:%d  Max:%d",
                   g_entradas, g_saidas, PESSOAS_MAX);

    display.display();
}

// ── Tela 2: Status ────────────────────────────────────────────────
void oledTelaStatus() {
    const char* sT[]  = {"OK", "FRIO", "QUENTE", "CRIT"};
    const char* sU[]  = {"OK", "SECO", "UMIDO"};
    const char* sN[]  = {"NORMAL", "AVISO", "CRITICO"};

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);

    display.setCursor(0, 0);
    display.println("   STATUS SISTEMA   ");
    display.drawLine(0, 9, 127, 9, SSD1306_WHITE);

    display.setCursor(0, 12);
    display.print("WiFi: ");
    display.print(WiFi.status() == WL_CONNECTED ? "OK " : "OFF");
    display.print("  RSSI:");
    display.print(WiFi.RSSI());

    display.setCursor(0, 22);
    display.printf("T:%.1fC[%s] U:%.0f%%[%s]",
                   g_temp, sT[g_estadoTemp],
                   g_umid, sU[g_estadoUmid]);

    display.setCursor(0, 32);
    display.printf("Sen:%s", g_dtUltimoSensor.c_str());

    display.setCursor(0, 42);
    display.printf("Ges:%s", g_dtUltimoGesto.c_str());

    display.setCursor(0, 52);
    display.printf("FB :%s", g_dtUltimoFirebase.c_str());

    bool mostraRodape = (g_nivelAlarme == AL_NORMAL && !g_alertaPessoas
                         && !g_alarmeCombinado) || piscaEstado;
    if (mostraRodape) {
        bool destaque = (g_nivelAlarme != AL_NORMAL)
                        || g_alertaPessoas || g_alarmeCombinado;
        if (destaque) {
            display.fillRect(0, 56, 128, 8, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK);
        }
        display.setCursor(2, 57);
        if (g_alertaPessoas)
            display.printf("LOTADO! %d/%d pessoas", g_total(), PESSOAS_MAX);
        else if (g_alarmeCombinado)
            display.print("COMB! T+U fora da faixa");
        else
            display.printf("Alarme:%s P:%d/%d",
                           sN[g_nivelAlarme], g_total(), PESSOAS_MAX);
        display.setTextColor(SSD1306_WHITE);
    }

    display.display();
}

// ── Tela Aviso ────────────────────────────────────────────────────
void oledTelaAviso() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);

    if (piscaEstado) {
        display.fillRect(0, 0, 128, 11, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
    }
    display.setCursor(22, 2);
    display.print(">>>  AVISO  <<<");
    display.setTextColor(SSD1306_WHITE);
    display.drawLine(0, 11, 127, 11, SSD1306_WHITE);

    int y = 14;

    if (g_estadoTemp != TEMP_OK) {
        const char* titulo[] = {"", "TEMPERATURA BAIXA", "TEMPERATURA ALTA"};
        const char* limite[] = {"", "min 18.0 C",        "max 27.0 C"};
        display.setCursor(0, y); display.print(titulo[g_estadoTemp]); y += 10;
        display.setCursor(0, y);
        display.printf("%.1f C  (%s)", g_temp, limite[g_estadoTemp]); y += 10;
        float pT = (g_temp - TEMP_COLD) / (TEMP_CRIT - TEMP_COLD);
        barraHoriz(0, y, 128, 6, pT);
        int xW2 = 1 + (int)((TEMP_WARN - TEMP_COLD) / (TEMP_CRIT - TEMP_COLD) * 126);
        display.drawLine(xW2, y-2, xW2, y+8, SSD1306_WHITE);
        y += 10;
    }

    if (g_estadoUmid != UMID_OK) {
        const char* titulo[] = {"", "UMIDADE BAIXA (SECO)", "UMIDADE ALTA"};
        const char* limite[] = {"", "min 40 %",             "max 55 %"};
        display.setCursor(0, y); display.print(titulo[g_estadoUmid]); y += 10;
        display.setCursor(0, y);
        display.printf("%.0f %%  (%s)", g_umid, limite[g_estadoUmid]); y += 10;
        float pU2 = (g_umid - 20.0f) / 60.0f;
        barraHoriz(0, y, 128, 6, pU2);
        int xMn = 1 + (int)((UMID_MIN - 20.0f) / 60.0f * 126);
        int xMx = 1 + (int)((UMID_MAX - 20.0f) / 60.0f * 126);
        display.drawLine(xMn, y-2, xMn, y+8, SSD1306_WHITE);
        display.drawLine(xMx, y-2, xMx, y+8, SSD1306_WHITE);
    }

    display.display();
}

// ── Tela Alarme Combinado ─────────────────────────────────────────
void oledAlarmeCombinado() {
    // Títulos e mensagens de risco para cada combinação
    const char* titulos[] = {
        "",
        "! TEMP ALTA + UMID ALTA !",   // COMB_ALTO_ALTO
        "! TEMP ALTA + UMID BAIXA !",  // COMB_ALTO_BAIXO
        "! TEMP BAIXA + UMID ALTA !",  // COMB_BAIXO_ALTO
        "! TEMP BAIXA + UMID BAIXA !"  // COMB_BAIXO_BAIXO
    };
    const char* riscos[] = {
        "",
        "Risco de condensacao!",        // COMB_ALTO_ALTO
        "Risco de ressecamento!",       // COMB_ALTO_BAIXO
        "Risco de corrosao!",           // COMB_BAIXO_ALTO
        "Risco de carga eletrostatica!" // COMB_BAIXO_BAIXO
    };

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);

    if (piscaEstado) {
        display.fillRect(0, 0, 128, 11, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
    }
    display.setCursor(0, 2);
    display.print(titulos[g_tipoCombinado]);
    display.setTextColor(SSD1306_WHITE);
    display.drawLine(0, 11, 127, 11, SSD1306_WHITE);

    // Temperatura
    display.setCursor(0, 14);
    display.printf("Temp : %.1f C", g_temp);
    display.setCursor(80, 14);
    display.printf("[%s]", g_estadoTemp == TEMP_FRIO ? "FRIO" : "ALTO");

    float pT = (g_temp - TEMP_COLD) / (TEMP_CRIT - TEMP_COLD);
    barraHoriz(0, 24, 128, 6, pT);
    int xW = 1 + (int)((TEMP_WARN - TEMP_COLD) / (TEMP_CRIT - TEMP_COLD) * 126);
    display.drawLine(xW, 22, xW, 32, SSD1306_WHITE);

    // Umidade
    display.setCursor(0, 35);
    display.printf("Umid : %.0f %%", g_umid);
    display.setCursor(80, 35);
    display.printf("[%s]", g_estadoUmid == UMID_SECO ? "SECO" : "ALTO");

    float pU = (g_umid - 20.0f) / 60.0f;
    barraHoriz(0, 45, 128, 6, pU);
    int xMn = 1 + (int)((UMID_MIN - 20.0f) / 60.0f * 126);
    int xMx = 1 + (int)((UMID_MAX - 20.0f) / 60.0f * 126);
    display.drawLine(xMn, 43, xMn, 53, SSD1306_WHITE);
    display.drawLine(xMx, 43, xMx, 53, SSD1306_WHITE);

    display.setCursor(0, 56);
    display.print(riscos[g_tipoCombinado]);

    display.display();
}

// ── Tela Crítica ──────────────────────────────────────────────────
void oledAlarmeCritico() {
    const char* bU[] = {"OK", "SECO", "UMIDO"};

    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);

    if (piscaEstado) {
        display.fillRect(0, 0, 128, 11, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
    }
    display.setCursor(8, 2);
    display.print("!!! TEMP CRITICA !!!");
    display.setTextColor(SSD1306_WHITE);

    display.setTextSize(2);
    display.setCursor(4, 14);
    display.printf("%.1f", g_temp);
    display.setTextSize(1);
    display.setCursor(64, 14); display.print("o");
    display.setCursor(68, 20); display.print("C");

    display.setTextSize(1);
    float pT = (g_temp - TEMP_COLD) / (TEMP_CRIT - TEMP_COLD);
    barraHoriz(0, 34, 128, 6, pT);
    display.setCursor(108, 25); display.print("35C");

    display.setCursor(0, 45);
    display.printf("Umid: %.0f%%  [%s]", g_umid, bU[g_estadoUmid]);

    display.setCursor(0, 55);
    unsigned long seg = (millis() - g_tInicioAlarme) / 1000;
    display.printf("Ha: %lus  Lim: %.0fC", seg, TEMP_CRIT);

    display.display();
}

// ── Dispatcher ────────────────────────────────────────────────────
void atualizaDisplay() {
    if (g_nivelAlarme == AL_CRITICO) { oledAlarmeCritico();   return; }
    if (g_alarmeCombinado)           { oledAlarmeCombinado(); return; }
    if (g_nivelAlarme == AL_AVISO)   { oledTelaAviso();       return; }
    switch (g_tela) {
        case TELA_TEMP:    oledTelaTemp();    break;
        case TELA_PESSOAS: oledTelaPessoas(); break;
        case TELA_STATUS:  oledTelaStatus();  break;
        default: break;
    }
}

// ════════════════════════════════════════════════════════════════
//   FIREBASE
// ════════════════════════════════════════════════════════════════
void enviaFirebase() {
    if (!Firebase.ready()) return;

    const char* sT[] = {"ok", "frio", "quente", "critico"};
    const char* sU[] = {"ok", "seco", "umido"};
    const char* sN[] = {"normal", "aviso", "critico"};
    const char* sC[] = {
        "nenhum", "temp_alta+umid_alta",
        "temp_alta+umid_baixa",
        "temp_baixa+umid_alta",
        "temp_baixa+umid_baixa"
    };

    Firebase.setFloat (fbdo, "/ambiente/temperatura",          g_temp);
    Firebase.setFloat (fbdo, "/ambiente/umidade",              g_umid);
    Firebase.setString(fbdo, "/ambiente/ultima_leitura",       dataHoraAtual());

    Firebase.setInt   (fbdo, "/pessoas/entradas",              g_entradas);
    Firebase.setInt   (fbdo, "/pessoas/saidas",                g_saidas);
    Firebase.setInt   (fbdo, "/pessoas/total",                 g_total());
    Firebase.setInt   (fbdo, "/pessoas/maximo",                PESSOAS_MAX);
    Firebase.setBool  (fbdo, "/pessoas/alerta_lotacao",        g_alertaPessoas);
    Firebase.setString(fbdo, "/pessoas/ultimo_gesto",          g_dtUltimoGesto);

    Firebase.setString(fbdo, "/alarme/nivel",                  sN[g_nivelAlarme]);
    Firebase.setString(fbdo, "/alarme/temp_estado",            sT[g_estadoTemp]);
    Firebase.setString(fbdo, "/alarme/umid_estado",            sU[g_estadoUmid]);
    Firebase.setBool  (fbdo, "/alarme/ativo",                  g_alarmeAtivo);
    Firebase.setFloat (fbdo, "/alarme/temp_atual",             g_temp);
    Firebase.setFloat (fbdo, "/alarme/umid_atual",             g_umid);
    Firebase.setBool  (fbdo, "/alarme/combinado/ativo",        g_alarmeCombinado);
    Firebase.setString(fbdo, "/alarme/combinado/tipo",         sC[g_tipoCombinado]);

    g_dtUltimoFirebase = dataHoraResumida();
    Firebase.setString(fbdo, "/sistema/ultimo_envio",          dataHoraAtual());

    Serial.printf("[Firebase] T:%.1f[%s] U:%.0f[%s] P:%d/%d Comb:%s -> %s @ %s\n",
                  g_temp, sT[g_estadoTemp],
                  g_umid, sU[g_estadoUmid],
                  g_total(), PESSOAS_MAX,
                  sC[g_tipoCombinado],
                  sN[g_nivelAlarme],
                  dataHoraAtual().c_str());
}

void registraHistoricoAlarme() {
    if (!Firebase.ready() || g_alarmeLogado) return;
    const char* sU[] = {"ok", "seco", "umido"};
    String path = "/alarme/historico/"; path += millis();
    FirebaseJson json;
    json.set("temperatura", g_temp);
    json.set("umidade",     g_umid);
    json.set("umid_estado", sU[g_estadoUmid]);
    json.set("pessoas",     g_total());
    json.set("datetime",    dataHoraAtual().c_str());
    Firebase.updateNode(fbdo, path, json);
    g_alarmeLogado = true;
    Serial.println("[Firebase] Alarme critico registrado.");
}

void registraHistoricoPessoas() {
    if (!Firebase.ready() || g_pessoasLogado) return;
    String path = "/pessoas/historico/lotacao/"; path += millis();
    FirebaseJson json;
    json.set("total",    g_total());
    json.set("maximo",   PESSOAS_MAX);
    json.set("datetime", dataHoraAtual().c_str());
    Firebase.updateNode(fbdo, path, json);
    g_pessoasLogado = true;
    Serial.println("[Firebase] Lotacao registrada.");
}

void registraHistoricoCombinado() {
    if (!Firebase.ready() || g_combinadoLogado) return;
    const char* sC[] = {
        "nenhum", "temp_alta+umid_alta",
        "temp_alta+umid_baixa",
        "temp_baixa+umid_alta",
        "temp_baixa+umid_baixa"
    };
    String path = "/alarme/combinado/historico/"; path += millis();
    FirebaseJson json;
    json.set("temperatura", g_temp);
    json.set("umidade",     g_umid);
    json.set("tipo",        sC[g_tipoCombinado]);
    json.set("pessoas",     g_total());
    json.set("datetime",    dataHoraAtual().c_str());
    Firebase.updateNode(fbdo, path, json);
    Firebase.setBool  (fbdo, "/alarme/combinado/ativo", true);
    Firebase.setString(fbdo, "/alarme/combinado/tipo",  sC[g_tipoCombinado]);
    g_combinadoLogado = true;
    Serial.printf("[Firebase] Alarme combinado [%s] registrado.\n",
                  sC[g_tipoCombinado]);
}

// ════════════════════════════════════════════════════════════════
//   VERIFICAÇÃO DE OCUPAÇÃO
// ════════════════════════════════════════════════════════════════
void verificaPessoas() {
    bool lotado = (g_total() > PESSOAS_MAX);

    if (lotado && !g_alertaPessoas) {
        g_alertaPessoas = true;
        g_pessoasLogado = false;
        Serial.printf("[LOTADO] %d/%d pessoas!\n", g_total(), PESSOAS_MAX);
        padraoAlarmelotacao();
        registraHistoricoPessoas();
        g_tela = TELA_PESSOAS;
    } else if (!lotado && g_alertaPessoas) {
        g_alertaPessoas = false;
        Serial.printf("[Pessoas] Normalizado: %d/%d\n", g_total(), PESSOAS_MAX);
        if (Firebase.ready())
            Firebase.setBool(fbdo, "/pessoas/alerta_lotacao", false);
    }
}

// ════════════════════════════════════════════════════════════════
//   SENSORES E ALARME
// ════════════════════════════════════════════════════════════════
void atualizaAlarme() {
    EstadoTemp novoTemp;
    if      (g_temp >= TEMP_CRIT) novoTemp = TEMP_CRITICO;
    else if (g_temp >  TEMP_WARN) novoTemp = TEMP_QUENTE;
    else if (g_temp <  TEMP_COLD) novoTemp = TEMP_FRIO;
    else                           novoTemp = TEMP_OK;

    EstadoUmid novoUmid;
    if      (g_umid < UMID_MIN) novoUmid = UMID_SECO;
    else if (g_umid > UMID_MAX) novoUmid = UMID_UMIDO;
    else                         novoUmid = UMID_OK;

    NivelAlarme novoNivel;
    if      (novoTemp == TEMP_CRITICO)                   novoNivel = AL_CRITICO;
    else if (novoTemp != TEMP_OK || novoUmid != UMID_OK) novoNivel = AL_AVISO;
    else                                                  novoNivel = AL_NORMAL;

    if (novoTemp != g_estadoTemp || novoUmid != g_estadoUmid) {
        const char* sT[] = {"OK", "FRIO", "QUENTE", "CRITICO"};
        const char* sU[] = {"OK", "SECO", "UMIDO"};
        Serial.printf("[Alarme] Temp:%s(%.1fC)  Umid:%s(%.0f%%)\n",
                      sT[novoTemp], g_temp, sU[novoUmid], g_umid);
    }

    g_estadoTemp  = novoTemp;
    g_estadoUmid  = novoUmid;
    g_nivelAlarme = novoNivel;

    // ── Crítico de temperatura ────────────────────────────────────
    bool deveAlarmar = (g_nivelAlarme == AL_CRITICO);
    if (deveAlarmar && !g_alarmeAtivo) {
        g_alarmeAtivo   = true;
        g_tInicioAlarme = millis();
        g_alarmeLogado  = false;
        Serial.printf("[CRITICO] T:%.1fC / U:%.0f%%\n", g_temp, g_umid);
        padraoAlarme();
        registraHistoricoAlarme();
    } else if (!deveAlarmar && g_alarmeAtivo) {
        g_alarmeAtivo = false;
        Serial.println("[Alarme] Temperatura normalizada.");
    }

    // ── Alarme combinado: QUALQUER combinação fora dos dois lados ─
    // Cobre os 4 quadrantes:
    //   temp alta  + umid alta   → condensação
    //   temp alta  + umid baixa  → ressecamento
    //   temp baixa + umid alta   → corrosão
    //   temp baixa + umid baixa  → eletrostática
    bool tempFora = (g_estadoTemp == TEMP_QUENTE  || g_estadoTemp == TEMP_CRITICO
                  || g_estadoTemp == TEMP_FRIO);
    bool umidFora = (g_estadoUmid == UMID_UMIDO   || g_estadoUmid == UMID_SECO);
    bool deveCombinado = tempFora && umidFora;

    // Determina o subtipo
    TipoCombinado novoTipo = COMB_NENHUM;
    if (deveCombinado) {
        bool tempAlta = (g_estadoTemp == TEMP_QUENTE || g_estadoTemp == TEMP_CRITICO);
        bool umidAlta = (g_estadoUmid == UMID_UMIDO);
        if      ( tempAlta &&  umidAlta) novoTipo = COMB_ALTO_ALTO;
        else if ( tempAlta && !umidAlta) novoTipo = COMB_ALTO_BAIXO;
        else if (!tempAlta &&  umidAlta) novoTipo = COMB_BAIXO_ALTO;
        else                             novoTipo = COMB_BAIXO_BAIXO;
    }

    if (deveCombinado && (!g_alarmeCombinado || novoTipo != g_tipoCombinado)) {
        // Dispara se for novo ou se o tipo mudou
        g_alarmeCombinado = true;
        g_combinadoLogado = (novoTipo == g_tipoCombinado); // reusa log se mesmo tipo
        g_tipoCombinado   = novoTipo;
        const char* sC[] = {
            "", "TEMP_ALTA+UMID_ALTA",
            "TEMP_ALTA+UMID_BAIXA",
            "TEMP_BAIXA+UMID_ALTA",
            "TEMP_BAIXA+UMID_BAIXA"
        };
        Serial.printf("[COMBINADO] %s  T:%.1fC U:%.0f%%\n",
                      sC[g_tipoCombinado], g_temp, g_umid);
        soaCombinado(g_tipoCombinado);
        registraHistoricoCombinado();
    } else if (!deveCombinado && g_alarmeCombinado) {
        g_alarmeCombinado = false;
        g_tipoCombinado   = COMB_NENHUM;
        Serial.println("[Combinado] Normalizado.");
        if (Firebase.ready()) {
            Firebase.setBool  (fbdo, "/alarme/combinado/ativo", false);
            Firebase.setString(fbdo, "/alarme/combinado/tipo",  "nenhum");
        }
    }
}

void leSensores() {
    float t = htu21d.readTemperature();
    float u = htu21d.readHumidity();
    if (t != ERROR_I2C_TIMEOUT && u != ERROR_I2C_TIMEOUT) {
        g_temp = t;
        g_umid = u;
        g_dtUltimoSensor = dataHoraResumida();
        Serial.printf("[Sensor] T:%.1fC U:%.1f%% @ %s\n",
                      g_temp, g_umid, g_dtUltimoSensor.c_str());
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
            g_dtUltimoGesto = dataHoraResumida();
            Serial.printf("[Gesto] ENTRADA | Ent:%d Sai:%d Total:%d @ %s\n",
                          g_entradas, g_saidas, g_total(),
                          g_dtUltimoGesto.c_str());
            setRGB(false, true, false); delay(200); setRGB(false, false, false);
            verificaPessoas();
            break;
        case DIR_LEFT:
            g_saidas++;
            if (g_saidas > g_entradas) g_saidas = g_entradas;
            g_dtUltimoGesto = dataHoraResumida();
            Serial.printf("[Gesto] SAIDA   | Ent:%d Sai:%d Total:%d @ %s\n",
                          g_entradas, g_saidas, g_total(),
                          g_dtUltimoGesto.c_str());
            setRGB(false, false, true); delay(200); setRGB(false, false, false);
            verificaPessoas();
            break;
        default: break;
    }
}

// ════════════════════════════════════════════════════════════════
//   SETUP
// ════════════════════════════════════════════════════════════════
void setup() {
    Serial.begin(9600);
    Wire.begin(21, 22);

    pinMode(LED_R,   OUTPUT);
    pinMode(LED_G,   OUTPUT);
    pinMode(LED_B,   OUTPUT);
    setRGB(false, false, false);

    pinMode(BTN_PIN, INPUT_PULLUP);

    pinMode(BAR_1, OUTPUT);
    pinMode(BAR_2, OUTPUT);
    pinMode(BAR_3, OUTPUT);
    pinMode(BAR_4, OUTPUT);
    setBarGraph(false, false, false, false);

#if USE_BUZZER
    pinMode(BUZZER_PIN, OUTPUT);
#endif

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("ERRO: OLED!"); while (1);
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 22); display.println("DATACENTER MONITOR");
    display.setCursor(20, 34); display.println("Inicializando...");
    display.display();
    delay(1500);

    htu21d.begin();
    if (htu21d.readTemperature() == ERROR_I2C_TIMEOUT) {
        Serial.println("ERRO: HTU21D!");
        display.clearDisplay();
        display.setCursor(0, 24); display.println("ERRO: HTU21D!");
        display.display();
        while (1);
    }
    Serial.println("HTU21D OK");

    if (!apds.init() || !apds.enableGestureSensor(true))
        Serial.println("AVISO: APDS-9960 sem gestos.");
    else
        Serial.println("APDS-9960 OK");

    // Wi-Fi
    display.clearDisplay();
    display.setCursor(0, 24); display.println("Conectando Wi-Fi...");
    display.display();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Wi-Fi");
    int t = 0;
    while (WiFi.status() != WL_CONNECTED && t++ < 30) { delay(500); Serial.print("."); }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("\nWi-Fi OK: "); Serial.println(WiFi.localIP());
    } else {
        Serial.println("\nWi-Fi FALHOU — modo offline");
    }

    // NTP
    if (WiFi.status() == WL_CONNECTED) {
        configTime(GMT_OFFSET, DST_OFFSET, NTP_SERVER);
        Serial.print("NTP sync");
        struct tm info;
        int tentativas = 0;
        while (!getLocalTime(&info) && tentativas++ < 20) {
            delay(500); Serial.print(".");
        }
        if (getLocalTime(&info))
            Serial.printf("\nNTP OK: %s\n", dataHoraAtual().c_str());
        else
            Serial.println("\nNTP FALHOU — sem data/hora");
    }

    // Firebase
    config.database_url               = DATABASE_URL;
    config.signer.tokens.legacy_token = DATABASE_SECRET;
    Firebase.reconnectNetwork(true);
    fbdo.setBSSLBufferSize(4096, 1024);
    Firebase.begin(&config, &auth);

    if (Firebase.getInt(fbdo, "/pessoas/entradas")) g_entradas = fbdo.intData();
    if (Firebase.getInt(fbdo, "/pessoas/saidas"))   g_saidas   = fbdo.intData();

    Firebase.setBool  (fbdo, "/alarme/ativo",               false);
    Firebase.setBool  (fbdo, "/alarme/combinado/ativo",      false);
    Firebase.setString(fbdo, "/alarme/combinado/tipo",       "nenhum");
    Firebase.setBool  (fbdo, "/pessoas/alerta_lotacao",      false);
    Firebase.setFloat (fbdo, "/config/temp_cold",            TEMP_COLD);
    Firebase.setFloat (fbdo, "/config/temp_warn",            TEMP_WARN);
    Firebase.setFloat (fbdo, "/config/temp_crit",            TEMP_CRIT);
    Firebase.setFloat (fbdo, "/config/umid_min",             UMID_MIN);
    Firebase.setFloat (fbdo, "/config/umid_max",             UMID_MAX);
    Firebase.setInt   (fbdo, "/config/pessoas_max",          PESSOAS_MAX);

    verificaPessoas();

    Serial.println("Sistema pronto!");
    setRGB(false, true, false); delay(800); setRGB(false, false, false);
}

// ════════════════════════════════════════════════════════════════
//   LOOP
// ════════════════════════════════════════════════════════════════
void loop() {
    unsigned long agora = millis();

    // 1. Gestos
    processaGestos();

    // 2. Botão: troca de tela
    bool btnAtual = digitalRead(BTN_PIN);
    if (btnAnterior == HIGH && btnAtual == LOW) {
        g_tela = (Tela)((g_tela + 1) % TELA_COUNT);
        t_tela = agora;
        Serial.printf("[Display] Tela: %d\n", (int)g_tela);
    }
    btnAnterior = btnAtual;

    // 3. Troca automática (só em modo totalmente normal)
    if (g_nivelAlarme == AL_NORMAL && !g_alertaPessoas
        && !g_alarmeCombinado && agora - t_tela >= INTERVALO_TELA) {
        g_tela = (Tela)((g_tela + 1) % TELA_COUNT);
        t_tela = agora;
    }

    // 4. Leitura de sensores
    if (agora - t_sensor >= INTERVALO_SENSOR) {
        t_sensor = agora;
        leSensores();
    }

    // 5. Envio Firebase
    if (agora - t_firebase >= INTERVALO_FIREBASE) {
        t_firebase = agora;
        enviaFirebase();
    }

    // 6. LED RGB + Bar Graph (non-blocking)
    unsigned long intervPisca = (g_nivelAlarme == AL_CRITICO) ? 200UL : 400UL;
    if (agora - t_pisca >= intervPisca) {
        t_pisca     = agora;
        piscaEstado = !piscaEstado;

        if (g_alarmeAtivo) {
            if (piscaEstado) {
                setRGB(true, false, false);
#if USE_BUZZER
                tone(BUZZER_PIN, 1200, 80);
#endif
            } else {
                setRGB(false, false, false);
            }
        } else if (g_alertaPessoas) {
            if (piscaEstado) setRGB(true,  true,  true);
            else             setRGB(false, false, false);
        } else if (g_alarmeCombinado || g_nivelAlarme == AL_AVISO) {
            if (piscaEstado) ledStatus();
            else             setRGB(false, false, false);
        } else {
            ledStatus();
        }
    }

    // 7. Bar Graph
    atualizaBarGraph();

    // 8. Display
    atualizaDisplay();

    delay(50);
}
