#include <Wire.h>
#include <U8g2lib.h>
#include <Adafruit_INA219.h>
#include <MAX17043.h>

#define FRAME_RATE 8.0

U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
Adafruit_INA219 ina219;

enum BatteryState {
  CHARGING,
  DISCHARGING,
  IDLING,
  MISSING
};

BatteryState batteryState = MISSING;
BatteryState lastBatteryState = MISSING;
char* lblTime = new char[8];

void getTimeStr(char *& str, int seconds) {
  // refresh string and return
  int h = seconds / 60 / 60;
  int m = (seconds / 60) % 60;
  int s = seconds % 60;

  String t =   (h < 10 ? "0" : "") + String(h) + "h" //
               + (m < 10 ? "0" : "") + String(m) + "m" //
               + (s < 10 ? "0" : "") + String(s);

  strcpy(str, t.c_str());
}

void setup() {
  u8g2.begin();
  u8g2.setContrast(0);
  ina219.begin();
  ina219.setCalibration_32V_1A();
  FuelGauge.begin();
  FuelGauge.quickstart();
}

void drawBattery(uint8_t x, uint8_t y, uint8_t w, uint8_t h, float percent) {
  float fixedPercent = percent >= 100 ? 100 : percent;
  fixedPercent = percent <= 10 ? 10 : percent;

  u8g2.drawBox(x + w / 4, y, w / 2, h / 16);
  u8g2.drawRFrame(x, y + h / 16, w, (h / 16) * 15, 2);
  // DEBUG: u8g2.drawStr(64, 30, String(fixedPercent).c_str());
  u8g2.drawBox(x + 1, //
               y + h / 16.0 + (((100.0 - fixedPercent) / 100.0) * (h - h / 16.0)), //
               w - 2, //
               ((fixedPercent / 100.0) * (h - h / 16)));
}

uint8_t loopCount = 0;
unsigned long loopStart = 0;

float charged_mA = 0;
float discharged_mA = 0;
unsigned long charged_ms = 0;
unsigned long discharged_ms = 0;

void loop() {
  loopStart = millis();

  float shuntVoltage = 0;
  float busVoltage = 0;
  float current_mA = 0;
  float current_mA_3_7V = 0;
  float loadVoltage = 0;
  float power_mW = 0;
  float charge = 0;
  float voltage = 0;

  shuntVoltage = ina219.getShuntVoltage_mV();
  busVoltage = ina219.getBusVoltage_V();
  current_mA = ina219.getCurrent_mA();
  power_mW = ina219.getPower_mW();
  loadVoltage = busVoltage + (shuntVoltage / 1000);
  charge = FuelGauge.percent();
  voltage = FuelGauge.voltage();

  u8g2.clearBuffer();
  if (voltage >= 4.9) {
    // uninitialized values give a FuelGauge.voltage() of around 5,
    // this way can detect the program has just started
    u8g2.setFont(u8g2_font_profont12_tf);
    u8g2.drawStr(20, 20, "Booting.");
  }  else if (voltage < 3) {
    // we have no battery (this is a guess)
    u8g2.setFont(u8g2_font_profont12_tf);
    u8g2.drawStr(20, 20, "No Battery?");
    drawBattery(100, 0, 16, 32, 0);
    u8g2.setFont(u8g2_font_profont22_tf);
    u8g2.drawStr(103, 25, "?");
    batteryState = MISSING;
  } else {
    // we have a battery, i.e. >= 3.0V and < 4.9V
    u8g2.setFont(u8g2_font_profont12_tf);

    if (current_mA < -3) {
      batteryState = CHARGING;
    } else if (current_mA > 3) {
      batteryState = DISCHARGING;
    } else {
      batteryState = IDLING;
    }

    if (current_mA < -3 || current_mA > 3) {
    }

    if (batteryState == IDLING) {
      u8g2.drawStr(0, 10, ("IDLE " + String(voltage, 2) + "V " + String(charge, charge >= 100 ? 0 : (charge >= 10 ? 1 : 2)) + "%").c_str());
      getTimeStr(lblTime, charged_ms / 1000);
      uint8_t digits = charged_mA >= 100 ? 0 : charged_mA >= 10 ? 1 : 2;
      u8g2.drawStr(0, 20, ((charged_mA < 1000 && charged_mA >= 100 ? " " : "") + String(charged_mA, digits) + "mA " +  String(lblTime)).c_str());
      u8g2.drawStr(94, 20, ">");
      getTimeStr(lblTime, discharged_ms / 1000);
      digits = discharged_mA >= 100 ? 0 : discharged_mA >= 10 ? 1 : 2;
      u8g2.drawStr(0, 30, ( (discharged_mA < 1000 && discharged_mA >= 100 ? " " : "") + String(discharged_mA, digits) + "mA " + String(lblTime)).c_str());
      u8g2.drawStr(94, 30, "<");
    } else if (batteryState == CHARGING) {
      getTimeStr(lblTime, charged_ms / 1000);
      u8g2.drawStr(0, 10, "CHARGING");
      u8g2.drawStr(0, 20, (String(voltage) + "V").c_str());
      u8g2.drawStr(40, 20, (String(-current_mA) + "mA >").c_str());
      uint8_t digits = charged_mA >= 100 ? 0 : charged_mA >= 10 ? 1 : 2;
      u8g2.drawStr(0, 30, ((discharged_mA < 1000 && discharged_mA >= 100 ? " " : "") + String(charged_mA, digits) + "mAh " + lblTime).c_str());
    } else if (batteryState == DISCHARGING) {
      getTimeStr(lblTime, discharged_ms / 1000);
      u8g2.drawStr(0, 10, "DISCHARGING");
      u8g2.drawStr(0, 20, (String(voltage) + "V").c_str());
      u8g2.drawStr(40, 20, (String(current_mA) + "mA <").c_str());
      uint8_t digits = discharged_mA >= 100 ? 0 : discharged_mA >= 10 ? 1 : 2;
      u8g2.drawStr(0, 30, ((discharged_mA < 1000 && discharged_mA >= 100 ? " " : "") + String(discharged_mA, digits) + "mAh " + lblTime).c_str());
    }

    drawBattery(100, 0, 16, 32, batteryState == CHARGING ? (loopCount % 100) : charge);
  }
  u8g2.sendBuffer();

  if (lastBatteryState != batteryState) {
    // restart the chip to detect correct SOC
    FuelGauge.reset();
  }

  delay(1000 / FRAME_RATE);

  unsigned long delta = millis() - loopStart;
  // convert mA to mA measured at 3.7V to get closer to 
  // lipo specs capacity at 3.7V
  current_mA_3_7V = current_mA * (loadVoltage / 3.7);
  
  if (batteryState == CHARGING) {
    charged_mA += -current_mA_3_7V * (delta / (1000.0 * 60.0 * 60.0));
    charged_ms += delta;
  } else if (batteryState == DISCHARGING) {
    discharged_mA += current_mA_3_7V * (delta / (1000.0 * 60.0 * 60.0));
    discharged_ms += delta;
  }


  lastBatteryState = batteryState;
  loopCount++;
}
