/*
 * 04_board1_espnow_sender - Board 1 (Remote Sensor Node), ESP-NOW transmitter
 *
 * Stage 4 of the P1 project. Board 1 reads its own DHT11 and unicasts the reading
 * to Board 2 over ESP-NOW. It deliberately never joins the Wi-Fi access point.
 *
 * Board:  ESP32 Dev Module      (FQBN esp32:esp32:esp32)
 * Serial: 115200
 * Sensor: DHT11 on GPIO4 (silkscreen D4)
 *
 * Why Board 1 must not join the access point
 * ------------------------------------------
 * An ESP32 has one radio, therefore one channel at a time. ESP-NOW frames are
 * only heard by a receiver sitting on the same channel. Board 2 joins the access
 * point for its cloud connection, and the access point dictates Board 2's
 * channel. Board 1 therefore has to be pinned to that same channel by hand. If
 * Board 1 also associated with the access point, it would negotiate its own
 * channel and DHCP lease for no benefit, and the two boards could drift apart
 * whenever the router moved either of them. Station mode without association
 * gives Board 1 a working radio and full manual control of the channel.
 *
 * Upload this to BOARD 1, not Board 2.
 */

#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <atomic>
#include "DHT.h"
#include "secrets.h"

// ===========================================================================
// CONFIG - the two values below MUST be replaced before this sketch will work
// ===========================================================================

// Board 2's station MAC, as reported by sketch 03 and confirmed by esptool.
static uint8_t GATEWAY_MAC[6] = { 0x44, 0x1D, 0x64, 0xF4, 0xF1, 0xC8 };

// Must equal the channel sketch 05 reports after Board 2 associates.
//
// This value is NOT permanent, and that is not theoretical. During testing the
// gateway roamed from channel 1 to channel 11 mid-session; delivery stopped
// dead until this constant was updated and re-uploaded, while Wi-Fi and MQTT
// on the gateway carried on looking perfectly healthy. eduroam has access
// points on channels 1, 6 and 11. Always read the channel off the gateway's
// boot banner before a demonstration.
static const uint8_t ESPNOW_CHANNEL = 6;

// How often a reading is transmitted.
static const uint32_t SEND_INTERVAL_MS = 5000;

// Demonstration aid. Set to 1 to transmit every reading twice with an identical
// sequence number, which lets the gateway prove that its duplicate detection
// works. Leave at 0 for normal operation.
#define SEND_DUPLICATE_FOR_DEMO 0

// ESP-NOW link encryption. MUST match ESPNOW_ENCRYPT in sketch 05 - if the two
// sides disagree, frames are discarded with no error on either board.
//
// Verified working on this hardware with the gateway associated to an access
// point and this node unassociated. The keys live in secrets.h and must be
// byte-identical on both boards.
#define ESPNOW_ENCRYPT 1

// ===========================================================================
// Sensor
// ===========================================================================

static const uint8_t DHT_PIN  = 4;  // GPIO4, silkscreen D4
static const uint8_t DHT_KIND = DHT11;

DHT dht(DHT_PIN, DHT_KIND);

// ===========================================================================
// Mesh packet
// ===========================================================================
//
// This struct is duplicated verbatim in 05_board2_gateway_cloud.ino. Arduino
// sketches are separate compilation units with no shared header mechanism, so
// there is no way to include it from one place. The static_assert below exists
// because a silent layout difference between the two copies would present as
// "the gateway rejects every single packet on length", which is a thoroughly
// unpleasant thing to debug. If you edit this struct, edit both copies and
// update both assertions.
//
// ttl and hop_count are carried but not acted on. In a one-hop root/node
// topology there is nothing to forward to. They are here so the packet format
// does not have to change if the network ever grows a second hop.

static const uint8_t PROTO_VERSION   = 1;
static const uint8_t MSG_SENSOR_DATA = 1;
static const uint8_t NODE_ID         = 1;  // this board
static const uint8_t ROOT_ID         = 0;  // the gateway
static const uint8_t DEFAULT_TTL     = 3;

#pragma pack(push, 1)
typedef struct {
  uint8_t  version;
  uint8_t  msg_type;
  uint8_t  src_id;
  uint8_t  dst_id;
  uint32_t boot_id;
  uint32_t seq;
  uint8_t  ttl;
  uint8_t  hop_count;
  float    temperature_c;
  float    humidity_pct;
  uint32_t uptime_ms;
} mesh_packet_t;
#pragma pack(pop)

static_assert(sizeof(mesh_packet_t) == 26, "mesh_packet_t layout changed, update sketch 05 to match");

// ===========================================================================
// State
// ===========================================================================

static uint32_t bootId     = 0;  // randomised once per boot
static uint32_t sequence   = 0;
static uint32_t lastSendMs = 0;
static uint32_t sensorFail = 0;  // Arduino task only

// Shared between the send callback (Wi-Fi task) and loop() (Arduino task), so
// all of them are atomic. The pending pair is drained with exchange() to report
// results; the cumulative pair is only ever incremented and read.
//
// There is deliberately no "last destination MAC" variable. Every send in this
// sketch goes to GATEWAY_MAC, which is a compile-time constant, so loop() can
// format that directly. Copying the address out of the callback would have
// introduced a genuine data race for no information gain.
static std::atomic<uint32_t> pendingOk{0};
static std::atomic<uint32_t> pendingFail{0};
static std::atomic<uint32_t> sendOk{0};
static std::atomic<uint32_t> sendFail{0};

static void macToString(const uint8_t *mac, char *out, size_t outLen) {
  snprintf(out, outLen, "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// ---------------------------------------------------------------------------
// ESP-NOW send callback
//
// IMPORTANT: this signature is specific to the installed core. arduino-esp32
// 3.3.x is built on ESP-IDF 5.5, which replaced the old
//   void cb(const uint8_t *mac, esp_now_send_status_t status)
// with the tx_info form below. Older tutorials use the old form and will not
// compile against this core. The destination MAC now lives in tx_info->des_addr.
// ---------------------------------------------------------------------------
// This callback runs on the Wi-Fi task, so it records the outcome and returns
// rather than printing. Serial output blocks once the UART buffer fills, and
// blocking here delays the radio driver, which can cost later packets or trip
// the task watchdog. reportSendResults() does the printing from loop().
static void onEspNowSent(const esp_now_send_info_t *tx_info, esp_now_send_status_t status) {
  (void)tx_info;  // always GATEWAY_MAC; loop() formats the constant instead
  if (status == ESP_NOW_SEND_SUCCESS) {
    sendOk++;
    pendingOk++;
  } else {
    sendFail++;
    pendingFail++;
  }
}

// Called from loop(), where blocking on the UART is harmless.
static void reportSendResults() {
  uint32_t ok   = pendingOk.exchange(0);
  uint32_t fail = pendingFail.exchange(0);
  if (ok == 0 && fail == 0) {
    return;
  }

  // GATEWAY_MAC rather than an address captured in the callback: it is the only
  // destination this sketch uses, and reading a constant cannot race.
  char macStr[18];
  macToString(GATEWAY_MAC, macStr, sizeof(macStr));

  // Report the drained counts, not just "one happened". If several callbacks
  // land between passes through loop(), collapsing them to a single line would
  // under-report what the radio actually did.
  if (ok > 0) {
    Serial.printf("[NODE] send to %s : OK x%lu (link-layer ack received)  total_ok=%lu\n",
                  macStr, (unsigned long)ok, (unsigned long)sendOk.load());
  }
  if (fail > 0) {
    Serial.printf("[NODE] send to %s : FAILED x%lu (no ack)  total_ok=%lu total_fail=%lu\n",
                  macStr, (unsigned long)fail,
                  (unsigned long)sendOk.load(), (unsigned long)sendFail.load());
    Serial.println("[NODE]   likely causes: wrong gateway MAC, wrong channel, gateway powered off");
  }
}

static bool gatewayMacIsStillPlaceholder() {
  for (int i = 0; i < 6; i++) {
    if (GATEWAY_MAC[i] != 0xAA) {
      return false;
    }
  }
  return true;
}

static void setupRadio() {
  // Station mode brings the radio up. It does NOT associate with anything.
  WiFi.mode(WIFI_STA);

  // Prevent the board from silently rejoining a network whose credentials are
  // still sitting in NVS from an earlier sketch. That association would move
  // the channel out from under ESP-NOW.
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(false, true);  // wifioff=false, eraseap=true
  delay(100);

  // Pin the radio to the gateway's channel. The promiscuous toggle is the
  // documented way to make esp_wifi_set_channel take effect reliably; it is
  // harmless when the station is unassociated.
  esp_wifi_set_promiscuous(true);
  esp_err_t err = esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  if (err != ESP_OK) {
    Serial.printf("[NODE] esp_wifi_set_channel failed: %s\n", esp_err_to_name(err));
  }

  // Read the channel back rather than assuming the request was honoured. A
  // mismatch here is the single most common reason a two-board ESP-NOW demo
  // reports successful sends that never arrive.
  uint8_t actual = 0;
  wifi_second_chan_t secondary = WIFI_SECOND_CHAN_NONE;
  esp_wifi_get_channel(&actual, &secondary);
  Serial.printf("[NODE] channel requested=%u actual=%u\n", ESPNOW_CHANNEL, actual);
  if (actual != ESPNOW_CHANNEL) {
    Serial.println("[NODE] WARNING channel mismatch. ESP-NOW will not reach the gateway.");
  }

  uint8_t ownMac[6] = {0};
  esp_wifi_get_mac(WIFI_IF_STA, ownMac);
  char macStr[18];
  macToString(ownMac, macStr, sizeof(macStr));
  Serial.printf("[NODE] own station MAC = %s\n", macStr);
  Serial.printf("[NODE] Wi-Fi association: none (by design), status=%d\n", (int)WiFi.status());
}

static void setupEspNow() {
  esp_err_t err = esp_now_init();
  if (err != ESP_OK) {
    Serial.printf("[NODE] esp_now_init failed: %s - halting\n", esp_err_to_name(err));
    while (true) {
      delay(1000);
    }
  }
  Serial.println("[NODE] ESP-NOW initialised");

  esp_now_register_send_cb(onEspNowSent);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, GATEWAY_MAC, 6);
  peer.channel = ESPNOW_CHANNEL;
  peer.ifidx   = WIFI_IF_STA;

#if ESPNOW_ENCRYPT
  // Install the primary master key before adding any encrypted peer. The PMK
  // protects the per-peer LMK during setup; the LMK then encrypts the payload.
  // Both keys must be byte-identical on the two boards.
  static const uint8_t pmk[16] = ESPNOW_PMK;
  err = esp_now_set_pmk(pmk);
  if (err != ESP_OK) {
    Serial.printf("[NODE] esp_now_set_pmk failed: %s\n", esp_err_to_name(err));
  }
  static const uint8_t lmk[16] = ESPNOW_LMK;
  memcpy(peer.lmk, lmk, 16);
  peer.encrypt = true;
  Serial.println("[NODE] ESP-NOW encryption ENABLED (PMK/LMK set)");
#else
  peer.encrypt = false;
  Serial.println("[NODE] ESP-NOW encryption disabled - see report security section");
#endif

  // Retry with backoff, then restart. Without a peer entry every send fails, so
  // carrying on would produce an endless stream of send errors that hides the
  // real cause. Halting forever is equally unhelpful, since the failure may be
  // transient - a restart recovers without anyone touching the board.
  const int MAX_ATTEMPTS = 5;
  for (int attempt = 1; ; attempt++) {
    err = esp_now_add_peer(&peer);
    if (err == ESP_OK) {
      break;
    }
    Serial.printf("[NODE] esp_now_add_peer failed (attempt %d/%d): %s\n",
                  attempt, MAX_ATTEMPTS, esp_err_to_name(err));
    if (attempt >= MAX_ATTEMPTS) {
      Serial.println("[NODE] no peer means every send would fail - restarting");
      Serial.println("[NODE] if this repeats, check GATEWAY_MAC and ESPNOW_CHANNEL");
      delay(200);
      ESP.restart();
    }
    delay(200 * attempt);
  }

  {
    char macStr[18];
    macToString(GATEWAY_MAC, macStr, sizeof(macStr));
    Serial.printf("[NODE] gateway peer added: %s on channel %u\n", macStr, ESPNOW_CHANNEL);
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);  // let the USB serial port settle so the banner is not lost
  Serial.println();
  Serial.println("[NODE] ============================================");
  Serial.println("[NODE] 04_board1_espnow_sender");
  Serial.println("[NODE] ============================================");

  // A fresh random identifier each boot. The gateway uses it to tell "Board 1
  // restarted and its sequence numbers went back to zero" apart from "Board 1
  // is replaying old packets".
  bootId = esp_random();
  Serial.printf("[NODE] boot_id = %lu\n", (unsigned long)bootId);
  Serial.printf("[NODE] packet size = %u bytes (ESP-NOW limit is 250)\n",
                (unsigned)sizeof(mesh_packet_t));

  if (gatewayMacIsStillPlaceholder()) {
    Serial.println("[NODE] --------------------------------------------------------");
    Serial.println("[NODE] WARNING GATEWAY_MAC is still the AA:AA:AA:AA:AA:AA");
    Serial.println("[NODE] placeholder. Run sketch 03 on Board 2, copy the MAC line");
    Serial.println("[NODE] it prints into this sketch, and re-upload. Every send");
    Serial.println("[NODE] will fail until you do.");
    Serial.println("[NODE] --------------------------------------------------------");
  }

  dht.begin();
  setupRadio();
  setupEspNow();

  Serial.printf("[NODE] sending every %lu ms\n", (unsigned long)SEND_INTERVAL_MS);
#if SEND_DUPLICATE_FOR_DEMO
  Serial.println("[NODE] SEND_DUPLICATE_FOR_DEMO is on: every packet is sent twice");
  Serial.println("[NODE] with the same sequence number, so the gateway should count");
  Serial.println("[NODE] one duplicate per reading.");
#endif
}

void loop() {
  // Print any send results the callback recorded since the last pass. Done
  // here rather than in the callback so the Wi-Fi task is never blocked on the
  // UART.
  reportSendResults();

  if (millis() - lastSendMs < SEND_INTERVAL_MS) {
    return;
  }
  lastSendMs = millis();

  float humidity    = dht.readHumidity();
  float temperature = dht.readTemperature();  // degrees Celsius

  // isfinite rather than isnan: an infinity would pass an isnan test and then
  // be rejected by the gateway's range check, wasting a sequence number.
  if (!isfinite(humidity) || !isfinite(temperature)) {
    // Skip the transmission entirely and do not consume a sequence number.
    // Sending NaN would only give the gateway something to reject, and burning
    // a sequence number would make the gap look like a lost packet.
    sensorFail++;
    Serial.printf("[NODE] sensor read FAILED (NaN), skipping send. sensor_fails=%lu\n",
                  (unsigned long)sensorFail);
    return;
  }

  mesh_packet_t pkt = {};
  pkt.version       = PROTO_VERSION;
  pkt.msg_type      = MSG_SENSOR_DATA;
  pkt.src_id        = NODE_ID;
  pkt.dst_id        = ROOT_ID;
  pkt.boot_id       = bootId;
  pkt.seq           = ++sequence;
  pkt.ttl           = DEFAULT_TTL;
  pkt.hop_count     = 0;
  pkt.temperature_c = temperature;
  pkt.humidity_pct  = humidity;
  pkt.uptime_ms     = millis();

  Serial.printf("[NODE] tx seq=%lu temperature=%.1f C humidity=%.1f %% uptime=%lu ms\n",
                (unsigned long)pkt.seq, temperature, humidity,
                (unsigned long)pkt.uptime_ms);

  esp_err_t err = esp_now_send(GATEWAY_MAC, (const uint8_t *)&pkt, sizeof(pkt));
  if (err != ESP_OK) {
    Serial.printf("[NODE] esp_now_send queue error: %s\n", esp_err_to_name(err));
  }

#if SEND_DUPLICATE_FOR_DEMO
  // Same sequence number on purpose. The gateway must accept the first copy and
  // count the second as a duplicate.
  delay(50);
  Serial.printf("[NODE] tx seq=%lu again (deliberate duplicate for the demo)\n",
                (unsigned long)pkt.seq);
  esp_now_send(GATEWAY_MAC, (const uint8_t *)&pkt, sizeof(pkt));
#endif
}
