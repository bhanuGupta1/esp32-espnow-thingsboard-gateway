/*
 * 01_board1_dht_test - Board 1 (Remote Sensor Node) DHT11 bring-up test
 *
 * Stage 1 of the P1 project. Proves the DHT11 on Board 1 is wired correctly and
 * returns plausible values before any wireless code is introduced. If this
 * sketch does not produce readings, nothing further will work.
 *
 * Board:  ESP32 Dev Module      (FQBN esp32:esp32:esp32)
 * Serial: 115200
 *
 * Board 1 wiring:
 *   DHT11 "+" / VCC       -> ESP32 3V3            (green wire)
 *   DHT11 "-" / GND       -> ESP32 GND            (orange wire)
 *   DHT11 "S" / OUT/DATA  -> ESP32 D4 = GPIO4     (yellow wire)
 *
 * The three-pin DHT11 module carries its own pull-up resistor on the data line,
 * so no external resistor is required. Run the sensor at 3.3V, not VIN.
 */

#include "DHT.h"

// GPIO4. The board silkscreen says D4; the Arduino API wants the integer 4.
static const uint8_t DHT_PIN  = 4;
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
  Serial.println("[NODE] ============================================");
  Serial.println("[NODE] 01_board1_dht_test");
  Serial.printf("[NODE] DHT11 on GPIO%u, reading every %lu ms\n",
                DHT_PIN, (unsigned long)READ_INTERVAL_MS);
  Serial.println("[NODE] ============================================");

  dht.begin();

  Serial.println("[NODE] Note: the first read or two often return NaN on a DHT11.");
  Serial.println("[NODE] That is normal. Persistent NaN means a wiring problem.");
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
    Serial.printf("[NODE] read %lu FAILED (NaN)  fails=%lu\n",
                  (unsigned long)readCount, (unsigned long)failCount);
    Serial.printf("[NODE]   check: yellow data wire on GPIO%u, green on 3V3, orange on GND\n",
                  DHT_PIN);
    return;
  }

  Serial.printf("[NODE] read %lu  temperature=%.1f C  humidity=%.1f %%  (fails=%lu)\n",
                (unsigned long)readCount, temperature, humidity,
                (unsigned long)failCount);
}
