#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <time.h>
#include <SPI.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <DHT.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// ==========================================
// DEFAULT WIFI CREDENTIALS
// ==========================================
String currentSSID = "";
String currentPASS = "";

// Hardware Settings
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW 
#define MAX_DEVICES 4
#define CS_PIN 5
#define DHTPIN 4
#define DHTTYPE DHT11

const long gmtOffset_sec = 19800; // IST Time
const int daylightOffset_sec = 0;

// Globals
MD_Parola myDisplay = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
DHT dht(DHTPIN, DHTTYPE);
WebServer server(80);
Preferences preferences;

// State Variables
char customText[150];
char extraData[250]; 
char customGreeting[100]; 
char finalBootMsg[200]; 

String apiUrl = "";
unsigned long lastApiTime = 0;
const unsigned long apiInterval = 300000; 

int scrollSpeed = 40; 
int brightness = 2;
int charSpace = 2;
int displayMode = 3; 
int fontStyle = 0; 
int greetingMode = 0; 

bool wifiWasConnected = false;
bool pendingRestart = false; 
unsigned long restartTime = 0; 

// Buffers
char timeStr[15];
char sensorStr[50]; 

// Separated boot messages
char bootMsg1[100]; 
char bootMsg2[100]; 

// State Machine
enum DisplayState { BOOT_G1, BOOT_PAUSE, BOOT_G2, SHOW_TIME, CLEAR_SCREEN, SHOW_SENSOR, SHOW_EXTRA, SHOW_TEXT };
DisplayState currentState = BOOT_G1;

// ==========================================
// FULLY PADDED BOLD FONT (256 Chars)
// ==========================================
const uint8_t boldFont[] PROGMEM = {
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0, 3, 0x00, 0x5f, 0x00, 3, 0x07, 0x00, 0x07, 5, 0x14, 0x7f, 0x14, 0x7f, 0x14, 
  5, 0x24, 0x2a, 0x7f, 0x2a, 0x12, 5, 0x23, 0x13, 0x08, 0x64, 0x62, 5, 0x36, 0x49, 0x55, 0x22, 0x50, 
  2, 0x00, 0x07, 3, 0x1c, 0x22, 0x41, 3, 0x41, 0x22, 0x1c, 5, 0x14, 0x08, 0x3e, 0x08, 0x14, 
  5, 0x08, 0x08, 0x3e, 0x08, 0x08, 2, 0x50, 0x30, 5, 0x08, 0x08, 0x08, 0x08, 0x08, 2, 0x60, 0x60, 
  5, 0x20, 0x10, 0x08, 0x04, 0x02, 5, 0x3e, 0x51, 0x49, 0x45, 0x3e, 3, 0x42, 0x7f, 0x40, 
  5, 0x42, 0x61, 0x51, 0x49, 0x46, 5, 0x21, 0x41, 0x45, 0x4b, 0x31, 5, 0x18, 0x14, 0x12, 0x7f, 0x10, 
  5, 0x27, 0x45, 0x45, 0x45, 0x39, 5, 0x3c, 0x4a, 0x49, 0x49, 0x30, 5, 0x01, 0x71, 0x09, 0x05, 0x03, 
  5, 0x36, 0x49, 0x49, 0x49, 0x36, 5, 0x06, 0x49, 0x49, 0x29, 0x1e, 2, 0x36, 0x36, 2, 0x56, 0x36, 
  4, 0x08, 0x14, 0x22, 0x41, 5, 0x14, 0x14, 0x14, 0x14, 0x14, 4, 0x41, 0x22, 0x14, 0x08, 
  5, 0x02, 0x01, 0x51, 0x09, 0x06, 5, 0x32, 0x49, 0x79, 0x41, 0x3e, 5, 0x7e, 0x11, 0x11, 0x11, 0x7e, 
  5, 0x7f, 0x49, 0x49, 0x49, 0x36, 5, 0x3e, 0x41, 0x41, 0x41, 0x22, 5, 0x7f, 0x41, 0x41, 0x22, 0x1c, 
  5, 0x7f, 0x49, 0x49, 0x49, 0x41, 5, 0x7f, 0x09, 0x09, 0x09, 0x01, 5, 0x3e, 0x41, 0x49, 0x49, 0x7a, 
  5, 0x7f, 0x08, 0x08, 0x08, 0x7f, 3, 0x41, 0x7f, 0x41, 5, 0x20, 0x40, 0x41, 0x3f, 0x01, 
  5, 0x7f, 0x08, 0x14, 0x22, 0x41, 5, 0x7f, 0x40, 0x40, 0x40, 0x40, 5, 0x7f, 0x02, 0x0c, 0x02, 0x7f, 
  5, 0x7f, 0x04, 0x08, 0x10, 0x7f, 5, 0x3e, 0x41, 0x41, 0x41, 0x3e, 5, 0x7f, 0x09, 0x09, 0x09, 0x06, 
  5, 0x3e, 0x41, 0x51, 0x21, 0x5e, 5, 0x7f, 0x09, 0x19, 0x29, 0x46, 5, 0x46, 0x49, 0x49, 0x49, 0x31, 
  5, 0x01, 0x01, 0x7f, 0x01, 0x01, 5, 0x3f, 0x40, 0x40, 0x40, 0x3f, 5, 0x1f, 0x20, 0x40, 0x20, 0x1f, 
  5, 0x3f, 0x40, 0x38, 0x40, 0x3f, 5, 0x63, 0x14, 0x08, 0x14, 0x63, 5, 0x07, 0x08, 0x70, 0x08, 0x07, 
  5, 0x61, 0x51, 0x49, 0x45, 0x43, 3, 0x7f, 0x41, 0x41, 5, 0x02, 0x04, 0x08, 0x10, 0x20, 
  3, 0x41, 0x41, 0x7f, 4, 0x04, 0x02, 0x01, 0x02, 5, 0x40, 0x40, 0x40, 0x40, 0x40, 2, 0x01, 0x02, 
  5, 0x20, 0x54, 0x54, 0x54, 0x78, 5, 0x7f, 0x48, 0x44, 0x44, 0x38, 5, 0x38, 0x44, 0x44, 0x44, 0x20, 
  5, 0x38, 0x44, 0x44, 0x48, 0x7f, 5, 0x38, 0x54, 0x54, 0x54, 0x18, 5, 0x08, 0x7e, 0x09, 0x01, 0x02, 
  5, 0x0c, 0x52, 0x52, 0x52, 0x3e, 5, 0x7f, 0x08, 0x04, 0x04, 0x78, 3, 0x44, 0x7d, 0x40, 
  5, 0x20, 0x40, 0x44, 0x3d, 0x00, 5, 0x7f, 0x10, 0x28, 0x44, 0x00, 3, 0x41, 0x7f, 0x40, 
  5, 0x7c, 0x04, 0x18, 0x04, 0x78, 5, 0x7c, 0x08, 0x04, 0x04, 0x78, 5, 0x38, 0x44, 0x44, 0x44, 0x38, 
  5, 0x7c, 0x14, 0x14, 0x14, 0x08, 5, 0x08, 0x14, 0x14, 0x18, 0x7c, 5, 0x7c, 0x08, 0x04, 0x04, 0x08, 
  5, 0x48, 0x54, 0x54, 0x54, 0x20, 5, 0x04, 0x3f, 0x44, 0x40, 0x20, 5, 0x3c, 0x40, 0x40, 0x20, 0x7c, 
  5, 0x1c, 0x20, 0x40, 0x20, 0x1c, 5, 0x3c, 0x40, 0x30, 0x40, 0x3c, 5, 0x44, 0x28, 0x10, 0x28, 0x44, 
  5, 0x0c, 0x50, 0x50, 0x50, 0x3c, 5, 0x44, 0x64, 0x54, 0x4c, 0x44, 4, 0x08, 0x36, 0x41, 0x00, 
  1, 0x7f, 4, 0x41, 0x36, 0x08, 0x00, 4, 0x02, 0x01, 0x02, 0x01,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0
};

// ==========================================
// ESCAPED C-STRING WEB UI
// ==========================================
const char index_html[] PROGMEM = 
"<!DOCTYPE html>\n"
"<html lang=\"en\">\n"
"<head>\n"
"<meta charset=\"UTF-8\">\n"
"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
"<title>Matrix Hub Pro</title>\n"
"<style>\n"
":root { --bg: #0f172a; --card: #1e293b; --text: #f8fafc; --primary: #3b82f6; --accent: #10b981; }\n"
"body { font-family: 'Segoe UI', Tahoma, sans-serif; background: var(--bg); color: var(--text); margin: 0; padding: 20px; display: flex; justify-content: center; }\n"
".container { background: var(--card); padding: 25px; border-radius: 12px; width: 100%; max-width: 500px; box-shadow: 0 10px 25px -5px rgba(0,0,0,0.5); }\n"
"h2 { text-align: center; color: var(--primary); margin-top: 0; margin-bottom: 25px; font-weight: 600; }\n"
".tabs { display: flex; gap: 10px; margin-bottom: 20px; }\n"
".tab-btn { flex: 1; padding: 10px; background: #334155; color: white; border: none; border-radius: 6px; cursor: pointer; font-weight: bold; transition: 0.2s; }\n"
".tab-btn.active { background: var(--primary); }\n"
".tab-content { display: none; }\n"
".tab-content.active { display: block; }\n"
".form-group { margin-bottom: 22px; }\n"
"label { display: block; margin-bottom: 8px; font-weight: 500; font-size: 14px; color: #cbd5e1; }\n"
"input[type=\"text\"], input[type=\"password\"], select, textarea { width: 100%; padding: 12px; border: 1px solid #475569; border-radius: 6px; background: #0f172a; color: white; box-sizing: border-box; font-family: inherit; font-size: 15px; }\n"
"textarea { resize: vertical; min-height: 70px; }\n"
"input[type=\"range\"] { width: 100%; cursor: pointer; accent-color: var(--primary); }\n"
".val-display { float: right; font-weight: bold; color: var(--accent); }\n"
".note { display: block; font-size: 12px; color: #94a3b8; margin-top: 5px; }\n"
"button.save-btn { width: 100%; padding: 14px; background: var(--primary); color: white; border: none; border-radius: 6px; font-size: 16px; font-weight: bold; cursor: pointer; transition: 0.2s; }\n"
"button.save-btn:hover { background: #2563eb; }\n"
"button.save-btn:active { transform: scale(0.98); }\n"
"button.danger-btn { background: #ef4444; margin-top: 10px; }\n"
"button.danger-btn:hover { background: #dc2626; }\n"
".toast { display: none; text-align: center; margin-top: 15px; padding: 12px; background: var(--accent); border-radius: 6px; font-weight: bold; transition: 0.3s; }\n"
"</style>\n"
"</head>\n"
"<body>\n"
"<div class=\"container\">\n"
"<h2>MATRIX HUB PRO</h2>\n"
"<div class=\"tabs\">\n"
"<button type=\"button\" class=\"tab-btn active\" onclick=\"switchTab(this, 'display')\">Display</button>\n"
"<button type=\"button\" class=\"tab-btn\" onclick=\"switchTab(this, 'data')\">Data</button>\n"
"<button type=\"button\" class=\"tab-btn\" onclick=\"switchTab(this, 'network')\">Network</button>\n"
"</div>\n"
"<div id=\"display\" class=\"tab-content active\">\n"
"<div class=\"form-group\">\n"
"<label>Display Mode</label>\n"
"<select id=\"mode\">\n"
"<option value=\"0\">Custom Text Only</option>\n"
"<option value=\"1\">Clock Only</option>\n"
"<option value=\"2\">Sensor Only</option>\n"
"<option value=\"3\">Cycle All Automatically</option>\n"
"</select>\n"
"</div>\n"
"<div class=\"form-group\">\n"
"<label>Font Weight</label>\n"
"<select id=\"fontStyle\">\n"
"<option value=\"0\">Normal (Default)</option>\n"
"<option value=\"1\">Bold (Thick Typography)</option>\n"
"</select>\n"
"</div>\n"
"<div class=\"form-group\">\n"
"<label>Scroll Speed <span id=\"speedVal\" class=\"val-display\">40 ms</span></label>\n"
"<input type=\"range\" id=\"speed\" min=\"10\" max=\"120\" step=\"5\" oninput=\"updateLabel('speedVal', this.value, ' ms')\">\n"
"</div>\n"
"<div class=\"form-group\">\n"
"<label>Alphabet Spacing <span id=\"charSpaceVal\" class=\"val-display\">2 px</span></label>\n"
"<input type=\"range\" id=\"charSpace\" min=\"0\" max=\"5\" step=\"1\" oninput=\"updateLabel('charSpaceVal', this.value, ' px')\">\n"
"</div>\n"
"<div class=\"form-group\">\n"
"<label>Brightness <span id=\"brightVal\" class=\"val-display\">2</span></label>\n"
"<input type=\"range\" id=\"brightness\" min=\"0\" max=\"15\" step=\"1\" oninput=\"updateLabel('brightVal', this.value, '')\">\n"
"</div>\n"
"<button type=\"button\" class=\"save-btn\" onclick=\"saveSettings(this)\">Apply Changes</button>\n"
"</div>\n"
"<div id=\"data\" class=\"tab-content\">\n"
"<div class=\"form-group\">\n"
"<label>Primary Custom Text</label>\n"
"<textarea id=\"text\" placeholder=\"Main scrolling message...\"></textarea>\n"
"</div>\n"
"<div class=\"form-group\">\n"
"<label>Extra Data (Leave blank to hide)</label>\n"
"<textarea id=\"extraData\" placeholder=\"Manually type extra data here...\"></textarea>\n"
"</div>\n"
"<div class=\"form-group\">\n"
"<label>Live API Endpoint URL (Optional)</label>\n"
"<input type=\"text\" id=\"apiUrl\" placeholder=\"https://api.yourbackend.com/data\">\n"
"</div>\n"
"<div style=\"border-top: 1px solid #475569; margin: 20px 0;\"></div>\n"
"<div class=\"form-group\">\n"
"<label>Custom Boot Greeting</label>\n"
"<input type=\"text\" id=\"cGreet\" placeholder=\"e.g. Hello Rishabh!\">\n"
"</div>\n"
"<div class=\"form-group\">\n"
"<label>Boot Greeting Position</label>\n"
"<select id=\"gMode\">\n"
"<option value=\"0\">System Greeting Only</option>\n"
"<option value=\"1\">Custom First, Then System</option>\n"
"<option value=\"2\">System First, Then Custom</option>\n"
"<option value=\"3\">Custom Greeting Only</option>\n"
"</select>\n"
"</div>\n"
"<button type=\"button\" class=\"save-btn\" onclick=\"saveSettings(this)\">Apply Changes</button>\n"
"</div>\n"
"<div id=\"network\" class=\"tab-content\">\n"
"<div class=\"form-group\">\n"
"<label>Wi-Fi Network Name (SSID)</label>\n"
"<input type=\"text\" id=\"ssidInput\" placeholder=\"Enter new Wi-Fi name\">\n"
"</div>\n"
"<div class=\"form-group\">\n"
"<label>Wi-Fi Password</label>\n"
"<input type=\"password\" id=\"passInput\" placeholder=\"Enter new Wi-Fi password\">\n"
"</div>\n"
"<button type=\"button\" class=\"save-btn danger-btn\" onclick=\"saveNetwork(this)\">Update Wi-Fi & Restart</button>\n"
"</div>\n"
"<div id=\"toast\" class=\"toast\">Changes Applied Successfully!</div>\n"
"</div>\n"
"<script>\n"
"function updateLabel(id, val, suffix) {\n"
"document.getElementById(id).innerText = val + suffix;\n"
"}\n"
"function switchTab(btn, tabId) {\n"
"var contents = document.querySelectorAll('.tab-content');\n"
"for (var i = 0; i < contents.length; i++) contents[i].classList.remove('active');\n"
"var buttons = document.querySelectorAll('.tab-btn');\n"
"for (var j = 0; j < buttons.length; j++) buttons[j].classList.remove('active');\n"
"document.getElementById(tabId).classList.add('active');\n"
"btn.classList.add('active');\n"
"}\n"
"window.onload = function() {\n"
"fetch('/api/settings').then(function(res) { return res.json(); }).then(function(data) {\n"
"document.getElementById('mode').value = data.mode;\n"
"document.getElementById('text').value = data.text;\n"
"document.getElementById('extraData').value = data.extraData;\n"
"document.getElementById('apiUrl').value = data.apiUrl;\n"
"document.getElementById('cGreet').value = data.cGreet;\n"
"document.getElementById('gMode').value = data.gMode;\n"
"document.getElementById('speed').value = data.speed;\n"
"updateLabel('speedVal', data.speed, ' ms');\n"
"document.getElementById('brightness').value = data.brightness;\n"
"updateLabel('brightVal', data.brightness, '');\n"
"document.getElementById('charSpace').value = data.charSpace;\n"
"updateLabel('charSpaceVal', data.charSpace, ' px');\n"
"document.getElementById('fontStyle').value = data.fontStyle;\n"
"});\n"
"};\n"
"function saveSettings(btn) {\n"
"var originalText = btn.innerText;\n"
"btn.innerText = \"Applying...\";\n"
"btn.style.opacity = \"0.7\";\n"
"var params = new URLSearchParams();\n"
"params.append('mode', document.getElementById('mode').value);\n"
"params.append('text', document.getElementById('text').value);\n"
"params.append('extraData', document.getElementById('extraData').value);\n"
"params.append('apiUrl', document.getElementById('apiUrl').value);\n"
"params.append('cGreet', document.getElementById('cGreet').value);\n"
"params.append('gMode', document.getElementById('gMode').value);\n"
"params.append('speed', document.getElementById('speed').value);\n"
"params.append('brightness', document.getElementById('brightness').value);\n"
"params.append('charSpace', document.getElementById('charSpace').value);\n"
"params.append('fontStyle', document.getElementById('fontStyle').value);\n"
"fetch('/update', { method: 'POST', body: params }).then(function(res) {\n"
"btn.innerText = originalText;\n"
"btn.style.opacity = \"1\";\n"
"if(res.ok) {\n"
"var t = document.getElementById('toast');\n"
"t.style.display = 'block';\n"
"setTimeout(function() { t.style.display = 'none'; }, 3000);\n"
"} else { alert(\"Error saving settings.\"); }\n"
"}).catch(function(err) {\n"
"btn.innerText = originalText;\n"
"btn.style.opacity = \"1\";\n"
"alert(\"Network Connection Error.\");\n"
"});\n"
"}\n"
"function saveNetwork(btn) {\n"
"var ssid = document.getElementById('ssidInput').value;\n"
"var pass = document.getElementById('passInput').value;\n"
"if(!ssid) { alert(\"SSID cannot be empty!\"); return; }\n"
"btn.innerText = \"Restarting...\";\n"
"var params = new URLSearchParams();\n"
"params.append('ssid', ssid);\n"
"params.append('pass', pass);\n"
"fetch('/wifi', { method: 'POST', body: params }).then(function() { alert(\"Wi-Fi credentials saved! Please connect your phone to the new network.\"); });\n"
"}\n"
"</script>\n"
"</body>\n"
"</html>";

// ==========================================
// HELPER FUNCTIONS
// ==========================================

String escapeJSON(const String& input) {
    String output = "";
    for (int i = 0; i < input.length(); i++) {
        char c = input.charAt(i);
        if (c == '"') output += "\\\"";
        else if (c == '\\') output += "\\\\";
        else if (c == '\n') output += "\\n";
        else if (c == '\r') output += "\\r";
        else if (c == '\t') output += "\\t";
        else output += c;
    }
    return output;
}

void removeZeroSlash(char* str) {
    for(int i = 0; str[i] != '\0'; i++) {
        if(str[i] == '0') str[i] = 'O'; 
    }
}

void applyFontStyle() {
    if (fontStyle == 1) myDisplay.setFont(boldFont);
    else myDisplay.setFont(nullptr);
}

void fetchApiData() {
    if(apiUrl.length() == 0 || WiFi.status() != WL_CONNECTED) return;
    
    HTTPClient http;
    http.setTimeout(2500); 
    
    if(apiUrl.startsWith("https")) {
        WiFiClientSecure client;
        client.setInsecure();
        http.begin(client, apiUrl);
    } else {
        http.begin(apiUrl);
    }
    
    if(http.GET() == 200) {
        String payload = http.getString();
        payload.substring(0, sizeof(extraData) - 1).toCharArray(extraData, sizeof(extraData));
        removeZeroSlash(extraData);
    } else {
        strcpy(extraData, "API Error"); 
    }
    http.end();
}

void fetchTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo) || timeinfo.tm_year < 120) {
        strcpy(timeStr, "No Sync");
        return;
    }
    strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
    removeZeroSlash(timeStr); 
}

void fetchSensor() {
    float h = dht.readHumidity() - 3.5;
    float t = dht.readTemperature();
    if (isnan(h) || isnan(t)) {
        strcpy(sensorStr, "Sensor Err");
    } else {
        sprintf(sensorStr, "Temp: %.0f C  |  Hum: %.0f %%", t, h);
        removeZeroSlash(sensorStr); 
    }
}

void generateBootGreeting() {
    struct tm timeinfo;
    char sysGreet[30];

    if (getLocalTime(&timeinfo) && timeinfo.tm_year >= 120) {
        if (timeinfo.tm_hour < 12) strcpy(sysGreet, "Good Morning!");
        else if (timeinfo.tm_hour < 17) strcpy(sysGreet, "Good Afternoon!");
        else strcpy(sysGreet, "Good Evening!");
    } else {
        strcpy(sysGreet, "Welcome!"); 
    }

    if (greetingMode == 0) {
        strcpy(bootMsg1, sysGreet);
        bootMsg2[0] = '\0';
    } 
    else if (greetingMode == 1) {
        strcpy(bootMsg1, customGreeting);
        strcpy(bootMsg2, sysGreet);
    } 
    else if (greetingMode == 2) {
        strcpy(bootMsg1, sysGreet);
        strcpy(bootMsg2, customGreeting);
    } 
    else if (greetingMode == 3) {
        strcpy(bootMsg1, customGreeting);
        bootMsg2[0] = '\0';
    }

    removeZeroSlash(bootMsg1);
    if(strlen(bootMsg2) > 0) removeZeroSlash(bootMsg2);
}

// ==========================================
// SETUP
// ==========================================
void setup() {
    Serial.begin(115200);

    preferences.begin("matrix_app", false);
    currentSSID = preferences.getString("ssid", currentSSID);
    currentPASS = preferences.getString("pass", currentPASS);
    
    String savedText = preferences.getString("text", "B S SOLUTION");
    savedText.toCharArray(customText, sizeof(customText));
    
    String savedExtra = preferences.getString("extra", ""); 
    savedExtra.toCharArray(extraData, sizeof(extraData));

    String savedCGreet = preferences.getString("cGreet", "Hello"); 
    savedCGreet.toCharArray(customGreeting, sizeof(customGreeting));
    
    apiUrl = preferences.getString("apiUrl", "");
    
    removeZeroSlash(customText); 
    removeZeroSlash(extraData); 
    
    scrollSpeed = preferences.getInt("speed", 40);
    brightness = preferences.getInt("brightness", 2);
    charSpace = preferences.getInt("charSpace", 2);
    displayMode = preferences.getInt("mode", 3);
    fontStyle = preferences.getInt("fontStyle", 0);
    greetingMode = preferences.getInt("gMode", 0);

    dht.begin();
    myDisplay.begin();
    myDisplay.setIntensity(brightness);
    myDisplay.setCharSpacing(charSpace); 
    applyFontStyle();
    myDisplay.displayClear();
    
    myDisplay.displayText("Booting", PA_CENTER, 50, 0, PA_PRINT, PA_NO_EFFECT);
    delay(1000);

    // CRITICAL: Permanently enables AP mode so you are never locked out
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("Matrix_Setup", "12345678"); 
    WiFi.begin(currentSSID.c_str(), currentPASS.c_str());
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 8) {
        delay(500);
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org");
        delay(500); 
        wifiWasConnected = true;
    } else {
        // EXPLICIT AP WARNING: If Wi-Fi fails, scroll this warning once before booting
        myDisplay.displayClear();
        myDisplay.displayText("AP Mode: Matrix_Setup", PA_CENTER, scrollSpeed, 2000, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
        while (!myDisplay.displayAnimate()) {
            // Block everything else until the warning finishes scrolling
        }
    }

    generateBootGreeting();
    
    server.on("/", HTTP_GET, []() { server.send(200, "text/html", index_html); });

    server.on("/api/settings", HTTP_GET, []() {
        String json = "{";
        json += "\"text\":\"" + escapeJSON(String(customText)) + "\",";
        json += "\"extraData\":\"" + escapeJSON(String(extraData)) + "\",";
        json += "\"apiUrl\":\"" + escapeJSON(apiUrl) + "\",";
        json += "\"cGreet\":\"" + escapeJSON(String(customGreeting)) + "\",";
        json += "\"gMode\":" + String(greetingMode) + ",";
        json += "\"speed\":" + String(scrollSpeed) + ",";
        json += "\"brightness\":" + String(brightness) + ",";
        json += "\"charSpace\":" + String(charSpace) + ",";
        json += "\"mode\":" + String(displayMode) + ",";
        json += "\"fontStyle\":" + String(fontStyle);
        json += "}";
        server.send(200, "application/json", json);
    });

    server.on("/update", HTTP_POST, []() {
        if (server.hasArg("text")) {
            String t = server.arg("text");
            t.toCharArray(customText, sizeof(customText));
            removeZeroSlash(customText); 
            preferences.putString("text", String(customText));
        }
        if (server.hasArg("extraData")) {
            String e = server.arg("extraData");
            e.toCharArray(extraData, sizeof(extraData));
            removeZeroSlash(extraData); 
            preferences.putString("extra", String(extraData));
        }
        if (server.hasArg("apiUrl")) {
            apiUrl = server.arg("apiUrl");
            preferences.putString("apiUrl", apiUrl);
            if(apiUrl.length() > 0) fetchApiData(); 
        }
        if (server.hasArg("cGreet")) {
            String c = server.arg("cGreet");
            c.toCharArray(customGreeting, sizeof(customGreeting));
            preferences.putString("cGreet", String(customGreeting));
        }
        if (server.hasArg("gMode")) {
            greetingMode = server.arg("gMode").toInt();
            preferences.putInt("gMode", greetingMode);
        }
        if (server.hasArg("speed")) {
            scrollSpeed = server.arg("speed").toInt();
            preferences.putInt("speed", scrollSpeed);
        }
        if (server.hasArg("charSpace")) {
            charSpace = server.arg("charSpace").toInt();
            preferences.putInt("charSpace", charSpace);
            myDisplay.setCharSpacing(charSpace);
        }
        if (server.hasArg("brightness")) {
            brightness = server.arg("brightness").toInt();
            preferences.putInt("brightness", brightness);
            myDisplay.setIntensity(brightness);
        }
        if (server.hasArg("fontStyle")) {
            fontStyle = server.arg("fontStyle").toInt();
            preferences.putInt("fontStyle", fontStyle);
            applyFontStyle();
        }
        
        myDisplay.displayClear();
        currentState = CLEAR_SCREEN; 
        myDisplay.displayReset();
        
        server.send(200, "text/plain", "OK");
    });

    server.on("/wifi", HTTP_POST, []() {
        if (server.hasArg("ssid") && server.hasArg("pass")) {
            preferences.putString("ssid", server.arg("ssid"));
            preferences.putString("pass", server.arg("pass"));
            
            // Send OK to browser FIRST, then trigger the safe restart flag
            server.send(200, "text/plain", "Restarting...");
            pendingRestart = true;
            restartTime = millis() + 1000;
        }
    });

    server.begin();
    
    myDisplay.displayClear();
    currentState = BOOT_G1;
    myDisplay.displayText(bootMsg1, PA_CENTER, scrollSpeed, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
}

// ==========================================
// LOOP
// ==========================================
void loop() {
    server.handleClient();
    
    // SAFE RESTART LOGIC: Prevents the RTOS panic crash when changing Wi-Fi
    if (pendingRestart && millis() > restartTime) {
        ESP.restart();
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        if (!wifiWasConnected) {
            configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org");
            if(apiUrl.length() > 0) fetchApiData();
            wifiWasConnected = true;
        }
        
        if (apiUrl.length() > 0 && (millis() - lastApiTime >= apiInterval)) {
            fetchApiData();
            lastApiTime = millis();
        }
    } else {
        wifiWasConnected = false;
    }

    if (myDisplay.displayAnimate()) {
        
        if (currentState == BOOT_G1) {
            if (strlen(bootMsg2) > 0) currentState = BOOT_PAUSE;
            else currentState = CLEAR_SCREEN;
        } 
        else if (currentState == BOOT_PAUSE) {
            currentState = BOOT_G2;
        } 
        else if (currentState == BOOT_G2) {
            currentState = CLEAR_SCREEN;
        } 
        else if (displayMode == 0) {
            currentState = SHOW_TEXT;
        } 
        else if (displayMode == 1) {
            currentState = SHOW_TIME;
        } 
        else if (displayMode == 2) {
            currentState = SHOW_SENSOR;
        } 
        else {
            if (currentState == SHOW_TIME) currentState = CLEAR_SCREEN;
            else if (currentState == CLEAR_SCREEN) currentState = SHOW_SENSOR;
            else if (currentState == SHOW_SENSOR) {
                if (strlen(extraData) > 0) currentState = SHOW_EXTRA;
                else currentState = SHOW_TEXT;
            }
            else if (currentState == SHOW_EXTRA) currentState = SHOW_TEXT;
            else currentState = SHOW_TIME;
        }

        switch(currentState) {
            case BOOT_G1:
                myDisplay.displayText(bootMsg1, PA_CENTER, scrollSpeed, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
                break;
                
            case BOOT_PAUSE:
                myDisplay.displayText("", PA_CENTER, 0, 1000, PA_PRINT, PA_NO_EFFECT);
                break;
                
            case BOOT_G2:
                myDisplay.displayText(bootMsg2, PA_CENTER, scrollSpeed, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
                break;
                
            case SHOW_TIME:
                fetchTime(); 
                myDisplay.displayText(timeStr, PA_CENTER, scrollSpeed, 4000, PA_PRINT, PA_NO_EFFECT);
                break;
                
            case CLEAR_SCREEN:
                myDisplay.displayText("", PA_CENTER, 0, 500, PA_PRINT, PA_NO_EFFECT);
                break;
                
            case SHOW_SENSOR:
                fetchSensor();
                myDisplay.displayText(sensorStr, PA_CENTER, scrollSpeed, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
                break;
                
            case SHOW_EXTRA:
                myDisplay.displayText(extraData, PA_CENTER, scrollSpeed, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
                break;

            case SHOW_TEXT:
                myDisplay.displayText(customText, PA_CENTER, scrollSpeed, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
                break;
        }
    }
}
