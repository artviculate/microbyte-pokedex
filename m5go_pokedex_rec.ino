#include <FS.h>
#include <SD.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <esp_now.h>

struct Pokemon {
    int id;
    String name, form, type1, type2;
    int total, hp, atk, def, spAtk, spDef, speed, gen;
};

Pokemon currentMon;
int currentDexId = 1;
bool found = false;
String statusInfo = "";
String searchBuffer = "";

typedef struct struct_message {
    char key;
    char action[10]; // "KEY", "ENTER", "BACKSPACE"
} struct_message;

struct_message incomingMsg;

// Forward declarations
void drawUI();
bool searchPokedex(String query);

// ESP-NOW Data Received Callback (Receives keypresses from Cardputer)
void OnDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
    memcpy(&incomingMsg, incomingData, sizeof(incomingMsg));

    if (strcmp(incomingMsg.action, "BACKSPACE") == 0) {
        if (searchBuffer.length() > 0) {
            searchBuffer.remove(searchBuffer.length() - 1);
        }
    } else if (strcmp(incomingMsg.action, "ENTER") == 0) {
        if (searchBuffer.length() > 0) {
            found = searchPokedex(searchBuffer);
            searchBuffer = ""; // Clear buffer after search
        }
    } else if (strcmp(incomingMsg.action, "KEY") == 0) {
        if (searchBuffer.length() < 15) {
            searchBuffer += incomingMsg.key;
        }
    }
    drawUI();
}

bool parseCsvLine(String line, Pokemon &mon) {
    line.trim();
    line.replace("\r", "");
    if (line.length() == 0) return false;

    String fields[15];
    int idx = 0, start = 0;

    for (int i = 0; i < line.length(); i++) {
        if (line.charAt(i) == ',') {
            if (idx < 15) {
                fields[idx] = line.substring(start, i);
                fields[idx].trim();
                fields[idx].replace("\"", "");
            }
            idx++;
            start = i + 1;
        }
    }
    if (idx < 15 && start <= line.length()) {
        fields[idx] = line.substring(start);
        fields[idx].trim();
        fields[idx].replace("\"", "");
    }

    if (idx < 1 || fields[0].length() == 0) return false;

    mon.id    = fields[0].toInt();
    mon.name  = fields[1];
    mon.form  = (idx >= 2) ? fields[2] : "";
    mon.type1 = (idx >= 3) ? fields[3] : "";
    mon.type2 = (idx >= 4) ? fields[4] : "";
    mon.total = (idx >= 5) ? fields[5].toInt() : 0;
    mon.hp    = (idx >= 6) ? fields[6].toInt() : 0;
    mon.atk   = (idx >= 7) ? fields[7].toInt() : 0;
    mon.def   = (idx >= 8) ? fields[8].toInt() : 0;
    mon.spAtk = (idx >= 9) ? fields[9].toInt() : 0;
    mon.spDef = (idx >= 10) ? fields[10].toInt() : 0;
    mon.speed = (idx >= 11) ? fields[11].toInt() : 0;
    mon.gen   = (idx >= 12) ? fields[12].toInt() : 1;

    return true;
}

bool searchPokedex(String query) {
    statusInfo = "";
    File file = SD.open("/pokemon.csv", FILE_READ);
    if (!file) file = SD.open("/pokemon", FILE_READ);

    if (!file) {
        statusInfo = "Error: File not found on SD!";
        return false;
    }

    query.toLowerCase();
    query.trim();

    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        line.replace("\r", "");

        if (line.length() == 0 || line.startsWith("ID,") || line.startsWith("id,")) continue;

        Pokemon tempMon;
        if (parseCsvLine(line, tempMon)) {
            String checkName = tempMon.name;
            checkName.toLowerCase();

            if (query == String(tempMon.id) || checkName.equalsIgnoreCase(query) || checkName.startsWith(query)) {
                currentMon = tempMon;
                currentDexId = tempMon.id;
                file.close();
                return true;
            }
        }
    }
    file.close();
    statusInfo = "No match found";
    return false;
}

void drawSprite(int dexId) {
    String pngPath = "/sprites/" + String(dexId) + ".png";
    String bmpPath = "/sprites/" + String(dexId) + ".bmp";
    
    // Scaled up to 2.0x (128x128 pixels) positioned at X=180, Y=35
    int imgX = 180;
    int imgY = 35;

    if (SD.exists(pngPath)) {
        M5.Display.drawPngFile(SD, pngPath.c_str(), imgX, imgY, 0, 0, 0, 0, 2.0, 2.0);
    } else if (SD.exists(bmpPath)) {
        M5.Display.drawBmpFile(SD, bmpPath.c_str(), imgX, imgY, 0, 0, 0, 0, 2.0, 2.0);
    } else {
        M5.Display.drawRect(imgX, imgY, 128, 128, DARKGREY);
        M5.Display.setTextColor(DARKGREY);
        M5.Display.setTextSize(1.0);
        M5.Display.setCursor(imgX + 30, imgY + 55);
        M5.Display.print("No Image");
    }
}

void drawUI() {
    M5.Display.fillScreen(BLACK);
    
    // Header Bar
    M5.Display.fillRect(0, 0, 320, 26, BLUE);
    M5.Display.setTextColor(WHITE);
    M5.Display.setTextSize(1.2);
    M5.Display.setCursor(10, 6);
    
    if (searchBuffer.length() > 0) {
        M5.Display.printf("SEARCH: %s_", searchBuffer.c_str());
    } else {
        M5.Display.print("M5GO POKEDEX (Cardputer Remote)");
    }

    if (found) {
        // Name & ID
        M5.Display.setTextColor(YELLOW);
        M5.Display.setTextSize(1.4);
        M5.Display.setCursor(8, 33);
        M5.Display.printf("#%04d %s", currentMon.id, currentMon.name.c_str());

        // Type & Base Stat Total
        M5.Display.setTextColor(CYAN);
        M5.Display.setTextSize(1.1);
        M5.Display.setCursor(8, 54);
        if (currentMon.type2.length() > 0) {
            M5.Display.printf("%s/%s | %d", currentMon.type1.c_str(), currentMon.type2.c_str(), currentMon.total);
        } else {
            M5.Display.printf("%s | %d", currentMon.type1.c_str(), currentMon.total);
        }

        // Base Stats Text
        M5.Display.setTextColor(WHITE);
        M5.Display.setTextSize(1.0);
        M5.Display.setCursor(8, 76);  M5.Display.printf("HP : %-3d  ATK: %-3d", currentMon.hp, currentMon.atk);
        M5.Display.setCursor(8, 93);  M5.Display.printf("DEF: %-3d  SPD: %-3d", currentMon.def, currentMon.speed);
        M5.Display.setCursor(8, 110); M5.Display.printf("SpA: %-3d  SpD: %-3d", currentMon.spAtk, currentMon.spDef);

        // Visual HP Bar Gauge
        int hpWidth = map(constrain(currentMon.hp, 1, 255), 1, 255, 0, 150);
        M5.Display.setCursor(8, 132); M5.Display.setTextColor(GREEN); M5.Display.print("HP BAR:");
        M5.Display.drawRect(8, 145, 154, 10, WHITE);
        M5.Display.fillRect(10, 147, hpWidth, 6, GREEN);

        // Render larger sprite
        drawSprite(currentMon.id);

    } else if (statusInfo.length() > 0) {
        M5.Display.setTextColor(RED);
        M5.Display.setTextSize(1.2);
        M5.Display.setCursor(10, 50);
        M5.Display.println(statusInfo);
    }

    // On-Screen Button Labels
    M5.Display.fillRect(0, 210, 320, 30, DARKGREY);
    M5.Display.setTextColor(WHITE);
    M5.Display.setTextSize(1.2);
    M5.Display.setCursor(25, 218);  M5.Display.print("< PREV");
    M5.Display.setCursor(125, 218); M5.Display.print("RANDOM");
    M5.Display.setCursor(235, 218); M5.Display.print("NEXT >");
}

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    
    // Power management for M5Unified
    M5.Power.begin();

    M5.Display.setRotation(1);
    M5.Display.fillScreen(BLACK);

    // Init Wi-Fi for ESP-NOW wireless receiving
    WiFi.mode(WIFI_STA);
    if (esp_now_init() == ESP_OK) {
        esp_now_register_recv_cb(OnDataRecv);
    }

    // M5GO / M5Core SD CS pin is GPIO 4
    if (!SD.begin(4, SPI, 25000000)) {
        M5.Display.setTextColor(RED);
        M5.Display.setTextSize(1.2);
        M5.Display.setCursor(10, 20);
        M5.Display.println("SD Mount Failed!");
        while (1) delay(100);
    }

    found = searchPokedex(String(currentDexId));
    drawUI();
}

void loop() {
    M5.update();

    if (M5.BtnA.wasPressed()) { // Previous
        currentDexId = (currentDexId > 1) ? currentDexId - 1 : 1025;
        found = searchPokedex(String(currentDexId));
        drawUI();
    }

    if (M5.BtnB.wasPressed()) { // Random
        currentDexId = random(1, 1026);
        found = searchPokedex(String(currentDexId));
        drawUI();
    }

    if (M5.BtnC.wasPressed()) { // Next
        currentDexId = (currentDexId < 1025) ? currentDexId + 1 : 1;
        found = searchPokedex(String(currentDexId));
        drawUI();
    }
}
