/*
 * 05_board2_gateway_cloud - Board 2 (Root Gateway + Local Sensor)
 *
 * Stage 5 of the P1 project, and the sketch that runs during the demonstration.
 * Board 2 does four jobs at once:
 *
 *   1. joins the 2.4 GHz access point, which is what gives it a cloud path
 *   2. receives ESP-NOW sensor packets from Board 1 and deduplicates them
 *   3. reads its own local DHT11
 *   4. merges local and remote readings into one JSON payload and publishes it
 *      to ThingsBoard over MQTT
 *
 * Board:  ESP32 Dev Module      (FQBN esp32:esp32:esp32)
 * Serial: 115200
 * Sensor: DHT11 on GPIO5 (silkscreen D5)
 *
 * Order of operations matters
 * ---------------------------
 * Wi-Fi is brought up FIRST, then ESP-NOW. An ESP32 has one radio and therefore
 * one channel. Once the station associates, the access point owns that channel
 * and it cannot be overridden without dropping the connection. So the channel is
 * a value this sketch DISCOVERS and reports, not one it chooses. Whatever
 * channel it prints must be typed into sketch 04 on Board 1.
 *
 * Copy secrets.example.h to secrets.h in this folder and fill it in before
 * uploading. secrets.h is gitignored.
 *
 * Upload this to BOARD 2, not Board 1.
 */

#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <PubSubClient.h>
#include <stdarg.h>
#include <atomic>
#include "DHT.h"
#include "secrets.h"

// ===========================================================================
// CONFIG
// ===========================================================================

static const uint8_t  DHT_PIN                = 5;      // GPIO5, silkscreen D5
static const uint8_t  DHT_KIND               = DHT11;

// Board 1's station MAC. Frames from any other sender are discarded before
// their contents are examined, because the src_id field inside the packet is a
// claim rather than proof of identity. Read this off Board 1's own boot banner
// ("own station MAC") in sketch 04 if you swap boards.
static const uint8_t NODE_MAC[6] = { 0x44, 0x1D, 0x64, 0xF5, 0xFA, 0x24 };

static const uint32_t LOCAL_READ_INTERVAL_MS = 2000;   // DHT11 needs >= 1000 ms
static const uint32_t PUBLISH_INTERVAL_MS    = 10000;  // MQTT telemetry cadence
static const uint32_t NODE_STALE_TIMEOUT_MS  = 20000;  // Board 1 declared offline after this

static const uint32_t WIFI_RETRY_MS          = 5000;
static const uint32_t MQTT_RETRY_MS          = 5000;
static const uint32_t WIFI_BOOT_TIMEOUT_MS   = 30000;

// MQTT. ThingsBoard uses the device access token as the username and an empty
// password. Port 1883 is plain MQTT, which is what the classroom setup uses.
static const uint16_t MQTT_PORT  = 1883;
static const char    *MQTT_TOPIC = "v1/devices/me/telemetry";

// Server-to-device RPC. ThingsBoard publishes a command to
// v1/devices/me/rpc/request/<id> and expects the reply on
// v1/devices/me/rpc/response/<id>. Subscribing with a wildcard on the id is
// what makes the link bidirectional rather than telemetry-only.
static const char    *RPC_REQUEST_TOPIC = "v1/devices/me/rpc/request/+";
static const char    *RPC_RESPONSE_BASE = "v1/devices/me/rpc/response/";

// Publish interval is a runtime value rather than a constant, because the
// setPublishInterval RPC changes it from the dashboard. Bounds exist so a
// mistyped command cannot stop telemetry altogether or flood the broker.
static const uint32_t PUBLISH_INTERVAL_MIN_MS = 2000;
static const uint32_t PUBLISH_INTERVAL_MAX_MS = 300000;

// Store-and-forward buffer.
//
// Without this, telemetry generated while the broker is unreachable is simply
// discarded: the report previously listed that as a limitation. The buffer is
// a fixed-size ring in RAM, so a long outage still loses the oldest samples,
// but a short one - a Wi-Fi reconnect, a broker hiccup - now costs nothing.
// Flash-backed storage would survive a reboot as well; RAM was chosen because
// the failure this actually addresses is transient loss of connectivity, not
// power loss.
static const size_t   BUFFER_SLOTS      = 12;   // 12 x 10 s = 2 minutes of outage
static const size_t   BUFFER_SLOT_BYTES = 288;

// Plausibility bounds for incoming sensor values. Deliberately wider than the
// DHT11's own rated range so a cold room or a dry day is not rejected as
// corrupt. Tighten these if you want stricter validation.
static const float TEMP_MIN_C   = -40.0f;
static const float TEMP_MAX_C   =  80.0f;
static const float HUM_MIN_PCT  =   0.0f;
static const float HUM_MAX_PCT  = 100.0f;

// ESP-NOW link encryption. MUST match ESPNOW_ENCRYPT in sketch 04.
// Verified working with this board associated to an access point and the node
// unassociated. Keys are in secrets.h and must match on both boards.
#define ESPNOW_ENCRYPT 1

// Guard against isDuplicate() being reduced to a stub again. While this is 0 the
// sketch prints a warning at boot, because a stub accepts every packet and
// duplicate_count would never move off zero.
#define DEDUP_IMPLEMENTED 1

// ===========================================================================
// Mesh packet - MUST stay byte-identical to the copy in sketch 04
// ===========================================================================

static const uint8_t PROTO_VERSION   = 1;
static const uint8_t MSG_SENSOR_DATA = 1;
static const uint8_t NODE_ID         = 1;  // Board 1
static const uint8_t ROOT_ID         = 0;  // this board

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

static_assert(sizeof(mesh_packet_t) == 26, "mesh_packet_t layout changed, update sketch 04 to match");

// What actually travels through the queue: the validated packet plus the MAC it
// arrived from, so the log line can name the sender.
typedef struct {
  mesh_packet_t pkt;
  uint8_t       src_mac[6];
} rx_item_t;

// ===========================================================================
// State
// ===========================================================================

DHT dht(DHT_PIN, DHT_KIND);

WiFiClient   netClient;
PubSubClient mqtt(netClient);

static char     mqttClientId[32] = {0};

// The channel the access point put us on. Board 1 must be pinned to this.
static uint8_t  espnowChannel  = 0;

// Handoff from the ESP-NOW receive callback to loop(). A FreeRTOS queue is used
// rather than a shared struct plus a flag because the callback runs in the Wi-Fi
// task while loop() runs in the Arduino task. The queue gives correct
// cross-task handoff without hand-rolled locking, and its small depth absorbs a
// burst without dropping anything.
static QueueHandle_t rxQueue = nullptr;

// Written by the ESP-NOW callback on the Wi-Fi task, read by loop() on the
// Arduino task.
//
// `volatile` alone is not enough here. It stops the compiler caching the value
// in a register, but it does not make read-modify-write atomic across two
// tasks. rxQueueFullCount is the case that actually bites: the callback does
// `count++` while loop() does read-print-reset, so an increment landing between
// the read and the reset is silently discarded and the reported queue-overflow
// figure comes out lower than reality. std::atomic makes the increment and the
// read-and-clear indivisible.
static std::atomic<uint32_t> rxInvalidCount{0};
static std::atomic<uint32_t> rxQueueFullCount{0};
static std::atomic<uint32_t> rxUnknownSenderCount{0};

// Telemetry counters.
static uint32_t espnowReceivedCount = 0;  // packets that passed validation
static uint32_t duplicateCount      = 0;  // of those, rejected as duplicates

// Last known good remote reading. Kept after Board 1 goes offline so dashboard
// widgets do not blank out; node1_online carries the truth about freshness.
static bool     node1HaveEver  = false;
static float    node1T         = 0.0f;
static float    node1H         = 0.0f;
static uint32_t node1Seq       = 0;
static uint32_t node1LastRxMs  = 0;

// Set once the stale timeout is crossed, cleared only by a genuine packet.
// See node1Liveness() for why the latch is needed rather than recomputing the
// age each time.
static bool     node1StaleLatched = false;

// Last known good local reading, plus whether the most recent attempt worked.
static bool  localHaveEver = false;
static bool  localOk       = false;
static float localT        = 0.0f;
static float localH        = 0.0f;

// Scheduling. publishIntervalMs is not const - the setPublishInterval RPC
// rewrites it at runtime.
static uint32_t publishIntervalMs = PUBLISH_INTERVAL_MS;
static uint32_t lastLocalReadMs = 0;
static uint32_t lastPublishMs   = 0;
static uint32_t lastWifiTryMs   = 0;
static uint32_t lastMqttTryMs   = 0;
static bool     wasWifiUp       = false;

// Store-and-forward ring. head is the next slot to write; count is how many
// slots hold unsent payloads. When the ring is full the oldest entry is
// overwritten, which is the right trade for telemetry: recent readings matter
// more than old ones, and a bounded buffer cannot exhaust memory.
static char     bufferSlots[BUFFER_SLOTS][BUFFER_SLOT_BYTES];
static size_t   bufferHead    = 0;
static size_t   bufferCount   = 0;
static uint32_t bufferedTotal = 0;  // lifetime count, published as telemetry
static uint32_t droppedTotal  = 0;  // overwritten before they could be sent

// RPC counters, published so the dashboard can show the link is bidirectional.
static uint32_t rpcHandledCount = 0;
static uint32_t rpcRejectedCount = 0;

// ---------------------------------------------------------------------------
// Duplicate detection state
// ---------------------------------------------------------------------------
//
// The dedup key is (src_id, boot_id, seq). boot_id is the important part: it is
// randomised every time Board 1 powers up, so when Board 1 restarts and its
// sequence numbers drop back to 1, the gateway can tell that apart from an old
// packet being replayed. With only one remote node there is a single slot; a
// larger mesh would make this an array indexed by src_id.
typedef struct {
  bool     seen;
  uint32_t boot_id;
  uint32_t last_seq;
} remote_state_t;

static remote_state_t node1State = { false, 0, 0 };

// ===========================================================================
// Duplicate detection
// ===========================================================================
//
// Strategy: strict last-sequence-wins. ESP-NOW unicast is acknowledged at the
// link layer and does not reorder in a two-radio link, so anything at or below
// the highest sequence number already accepted in this session is a
// retransmission rather than a late arrival. A sliding window would tolerate
// reordering, but nothing here reorders, and the extra state would be solving a
// problem this link does not have.
//
// The boot_id branch is the part that is not optional. Board 1 randomises
// boot_id on every power-up and restarts its sequence numbering at 1. Without
// that branch, restarting Board 1 would make every subsequent packet look like
// a replay of something already seen, and the node would never come back.
//
// Called only from processReceivedPackets() on the Arduino task, so node1State
// needs no locking.
static bool isDuplicate(const mesh_packet_t &pkt) {
  if (!node1State.seen || node1State.boot_id != pkt.boot_id) {
    // First packet ever, or Board 1 rebooted. Either way this is a new session
    // and its sequence numbering starts fresh, so accept and rebase.
    node1State.seen     = true;
    node1State.boot_id  = pkt.boot_id;
    node1State.last_seq = pkt.seq;
    return false;
  }

  // Wrap-aware comparison rather than a plain `pkt.seq <= last_seq`. Casting
  // the difference to a signed type means "newer" stays correct when the
  // counter rolls over past UINT32_MAX: a straight comparison would reject
  // sequence 0 and everything after it for the rest of that session. At one
  // packet every five seconds the rollover is roughly 681 years away, so this
  // is correctness for its own sake rather than a practical concern.
  if ((int32_t)(pkt.seq - node1State.last_seq) <= 0) {
    return true;  // already seen this sequence number in this session
  }

  // A jump forward is a gap, not a duplicate: those packets were lost, not
  // repeated. Accept and move the baseline up.
  node1State.last_seq = pkt.seq;
  return false;
}

// ===========================================================================
// ESP-NOW
// ===========================================================================

static void macToString(const uint8_t *mac, char *out, size_t outLen) {
  snprintf(out, outLen, "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// Full validation of a raw ESP-NOW frame. Cheap, allocation-free, and safe to
// run inside the receive callback.
static bool validatePacket(const uint8_t *data, int len, mesh_packet_t &out) {
  if (data == nullptr) {
    return false;
  }
  // Exact length, not "at least". A short frame would read past the buffer and a
  // long one means the sender is speaking a different protocol.
  if (len != (int)sizeof(mesh_packet_t)) {
    return false;
  }
  memcpy(&out, data, sizeof(out));

  if (out.version  != PROTO_VERSION)   return false;
  if (out.msg_type != MSG_SENSOR_DATA) return false;
  if (out.dst_id   != ROOT_ID)         return false;
  if (out.src_id   != NODE_ID)         return false;

  if (isnan(out.temperature_c) || isnan(out.humidity_pct)) return false;
  if (out.temperature_c < TEMP_MIN_C  || out.temperature_c > TEMP_MAX_C)  return false;
  if (out.humidity_pct  < HUM_MIN_PCT || out.humidity_pct  > HUM_MAX_PCT) return false;

  return true;
}

// ---------------------------------------------------------------------------
// ESP-NOW receive callback
//
// IMPORTANT: this signature is specific to the installed core. arduino-esp32
// 3.3.x is built on ESP-IDF 5.5, where the sender MAC arrives inside
// esp_now_recv_info_t rather than as a bare uint8_t pointer. Older tutorials use
// the old form and will not compile against this core.
//
// This runs in the Wi-Fi task. It validates, copies, and hands off. No MQTT, no
// sensor reads, no delays, no Serial output: blocking here starves the Wi-Fi
// stack, drops subsequent frames, and can trip the task watchdog.
// ---------------------------------------------------------------------------
static void onEspNowRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  rx_item_t item;

  // Check who sent it before looking at what they sent.
  //
  // src_id inside the packet is a claim by the sender, not proof of identity.
  // This gateway accepts frames from unregistered, unencrypted peers, so
  // without this check any ESP-NOW device in radio range could submit a
  // well-formed 26-byte packet and it would be accepted. The damaging case is
  // not a wrong temperature on the dashboard: an injected packet carrying a
  // very high sequence number under the live boot_id would advance the
  // deduplication baseline, after which every genuine packet from Board 1 is
  // rejected as a duplicate until it reboots.
  //
  // The source MAC comes from the radio driver rather than the payload. It is
  // still spoofable by a determined attacker, so this raises the effort
  // required rather than making the link secure - see the security section of
  // the report. Real authentication needs ESP-NOW encryption with a PMK/LMK.
  if (info == nullptr || info->src_addr == nullptr ||
      memcmp(info->src_addr, NODE_MAC, 6) != 0) {
    rxUnknownSenderCount++;
    return;
  }

  if (!validatePacket(data, len, item.pkt)) {
    rxInvalidCount++;
    return;
  }

  memcpy(item.src_mac, info->src_addr, 6);

  if (xQueueSend(rxQueue, &item, 0) != pdTRUE) {
    rxQueueFullCount++;
  }
}

// Drain everything the callback handed over. Deduplication, bookkeeping and
// printing all happen here, on the Arduino task, where blocking is harmless.
static void processReceivedPackets() {
  rx_item_t item;

  while (xQueueReceive(rxQueue, &item, 0) == pdTRUE) {
    espnowReceivedCount++;

    char macStr[18];
    macToString(item.src_mac, macStr, sizeof(macStr));

    if (isDuplicate(item.pkt)) {
      duplicateCount++;
      Serial.printf("[GATEWAY] DUPLICATE rejected  node=%u boot_id=%lu seq=%lu  duplicates=%lu\n",
                    item.pkt.src_id,
                    (unsigned long)item.pkt.boot_id,
                    (unsigned long)item.pkt.seq,
                    (unsigned long)duplicateCount);
      continue;
    }

    node1HaveEver     = true;
    node1T            = item.pkt.temperature_c;
    node1H            = item.pkt.humidity_pct;
    node1Seq          = item.pkt.seq;
    node1LastRxMs     = millis();
    node1StaleLatched = false;  // a real packet is the only thing that clears it

    Serial.printf("[GATEWAY] ESP-NOW rx from %s  node=%u seq=%lu boot_id=%lu "
                  "temperature=%.1f C humidity=%.1f %% node_uptime=%lu ms ttl=%u hops=%u\n",
                  macStr,
                  item.pkt.src_id,
                  (unsigned long)item.pkt.seq,
                  (unsigned long)item.pkt.boot_id,
                  item.pkt.temperature_c,
                  item.pkt.humidity_pct,
                  (unsigned long)item.pkt.uptime_ms,
                  item.pkt.ttl,
                  item.pkt.hop_count);
  }
}

static void setupEspNow() {
  rxQueue = xQueueCreate(8, sizeof(rx_item_t));
  if (rxQueue == nullptr) {
    Serial.println("[GATEWAY] xQueueCreate failed - halting");
    while (true) {
      delay(1000);
    }
  }

  esp_err_t err = esp_now_init();
  if (err != ESP_OK) {
    Serial.printf("[GATEWAY] esp_now_init failed: %s - halting\n", esp_err_to_name(err));
    while (true) {
      delay(1000);
    }
  }

  // Retry with backoff, then restart. Continuing without a registered callback
  // would print "ESP-NOW listening" while the gateway was permanently deaf,
  // which is the most confusing possible failure. But halting forever is just
  // as bad: registration can fail for recoverable reasons such as transient
  // memory pressure, and a board that needs a physical reset to recover is
  // inconsistent with Wi-Fi and MQTT both self-healing in loop(). A reboot
  // recovers autonomously and is cheap on a device with nothing to lose.
  const int MAX_ATTEMPTS = 5;
  for (int attempt = 1; ; attempt++) {
    err = esp_now_register_recv_cb(onEspNowRecv);
    if (err == ESP_OK) {
      break;
    }
    Serial.printf("[GATEWAY] esp_now_register_recv_cb failed (attempt %d/%d): %s\n",
                  attempt, MAX_ATTEMPTS, esp_err_to_name(err));
    if (attempt >= MAX_ATTEMPTS) {
      Serial.println("[GATEWAY] cannot receive without a callback - restarting");
      delay(200);  // let the message reach the UART before the reset
      ESP.restart();
    }
    delay(200 * attempt);  // linear backoff
  }

  // The gateway registers the node as an encrypted peer even though it only
  // ever receives. Encryption is symmetric: without a peer entry carrying the
  // LMK, the driver cannot decrypt incoming frames and they are discarded
  // before reaching the callback.
  //
  // This replaces an earlier design that registered no peers and accepted any
  // sender, filtering afterwards on source MAC. That filter was not a trust
  // boundary - MAC addresses are forgeable by anyone able to transmit, so an
  // attacker could still inject a frame carrying a high sequence number and
  // poison the deduplication baseline. Requiring a valid ciphertext closes it:
  // a frame that decrypts correctly must have come from something holding the
  // key. The MAC comparison in the callback is retained as defence in depth.
  char nodeMacStr[18];
  macToString(NODE_MAC, nodeMacStr, sizeof(nodeMacStr));

#if ESPNOW_ENCRYPT
  static const uint8_t pmk[16] = ESPNOW_PMK;
  err = esp_now_set_pmk(pmk);
  if (err != ESP_OK) {
    Serial.printf("[GATEWAY] esp_now_set_pmk failed: %s\n", esp_err_to_name(err));
  }

  static const uint8_t lmk[16] = ESPNOW_LMK;
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, NODE_MAC, 6);
  memcpy(peer.lmk, lmk, 16);
  peer.channel = espnowChannel;
  peer.ifidx   = WIFI_IF_STA;
  peer.encrypt = true;

  err = esp_now_add_peer(&peer);
  if (err != ESP_OK) {
    Serial.printf("[GATEWAY] esp_now_add_peer failed: %s\n", esp_err_to_name(err));
    Serial.println("[GATEWAY] without an encrypted peer entry nothing can be decrypted");
  }
  Serial.printf("[GATEWAY] encrypted peer registered: %s (PMK/LMK set)\n", nodeMacStr);
#else
  // No peer entry is required to receive unencrypted ESP-NOW. Sender identity
  // rests on the source MAC comparison in the receive callback, which is a
  // filter rather than authentication - see the report's security section.
  Serial.printf("[GATEWAY] encryption disabled; accepting frames only from %s\n", nodeMacStr);
#endif

  Serial.printf("[GATEWAY] ESP-NOW listening on channel %u, expecting %u byte packets\n",
                espnowChannel, (unsigned)sizeof(mesh_packet_t));
}

// ===========================================================================
// Wi-Fi
// ===========================================================================

static uint8_t currentChannel() {
  uint8_t channel = 0;
  wifi_second_chan_t secondary = WIFI_SECOND_CHAN_NONE;
  esp_wifi_get_channel(&channel, &secondary);
  return channel;
}

static void reportWifi() {
  uint8_t mac[6] = {0};
  esp_wifi_get_mac(WIFI_IF_STA, mac);
  char macStr[18];
  macToString(mac, macStr, sizeof(macStr));

  Serial.printf("[GATEWAY] station MAC = %s\n", macStr);
  Serial.printf("[GATEWAY] ip=%s  rssi=%d dBm\n",
                WiFi.localIP().toString().c_str(), WiFi.RSSI());
  Serial.println("[GATEWAY] --------------------------------------------------------");
  Serial.printf("[GATEWAY] ACTIVE 2.4 GHz CHANNEL = %u\n", espnowChannel);
  Serial.printf("[GATEWAY] Put this in sketch 04: ESPNOW_CHANNEL = %u\n", espnowChannel);
  Serial.println("[GATEWAY] --------------------------------------------------------");
}

static void setupWifi() {
  WiFi.mode(WIFI_STA);

  // Power save parks the radio between access point beacons. A parked radio does
  // not hear ESP-NOW frames, so this line is the difference between receiving
  // every packet and losing an unpredictable share of them.
  WiFi.setSleep(false);

#if WIFI_USE_ENTERPRISE
  // WPA2-Enterprise, PEAP with MSCHAPv2 inside. This is what university
  // networks such as eduroam use: there is no shared password, each user
  // authenticates with their own account against a RADIUS server.
  //
  // The identity argument is the OUTER identity, sent unencrypted during the
  // handshake. The username and password travel inside the TLS tunnel.
  //
  // The certificate arguments are left NULL, so the board does not verify the
  // RADIUS server's certificate. That is what makes this connect at all without
  // shipping a CA bundle, and it is also why this configuration would be
  // unacceptable on a production device: a rogue access point advertising the
  // same SSID could harvest the credentials.
  Serial.printf("[GATEWAY] connecting to \"%s\" (WPA2-Enterprise, PEAP) ...\n", WIFI_SSID);
  Serial.printf("[GATEWAY] outer identity = %s\n", WIFI_EAP_IDENTITY);
  WiFi.begin(WIFI_SSID, WPA2_AUTH_PEAP,
             WIFI_EAP_IDENTITY, WIFI_EAP_USERNAME, WIFI_EAP_PASSWORD);
#else
  Serial.printf("[GATEWAY] connecting to \"%s\" (WPA2-Personal) ...\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
#endif

  uint32_t startedMs = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startedMs < WIFI_BOOT_TIMEOUT_MS) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    espnowChannel = currentChannel();
    wasWifiUp = true;
    Serial.println("[GATEWAY] Wi-Fi connected");
    reportWifi();
    return;
  }

  // Carry on regardless. ESP-NOW does not need the access point, so the mesh
  // half of the demonstration can still be shown while Wi-Fi keeps retrying in
  // loop(). The channel is provisional until the association succeeds.
  espnowChannel = currentChannel();
  Serial.printf("[GATEWAY] Wi-Fi connect FAILED (status=%d). Retrying in the background.\n",
                (int)WiFi.status());
#if WIFI_USE_ENTERPRISE
  Serial.println("[GATEWAY] Enterprise auth checks: identity, username and password in");
  Serial.println("[GATEWAY] secrets.h; whether the account needs a domain suffix such as");
  Serial.println("[GATEWAY] user@institution.ac.nz; and whether the network insists on");
  Serial.println("[GATEWAY] server certificate validation, which this sketch does not do.");
#else
  Serial.println("[GATEWAY] Check: 2.4 GHz network, SSID spelling, password in secrets.h.");
#endif
  Serial.printf("[GATEWAY] Provisional channel is %u and may change once connected.\n",
                espnowChannel);
}

static void ensureWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!wasWifiUp) {
      wasWifiUp = true;
      uint8_t nowChannel = currentChannel();
      Serial.println("[GATEWAY] Wi-Fi reconnected");

      // A reconnect can land on a different channel, for example after the
      // router does an automatic channel selection. Wi-Fi and MQTT then look
      // perfectly healthy while ESP-NOW silently stops working, so it is worth
      // saying so out loud.
      if (nowChannel != espnowChannel) {
        Serial.printf("[GATEWAY] WARNING channel changed %u -> %u\n",
                      espnowChannel, nowChannel);
        Serial.printf("[GATEWAY] ESP-NOW is now deaf to Board 1. Set ESPNOW_CHANNEL = %u\n",
                      nowChannel);
        Serial.println("[GATEWAY] in sketch 04 and re-upload it to Board 1.");
        espnowChannel = nowChannel;
      }
      reportWifi();
    }
    return;
  }

  if (wasWifiUp) {
    wasWifiUp = false;
    Serial.println("[GATEWAY] Wi-Fi connection lost");
  }

  if (millis() - lastWifiTryMs < WIFI_RETRY_MS) {
    return;
  }
  lastWifiTryMs = millis();

  // Non-blocking retry. A blocking wait here would stall packet processing and
  // the local sensor for as long as the network stayed down.
  Serial.println("[GATEWAY] Wi-Fi down, attempting reconnect");
  WiFi.reconnect();
}

// ===========================================================================
// Store-and-forward buffer
// ===========================================================================

// Queue a payload that could not be sent. Oldest is overwritten when full.
static void bufferPayload(const char *payload) {
  size_t len = strlen(payload);
  if (len >= BUFFER_SLOT_BYTES) {
    return;  // cannot store it; buildPayload already bounds this in practice
  }
  if (bufferCount == BUFFER_SLOTS) {
    // Ring is full: the slot about to be written still holds an unsent
    // payload, so record it as dropped rather than losing it silently.
    droppedTotal++;
    bufferCount--;
  }
  memcpy(bufferSlots[bufferHead], payload, len + 1);
  bufferHead = (bufferHead + 1) % BUFFER_SLOTS;
  bufferCount++;
  bufferedTotal++;
}

// Send everything queued, oldest first. Called once the broker is reachable.
// Stops at the first failure so ordering is preserved and nothing is lost.
static void flushBuffer() {
  if (bufferCount == 0 || !mqtt.connected()) {
    return;
  }
  Serial.printf("[GATEWAY] flushing %u buffered payload(s)\n", (unsigned)bufferCount);
  while (bufferCount > 0) {
    size_t tail = (bufferHead + BUFFER_SLOTS - bufferCount) % BUFFER_SLOTS;
    if (!mqtt.publish(MQTT_TOPIC, bufferSlots[tail])) {
      Serial.println("[GATEWAY] flush interrupted, remainder stays queued");
      return;
    }
    bufferCount--;
  }
  Serial.println("[GATEWAY] buffer empty");
}

// ===========================================================================
// MQTT
// ===========================================================================

// Server-to-device RPC.
//
// ThingsBoard sends {"method":"<name>","params":<value>} to
// v1/devices/me/rpc/request/<id>, and a reply on the matching response topic
// is echoed back to whatever triggered it. This makes the device controllable
// from the dashboard rather than only observable, which is the difference
// between telemetry and management.
//
// Runs on the PubSubClient callback, which PubSubClient invokes from
// mqtt.loop() on the Arduino task - so ordinary code is safe here, unlike the
// ESP-NOW receive callback.
static void onMqttMessage(char *topic, byte *payload, unsigned int length) {
  char body[192];
  size_t n = length < sizeof(body) - 1 ? length : sizeof(body) - 1;
  memcpy(body, payload, n);
  body[n] = '\0';

  // The request id is the last path segment; the reply must carry it back.
  const char *idStr = strrchr(topic, '/');
  idStr = idStr ? idStr + 1 : "0";

  char respTopic[64];
  snprintf(respTopic, sizeof(respTopic), "%s%s", RPC_RESPONSE_BASE, idStr);

  Serial.printf("[GATEWAY] RPC request id=%s: %s\n", idStr, body);

  char reply[160];

  if (strstr(body, "\"getStatus\"") != nullptr) {
    uint32_t ageMs = 0;
    bool online = false;
    node1Liveness(ageMs, online);
    snprintf(reply, sizeof(reply),
             "{\"node1_online\":%s,\"received\":%lu,\"duplicates\":%lu,"
             "\"buffered\":%u,\"publish_interval_ms\":%lu,\"channel\":%u}",
             online ? "true" : "false",
             (unsigned long)espnowReceivedCount,
             (unsigned long)duplicateCount,
             (unsigned)bufferCount,
             (unsigned long)publishIntervalMs,
             espnowChannel);
    rpcHandledCount++;

  } else if (strstr(body, "\"setPublishInterval\"") != nullptr) {
    // params is a bare number here, e.g. {"method":"setPublishInterval",
    // "params":5000}. Advance past the key and its colon to the first digit or
    // sign, then parse. A full JSON parser would be overkill for three fixed
    // commands, but the range check below is what actually makes this safe -
    // it is applied to whatever was parsed, including 0 when nothing was.
    long requested = 0;
    const char *p = strstr(body, "\"params\"");
    if (p != nullptr) {
      const char *q = p + strlen("\"params\"");
      while (*q != '\0' && *q != '-' && (*q < '0' || *q > '9')) {
        q++;
      }
      requested = strtol(q, nullptr, 10);
    }

    if (requested >= (long)PUBLISH_INTERVAL_MIN_MS &&
        requested <= (long)PUBLISH_INTERVAL_MAX_MS) {
      publishIntervalMs = (uint32_t)requested;
      snprintf(reply, sizeof(reply),
               "{\"ok\":true,\"publish_interval_ms\":%lu}",
               (unsigned long)publishIntervalMs);
      Serial.printf("[GATEWAY] publish interval now %lu ms\n",
                    (unsigned long)publishIntervalMs);
      rpcHandledCount++;
    } else {
      // Reject out-of-range rather than clamping: silently accepting a value
      // the caller did not ask for is worse than telling them it was refused.
      snprintf(reply, sizeof(reply),
               "{\"ok\":false,\"error\":\"out of range\",\"min\":%lu,\"max\":%lu}",
               (unsigned long)PUBLISH_INTERVAL_MIN_MS,
               (unsigned long)PUBLISH_INTERVAL_MAX_MS);
      Serial.printf("[GATEWAY] RPC rejected: %ld ms out of range\n", requested);
      rpcRejectedCount++;
    }

  } else if (strstr(body, "\"resetCounters\"") != nullptr) {
    espnowReceivedCount = 0;
    duplicateCount      = 0;
    rxInvalidCount      = 0;
    rxUnknownSenderCount = 0;
    droppedTotal        = 0;
    snprintf(reply, sizeof(reply), "{\"ok\":true,\"reset\":true}");
    Serial.println("[GATEWAY] counters reset by RPC");
    rpcHandledCount++;

  } else {
    snprintf(reply, sizeof(reply),
             "{\"ok\":false,\"error\":\"unknown method\"}");
    rpcRejectedCount++;
  }

  mqtt.publish(respTopic, reply);
}

static void buildMqttClientId() {
  uint8_t mac[6] = {0};
  esp_wifi_get_mac(WIFI_IF_STA, mac);
  snprintf(mqttClientId, sizeof(mqttClientId), "p1gw-%02X%02X%02X%02X%02X%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void setupMqtt() {
  buildMqttClientId();
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setKeepAlive(30);

  // PubSubClient's default buffer is 256 bytes and it silently refuses to send
  // anything larger: publish() just returns false and nothing reaches the
  // broker. The combined payload is around 300 bytes, so this is required, not
  // defensive.
  if (!mqtt.setBufferSize(512)) {
    Serial.println("[GATEWAY] WARNING setBufferSize(512) failed, telemetry may be dropped");
  }

  mqtt.setCallback(onMqttMessage);

  Serial.printf("[GATEWAY] MQTT target %s:%u topic %s client_id %s\n",
                MQTT_HOST, MQTT_PORT, MQTT_TOPIC, mqttClientId);
}

static void ensureMqtt() {
  if (WiFi.status() != WL_CONNECTED) {
    return;  // nothing to do until the network is back
  }
  if (mqtt.connected()) {
    return;
  }
  if (millis() - lastMqttTryMs < MQTT_RETRY_MS) {
    return;
  }
  lastMqttTryMs = millis();

  Serial.printf("[GATEWAY] MQTT connecting to %s:%u ...\n", MQTT_HOST, MQTT_PORT);

  // ThingsBoard authentication: device access token as the username, empty
  // password. There is no separate device identity beyond the token.
  if (mqtt.connect(mqttClientId, THINGSBOARD_ACCESS_TOKEN, "")) {
    Serial.println("[GATEWAY] MQTT connected");
    // Subscribe on every connect, not once at startup: the broker discards
    // subscriptions when the session ends, so a reconnect without this leaves
    // the device publishing normally while silently ignoring every command.
    if (mqtt.subscribe(RPC_REQUEST_TOPIC)) {
      Serial.printf("[GATEWAY] subscribed to %s\n", RPC_REQUEST_TOPIC);
    } else {
      Serial.println("[GATEWAY] WARNING RPC subscribe failed - commands will be ignored");
    }
    flushBuffer();
    return;
  }

  Serial.printf("[GATEWAY] MQTT connect FAILED, state=%d\n", mqtt.state());
  Serial.println("[GATEWAY] state -2 = TCP refused (host or port wrong, or firewall)");
  Serial.println("[GATEWAY] state  4 = bad credentials (check the access token)");
  Serial.println("[GATEWAY] state  5 = not authorised (token not accepted by the device)");
}

// ===========================================================================
// Local sensor
// ===========================================================================

static void readLocalSensor() {
  if (millis() - lastLocalReadMs < LOCAL_READ_INTERVAL_MS) {
    return;
  }
  lastLocalReadMs = millis();

  float humidity    = dht.readHumidity();
  float temperature = dht.readTemperature();

  // isfinite rather than isnan, and the same range check applied to remote
  // readings. An infinity would slip past an isnan test and then render as
  // "inf" in the payload, which is not valid JSON and would cost the whole
  // message. Holding local data to the same standard as data arriving over the
  // radio also removes a silent asymmetry in what the two paths trust.
  if (!isfinite(humidity) || !isfinite(temperature) ||
      temperature < TEMP_MIN_C  || temperature > TEMP_MAX_C ||
      humidity    < HUM_MIN_PCT || humidity    > HUM_MAX_PCT) {
    // Keep the previous good values but flag the sensor as unhealthy. The
    // gateway_sensor_ok key is what tells the dashboard the truth.
    localOk = false;
    return;
  }

  localOk       = true;
  localHaveEver = true;
  localT        = temperature;
  localH        = humidity;
}

// ===========================================================================
// Telemetry
// ===========================================================================

// Append to buf while tracking the running length, refusing to produce a
// truncated payload. A half-written JSON object published to ThingsBoard is
// worse than no publish at all, because it looks like it worked.
static bool appendf(char *buf, size_t bufLen, int &n, const char *fmt, ...) {
  if (n < 0 || (size_t)n >= bufLen) {
    return false;
  }
  va_list ap;
  va_start(ap, fmt);
  int written = vsnprintf(buf + n, bufLen - n, fmt, ap);
  va_end(ap);

  if (written < 0 || (size_t)written >= bufLen - (size_t)n) {
    return false;  // would have been truncated
  }
  n += written;
  return true;
}

// Age since the last valid packet, and whether the node counts as online.
//
// The latch matters. millis() wraps every ~49.7 days, so a node that has been
// absent for that long would see its computed age wrap back through zero and
// briefly report itself online again despite nothing having arrived. Once the
// stale threshold is crossed the state is latched and only a genuine reception
// clears it, which makes the flag monotonic in the way a reader expects.
static void node1Liveness(uint32_t &ageMs, bool &online) {
  if (!node1HaveEver) {
    ageMs  = 0;
    online = false;
    return;
  }
  ageMs = millis() - node1LastRxMs;
  if (ageMs >= NODE_STALE_TIMEOUT_MS) {
    node1StaleLatched = true;
  }
  online = !node1StaleLatched;
  if (node1StaleLatched && ageMs < NODE_STALE_TIMEOUT_MS) {
    // Age wrapped while still stale. Report the threshold rather than a
    // misleadingly small number.
    ageMs = NODE_STALE_TIMEOUT_MS;
  }
}

static int buildPayload(char *buf, size_t bufLen) {
  uint32_t ageMs = 0;
  bool     online = false;
  node1Liveness(ageMs, online);

  int  n  = 0;
  bool ok = true;

  ok = ok && appendf(buf, bufLen, n, "{");

  // Never let a NaN reach the payload. snprintf renders NaN as "nan", which is
  // not valid JSON, and ThingsBoard discards the entire message rather than the
  // one bad field. A value key is omitted until there is a real number for it;
  // the matching _ok / _online flag always ships so the dashboard can explain
  // the gap.
  if (localHaveEver) {
    ok = ok && appendf(buf, bufLen, n,
                       "\"gateway_temperature\":%.1f,\"gateway_humidity\":%.1f,",
                       localT, localH);
  }
  ok = ok && appendf(buf, bufLen, n, "\"gateway_sensor_ok\":%s,",
                     localOk ? "true" : "false");

  if (node1HaveEver) {
    ok = ok && appendf(buf, bufLen, n,
                       "\"node1_temperature\":%.1f,\"node1_humidity\":%.1f,"
                       "\"node1_sequence\":%lu,\"node1_age_ms\":%lu,",
                       node1T, node1H,
                       (unsigned long)node1Seq, (unsigned long)ageMs);
  }
  ok = ok && appendf(buf, bufLen, n, "\"node1_online\":%s,",
                     online ? "true" : "false");

  ok = ok && appendf(buf, bufLen, n,
                     "\"duplicate_count\":%lu,\"espnow_received_count\":%lu,",
                     (unsigned long)duplicateCount,
                     (unsigned long)espnowReceivedCount);

  // Operational metrics travel through the same pipeline as the data they
  // describe, so the dashboard can distinguish "the sensor reads 21 C" from
  // "nothing has arrived for 40 seconds and four frames were rejected".
  ok = ok && appendf(buf, bufLen, n,
                     "\"buffered_now\":%u,\"buffered_total\":%lu,\"dropped_total\":%lu,"
                     "\"rpc_handled\":%lu,\"publish_interval_ms\":%lu}",
                     (unsigned)bufferCount,
                     (unsigned long)bufferedTotal,
                     (unsigned long)droppedTotal,
                     (unsigned long)rpcHandledCount,
                     (unsigned long)publishIntervalMs);

  return ok ? n : -1;
}

static void publishTelemetry() {
  if (millis() - lastPublishMs < publishIntervalMs) {
    return;
  }
  lastPublishMs = millis();

  char payload[512];
  int  len = buildPayload(payload, sizeof(payload));
  if (len < 0) {
    Serial.println("[GATEWAY] payload build failed (would have been truncated), skipping publish");
    return;
  }

  uint32_t ageMs = 0;
  bool     online = false;
  node1Liveness(ageMs, online);

  Serial.printf("[GATEWAY] local_ok=%s  node1_online=%s  node1_age=%lu ms  "
                "received=%lu  duplicates=%lu  invalid=%lu  wrong_sender=%lu  buffered=%u\n",
                localOk ? "yes" : "no",
                online ? "yes" : "no",
                (unsigned long)ageMs,
                (unsigned long)espnowReceivedCount,
                (unsigned long)duplicateCount,
                (unsigned long)rxInvalidCount.load(),
                (unsigned long)rxUnknownSenderCount.load(),
                (unsigned)bufferCount);

  // Queue rather than discard when the broker is unreachable. This is the
  // difference between an outage costing nothing and an outage costing every
  // reading taken during it.
  if (!mqtt.connected()) {
    bufferPayload(payload);
    Serial.printf("[GATEWAY] MQTT offline, payload buffered (%u queued): %s\n",
                  (unsigned)bufferCount, payload);
    return;
  }

  // Drain anything queued before sending the current reading, so the cloud
  // receives them in the order they were taken.
  flushBuffer();

  if (mqtt.publish(MQTT_TOPIC, payload)) {
    Serial.printf("[GATEWAY] MQTT published %d bytes: %s\n", len, payload);
  } else {
    bufferPayload(payload);
    Serial.printf("[GATEWAY] MQTT publish FAILED (state=%d), buffered: %s\n",
                  mqtt.state(), payload);
  }
}

// ===========================================================================
// Arduino entry points
// ===========================================================================

void setup() {
  Serial.begin(115200);
  delay(300);  // let the USB serial port settle so the banner is not lost
  Serial.println();
  Serial.println("[GATEWAY] ============================================");
  Serial.println("[GATEWAY] 05_board2_gateway_cloud");
  Serial.println("[GATEWAY] ============================================");
  Serial.printf("[GATEWAY] packet size = %u bytes (ESP-NOW limit is 250)\n",
                (unsigned)sizeof(mesh_packet_t));

#if !DEDUP_IMPLEMENTED
  Serial.println("[GATEWAY] --------------------------------------------------------");
  Serial.println("[GATEWAY] WARNING isDuplicate() is still the stub. Every packet is");
  Serial.println("[GATEWAY] accepted, so duplicate_count will stay at 0 and the");
  Serial.println("[GATEWAY] duplicate-rejection acceptance check cannot pass. Write");
  Serial.println("[GATEWAY] the function, then set DEDUP_IMPLEMENTED to 1.");
  Serial.println("[GATEWAY] --------------------------------------------------------");
#endif

  dht.begin();

  // Wi-Fi first: the access point decides the channel that ESP-NOW must use.
  setupWifi();
  setupEspNow();
  setupMqtt();

  Serial.printf("[GATEWAY] publishing every %lu ms, node goes stale after %lu ms\n",
                (unsigned long)PUBLISH_INTERVAL_MS,
                (unsigned long)NODE_STALE_TIMEOUT_MS);
}

void loop() {
  ensureWifi();
  ensureMqtt();
  mqtt.loop();

  processReceivedPackets();
  readLocalSensor();
  publishTelemetry();

  // exchange() reads and clears in one indivisible step. Reading, printing and
  // then assigning zero would let an increment from the Wi-Fi task land in the
  // gap and be silently discarded, under-reporting the very overflow this
  // warning exists to surface.
  uint32_t overflowed = rxQueueFullCount.exchange(0);
  if (overflowed > 0) {
    Serial.printf("[GATEWAY] WARNING receive queue overflowed %lu time(s)\n",
                  (unsigned long)overflowed);
  }
}
