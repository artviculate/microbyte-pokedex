#include <M5Cardputer.h>
#include <WiFi.h>
#include <esp_now.h>

// Broadcast address (sends to any listening ESP-NOW device)
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

typedef struct struct_message {
    char key;
    char action[10]; // "KEY", "ENTER", "BACKSPACE"
} struct_message;

struct_message msg;
String localBuffer = "";

void updateDisplay() {
    M5Cardputer.Display.fillScreen(BLACK);
    
    // Header Bar
    M5Cardputer.Display.fillRect(0, 0, 240, 20, BLUE);
    M5Cardputer.Display.setTextColor(WHITE);
    M5Cardputer.Display.setTextSize(1.2);
    M5Cardputer.Display.setCursor(5, 4);
    M5Cardputer.Display.print("CARDPUTER REMOTE TX");

    // Input Label & Live Typed Text
    M5Cardputer.Display.setTextColor(GREEN);
    M5Cardputer.Display.setTextSize(1.5);
    M5Cardputer.Display.setCursor(10, 35);
    M5Cardputer.Display.print("Searching:");

    M5Cardputer.Display.setTextColor(YELLOW);
    M5Cardputer.Display.setTextSize(1.8);
    M5Cardputer.Display.setCursor(10, 65);
    
    if (localBuffer.length() > 0) {
        M5Cardputer.Display.print(localBuffer + "_");
    } else {
        M5Cardputer.Display.setTextColor(DARKGREY);
        M5Cardputer.Display.print("Type mon/ID...");
    }

    // Footer Hint
    M5Cardputer.Display.setTextColor(WHITE);
    M5Cardputer.Display.setTextSize(1.0);
    M5Cardputer.Display.setCursor(10, 115);
    M5Cardputer.Display.print("[Enter] Send  |  [Del] Clear");
}

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg);
    M5Cardputer.Display.setRotation(1);

    WiFi.mode(WIFI_STA);

    if (esp_now_init() != ESP_OK) {
        M5Cardputer.Display.println("ESP-NOW Init Failed!");
        return;
    }

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;  
    peerInfo.encrypt = false;
    
    esp_now_add_peer(&peerInfo);

    updateDisplay();
}

void loop() {
    M5Cardputer.update();

    if (M5Cardputer.Keyboard.isChange()) {
        if (M5Cardputer.Keyboard.isPressed()) {
            auto status = M5Cardputer.Keyboard.keysState();
            
            if (status.del) {
                if (localBuffer.length() > 0) {
                    localBuffer.remove(localBuffer.length() - 1);
                }
                strcpy(msg.action, "BACKSPACE");
                msg.key = 0;
                esp_now_send(broadcastAddress, (uint8_t *)&msg, sizeof(msg));
                updateDisplay();
            } else if (status.enter) {
                strcpy(msg.action, "ENTER");
                msg.key = 0;
                esp_now_send(broadcastAddress, (uint8_t *)&msg, sizeof(msg));
                localBuffer = ""; // Clear buffer after sending
                updateDisplay();
            } else {
                for (auto c : status.word) {
                    if (localBuffer.length() < 15) {
                        localBuffer += c;
                    }
                    strcpy(msg.action, "KEY");
                    msg.key = c;
                    esp_now_send(broadcastAddress, (uint8_t *)&msg, sizeof(msg));
                }
                updateDisplay();
            }
        }
    }
}
