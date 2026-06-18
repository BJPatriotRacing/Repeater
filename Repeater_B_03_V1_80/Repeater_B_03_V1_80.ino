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
  B03Vv1.78  Kris   1-19-25     added better sendping diagnostics
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

//#define DEBUG_ON

#define FONT_HEADER arial16
#define FONT_DATA arial12
#define FONT_ITEM arial10

#define COLUMN_1 50
#define COLUMN_2 150
#define COLUMN_3 250

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
uint8_t EncryptLow = 0, EncryptHigh = 0;
uint16_t REncryptKey = 0, WEncryptKey = 0, BEncryptKey = 0;

#define Serial_0 Serial2
SoftwareSerial Serial_1(33, 32);
SoftwareSerial Serial_2(21, 22);

EBYTE_E220 EBYTE_0(&Serial_0, 13, 13, 36);
EBYTE_E220 EBYTE_1(&Serial_1, 26, 26, 39);
EBYTE_E220 EBYTE_2(&Serial_2, 4, 4, 34);

PAYLOAD Data_0;
PAYLOAD Data_1;
PAYLOAD Data_2;

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

Button REncryptBtn(&Display);
Button WEncryptBtn(&Display);
Button BEncryptBtn(&Display);

Button RDataRateBtn(&Display);
Button WDataRateBtn(&Display);
Button BDataRateBtn(&Display);

Button RRadioPowerBtn(&Display);
Button WRadioPowerBtn(&Display);
Button BRadioPowerBtn(&Display);

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
  Display.print(F("Patriot Racing S/R"));

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
    Display.setCursor(STATUS_COL + 80, 110);
    Display.print(RChannel);
    Display.setCursor(STATUS_COL + 140, 110);
    Display.print(RDataRate);
  } else {
    Display.setTextColor(C_RED);
    Display.setCursor(STATUS_COL, 110);
    Display.print(F("FAIL"));
    Display.setCursor(STATUS_COL + 80, 110);
    Display.print(F("?"));
    Display.setCursor(STATUS_COL + 140, 110);
    Display.print(F("?"));
  }

  Serial_1.begin(9600);
  if (StartRadio_1()) {
    Display.setTextColor(C_GREEN);
    Display.setCursor(STATUS_COL, 135);
    Display.print(F("OK"));
    Display.setTextColor(C_WHITE);
    Display.setCursor(STATUS_COL + 80, 135);
    Display.print(WChannel);
    Display.setCursor(STATUS_COL + 140, 135);
    Display.print(WDataRate);
  } else {
    Display.setTextColor(C_RED);
    Display.setCursor(STATUS_COL, 135);
    Display.print(F("FAIL"));
    Display.setCursor(STATUS_COL + 80, 135);
    Display.print(F("?"));
    Display.setCursor(STATUS_COL + 140, 135);
    Display.print(F("?"));
  }

  Serial_2.begin(9600);
  if (StartRadio_2()) {
    Display.setTextColor(C_GREEN);
    Display.setCursor(STATUS_COL, 160);
    Display.print(F("OK"));
    Display.setTextColor(C_WHITE);
    Display.setCursor(STATUS_COL + 80, 160);
    Display.print(BChannel);
    Display.setCursor(STATUS_COL + 140, 160);
    Display.print(BDataRate);
  } else {
    Display.setTextColor(C_RED);
    Display.setCursor(STATUS_COL, 160);
    Display.print(F("FAIL"));
    Display.setCursor(STATUS_COL + 80, 160);
    Display.print(F("?"));
    Display.setCursor(STATUS_COL + 140, 160);
    Display.print(F("?"));
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

    // found
    Display.fillCircle(COLUMN_1 - 40, ROW_1, 9, C_DKGREY);
    Display.fillCircle(COLUMN_1 - 40, ROW_2, 9, C_GREEN);
    Display.fillCircle(COLUMN_1 - 40, ROW_3, 9, C_DKGREY);
    ProcessLoopTouch();
    SourceID = Data_0.RPM_DNO_DID & 0b0000000000000011;

    if (SourceID != DeviceID) {
      // zero out DID SID so we don't have any stray data
      Data_0.RPM_DNO_DID = Data_0.RPM_DNO_DID & 0b1111111111111100;
      Data_0.ALTITUDE_SID = Data_0.ALTITUDE_SID & 0b1111111111111100;
      // now save actual data
      Data_0.RPM_DNO_DID = Data_0.RPM_DNO_DID | (DeviceID & 0b0000000000000011);
      Data_0.ALTITUDE_SID = Data_0.ALTITUDE_SID | (SourceID & 0b0000000000000011);
      SmartDelay(100);
      DataPacket_0.sendData();
    }

    Display.fillCircle(COLUMN_1 - 40, ROW_1, 9, C_DKGREY);
    Display.fillCircle(COLUMN_1 - 40, ROW_2, 9, C_GREEN);
    Display.fillCircle(COLUMN_1 - 40, ROW_3, 9, C_GREEN);

    RTimer = 0;
  } else {
    // waiting
    ProcessLoopTouch();
    if (RTimer > 100) {
      Display.fillCircle(COLUMN_1 - 40, ROW_1, 9, C_GREEN);
      Display.fillCircle(COLUMN_1 - 40, ROW_2, 9, C_DKGREY);
      Display.fillCircle(COLUMN_1 - 40, ROW_3, 9, C_DKGREY);
    }
  }
  // WHITE car
  if (DataPacket_1.receiveData()) {
    Display.fillCircle(COLUMN_2 - 40, ROW_1, 9, C_DKGREY);
    Display.fillCircle(COLUMN_2 - 40, ROW_2, 9, C_GREEN);
    Display.fillCircle(COLUMN_2 - 40, ROW_3, 9, C_DKGREY);
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
    }

    Display.fillCircle(COLUMN_2 - 40, ROW_1, 9, C_DKGREY);
    Display.fillCircle(COLUMN_2 - 40, ROW_2, 9, C_GREEN);
    Display.fillCircle(COLUMN_2 - 40, ROW_3, 9, C_GREEN);
    WTimer = 0;
  } else {
    ProcessLoopTouch();
    if (WTimer > 100) {
      Display.fillCircle(COLUMN_2 - 40, ROW_1, 9, C_GREEN);
      Display.fillCircle(COLUMN_2 - 40, ROW_2, 9, C_DKGREY);
      Display.fillCircle(COLUMN_2 - 40, ROW_3, 9, C_DKGREY);
    }
  }
  // BLUE car
  if (DataPacket_2.receiveData()) {
    Display.fillCircle(COLUMN_3 - 40, ROW_1, 9, C_DKGREY);
    Display.fillCircle(COLUMN_3 - 40, ROW_2, 9, C_GREEN);
    Display.fillCircle(COLUMN_3 - 40, ROW_3, 9, C_DKGREY);
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
    }

    Display.fillCircle(COLUMN_3 - 40, ROW_1, 9, C_DKGREY);
    Display.fillCircle(COLUMN_3 - 40, ROW_2, 9, C_GREEN);
    Display.fillCircle(COLUMN_3 - 40, ROW_3, 9, C_GREEN);
    BTimer = 0;
  } else {
    ProcessLoopTouch();
    if (BTimer > 100) {
      Display.fillCircle(COLUMN_3 - 40, ROW_1, 9, C_GREEN);
      Display.fillCircle(COLUMN_3 - 40, ROW_2, 9, C_DKGREY);
      Display.fillCircle(COLUMN_3 - 40, ROW_3, 9, C_DKGREY);
    }
  }
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
  Display.print(F("Patriot Racing #"));
  Display.print(DeviceID);

  Display.setTextColor(C_RED, C_BLACK);
  Display.setFont(&FONT_ITEM);

  // print red header
  Display.setCursor(COLUMN_1 - 20, 80);
  Display.setFont(&FONT_HEADER);
  Display.print(RChannel);
  Display.setFont(&FONT_ITEM);
  Display.setCursor(COLUMN_1 - 25, ROW_1 + 5);
  Display.setTextColor(C_WHITE, C_BLACK);
  Display.print("Waiting");
  Display.drawCircle(COLUMN_1 - 40, ROW_1, 10, C_WHITE);
  Display.setCursor(COLUMN_1 - 25, ROW_2 + 5);
  Display.setTextColor(C_WHITE, C_BLACK);
  Display.print("Found");
  Display.drawCircle(COLUMN_1 - 40, ROW_2, 10, C_WHITE);
  Display.setCursor(COLUMN_1 - 25, ROW_3 + 5);
  Display.setTextColor(C_WHITE, C_BLACK);
  Display.print("Sent");
  Display.drawCircle(COLUMN_1 - 40, ROW_3, 10, C_WHITE);

  // print white header
  Display.setTextColor(C_WHITE, C_BLACK);
  Display.setCursor(COLUMN_2 - 20, 80);
  Display.setFont(&FONT_HEADER);
  Display.print(WChannel);
  Display.setFont(&FONT_ITEM);
  Display.setCursor(COLUMN_2 - 25, ROW_1 + 5);
  Display.setTextColor(C_WHITE, C_BLACK);
  Display.print("Waiting");
  Display.drawCircle(COLUMN_2 - 40, ROW_1, 10, C_WHITE);
  Display.setCursor(COLUMN_2 - 25, ROW_2 + 5);
  Display.setTextColor(C_WHITE, C_BLACK);
  Display.print("Found");
  Display.drawCircle(COLUMN_2 - 40, ROW_2, 10, C_WHITE);
  Display.setCursor(COLUMN_2 - 25, ROW_3 + 5);
  Display.setTextColor(C_WHITE, C_BLACK);
  Display.print("Sent");
  Display.drawCircle(COLUMN_2 - 40, ROW_3, 10, C_WHITE);

  // print blue header
  Display.setTextColor(C_BLUE, C_BLACK);
  Display.setCursor(COLUMN_3 - 20, 80);
  Display.setFont(&FONT_HEADER);
  Display.print(BChannel);
  Display.setFont(&FONT_ITEM);
  Display.setCursor(COLUMN_3 - 25, ROW_1 + 5);
  Display.setTextColor(C_WHITE, C_BLACK);
  Display.print("Waiting");
  Display.drawCircle(COLUMN_3 - 40, ROW_1, 10, C_WHITE);
  Display.setCursor(COLUMN_3 - 25, ROW_2 + 5);
  Display.setTextColor(C_WHITE, C_BLACK);
  Display.print("Found");
  Display.drawCircle(COLUMN_3 - 40, ROW_2, 10, C_WHITE);
  Display.setCursor(COLUMN_3 - 25, ROW_3 + 5);
  Display.setTextColor(C_WHITE, C_BLACK);
  Display.print("Sent");
  Display.drawCircle(COLUMN_3 - 40, ROW_3, 10, C_WHITE);

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
    SetupScreen();
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

void SetupScreen() {

  bool KeepIn = true;
  bool RDirty = false, WDirty = false, BDirty = false;

  DrawSetupScreen();

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
      DrawSetupScreen();
    }

    if (PressIt(RChannelBtn) == true) {
      NumberInput.setMinMax(0.0, 80.0);
      NumberInput.value = RChannel;
      NumberInput.getInput();
      RChannel = (uint8_t)NumberInput.value;
      RDirty = true;
      sprintf(buf, "%d", RChannel);
      RChannelBtn.setText(buf);
      DrawSetupScreen();
    }
    if (PressIt(WChannelBtn) == true) {
      NumberInput.setMinMax(0.0, 80.0);
      NumberInput.value = WChannel;
      NumberInput.getInput();
      WChannel = (uint8_t)NumberInput.value;
      WDirty = true;
      sprintf(buf, "%d", WChannel);
      WChannelBtn.setText(buf);
      DrawSetupScreen();
    }
    if (PressIt(BChannelBtn) == true) {
      NumberInput.setMinMax(0.0, 80.0);
      NumberInput.value = BChannel;
      NumberInput.getInput();
      BChannel = (uint8_t)NumberInput.value;
      BDirty = true;
      sprintf(buf, "%d", BChannel);
      BChannelBtn.setText(buf);
      DrawSetupScreen();
    }

    if (PressIt(REncryptBtn) == true) {
      NumberInput.setMinMax(0.0, 65535.0);
      NumberInput.value = REncryptKey;
      NumberInput.getInput();
      REncryptKey = (uint16_t)NumberInput.value;
      RDirty = true;
      DrawSetupScreen();
    }

    if (PressIt(WEncryptBtn) == true) {
      NumberInput.setMinMax(0.0, 65535.0);
      NumberInput.value = WEncryptKey;
      NumberInput.getInput();
      WEncryptKey = (uint16_t)NumberInput.value;
      WDirty = true;
      DrawSetupScreen();
    }

    if (PressIt(BEncryptBtn) == true) {
      NumberInput.setMinMax(0.0, 65535.0);
      NumberInput.value = BEncryptKey;
      NumberInput.getInput();
      BEncryptKey = (uint16_t)NumberInput.value;
      BDirty = true;
      DrawSetupScreen();
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
    if (PressIt(RRadioPowerBtn) == true) {
      RRadioPower++;
      if (RRadioPower >= ((sizeof(HighPowerText) / sizeof(HighPowerText[0])))) {
        RRadioPower = 0;
      }
      RRadioPowerBtn.setText(HighPowerText[RRadioPower]);
      RRadioPowerBtn.draw();
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
    if (PressIt(WRadioPowerBtn) == true) {
      WRadioPower++;

      if (WRadioPower >= ((sizeof(HighPowerText) / sizeof(HighPowerText[0])))) {
        WRadioPower = 0;
      }
      WRadioPowerBtn.setText(HighPowerText[WRadioPower]);
      WRadioPowerBtn.draw();
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
    if (PressIt(BRadioPowerBtn) == true) {
      BRadioPower++;
      if (BRadioPower >= ((sizeof(HighPowerText) / sizeof(HighPowerText[0])))) {
        BRadioPower = 0;
      }
      BRadioPowerBtn.setText(HighPowerText[BRadioPower]);
      BRadioPowerBtn.draw();
      BDirty = true;
    }

    if (PressIt(RResetBtn) == true) {
      EBYTE_0.restoreDefaults();
      EBYTE_0.setPacketSize(SUB_64BYTES);
      EBYTE_0.setRSSISignalStrength(false);
      EBYTE_0.setRSSIAmbientNoise(true);
      EBYTE_0.setChannel(5);
      EBYTE_0.saveParameters(EBYTE_WRITE_PERMANENT);
      SmartDelay(50);
#ifdef DEBUG_ON
      EBYTE_0.printParameters();
#endif
    }

    if (PressIt(WResetBtn) == true) {
      EBYTE_1.restoreDefaults();
      EBYTE_1.setPacketSize(SUB_64BYTES);
      EBYTE_1.setRSSISignalStrength(false);
      EBYTE_1.setRSSIAmbientNoise(true);
      EBYTE_1.setChannel(15);
      EBYTE_1.saveParameters(EBYTE_WRITE_PERMANENT);
      SmartDelay(50);
#ifdef DEBUG_ON
      EBYTE_1.printParameters();
#endif
    }
    if (PressIt(BResetBtn) == true) {
      EBYTE_2.restoreDefaults();
      EBYTE_2.setPacketSize(SUB_64BYTES);
      EBYTE_2.setRSSISignalStrength(false);
      EBYTE_2.setRSSIAmbientNoise(true);
      EBYTE_2.setChannel(1);
      EBYTE_2.saveParameters(EBYTE_WRITE_PERMANENT);
      SmartDelay(50);
#ifdef DEBUG_ON
      EBYTE_2.printParameters();
#endif
    }
  }

  SmartDelay(50);

  if (RDirty) {
    // save the encryption keys
    EncryptHigh = REncryptKey >> 8;
    EncryptLow = REncryptKey & 0b0000000011111111;
    EBYTE_0.setChannel(RChannel);
    EBYTE_0.setAirDataRate(RDataRate);
    EBYTE_0.setTransmitPower(RRadioPower);
    EBYTE_0.setRSSISignalStrength(false);
    EBYTE_0.setRSSIAmbientNoise(true);
    EBYTE_0.setEncryptonH(EncryptHigh);
    EBYTE_0.setEncryptonL(EncryptLow);
    EBYTE_0.saveParameters(EBYTE_WRITE_PERMANENT);
    EEPROM.put(2, EncryptHigh);
    EEPROM.put(3, EncryptLow);
    EEPROM.commit();

#ifdef DEBUG_ON
    SmartDelay(1000);
    EBYTE_0.printParameters();
#endif
  }
  if (WDirty) {
    // save the encryption keys
    EncryptHigh = WEncryptKey >> 8;
    EncryptLow = WEncryptKey & 0b0000000011111111;
    EBYTE_1.setChannel(WChannel);
    EBYTE_1.setAirDataRate(WDataRate);
    EBYTE_1.setTransmitPower(WRadioPower);
    EBYTE_1.setRSSISignalStrength(false);
    EBYTE_1.setRSSIAmbientNoise(true);
    EBYTE_1.setEncryptonH(EncryptHigh);
    EBYTE_1.setEncryptonL(EncryptLow);
    EBYTE_1.saveParameters(EBYTE_WRITE_PERMANENT);
    EEPROM.put(4, EncryptHigh);
    EEPROM.put(5, EncryptLow);
    EEPROM.commit();

#ifdef DEBUG_ON
    SmartDelay(1000);
    EBYTE_1.printParameters();
#endif
  }
  if (BDirty) {
    EncryptHigh = BEncryptKey >> 8;
    EncryptLow = BEncryptKey & 0b0000000011111111;
    EBYTE_2.setChannel(BChannel);
    EBYTE_2.setAirDataRate(BDataRate);
    EBYTE_2.setTransmitPower(BRadioPower);
    EBYTE_2.setRSSISignalStrength(false);
    EBYTE_2.setRSSIAmbientNoise(true);
    EBYTE_2.setEncryptonH(EncryptHigh);
    EBYTE_2.setEncryptonL(EncryptLow);
    EBYTE_2.saveParameters(EBYTE_WRITE_PERMANENT);
    EEPROM.put(6, EncryptHigh);
    EEPROM.put(7, EncryptLow);
    EEPROM.commit();

#ifdef DEBUG_ON
    SmartDelay(1000);
    EBYTE_2.printParameters();
#endif
  }

  EEPROM.put(1, DeviceID);
  EEPROM.commit();
  SmartDelay(10);
}

void DrawSetupScreen() {


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

  RDataRateBtn.setText(AirRateText[RDataRate]);
  WDataRateBtn.setText(AirRateText[WDataRate]);
  BDataRateBtn.setText(AirRateText[BDataRate]);

  RRadioPowerBtn.setText(HighPowerText[RRadioPower]);
  WRadioPowerBtn.setText(HighPowerText[WRadioPower]);
  BRadioPowerBtn.setText(HighPowerText[BRadioPower]);

  RChannelBtn.draw();
  WChannelBtn.draw();
  BChannelBtn.draw();

  REncryptBtn.draw();
  WEncryptBtn.draw();
  BEncryptBtn.draw();

  RDataRateBtn.draw();
  WDataRateBtn.draw();
  BDataRateBtn.draw();

  RRadioPowerBtn.draw();
  WRadioPowerBtn.draw();
  BRadioPowerBtn.draw();

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
  RChannelBtn.init(COLUMN_1 - 25, 67, 45, 40, C_GREY, C_RED, C_WHITE, C_BLACK, buf, 0, 0, FONT_ITEM);
  sprintf(buf, "%d", WChannel);
  WChannelBtn.init(COLUMN_2 - 25, 67, 45, 40, C_GREY, C_WHITE, C_BLACK, C_BLACK, buf, 0, 0, FONT_ITEM);
  sprintf(buf, "%d", BChannel);
  BChannelBtn.init(COLUMN_3 - 25, 67, 45, 40, C_GREY, C_BLUE, C_WHITE, C_BLACK, buf, 0, 0, FONT_ITEM);

  REncryptBtn.init(COLUMN_1 + 25, 67, 45, 40, C_GREY, C_WHITE, C_RED, C_BLACK, "X", 0, 0, FONT_ITEM);
  WEncryptBtn.init(COLUMN_2 + 25, 67, 45, 40, C_GREY, C_WHITE, C_RED, C_BLACK, "X", 0, 0, FONT_ITEM);
  BEncryptBtn.init(COLUMN_3 + 25, 67, 45, 40, C_GREY, C_WHITE, C_RED, C_BLACK, "X", 0, 0, FONT_ITEM);


  // air data rate
  if (RDataRate >= ((sizeof(AirRateText) / sizeof(AirRateText[0])))) {
    RDataRate = 0;
  }
  RDataRateBtn.init(COLUMN_1, 107, 95, 40, C_WHITE, C_DKGREY, C_WHITE, C_BLACK, AirRateText[RDataRate], 0, 0, FONT_ITEM);
  if (WDataRate >= ((sizeof(AirRateText) / sizeof(AirRateText[0])))) {
    WDataRate = 0;
  }
  WDataRateBtn.init(COLUMN_2, 107, 95, 40, C_WHITE, C_DKGREY, C_WHITE, C_BLACK, AirRateText[WDataRate], 0, 0, FONT_ITEM);
  if (BDataRate >= ((sizeof(AirRateText) / sizeof(AirRateText[0])))) {
    BDataRate = 0;
  }
  BDataRateBtn.init(COLUMN_3, 107, 95, 40, C_WHITE, C_DKGREY, C_WHITE, C_BLACK, AirRateText[BDataRate], 0, 0, FONT_ITEM);

  // radio power
  if (RRadioPower >= ((sizeof(HighPowerText) / sizeof(HighPowerText[0])))) {
    RRadioPower = 0;
  }
  RRadioPowerBtn.init(COLUMN_1, 147, 95, 40, C_WHITE, C_DKGREY, C_WHITE, C_BLACK, HighPowerText[RRadioPower], 0, 0, FONT_ITEM);
  if (WRadioPower >= ((sizeof(HighPowerText) / sizeof(HighPowerText[0])))) {
    WRadioPower = 0;
  }
  WRadioPowerBtn.init(COLUMN_2, 147, 95, 40, C_WHITE, C_DKGREY, C_WHITE, C_BLACK, HighPowerText[WRadioPower], 0, 0, FONT_ITEM);
  if (BRadioPower >= ((sizeof(HighPowerText) / sizeof(HighPowerText[0])))) {
    BRadioPower = 0;
  }
  BRadioPowerBtn.init(COLUMN_3, 147, 95, 40, C_WHITE, C_DKGREY, C_WHITE, C_BLACK, HighPowerText[BRadioPower], 0, 0, FONT_ITEM);

  RResetBtn.init(COLUMN_1, 207, 95, 40, C_WHITE, C_RED, C_WHITE, C_BLACK, "Reset", 0, 0, FONT_ITEM);
  WResetBtn.init(COLUMN_2, 207, 95, 40, C_WHITE, C_RED, C_WHITE, C_BLACK, "Reset", 0, 0, FONT_ITEM);
  BResetBtn.init(COLUMN_3, 207, 95, 40, C_WHITE, C_RED, C_WHITE, C_BLACK, "Reset", 0, 0, FONT_ITEM);

  NumberInput.init(C_BLACK, C_WHITE, C_BLUE, C_WHITE, C_DKBLUE, &FONT_ITEM);
  NumberInput.setMinMax(0.0, 80.0);
  NumberInput.enableDecimal(false);
  NumberInput.enableNegative(false);
  NumberInput.setTouchLimits(ScreenRight, ScreenLeft, ScreenBottom, ScreenTop);
}

bool StartRadio_0() {
  ///////////////////////////////////////////////
  // start radio #0

#ifdef SHOW_DEBUG
  Serial.println("Trans 0");
  EBYTE_0.printParameters();
#endif
  DataPacket_0.begin(details(Data_0), &Serial_0);
  if (EBYTE_0.init()) {
    RChannel = EBYTE_0.getChannel();
    RDataRate = EBYTE_0.getAirDataRate();
    return true;
  }
  return false;
}
bool StartRadio_1() {
  ///////////////////////////////////////////////
  // start radio #1

#ifdef SHOW_DEBUG
  Serial.println("Trans 1");
  EBYTE_0.printParameters();
#endif
  DataPacket_1.begin(details(Data_1), &Serial_1);
  if (EBYTE_1.init()) {
    WChannel = EBYTE_1.getChannel();
    WDataRate = EBYTE_1.getAirDataRate();

    return true;
  }
  return false;
}

bool StartRadio_2() {
  ///////////////////////////////////////////////
  // start radio #2

#ifdef SHOW_DEBUG
  Serial.println("Trans 2");
  EBYTE_0.printParameters();
#endif
  DataPacket_2.begin(details(Data_2), &Serial_2);
  if (EBYTE_2.init()) {
    //EBYTE_0.setPacketSize(SUB_64BYTES);
    BChannel = EBYTE_2.getChannel();
    BDataRate = EBYTE_2.getAirDataRate();

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
  // red
  EEPROM.get(2, EncryptLow);
  EEPROM.get(3, EncryptHigh);
  REncryptKey = (EncryptHigh << 8) | EncryptLow;
  // white
  EEPROM.get(4, EncryptLow);
  EEPROM.get(5, EncryptHigh);
  WEncryptKey = (EncryptHigh << 8) | EncryptLow;
  // blue
  EEPROM.get(6, EncryptLow);
  EEPROM.get(7, EncryptHigh);
  BEncryptKey = (EncryptHigh << 8) | EncryptLow;

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
