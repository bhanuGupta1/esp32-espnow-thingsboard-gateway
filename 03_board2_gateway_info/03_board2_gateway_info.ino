/*
 * 03_board2_gateway_info - discover Board 2's identity and the radio landscape
 *
 * Stage 3 of the P1 project. This sketch produces the one value that the rest of
 * the project cannot be built without: Board 2's station MAC address. Board 1
 * sends ESP-NOW unicast frames to that address.
 *
 * It also scans for nearby access points and prints their channels. An ESP32 has
 * a single 2.4 GHz radio, so the scan list is by definition the 2.4 GHz networks
 * that Board 2 is capable of joining. If your home network does not appear here,
 * it is almost certainly a 5 GHz-only SSID and Board 2 will never connect to it.
 *
 * Board:  ESP32 Dev Module      (FQBN esp32:esp32:esp32)
 * Serial: 115200
 *
 * Upload this to BOARD 2, not Board 1.
 *
 * Note on MAC addresses: an ESP32 exposes several. The station (STA) interface
 * MAC is the one ESP-NOW uses when the radio is in station mode, which is how
 * both sketches 04 and 05 run. The soft-AP MAC differs by one bit and using it
 * by mistake produces sends that report success but never arrive.
 */

#include <WiFi.h>
#include <esp_wifi.h>

// Optional: set this to 1 and fill in the two placeholders below to have this
// sketch join your network and report the channel it landed on. This is only a
// convenience preview. The channel that actually matters is the one printed by
// sketch 05, because that is the sketch running during the demonstration.
#define TRY_WIFI_CONNECT 0

#if TRY_WIFI_CONNECT
  #define PREVIEW_WIFI_SSID     "YOUR_2G4_WIFI_SSID"
  #define PREVIEW_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
  static const uint32_t CONNECT_TIMEOUT_MS = 20000;
#endif

static void printStationMac() {
  uint8_t mac[6] = {0};

  // Read the STA interface MAC directly rather than trusting a cached string.
  esp_err_t err = esp_wifi_get_mac(WIFI_IF_STA, mac);
  if (err != ESP_OK) {
    Serial.printf("[GATEWAY] esp_wifi_get_mac failed: %s\n", esp_err_to_name(err));
    return;
  }

  Serial.printf("[GATEWAY] station MAC (human readable) : %02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  // Emit a line that can be pasted straight into sketch 04 without retyping
  // hex by hand, which is where this step usually goes wrong.
  Serial.println("[GATEWAY]");
  Serial.println("[GATEWAY] Copy the next line into 04_board1_espnow_sender.ino:");
  Serial.printf("[GATEWAY] static uint8_t GATEWAY_MAC[6] = { 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X };\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.println("[GATEWAY]");
}

static void scanForAccessPoints() {
  Serial.println("[GATEWAY] scanning for 2.4 GHz access points ...");

  int found = WiFi.scanNetworks();
  if (found <= 0) {
    Serial.println("[GATEWAY] no networks found. Move closer to the router and reset the board.");
    return;
  }

  Serial.printf("[GATEWAY] %d network(s) visible:\n", found);
  Serial.println("[GATEWAY]   ch  rssi  security  ssid");
  for (int i = 0; i < found; i++) {
    Serial.printf("[GATEWAY]   %2d  %4d  %-8s  %s\n",
                  WiFi.channel(i),
                  WiFi.RSSI(i),
                  WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "open" : "secured",
                  WiFi.SSID(i).c_str());
  }
  WiFi.scanDelete();

  Serial.println("[GATEWAY] Every channel above is a 2.4 GHz channel, because that is");
  Serial.println("[GATEWAY] the only band this radio has. A missing SSID means 5 GHz only.");
}

#if TRY_WIFI_CONNECT
static void previewConnect() {
  Serial.printf("[GATEWAY] connecting to \"%s\" ...\n", PREVIEW_WIFI_SSID);
  WiFi.begin(PREVIEW_WIFI_SSID, PREVIEW_WIFI_PASSWORD);

  uint32_t startedMs = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startedMs < CONNECT_TIMEOUT_MS) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("[GATEWAY] connect failed (status=%d). Check the SSID, the password,\n",
                  (int)WiFi.status());
    Serial.println("[GATEWAY] and that the network is 2.4 GHz.");
    return;
  }

  uint8_t channel = 0;
  wifi_second_chan_t secondary = WIFI_SECOND_CHAN_NONE;
  esp_wifi_get_channel(&channel, &secondary);

  Serial.printf("[GATEWAY] connected  ip=%s  rssi=%d dBm\n",
                WiFi.localIP().toString().c_str(), WiFi.RSSI());
  Serial.printf("[GATEWAY] active 2.4 GHz channel = %u\n", channel);
  Serial.println("[GATEWAY] This is a preview only. Use the channel printed by sketch 05.");
}
#endif

void setup() {
  Serial.begin(115200);
  delay(300);  // let the USB serial port settle so the banner is not lost
  Serial.println();
  Serial.println("[GATEWAY] ============================================");
  Serial.println("[GATEWAY] 03_board2_gateway_info");
  Serial.println("[GATEWAY] ============================================");

  // Station mode brings the radio up without joining anything. The MAC is
  // readable from this point on.
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, true);  // drop any AP credentials stored in NVS
  delay(100);

  printStationMac();
  scanForAccessPoints();

#if TRY_WIFI_CONNECT
  previewConnect();
#else
  Serial.println("[GATEWAY] TRY_WIFI_CONNECT is 0, so no AP association was attempted.");
#endif

  Serial.println("[GATEWAY] done. Record the station MAC before moving on.");
}

void loop() {
  // Everything useful happened in setup(). Reprint the reminder occasionally so
  // the MAC is still on screen if the Serial Monitor was opened late.
  static uint32_t lastMs = 0;
  if (millis() - lastMs < 10000) {
    return;
  }
  lastMs = millis();
  Serial.println("[GATEWAY] idle. Reset the board to repeat the scan.");
}
