/*
 * 02_board2_dht_test - Board 2 (Root Gateway + Local Sensor) DHT11 bring-up test
 *
 * Stage 2 of the P1 project. Identical in purpose to sketch 01, but for Board 2
 * and its own local DHT11 on a different pin. Board 2 has a sensor of its own
 * because it publishes a combined payload: its local reading plus the reading it
 * receives wirelessly from Board 1.
 *
 * Board:  ESP32 Dev Module      (FQBN esp32:esp32:esp32)
 * Serial: 115200
 *
 * Board 2 wiring:
 *   DHT11 "+" / VCC       -> ESP32 3V3            (green wire)
 *   DHT11 "-" / GND       -> ESP32 GND            (orange wire)
 *   DHT11 "S" / OUT/DATA  -> ESP32 D5 = GPIO5     (yellow wire)
 *
 * The three-pin DHT11 module carries its own pull-up resistor on the data line,
 * so no external resistor is required. Run the sensor at 3.3V, not VIN.
 */

#include "DHT.h"

// GPIO5. The board silkscreen says D5; the Arduino API wants the integer 5.
static const uint8_t DHT_PIN  = 5;
static const uint8_t DHT_KIND = DHT11;

// A DHT11 needs at least one second between reads. Two seconds is comfortable.
static const uint32_t READ_INTERVAL_MS = 2000;

DHT dht(DHT_PIN, DHT_KIND);

static uint32_t lastReadMs = 0;
static uint32_t readCount  = 0;
static uint32_t failCount  = 0;

void setup() {
  Serial.begin(115200);
  delay(300);  // let the USB serial port settle so the banner is not lost
  Serial.println();
  Serial.println("[GATEWAY] ============================================");
  Serial.println("[GATEWAY] 02_board2_dht_test");
  Serial.printf("[GATEWAY] DHT11 on GPIO%u, reading every %lu ms\n",
                DHT_PIN, (unsigned long)READ_INTERVAL_MS);
  Serial.println("[GATEWAY] ============================================");

  dht.begin();

  Serial.println("[GATEWAY] Note: the first read or two often return NaN on a DHT11.");
  Serial.println("[GATEWAY] That is normal. Persistent NaN means a wiring problem.");
}

void loop() {
  if (millis() - lastReadMs < READ_INTERVAL_MS) {
    return;
  }
  lastReadMs = millis();
  readCount++;

  float humidity    = dht.readHumidity();
  float temperature = dht.readTemperature();  // degrees Celsius

  if (isnan(humidity) || isnan(temperature)) {
    failCount++;
    Serial.printf("[GATEWAY] read %lu FAILED (NaN)  fails=%lu\n",
                  (unsigned long)readCount, (unsigned long)failCount);
    Serial.printf("[GATEWAY]   check: yellow data wire on GPIO%u, green on 3V3, orange on GND\n",
                  DHT_PIN);
    return;
  }

  Serial.printf("[GATEWAY] read %lu  temperature=%.1f C  humidity=%.1f %%  (fails=%lu)\n",
                (unsigned long)readCount, temperature, humidity,
                (unsigned long)failCount);
}
