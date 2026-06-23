/*

  Program name: Telemetry system signal repeater
  Copyright (c) 2018, Kris Kasprak all rights reserved

  ///////////////////////////////////////////////////
  code for ESP32
  ///////////////////////////////////////////////////

  Purpose measure:
  1. recieve transmitted data from the car or another repeater
  2. send data back out
  3. change the ID so pit station knows where the data came from
  4. enable a ping mode to send data w/o incoming data (for repeater placement)

  Revision table
  rev   author      date        description
  1.0   kris        5-20-18     initial code
  2.0   Kyle        6-10-18     Added Display Support, Changed Pin Numbers, Added Set Menu.
  2.1   Kyle        6-24-18     Changed to New Menu, added button support, changed code to set transiever
  2.7   Kris        5-29-20     updated to new struct, added EBYTE.h, added more display colors
  2.8   Kris        5-29-20     updated reset ebyte code
  3.0   Kris        11-23-20    recompiled with updated driver list
  3.1   Kris        11-23-20    switched to easytransfer to handle struct packing issues

  B03Vv1.0   Kris   10-09-23    redesign for 3 transceivers, new board that is same as wifi sever
  B03Vv1.5   Kris   10-23-24    updated few new struct and numberpad lib and lastest compiler
  B03Vv1.7   Kris   1-6-25      added ping mechanism to make repeater placement easier
  B03Vv1.77  Kris   1-19-25     improved some ping stuff and removed password (just too sensitive to enter in field)
  B03Vv1.78  Kris   1-19-25     added better send ping diagnostics
  B05.02.00   Kris   6-11-2026   built on new boards, new Transceivers, better ping mode, etc.
*/

#include <SPI.h>
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"
#include <EasyTransfer.h>
#include <XPT2046_Touchscreen.h>
#include <SoftwareSerial.h>
#include <elapsedMillis.h>
#include <PatriotRacing_Utilities.h>
#include <EEPROM.h>
#include "Adafruit_fonts.h"
#include "EBYTE_E220.h"
#include "FlickerFreePrint.h"
#include <Adafruit_ILI9341_Controls.h>  // custom control define file
#include <Adafruit_ILI9341_Keypad.h>

#define VERSION "B5.02.00"

#define DEBUG_ON

#define FONT_HEADER arial16
#define FONT_DATA arial12
#define FONT_ITEM arial10

#define COLUMN_1 50
#define COLUMN_2 150
#define COLUMN_3 250

#define DATA_COL1 115
#define DATA_COL2 185
#define DATA_COL3 255



#define ROW_1 120
#define ROW_2 160
#define ROW_3 200

#define STATUS_COL 140
#define SEND_PING_TIME 1000
#define STATUS_PIN 15
#define PING_PIN 14

char buf[60];

uint8_t RChannel = 0, WChannel = 0, BChannel = 0;
uint8_t RDataRate = 0, WDataRate = 0, BDataRate = 0;
uint8_t RRadioPower = 0, WRadioPower = 0, BRadioPower = 0;
// uint8_t h = 0, m = 0, s = 0, b = 0, i = 0;
int16_t BtnX = 0, BtnY = 0, BtnZ = 0;
long ScreenLeft = 3900, ScreenRight = 360, ScreenTop = 3700, ScreenBottom = 300;
uint8_t DeviceID = 0, SourceID = 0;
bool AlertToggle = true;
uint16_t PingCount = 0;
uint8_t RSSINoise = 0;
uint8_t RAddressL = 0, WAddressL = 0, BAddressL = 0;
uint8_t RAddressH = 0, WAddressH = 0, BAddressH = 0;

#define Serial_0 Serial2
SoftwareSerial Serial_1(33, 32);
SoftwareSerial Serial_2(21, 22);

EBYTE_E220 EBYTE_0(&Serial_0, 13, 13, 36);
EBYTE_E220 EBYTE_1(&Serial_1, 26, 26, 39);
EBYTE_E220 EBYTE_2(&Serial_2, 4, 4, 34);

Transceiver Data_0;
Transceiver Data_1;
Transceiver Data_2;

EasyTransfer DataPacket_0;
EasyTransfer DataPacket_1;
EasyTransfer DataPacket_2;

Adafruit_ILI9341 Display = Adafruit_ILI9341(27, 12, 5);

XPT2046_Touchscreen Touch(2, 35);
TS_Point TP;

Button SetupBtn(&Display);
Button DoneBtn(&Display);

Button BChannelBtn(&Display);
Button RChannelBtn(&Display);
Button WChannelBtn(&Display);

Button RAddressLBtn(&Display);
Button RAddressHBtn(&Display);
Button WAddressLBtn(&Display);
Button WAddressHBtn(&Display);
Button BAddressLBtn(&Display);
Button BAddressHBtn(&Display);

Button RDataRateBtn(&Display);
Button WDataRateBtn(&Display);
Button BDataRateBtn(&Display);

Button RResetBtn(&Display);
Button WResetBtn(&Display);
Button BResetBtn(&Display);

Button DeviceIDBtn(&Display);

NumberPad NumberInput(&Display, &Touch);

elapsedMillis RTimer = 0;
elapsedMillis WTimer = 0;
elapsedMillis BTimer = 0;
elapsedMillis SmartDelayTimer = 0;
elapsedMillis PingTime = 0;
elapsedMillis AlertTime = 0;
elapsedMillis RSSITimer = 0;

FlickerFreePrint<Adafruit_ILI9341> RSSIText(&Display, C_WHITE, C_BLACK);


void setup() {

  Serial.begin(115200);

  pinMode(STATUS_PIN, OUTPUT);
  pinMode(PING_PIN, INPUT);

  Serial.println("Starting signal repeater");

  EEPROM.begin(100);

  GetParameters();

  SPI.begin();
  Display.begin();
  Display.setRotation(1);

  Display.fillScreen(C_BLACK);
  SmartDelay(100);

  // fire up the touch display
  Touch.begin();
  Touch.setRotation(1);

  TP = Touch.getPoint();
  if (TP.y > 1000) {
    CalibrateTouch(&Touch, &Display, &ScreenRight, &ScreenLeft, &ScreenBottom, &ScreenTop);

    EEPROM.put(10, ScreenRight);
    EEPROM.put(20, ScreenLeft);
    EEPROM.put(30, ScreenBottom);
    EEPROM.put(40, ScreenTop);
    delay(100);
    EEPROM.commit();
    delay(100);
  }

  Display.fillRect(0, 0, 319, 40, C_WHITE);
  Display.setTextColor(C_BLACK, C_WHITE);
  Display.setFont(&FONT_HEADER);
  Display.setCursor(15, 30);
  Display.print(F("Signal Booster"));

  Display.setTextColor(C_WHITE);
  Display.setFont(&FONT_ITEM);
  Display.setCursor(5, 60);
  Display.print("Version:");
  Display.setCursor(5, 85);
  Display.print("Interface: ");
  Display.setCursor(5, 110);
  Display.print(F("Red:"));
  Display.setCursor(5, 135);
  Display.print(F("White:"));
  Display.setCursor(5, 160);
  Display.print(F("Blue:"));
  Display.setCursor(5, 185);
  Display.print(F("Device ID:"));
  Display.setCursor(5, 210);
  Display.print(F("Ambient Noise:"));

  // start the boot up process
  Display.setTextColor(C_GREEN);
  Display.setCursor(STATUS_COL, 60);
  Display.print(F(VERSION));

  delay(100);

  CreateInterface();
  Display.setTextColor(C_GREEN);
  Display.setCursor(STATUS_COL, 85);
  Display.print("Created");

  ///////////////////////////////////////////////
  // start radio #0
  Serial_0.begin(9600, SERIAL_8N1, 16, 17);
  if (StartRadio_0()) {
    Display.setTextColor(C_GREEN);
    Display.setCursor(STATUS_COL, 110);
    Display.print(F("OK"));
    Display.setTextColor(C_WHITE);
    Display.setCursor(STATUS_COL + 40, 110);
    Display.print(RChannel);
    Display.print(" / ");
    Display.print(RDataRate);
    Display.print(" / ");
    Display.print(RAddressL);
    Display.print(" / ");
    Display.print(RAddressH);
    Display.print(" / ");
    Display.print(EBYTE_0.getTransmitPower());
  } else {
    Display.setTextColor(C_RED);
    Display.setCursor(STATUS_COL, 110);
    Display.print(F("FAIL"));
  }

  Serial_1.begin(9600);
  if (StartRadio_1()) {
    Display.setTextColor(C_GREEN);
    Display.setCursor(STATUS_COL, 135);
    Display.print(F("OK"));
    Display.setTextColor(C_WHITE);
    Display.setCursor(STATUS_COL + 40, 135);
    Display.print(WChannel);
    Display.print(" / ");
    Display.print(WDataRate);
    Display.print(" / ");
    Display.print(WAddressL);
    Display.print(" / ");
    Display.print(WAddressH);
    Display.print(" / ");
    Display.print(EBYTE_1.getTransmitPower());

  } else {
    Display.setTextColor(C_RED);
    Display.setCursor(STATUS_COL, 135);
    Display.print(F("FAIL"));
  }

  Serial_2.begin(9600);
  if (StartRadio_2()) {
    Display.setTextColor(C_GREEN);
    Display.setCursor(STATUS_COL, 160);
    Display.print(F("OK"));
    Display.setTextColor(C_WHITE);
    Display.setCursor(STATUS_COL + 40, 160);
    Display.print(BChannel);
    Display.print(" / ");
    Display.print(BDataRate);
    Display.print(" / ");
    Display.print(RAddressL);
    Display.print(" / ");
    Display.print(RAddressH);
    Display.print(" / ");
    Display.print(EBYTE_2.getTransmitPower());
  } else {
    Display.setTextColor(C_RED);
    Display.setCursor(STATUS_COL, 160);
    Display.print(F("FAIL"));
  }

  Display.setTextColor(C_GREEN);
  Display.setCursor(STATUS_COL, 185);
  Display.print(DeviceID);

  Display.setCursor(STATUS_COL, 260);
  Display.print(EBYTE_0.readRSSIAmbientNoise());

  SmartDelay(1000);

  Display.fillScreen(C_BLACK);
  DisplayHeader();
}

/*

continually listen for a signal from all three transceivers, if one is found, change the source ID to match this device
and send the singnal back out

*/

void loop() {

  ProcessLoopTouch();

  Display.setFont(&FONT_ITEM);

  if (AlertTime > 500) {
    AlertTime = 0;
    AlertToggle = !AlertToggle;
    digitalWrite(STATUS_PIN, AlertToggle);
  }

  /////////////////////////////////////////////////////////////////////////
  // RED car
  if (DataPacket_0.receiveData()) {
    ProcessLoopTouch();
    Display.fillCircle(DATA_COL1 + 21, 161, 14, C_GREEN);
    Display.drawCircle(DATA_COL1 + 20, 160, 15, C_WHITE);
    SourceID = Data_0.RPM_DNO_DID & 0b0000000000000011;

    if (SourceID != DeviceID) {
      // zero out DID SID so we don't have any stray data
      Data_0.RPM_DNO_DID = Data_0.RPM_DNO_DID & 0b1111111111111100;
      Data_0.ALTITUDE_SID = Data_0.ALTITUDE_SID & 0b1111111111111100;
      // now save actual data
      Data_0.RPM_DNO_DID = Data_0.RPM_DNO_DID | (DeviceID & 0b0000000000000011);
      Data_0.ALTITUDE_SID = Data_0.ALTITUDE_SID | (SourceID & 0b0000000000000011);

      DataPacket_0.sendData();
      SmartDelay(100);
      Display.fillCircle(DATA_COL1 + 21, 201, 14, C_GREEN);
      Display.drawCircle(DATA_COL1 + 20, 200, 15, C_WHITE);
    }

    RTimer = 0;
  } else {
    // waiting
    ProcessLoopTouch();
    if (RTimer > 100) {
      BlankRed();
    }
  }
  // WHITE car
  if (DataPacket_1.receiveData()) {
    Display.fillCircle(DATA_COL2 + 21, 161, 14, C_GREEN);
    Display.drawCircle(DATA_COL2 + 20, 160, 15, C_WHITE);
    ProcessLoopTouch();
    SourceID = Data_1.RPM_DNO_DID & 0b0000000000000011;
    if (SourceID != DeviceID) {
      // zero out DID SID so we don't have any stray data
      Data_1.RPM_DNO_DID = Data_1.RPM_DNO_DID & 0b1111111111111100;
      Data_1.ALTITUDE_SID = Data_1.ALTITUDE_SID & 0b1111111111111100;
      // now save actual data
      Data_1.RPM_DNO_DID = Data_1.RPM_DNO_DID | (DeviceID & 0b0000000000000011);
      Data_1.ALTITUDE_SID = Data_1.ALTITUDE_SID | (SourceID & 0b0000000000000011);
      DataPacket_1.sendData();
      SmartDelay(100);
      Display.fillCircle(DATA_COL2 + 21, 201, 14, C_GREEN);
      Display.drawCircle(DATA_COL2 + 20, 200, 15, C_WHITE);
    }
    WTimer = 0;
  } else {
    ProcessLoopTouch();
    if (WTimer > 100) {
      BlankWhite();
    }
  }
  // BLUE car
  if (DataPacket_2.receiveData()) {
    Display.fillCircle(DATA_COL3 + 21, 161, 14, C_GREEN);
    Display.drawCircle(DATA_COL3 + 20, 160, 15, C_WHITE);
    ProcessLoopTouch();
    SourceID = Data_2.RPM_DNO_DID & 0b0000000000000011;
    if (SourceID != DeviceID) {
      // zero out DID SID so we don't have any stray data
      Data_2.RPM_DNO_DID = Data_2.RPM_DNO_DID & 0b1111111111111100;
      Data_2.ALTITUDE_SID = Data_2.ALTITUDE_SID & 0b1111111111111100;
      // now save actual data
      Data_2.RPM_DNO_DID = Data_2.RPM_DNO_DID | (DeviceID & 0b0000000000000011);
      Data_2.ALTITUDE_SID = Data_2.ALTITUDE_SID | (SourceID & 0b0000000000000011);
      DataPacket_2.sendData();
      SmartDelay(100);
      Display.fillCircle(DATA_COL3 + 21, 201, 14, C_GREEN);
      Display.drawCircle(DATA_COL3 + 20, 200, 15, C_WHITE);
    }
    BTimer = 0;
  } else {
    ProcessLoopTouch();
    if (BTimer > 100) {
      BlankBlue();
    }
  }
}

void BlankRed() {
  Display.fillCircle(DATA_COL1 + 21, 121, 14, C_GREEN);
  Display.drawCircle(DATA_COL1 + 20, 120, 15, C_WHITE);
  Display.fillCircle(DATA_COL1 + 21, 161, 14, C_DKGREY);
  Display.drawCircle(DATA_COL1 + 20, 160, 15, C_WHITE);
  Display.fillCircle(DATA_COL1 + 21, 201, 14, C_DKGREY);
  Display.drawCircle(DATA_COL1 + 20, 200, 15, C_WHITE);
}

void BlankWhite() {
  Display.fillCircle(DATA_COL2 + 21, 121, 14, C_GREEN);
  Display.drawCircle(DATA_COL2 + 20, 120, 15, C_WHITE);
  Display.fillCircle(DATA_COL2 + 21, 161, 14, C_DKGREY);
  Display.drawCircle(DATA_COL2 + 20, 160, 15, C_WHITE);
  Display.fillCircle(DATA_COL2 + 21, 201, 14, C_DKGREY);
  Display.drawCircle(DATA_COL2 + 20, 200, 15, C_WHITE);
}
void BlankBlue() {
  Display.fillCircle(DATA_COL3 + 21, 121, 14, C_GREEN);
  Display.drawCircle(DATA_COL3 + 20, 120, 15, C_WHITE);
  Display.fillCircle(DATA_COL3 + 21, 161, 14, C_DKGREY);
  Display.drawCircle(DATA_COL3 + 20, 160, 15, C_WHITE);
  Display.fillCircle(DATA_COL3 + 21, 201, 14, C_DKGREY);
  Display.drawCircle(DATA_COL3 + 20, 200, 15, C_WHITE);
}

/* 

    function to send a signal to transceiver 0 w/o incoming singnal
    this is helpful when placing repeaters, as the base sees a "ping" and can alert when
    range impedes transmission
    you can only have one repeater pinging
    other repeaters will hear a ping, get it's id and pass both IDs along to base
    are repeater will not repeat it's signal

  */
void SendPings() {

  Display.fillScreen(C_BLACK);
  Display.fillRect(0, 0, 319, 40, C_WHITE);
  Display.setTextColor(C_BLACK, C_WHITE);
  Display.setFont(&FONT_HEADER);
  Display.setCursor(10, 30);
  Display.print(F("Ping Mode"));

  Display.setFont(&FONT_ITEM);
  Display.setTextColor(C_WHITE);

  RChannel = EBYTE_0.getChannel();

  Display.setCursor(30, 70);
  Display.print(F("ID: "));
  Display.print(DeviceID);

  Display.setCursor(30, 100);
  Display.print(F("Channel: "));
  Display.print(RChannel);

  Display.setCursor(30, 130);
  Display.print(F("Air Data rate: "));
  Display.print(AirRateText[RDataRate]);

  Display.setCursor(30, 160);
  Display.print(F("RSSI Ambient Noise: "));

  Display.setCursor(30, 220);
  Display.print(F("Ping: "));

  // enable RSSI signal bit, we don't do this in normal mode as it adds 1 byte to the
  // transmit payload

  SetRSSIState(true);

  PingCount = 0;

  while (1) {

    if (AlertTime > 100) {
      AlertTime = 0;
      AlertToggle = !AlertToggle;
      digitalWrite(STATUS_PIN, AlertToggle);
    }
    if (digitalRead(PING_PIN) == LOW) {
      break;
    }

    if (PingTime > SEND_PING_TIME) {

      PingTime = 0;
      Data_0.WARNINGS_PE = 0;
      Data_0.WARNINGS_PE = (PING_MODE << 10);
      // send the device ID
      Data_0.RPM_DNO_DID = DeviceID;
      // clear the source ID
      Data_0.ALTITUDE_SID = 0;
      // increment ping counter so we can see transmission on webserver side
      Data_0.LT_LAPENERGY = (PingCount << 6);

      if (RSSITimer > 4000) {
        RSSITimer = 0;
        RSSINoise = abs(EBYTE_0.readRSSIAmbientNoise());
      }

      Data_0.TEMPF_TEMPX = RSSINoise;

      Display.setCursor(230, 160);

      RSSIText.setTextColor(C_WHITE, C_BLACK);
      RSSIText.print(-RSSINoise);

      // send data
      DataPacket_0.sendData();

      Display.fillRect(130, 195, 105, 35, C_BLACK);
      Display.setCursor(135, 220);
      Display.print(PingCount);

      PingCount++;
    }
  }
  SetRSSIState(false);
}

void SetRSSIState(bool state) {

  EBYTE_0.setRSSISignalStrength(state);
  EBYTE_0.saveParameters(EBYTE_WRITE_PERMANENT);
}

void SmartDelay(uint32_t msDelay) {

  SmartDelayTimer = 0;
  while (SmartDelayTimer < msDelay) {
    if (Touch.touched()) {
      break;
    }
  }
}

void DisplayHeader() {

  Display.fillRect(0, 0, 319, 40, C_WHITE);
  Display.setTextColor(C_BLACK, C_WHITE);
  Display.setFont(&FONT_HEADER);
  Display.setCursor(10, 30);
  Display.print(F("Booster #"));
  Display.print(DeviceID);

  Display.setFont(&FONT_ITEM);
  Display.setTextColor(C_RED, C_BLACK);

  Display.setCursor(DATA_COL1, 80);
  Display.print(F("Red"));
  Display.setTextColor(C_WHITE, C_BLACK);
  Display.setCursor(DATA_COL2, 80);
  Display.print(F("White"));
  Display.setTextColor(C_BLUE, C_BLACK);
  Display.setCursor(DATA_COL3, 80);
  Display.print(F("Blue"));

  Display.setTextColor(C_WHITE, C_BLACK);
  Display.setFont(&FONT_ITEM);

  Display.setCursor(5, 120);
  Display.print(F("Waiting"));
  Display.setCursor(5, 160);
  Display.print(F("Processing"));
  Display.setCursor(5, 200);
  Display.print(F("Sending"));
  SetupBtn.draw();
}

void ProcessLoopTouch() {

  if (digitalRead(PING_PIN) == HIGH) {
    // were in ping mode
    SendPings();

    // done sending pings, reset vars
    Data_0.WARNINGS_PE = 0;
    Data_0.RPM_DNO_DID = 0;
    Data_0.ALTITUDE_SID = 0;
    Data_0.VOLTS_LAPS = 0;
    Data_0.LT_LAPENERGY = 0;
    // send data
    DataPacket_0.sendData();
    SmartDelay(1000);

    Display.fillScreen(C_BLACK);
    DisplayHeader();
  }

  ProcessTouch();

  if (PressIt(SetupBtn) == true) {

    Display.fillScreen(C_BLACK);
    Settings();
    Display.fillScreen(C_BLACK);
    DisplayHeader();
  }
}

bool PressIt(Button TheButton) {

  if (TheButton.press(BtnX, BtnY)) {
    TheButton.draw(B_PRESSED);
    while (Touch.touched()) {
      if (TheButton.press(BtnX, BtnY)) {
        TheButton.draw(B_PRESSED);
      } else {
        TheButton.draw(B_RELEASED);
        return false;
      }
      ProcessTouch();
    }

    TheButton.draw(B_RELEASED);
    return true;
  }
  return false;
}

void ProcessTouch() {

  BtnX = -1;
  BtnY = -1;

  if (Touch.touched()) {

    TP = Touch.getPoint();
    BtnX = TP.x;
    BtnY = TP.y;
    BtnZ = TP.z;
    //yellow
    BtnX = map(BtnX, ScreenLeft, ScreenRight, 0, 320);
    BtnY = map(BtnY, ScreenTop, ScreenBottom, 0, 240);

    //black headers
    //BtnX  = map(BtnX, 0, 3905, 320, 0);
    //BtnY  = map(BtnY, 0, 3970, 240, 0);
    /*
#ifdef DEBUG_ON
    Serial.print("Mapped coordinates : ");
    Serial.print(BtnX);
    Serial.print(", ");
    Serial.print(BtnY);
    Serial.print(", ");
    Serial.println(BtnZ);
    Display.fillCircle(BtnX, BtnY, 3, C_RED);
#endif
*/
  }
}

void Settings() {

  bool KeepIn = true;
  bool RDirty = false, WDirty = false, BDirty = false;

  DrawSettingsScreen();

  while (KeepIn) {

    ProcessTouch();

    if (PressIt(DoneBtn) == true) {
      KeepIn = false;
    }

    if (PressIt(DeviceIDBtn) == true) {
      DeviceID++;
      if (DeviceID > 3) {
        DeviceID = 1;
      }
      if (DeviceID < 1) {
        DeviceID = 1;
      }
      sprintf(buf, "%d", DeviceID);
      DeviceIDBtn.setText(buf);
      DrawSettingsScreen();
    }

    if (PressIt(RChannelBtn) == true) {
      NumberInput.setMinMax(0.0, 80.0);
      NumberInput.value = RChannel;
      NumberInput.getInput();
      RChannel = (uint8_t)NumberInput.value;
      RDirty = true;
      sprintf(buf, "%d", RChannel);
      RChannelBtn.setText(buf);
      DrawSettingsScreen();
    }
    if (PressIt(WChannelBtn) == true) {
      NumberInput.setMinMax(0.0, 80.0);
      NumberInput.value = WChannel;
      NumberInput.getInput();
      WChannel = (uint8_t)NumberInput.value;
      WDirty = true;
      sprintf(buf, "%d", WChannel);
      WChannelBtn.setText(buf);
      DrawSettingsScreen();
    }
    if (PressIt(BChannelBtn) == true) {
      NumberInput.setMinMax(0.0, 80.0);
      NumberInput.value = BChannel;
      NumberInput.getInput();
      BChannel = (uint8_t)NumberInput.value;
      BDirty = true;
      sprintf(buf, "%d", BChannel);
      BChannelBtn.setText(buf);
      DrawSettingsScreen();
    }

    if (PressIt(RAddressLBtn) == true) {
      NumberInput.setMinMax(0.0, 255.0);
      NumberInput.value = RAddressL;
      NumberInput.getInput();
      RAddressL = (uint8_t)NumberInput.value;
      RDirty = true;
      DrawSettingsScreen();
    }
    if (PressIt(RAddressHBtn) == true) {
      NumberInput.setMinMax(0.0, 255.0);
      NumberInput.value = RAddressH;
      NumberInput.getInput();
      RAddressH = (uint8_t)NumberInput.value;
      RDirty = true;
      DrawSettingsScreen();
    }

    if (PressIt(WAddressLBtn) == true) {
      NumberInput.setMinMax(0.0, 255.0);
      NumberInput.value = WAddressL;
      NumberInput.getInput();
      WAddressL = (uint8_t)NumberInput.value;
      WDirty = true;
      DrawSettingsScreen();
    }
    if (PressIt(WAddressHBtn) == true) {
      NumberInput.setMinMax(0.0, 255.0);
      NumberInput.value = WAddressH;
      NumberInput.getInput();
      WAddressH = (uint8_t)NumberInput.value;
      WDirty = true;
      DrawSettingsScreen();
    }

    if (PressIt(BAddressLBtn) == true) {
      NumberInput.setMinMax(0.0, 255.0);
      NumberInput.value = BAddressL;
      NumberInput.getInput();
      BAddressL = (uint8_t)NumberInput.value;
      BDirty = true;
      DrawSettingsScreen();
    }
    if (PressIt(BAddressHBtn) == true) {
      NumberInput.setMinMax(0.0, 255.0);
      NumberInput.value = BAddressH;
      NumberInput.getInput();
      BAddressH = (uint8_t)NumberInput.value;
      BDirty = true;
      DrawSettingsScreen();
    }

    if (PressIt(RDataRateBtn) == true) {
      RDataRate++;
      if (RDataRate >= ((sizeof(AirRateText) / sizeof(AirRateText[0])))) {
        RDataRate = 0;
      }
      RDataRateBtn.setText(AirRateText[RDataRate]);
      RDataRateBtn.draw();
      RDirty = true;
    }

    if (PressIt(WDataRateBtn) == true) {

      WDataRate++;
      if (WDataRate >= ((sizeof(AirRateText) / sizeof(AirRateText[0])))) {
        WDataRate = 0;
      }
      WDataRateBtn.setText(AirRateText[WDataRate]);
      WDataRateBtn.draw();
      WDirty = true;
    }
    if (PressIt(BDataRateBtn) == true) {
      BDataRate++;
      if (BDataRate >= ((sizeof(AirRateText) / sizeof(AirRateText[0])))) {
        BDataRate = 0;
      }
      BDataRateBtn.setText(AirRateText[BDataRate]);
      BDataRateBtn.draw();
      BDirty = true;
    }

    if (PressIt(RResetBtn) == true) {

      ShowResetScreen();

      EBYTE_0.restoreDefaults();
      EBYTE_0.init();

      EBYTE_0.setPacketSize(SUB_64BYTES);
      EBYTE_0.setRSSISignalStrength(false);
      EBYTE_0.setRSSIAmbientNoise(true);
      EBYTE_0.setChannel(5);
      EBYTE_0.saveParameters(EBYTE_WRITE_PERMANENT);
      RChannel = EBYTE_0.getChannel();
      RDataRate = EBYTE_0.getAirDataRate();
      RAddressL = EBYTE_0.getAddressL();
      RAddressH = EBYTE_0.getAddressH();
      DrawSettingsScreen();
#ifdef DEBUG_ON
      EBYTE_0.printParameters();
#endif
    }

    if (PressIt(WResetBtn) == true) {
      ShowResetScreen();
      EBYTE_1.restoreDefaults();
      EBYTE_1.init();
      EBYTE_1.setChannel(15);
      EBYTE_1.setPacketSize(SUB_64BYTES);
      EBYTE_1.setRSSISignalStrength(false);
      EBYTE_1.setRSSIAmbientNoise(true);
      EBYTE_1.saveParameters(EBYTE_WRITE_PERMANENT);
      WChannel = EBYTE_1.getChannel();
      WDataRate = EBYTE_1.getAirDataRate();
      WAddressL = EBYTE_1.getAddressL();
      WAddressH = EBYTE_1.getAddressH();
      DrawSettingsScreen();
#ifdef DEBUG_ON
      EBYTE_1.printParameters();
#endif
    }
    if (PressIt(BResetBtn) == true) {
      ShowResetScreen();
      EBYTE_2.restoreDefaults();
      EBYTE_2.init();
      EBYTE_2.setChannel(1);
      EBYTE_2.setPacketSize(SUB_64BYTES);
      EBYTE_2.setRSSISignalStrength(false);
      EBYTE_2.setRSSIAmbientNoise(true);
      EBYTE_2.setChannel(1);
      EBYTE_2.saveParameters(EBYTE_WRITE_PERMANENT);
      BChannel = EBYTE_2.getChannel();
      BDataRate = EBYTE_2.getAirDataRate();
      BAddressL = EBYTE_2.getAddressL();
      BAddressH = EBYTE_2.getAddressH();
      DrawSettingsScreen();
#ifdef DEBUG_ON
      EBYTE_2.printParameters();
#endif
    }
  }

  if (RDirty) {
    EBYTE_0.setChannel(RChannel);
    EBYTE_0.setPacketSize(SUB_64BYTES);
    EBYTE_0.setAirDataRate(RDataRate);
    EBYTE_0.setTransmitPower(RRadioPower);
    EBYTE_0.setRSSISignalStrength(false);
    EBYTE_0.setRSSIAmbientNoise(true);
    EBYTE_0.setAddressL(RAddressL);
    EBYTE_0.setAddressH(RAddressH);
    EBYTE_0.saveParameters(EBYTE_WRITE_PERMANENT);
#ifdef DEBUG_ON
    EBYTE_0.printParameters();
#endif
  }
  if (WDirty) {
    EBYTE_1.setChannel(WChannel);
    EBYTE_1.setPacketSize(SUB_64BYTES);
    EBYTE_1.setAirDataRate(WDataRate);
    EBYTE_1.setTransmitPower(WRadioPower);
    EBYTE_1.setRSSISignalStrength(false);
    EBYTE_1.setRSSIAmbientNoise(true);
    EBYTE_1.setAddressL(WAddressL);
    EBYTE_1.setAddressH(WAddressH);
    EBYTE_1.saveParameters(EBYTE_WRITE_PERMANENT);
#ifdef DEBUG_ON
    EBYTE_1.printParameters();
#endif
  }
  if (BDirty) {
    EBYTE_2.setChannel(BChannel);
    EBYTE_2.setPacketSize(SUB_64BYTES);
    EBYTE_2.setAirDataRate(BDataRate);
    EBYTE_2.setTransmitPower(BRadioPower);
    EBYTE_2.setRSSISignalStrength(false);
    EBYTE_2.setRSSIAmbientNoise(true);
    EBYTE_2.setAddressL(BAddressL);
    EBYTE_2.setAddressH(BAddressH);
    EBYTE_2.saveParameters(EBYTE_WRITE_PERMANENT);
#ifdef DEBUG_ON
    EBYTE_2.printParameters();
#endif
  }

  EEPROM.put(1, DeviceID);
  EEPROM.commit();
  SmartDelay(10);
}

void ShowResetScreen() {

  Display.fillRect(26, 26, 268, 188, C_WHITE);
  Display.fillRect(30, 30, 260, 180, C_RED);
  Display.setFont(&FONT_ITEM);
  Display.setTextColor(C_WHITE);
  Display.setCursor(60, 80);
  Display.print("Resetting...");
}

void DrawSettingsScreen() {

  digitalWrite(STATUS_PIN, LOW);

  Display.fillScreen(C_BLACK);

  Display.fillRect(0, 0, 319, 40, C_WHITE);
  Display.setTextColor(C_BLACK, C_WHITE);
  Display.setFont(&FONT_HEADER);
  Display.setCursor(10, 30);
  Display.print("Settings");

  DoneBtn.draw();
  DeviceIDBtn.draw();

  sprintf(buf, "%d", RChannel);
  RChannelBtn.setText(buf);
  sprintf(buf, "%d", WChannel);
  WChannelBtn.setText(buf);
  sprintf(buf, "%d", BChannel);
  BChannelBtn.setText(buf);
  sprintf(buf, "%d", RAddressL);
  RAddressLBtn.setText(buf);
  sprintf(buf, "%d", RAddressH);
  RAddressHBtn.setText(buf);
  sprintf(buf, "%d", WAddressL);
  WAddressLBtn.setText(buf);
  sprintf(buf, "%d", WAddressH);
  WAddressHBtn.setText(buf);
  sprintf(buf, "%d", BAddressL);
  BAddressLBtn.setText(buf);
  sprintf(buf, "%d", BAddressH);
  BAddressHBtn.setText(buf);

  RDataRateBtn.setText(AirRateText[RDataRate]);
  WDataRateBtn.setText(AirRateText[WDataRate]);
  BDataRateBtn.setText(AirRateText[BDataRate]);

  RChannelBtn.draw();
  WChannelBtn.draw();
  BChannelBtn.draw();

  RAddressLBtn.draw();
  RAddressHBtn.draw();
  WAddressLBtn.draw();
  WAddressHBtn.draw();
  BAddressLBtn.draw();
  BAddressHBtn.draw();

  RDataRateBtn.draw();
  WDataRateBtn.draw();
  BDataRateBtn.draw();

  RResetBtn.draw();
  WResetBtn.draw();
  BResetBtn.draw();
}

void CreateInterface() {

  SetupBtn.init(280, 20, 60, 35, C_WHITE, C_DKGREY, C_WHITE, C_BLACK, "Set", 0, 0, FONT_ITEM);
  DoneBtn.init(280, 20, 60, 35, C_WHITE, C_DKGREY, C_WHITE, C_BLACK, "OK", 0, 0, FONT_ITEM);

  DeviceIDBtn.init(210, 20, 60, 35, C_WHITE, C_DKGREY, C_WHITE, C_BLACK, "x", 0, 0, FONT_ITEM);
  sprintf(buf, "%d", DeviceID);
  DeviceIDBtn.setText(buf);

  sprintf(buf, "%d", RChannel);
  RChannelBtn.init(53, 71, 98, 40, C_GREY, C_RED, C_WHITE, C_BLACK, buf, 0, 0, FONT_ITEM);
  sprintf(buf, "%d", WChannel);
  WChannelBtn.init(160, 71, 98, 40, C_GREY, C_WHITE, C_BLACK, C_BLACK, buf, 0, 0, FONT_ITEM);
  sprintf(buf, "%d", BChannel);
  BChannelBtn.init(266, 71, 98, 40, C_GREY, C_BLUE, C_WHITE, C_BLACK, buf, 0, 0, FONT_ITEM);

  RAddressLBtn.init(27, 119, 45, 40, C_GREY, C_RED, C_WHITE, C_BLACK, "E1", 0, 0, FONT_ITEM);
  RAddressHBtn.init(80, 119, 45, 40, C_GREY, C_RED, C_WHITE, C_BLACK, "E2", 0, 0, FONT_ITEM);
  WAddressLBtn.init(134, 119, 45, 40, C_GREY, C_WHITE, C_BLACK, C_BLACK, "E1", 0, 0, FONT_ITEM);
  WAddressHBtn.init(186, 119, 45, 40, C_GREY, C_WHITE, C_BLACK, C_BLACK, "E2", 0, 0, FONT_ITEM);
  BAddressLBtn.init(239, 119, 45, 40, C_GREY, C_BLUE, C_WHITE, C_BLACK, "E1", 0, 0, FONT_ITEM);
  BAddressHBtn.init(293, 119, 45, 40, C_GREY, C_BLUE, C_WHITE, C_BLACK, "E2", 0, 0, FONT_ITEM);

  if (RDataRate >= ((sizeof(AirRateText) / sizeof(AirRateText[0])))) {
    RDataRate = 0;
  }
  RDataRateBtn.init(53, 166, 98, 40, C_GREY, C_RED, C_WHITE, C_BLACK, AirRateText[RDataRate], 0, 0, FONT_ITEM);
  if (WDataRate >= ((sizeof(AirRateText) / sizeof(AirRateText[0])))) {
    WDataRate = 0;
  }
  WDataRateBtn.init(160, 166, 98, 40, C_GREY, C_WHITE, C_BLACK, C_BLACK, AirRateText[WDataRate], 0, 0, FONT_ITEM);
  if (BDataRate >= ((sizeof(AirRateText) / sizeof(AirRateText[0])))) {
    BDataRate = 0;
  }
  BDataRateBtn.init(266, 166, 98, 40, C_GREY, C_BLUE, C_WHITE, C_BLACK, AirRateText[BDataRate], 0, 0, FONT_ITEM);

  RResetBtn.init(55, 214, 98, 40, C_RED, C_BLACK, C_RED, C_BLACK, "Reset", 0, 0, FONT_ITEM);
  WResetBtn.init(160, 214, 98, 40, C_RED, C_BLACK, C_RED, C_BLACK, "Reset", 0, 0, FONT_ITEM);
  BResetBtn.init(266, 214, 98, 40, C_RED, C_BLACK, C_RED, C_BLACK, "Reset", 0, 0, FONT_ITEM);

  NumberInput.init(C_BLACK, C_WHITE, C_BLUE, C_WHITE, C_DKBLUE, &FONT_ITEM);
  NumberInput.setMinMax(0.0, 80.0);
  NumberInput.enableDecimal(false);
  NumberInput.enableNegative(false);
  NumberInput.setTouchLimits(ScreenRight, ScreenLeft, ScreenBottom, ScreenTop);
}

bool StartRadio_0() {
  ///////////////////////////////////////////////
  // start radio #0
  DataPacket_0.begin(details(Data_0), &Serial_0);
  if (EBYTE_0.init()) {
    RChannel = EBYTE_0.getChannel();
    RDataRate = EBYTE_0.getAirDataRate();
    RAddressL = EBYTE_0.getAddressL();
    RAddressH = EBYTE_0.getAddressH();
#ifdef DEBUG_ON
    Serial.println("EBYTE_0");
    EBYTE_0.printParameters();
#endif
    return true;
  }
  return false;
}
bool StartRadio_1() {
  ///////////////////////////////////////////////
  // start radio #1
  DataPacket_1.begin(details(Data_1), &Serial_1);
  if (EBYTE_1.init()) {
    WChannel = EBYTE_1.getChannel();
    WDataRate = EBYTE_1.getAirDataRate();
    WAddressL = EBYTE_1.getAddressL();
    WAddressH = EBYTE_1.getAddressH();
#ifdef DEBUG_ON
    Serial.println("EBYTE_1");
    EBYTE_1.printParameters();
#endif
    return true;
  }
  return false;
}

bool StartRadio_2() {
  ///////////////////////////////////////////////
  // start radio #2
  DataPacket_2.begin(details(Data_2), &Serial_2);
  if (EBYTE_2.init()) {
    //EBYTE_0.setPacketSize(SUB_64BYTES);
    BChannel = EBYTE_2.getChannel();
    BDataRate = EBYTE_2.getAirDataRate();
    BAddressL = EBYTE_2.getAddressL();
    BAddressH = EBYTE_2.getAddressH();
#ifdef DEBUG_ON
    Serial.println("EBYTE_2");
    EBYTE_2.printParameters();
#endif
    return true;
  }
  return false;
}

void GetParameters() {

  EEPROM.get(1, DeviceID);
  if (DeviceID < 1) {
    DeviceID = 1;
    EEPROM.put(1, DeviceID);
    EEPROM.commit();
  }

  if (DeviceID > 3) {
    DeviceID = 3;
    EEPROM.put(1, DeviceID);
    EEPROM.commit();
  }

  EEPROM.get(10, ScreenRight);
  EEPROM.get(20, ScreenLeft);
  EEPROM.get(30, ScreenBottom);
  EEPROM.get(40, ScreenTop);
}

void CalibrateTouch(XPT2046_Touchscreen *t, Adafruit_ILI9341 *d, long *XMin, long *XMax, long *YMin, long *YMax) {

  TS_Point point;
  long x_min = 9999, x_max = 0, y_min = 9999, y_max = 0;
  uint32_t ctime = 0;
  int counter = 5;
  char buf[30];
  d->fillScreen(ILI9341_BLACK);
  d->setTextColor(ILI9341_WHITE, 0);
  d->setCursor(40, 60);
  d->println("Squiggle in top left");
  d->setCursor(40, 100);
  d->println("Until numbers are max.");
  // Wait for touch
  while (!t->touched()) {
    delay(100);
  }
  counter = 5;
  ctime = millis();
  while (counter > 0) {
    point = t->getPoint();
    *XMax = point.x;
    *YMax = point.y;
    if (*XMax > x_max) {
      x_max = *XMax;
    }
    if (*YMax > y_max) {
      y_max = *YMax;
    }
    if ((millis() - ctime) > 1000) {
      ctime = millis();
      counter--;
      d->setCursor(40, 150);
      d->println(counter);
    }
    *XMax = x_max;
    *YMax = y_max;
    sprintf(buf, "%4ld, %4ld", *XMax, *XMax);
    d->setCursor(10, 200);
    d->print(buf);
  }
  *XMax = x_max;
  *YMax = y_max;
  delay(1000);

  d->fillScreen(ILI9341_BLACK);
  d->setCursor(10, 60);
  d->println("Squiggle in lower right. ");
  d->setCursor(40, 100);
  d->println("Until numbers are min.  ");
  counter = 5;
  ctime = millis();
  while (counter > 0) {
    point = t->getPoint();
    *XMin = point.x;
    *YMin = point.y;
    if (*XMin < x_min) {
      x_min = *XMin;
    }
    if (*YMin < y_min) {
      y_min = *YMin;
    }
    if ((millis() - ctime) > 1000) {
      ctime = millis();
      counter--;
      d->setCursor(40, 150);
      d->println(counter);
    }
    *XMin = x_min;
    *YMin = y_min;
    sprintf(buf, "%4ld, %4ld", *XMin, *YMin);
    d->setCursor(10, 200);
    d->print(buf);
  }
  *XMin = x_min;
  *YMin = y_min;
  d->fillScreen(ILI9341_BLACK);
  d->setCursor(10, 100);
  d->println("Draw, lift when done.       ");
  d->fillScreen(ILI9341_BLACK);

  counter = 5;
  ctime = millis();

  while (counter > 0) {
    TP = t->getPoint();
    BtnX = TP.x;
    BtnY = TP.y;
    BtnX = map(BtnX, *XMax, *XMin, 0, 320);
    BtnY = map(BtnY, *YMax, *YMin, 0, 240);
    d->fillCircle(BtnX, BtnY, 2, 220);
    d->setCursor(40, 200);
    d->print(BtnX);
    d->print(" - ");
    d->print(BtnY);
    if ((millis() - ctime) > 1000) {
      ctime = millis();
      counter--;
    }
    d->setCursor(40, 150);
    d->println(counter);
    d->setCursor(10, 200);
    d->print(*XMin);
    d->print(", ");
    d->print(*YMin);
    d->print(" - ");
    d->print(*XMax);
    d->print(", ");
    d->print(*XMax);
  }

  d->fillScreen(ILI9341_BLACK);
}

// end of code
