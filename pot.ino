#include "pot.h"

#include <OneWire.h>
#include <LiquidCrystal.h>

const int kSsr = 11;
const int kPot = A1;

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

void setup(void) {
  Serial.begin(9600);
  pinMode(kSsr, OUTPUT);

  lcd.begin(16, 2);
}

void printLcd(int row, float celsius) {
  lcd.setCursor(0, row);
  lcd.print(celsius);
}

OneWire  ds(13);

void loop(void) {
  byte i;
  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];
  float celsius, fahrenheit;

  if ( !ds.search(addr)) {
    Serial.println("No more addresses.");
    Serial.println();
    ds.reset_search();
    delay(250);
    return;
  }

  if (OneWire::crc8(addr, 7) != addr[7]) {
      Serial.println("CRC is not valid!");
      return;
  }
  Serial.println();

  // the first ROM byte indicates which chip
  switch (addr[0]) {
    case 0x10:
      type_s = 1;
      break;
    case 0x28:
      type_s = 0;
      break;
    case 0x22:
      type_s = 0;
      break;
    default:
      Serial.println("Device is not a DS18x20 family device.");
      return;
  }

  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);        // start conversion, with parasite power on at the end

  delay(1000);     // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.

  present = ds.reset();
  ds.select(addr);
  ds.write(0xBE);         // Read Scratchpad

  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
  }

  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }

  celsius = (float)raw / 16.0;
  Serial.print("  Temperature = ");
  Serial.print(celsius);
  Serial.println(" Celsius");
  printLcd(0, celsius);

  // 0 <= voltage <= 676 (= 1024 * 3.3 / 5.0)
  int analog = analogRead(kPot);

  // 0 <= roundedAnalog <= 600
  int roundedAnalog = max(0, min(600, analog - 38));

  // 50 <= celsius <= 80
  float target = (float)roundedAnalog / 20.0 + 50.0;

  Serial.print("  Target = ");
  Serial.println(target);
  Serial.println(" Celsius");
  printLcd(1, target);

  process(raw, target);
}

float celsius(int16_t raw) {
  return (float)raw / 16.0;
}

const int kTemperatureSize = 100;
Temperature temperatures[kTemperatureSize];
int temperatures_i = 0;
int total_temperatures = 0;

int next(int i) { return (i + 1) % kTemperatureSize; }

void add_temperature(int16_t raw) {
  Temperature* curr = &temperatures[temperatures_i];
  curr->Raw = raw;
  curr->Millis = millis();
  temperatures_i = next(temperatures_i);
  total_temperatures++;

}

Temperature* get_temperature(int i) {
  return &temperatures[(temperatures_i - 1 - i + kTemperatureSize) % kTemperatureSize];
}

void process(int16_t raw, float target) {
  add_temperature(raw);

  Temperature* t = get_temperature(0);
  if (celsius(t->Raw) > target) {
    digitalWrite(kSsr, LOW);
    Serial.println("LOW");
  } else {
    digitalWrite(kSsr, HIGH);
    Serial.println("HIGH");
  }
}
