#include <IRremote.hpp>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ================= PIN CONFIGURATION =================

#define IR_RECEIVER 5

#define MQ2_ANALOG A0
#define MQ2_DIGITAL 7

#define BUZZER 4
#define RED_LED 3

#define DHTPIN 12
#define DHTTYPE DHT11

#define RELAY1 8
#define RELAY2 9
#define RELAY3 10
#define RELAY4 11

// ================= OBJECTS =================

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ================= IR REMOTE CODES =================

#define BTN1 0x16
#define BTN2 0x19
#define BTN3 0x0D
#define BTN4 0x0C
#define BTN5 0x18
#define BTN6 0x5E
#define BTN_STAR 0x42
#define BTN_HASH 0x4A

// ================= RELAY SETTINGS =================

// Active LOW relay module
const byte relayPins[4] = {
RELAY1, RELAY2, RELAY3, RELAY4
};

#define RELAY_ON LOW
#define RELAY_OFF HIGH

// Saved manual settings
bool manualFanState[4] = {
false, false, false, false
};

// Actual relay output states
bool fanState[4] = {
false, false, false, false
};

// ================= SYSTEM VARIABLES =================

bool automaticMode = true;
bool smokeDetected = false;

float temperature = 0;
float humidity = 0;

unsigned long lastSmokeRead = 0;
unsigned long lastDHTRead = 0;
unsigned long lastLCDUpdate = 0;

const unsigned long SMOKE_INTERVAL = 200;
const unsigned long DHT_INTERVAL = 2000;
const unsigned long LCD_INTERVAL = 500;

// ================= BUZZER VARIABLES =================

// Software-generated 2kHz passive buzzer
// No tone() or noTone() hardware timer used.

unsigned long lastBuzzerToggle = 0;
const unsigned long BUZZER_HALF_PERIOD = 250;

bool buzzerOutputState = false;

// ================= APPLY RELAY OUTPUTS =================

void applyFanOutputs() {

bool desiredState[4];

// SMOKE SAFETY OVERRIDE
if (smokeDetected) {

desiredState[0] = true;
desiredState[1] = true;

desiredState[2] = false;
desiredState[3] = false;

}

// AUTOMATIC MODE
else if (automaticMode) {

for (byte i = 0; i < 4; i++) {
desiredState[i] = false;
}

}

// MANUAL MODE
else {

for (byte i = 0; i < 4; i++) {
desiredState[i] = manualFanState[i];
}

}

// APPLY OUTPUTS
for (byte i = 0; i < 4; i++) {

fanState[i] = desiredState[i];

digitalWrite(
relayPins[i],
fanState[i] ? RELAY_ON : RELAY_OFF
);

}
}

// ================= MANUAL FAN CONTROL =================

void setFan(byte fan, bool state) {

if (fan >= 4) return;

// Save manual setting even during smoke
manualFanState[fan] = state;

applyFanOutputs();

Serial.print("Manual Fan ");
Serial.print(fan + 1);
Serial.print(": ");
Serial.println(state ? "ON" : "OFF");
}

void allFans(bool state) {

// Save all manual settings
for (byte i = 0; i < 4; i++) {
manualFanState[i] = state;
}

applyFanOutputs();

Serial.println(state ? "ALL MANUAL FANS ON" :
"ALL MANUAL FANS OFF");
}

// ================= MODE CONTROL =================

void manualMode() {

automaticMode = false;

// Restore saved manual settings if no smoke
applyFanOutputs();

Serial.println("MANUAL MODE");
}

void autoMode() {

automaticMode = true;

// Smoke override still has priority
applyFanOutputs();

Serial.println("AUTOMATIC MODE");
}

// ================= BUZZER CONTROL =================

void controlBuzzer() {

if (!smokeDetected) {

buzzerOutputState = false;
digitalWrite(BUZZER, LOW);

return;
}

unsigned long currentMicros = micros();

if (currentMicros - lastBuzzerToggle >=
BUZZER_HALF_PERIOD) {

lastBuzzerToggle = currentMicros;

buzzerOutputState = !buzzerOutputState;

digitalWrite(
BUZZER,
buzzerOutputState ? HIGH : LOW
);
}
}

// ================= SMOKE SENSOR =================

void readSmokeSensor() {

int smokeValue = digitalRead(MQ2_DIGITAL);

// LOW means smoke detected
bool newSmokeStatus = (smokeValue == LOW);

if (newSmokeStatus != smokeDetected) {
smokeDetected = newSmokeStatus;

    digitalWrite(
      RED_LED,
      smokeDetected ? HIGH : LOW
    );

    Serial.println(
      smokeDetected ? "SMOKE DETECTED!" :
                      "NO SMOKE DETECTED"
    );

    // Update relays immediately
    applyFanOutputs();

  }

}

// ================= DHT11 SENSOR =================

void readDHTSensor() {

  float newHumidity = dht.readHumidity();
  float newTemperature = dht.readTemperature();

  if (!isnan(newHumidity) && !isnan(newTemperature)) {

    humidity = newHumidity;
    temperature = newTemperature;

  }

}

// ================= LCD DISPLAY =================

void updateLCD() {

  lcd.clear();

  // FIRST LINE: MODE AND SMOKE STATUS
  lcd.setCursor(0, 0);

  if (automaticMode) {
    lcd.print("AUTO");
  } else {
    lcd.print("MANUAL");
  }

  lcd.setCursor(7, 0);

  if (smokeDetected) {
    lcd.print("SMOKE!");
  } else {
    lcd.print("SAFE");
  }

  // SECOND LINE: TEMPERATURE AND HUMIDITY
  lcd.setCursor(0, 1);

  lcd.print("T:");
  lcd.print(temperature, 1);
  lcd.print("C ");

  lcd.print("H:");
  lcd.print(humidity, 0);
  lcd.print("%");
}

// ================= IR REMOTE CONTROL =================

void processRemote() {

  if (IrReceiver.decode()) {

    byte command = IrReceiver.decodedIRData.command;

    // Ignore held-button repeat signals
    bool isRepeat =
      IrReceiver.decodedIRData.flags &
      IRDATA_FLAGS_IS_REPEAT;

    IrReceiver.resume();

    if (!isRepeat) {

      Serial.print("IR Command: 0x");
      Serial.println(command, HEX);

      // MODE BUTTONS WORK EVEN DURING SMOKE
      if (command == BTN_HASH) {

        manualMode();

      }

      else if (command == BTN_STAR) {

        autoMode();

      }

      // MANUAL BUTTONS
      // Also save settings while smoke is detected.
      else if (!automaticMode) {

        if (command == BTN1) {
          setFan(0, !manualFanState[0]);
        }

        else if (command == BTN2) {
          setFan(1, !manualFanState[1]);
        }

        else if (command == BTN3) {
          setFan(2, !manualFanState[2]);
        }

        else if (command == BTN4) {
          setFan(3, !manualFanState[3]);
        }

        else if (command == BTN5) {
          allFans(true);
        }

        else if (command == BTN6) {
          allFans(false);
        }

      }

    }

  }

}

// ================= SETUP =================

void setup() {

  Serial.begin(9600);

  // LED
  pinMode(RED_LED, OUTPUT);
  digitalWrite(RED_LED, LOW);

  // BUZZER
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);

  // MQ2
  pinMode(MQ2_DIGITAL, INPUT);

  // RELAYS
  for (byte i = 0; i < 4; i++) {

    pinMode(relayPins[i], OUTPUT);

    digitalWrite(relayPins[i], RELAY_OFF);

  }

  // DHT11
  dht.begin();

  // LCD
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("SYSTEM STARTING");

  // IR RECEIVER
  IrReceiver.begin(IR_RECEIVER, ENABLE_LED_FEEDBACK);

  delay(1500);

  // Startup defaults to AUTO
  automaticMode = true;

  smokeDetected = (digitalRead(MQ2_DIGITAL) == LOW);

  digitalWrite(
    RED_LED,
    smokeDetected ? HIGH : LOW
  );

  applyFanOutputs();

  Serial.println("SYSTEM READY");

}

// ================= MAIN LOOP =================

void loop() {

  unsigned long currentMillis = millis();

  // Read remote continuously
  processRemote();

  // Read smoke sensor
  if (currentMillis - lastSmokeRead >= SMOKE_INTERVAL) {

    lastSmokeRead = currentMillis;

    readSmokeSensor();

  }

  // Read temperature and humidity
  if (currentMillis - lastDHTRead >= DHT_INTERVAL) {

    lastDHTRead = currentMillis;

    readDHTSensor();

  }

  // Update LCD
  if (currentMillis - lastLCDUpdate >= LCD_INTERVAL) {

    lastLCDUpdate = currentMillis;

    updateLCD();

  }

  // Buzzer must run continuously
  controlBuzzer();

}
