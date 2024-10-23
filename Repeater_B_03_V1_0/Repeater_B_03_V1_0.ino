/*

  Program name: Bob Jones Patriot Racing transmitter repeater
  Copyright (c) 2018, all rights reserved
  this code is licensed to Bob Jones Only

  ///////////////////////////////////////////////////
  code for an arduino nano
  ///////////////////////////////////////////////////



  Purpose measure:
  1. recieve transmitted data from the car
  2. send data back out
  3. change the ID so pit station knows where the data came from


  Revision table
  rev   author      date        description
  1.0   kris        5-20-18     initial code
  2.0   Kyle        6-10-18     Added Display Support, Changed Pin Numbers, Added Set Menu.
  2.1   Kyle        6-24-18     Changed to New Menu, added button support, changed code to set transiever
  2.7   Kris        5-29-20     updated to new struct, added EBYTE.h, added more display colors
  2.8   Kris        5-29-20     updated reset ebyte code
  3.0   Kris        11-23-20    recompiled with updated driver list
  3.1   Kris        11-23-20    switched to easytransfer to handle struct packing issues
  
  B03Vv1.0   Kris   10-09-23    redesign for 3 transceivers
  B03Vv1.0   Kris   10-23-24    updated few new struct and numberpad lib and lastest compiler


*/

#include <SPI.h>
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"

#include <EasyTransfer.h>
#include <XPT2046_Touchscreen.h>
#include <SoftwareSerial.h>
#include <elapsedMillis.h>
#include <PatriotRacing_Utilities.h>

#include "Adafruit_fonts.h"
#include "EBYTE.h"
#include "FlickerFreePrint.h"
#include <Adafruit_ILI9341_Controls.h>  // custom control define file
#include <Adafruit_ILI9341_Keypad.h>


#define VERSION "Signal repeater 1.21"

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

bool RadioFound = false;

char buf[60];

uint8_t RChannel = 0, WChannel = 0, BChannel = 0;
uint8_t RDataRate = 0, WDataRate = 0, BDataRate = 0;
uint8_t RRadioPower = 0, WRadioPower = 0, BRadioPower = 0;
uint8_t h, m, s, b, i;
int BtnX, BtnY, BtnZ;
uint16_t ScreenLeft = 0, ScreenRight = 3500, ScreenTop = 700, ScreenBottom = 3600;

#define Serial_0 Serial2
SoftwareSerial Serial_1(33, 32);
SoftwareSerial Serial_2(21, 22);

EBYTE Trans_0(&Serial_0, 13, 13, 14);
EBYTE Trans_1(&Serial_1, 26, 26, 25);
EBYTE Trans_2(&Serial_2, 4, 4, 34);

Transceiver Data_0;
Transceiver Data_1;
Transceiver Data_2;

EasyTransfer DataPacket_0;
EasyTransfer DataPacket_1;
EasyTransfer DataPacket_2;

Adafruit_ILI9341 Display = Adafruit_ILI9341(27, 12, 5);

XPT2046_Touchscreen Touch(2, 15);
TS_Point TP;

Button SetupBtn(&Display);
Button DoneBtn(&Display);
Button BChannelBtn(&Display);
Button RChannelBtn(&Display);
Button WChannelBtn(&Display);

Button RDataRateBtn(&Display);
Button WDataRateBtn(&Display);
Button BDataRateBtn(&Display);

Button RRadioPowerBtn(&Display);
Button WRadioPowerBtn(&Display);
Button BRadioPowerBtn(&Display);

Button RResetBtn(&Display);
Button WResetBtn(&Display);
Button BResetBtn(&Display);

NumberPad NumberInput(&Display, &Touch);

NumberPad Password(&Display, &Touch);
elapsedMillis RTimer = 0;
elapsedMillis WTimer = 0;
elapsedMillis BTimer = 0;

void setup() {

  Serial.begin(115200);

  Serial.println("Starting signal repeater");

  SPI.begin();
  Display.begin();
  Display.setRotation(3);
  Display.fillScreen(C_BLACK);
  delay(100);

  // fire up the touch display
  Touch.begin();
  Touch.setRotation(3);

  Display.fillRect(10, 40, 300, 20, C_RED);
  Display.fillRect(10, 160, 300, 20, C_BLUE);

  Display.setTextColor(C_WHITE);
  Display.setFont(&FONT_HEADER);
  Display.setCursor(20, 110);
  Display.print(F("PATRIOT RACING"));
  Display.setFont(&FONT_ITEM);
  Display.setCursor(20, 140);
  Display.print(F(VERSION));

  Display.setTextColor(C_BLACK);

  ///////////////////////////////////////////////
  // start radio #0
  //Serial.println("Starting Transceiver 0");

  RadioFound = false;
  Display.fillRoundRect(20, 200, 280, 30, 4, C_DKGREY);
  Display.fillRoundRect(20, 200, 25, 30, 4, C_GREY);
  Display.setCursor(30, 220);
  Display.setTextColor(C_BLACK);
  Display.print(F("Starting Transceiver 0"));
  delay(100);
  //Serial_0.begin(9600);
    Serial_0.begin(9600, SERIAL_8N1, 16, 17);


  delay(100);
  for (i = 0; i < 3; i++) {
    RadioFound = Trans_0.init();

    if (RadioFound) {
      break;
    }
    delay(1000);
  }

  if (RadioFound) {
#ifdef DEBUG_ON
    Trans_0.PrintParameters();
#endif

    RChannel = Trans_0.GetChannel();
    RDataRate = Trans_0.GetAirDataRate();
    RRadioPower = Trans_0.GetTransmitPower();
    DataPacket_0.begin(details(Data_0), &Serial_0);
    Display.fillRoundRect(20, 200, 280, 30, 4, C_DKGREY);
    Display.fillRoundRect(20, 200, 50, 30, 4, C_GREY);
    Display.setCursor(30, 220);
    Display.print(F("Transceiver 0 SUCCESS"));
  } else {
    Display.fillRoundRect(20, 200, 280, 30, 4, C_DKGREY);
    Display.fillRoundRect(20, 200, 50, 30, 4, C_GREY);
    Display.setCursor(30, 220);
    Display.print(F("Transceiver 0 FAIL"));
  }


  //Serial.println("Starting Transceiver 1");
  ///////////////////////////////////////////////
  // start radio #1
  RadioFound = false;
  Display.fillRoundRect(20, 200, 280, 30, 4, C_DKGREY);
  Display.fillRoundRect(20, 200, 75, 30, 4, C_GREY);
  Display.setCursor(30, 220);
  Display.print(F("Starting Transceiver 1"));
  delay(100);
  Serial_1.begin(9600);
  delay(100);
  for (i = 0; i < 3; i++) {
    RadioFound = Trans_1.init();

    if (RadioFound) {
      break;
    }
    delay(1000);
  }
  if (RadioFound) {
#ifdef DEBUG_ON
    Trans_1.PrintParameters();
#endif
    WChannel = Trans_1.GetChannel();
    WDataRate = Trans_1.GetAirDataRate();
    WRadioPower = Trans_1.GetTransmitPower();
    DataPacket_1.begin(details(Data_1), &Serial_1);
    Display.fillRoundRect(20, 200, 280, 30, 4, C_DKGREY);
    Display.fillRoundRect(20, 200, 100, 30, 4, C_GREY);
    Display.setCursor(30, 220);
    Display.print(F("Transceiver 1 SUCCESS"));
  } else {
    Display.fillRoundRect(20, 200, 280, 30, 4, C_DKGREY);
    Display.fillRoundRect(20, 200, 100, 30, 4, C_GREY);
    Display.setCursor(30, 220);
    Display.print(F("Transceiver 1 FAIL"));
  }

  //Serial.println("Starting Transceiver 2");
  ///////////////////////////////////////////////
  // start radio #2
  RadioFound = false;
  Display.fillRoundRect(20, 200, 280, 30, 4, C_DKGREY);
  Display.fillRoundRect(20, 200, 125, 30, 4, C_GREY);
  Display.setCursor(30, 220);
  Display.print(F("Starting Transceiver 2"));
  delay(100);
  Serial_2.begin(9600);
  delay(100);
  for (i = 0; i < 3; i++) {
    RadioFound = Trans_2.init();

    if (RadioFound) {
      break;
    }
    delay(1000);
  }

  if (RadioFound) {
#ifdef DEBUG_ON
    Trans_2.PrintParameters();
#endif
    BChannel = Trans_2.GetChannel();
    BDataRate = Trans_2.GetAirDataRate();
    BRadioPower = Trans_2.GetTransmitPower();
    DataPacket_2.begin(details(Data_2), &Serial_2);
    Display.fillRoundRect(20, 200, 280, 30, 4, C_DKGREY);
    Display.fillRoundRect(20, 200, 150, 30, 4, C_GREY);
    Display.setCursor(30, 220);
    Display.print(F("Transceiver 2 SUCCESS"));
  } else {
    Display.fillRoundRect(20, 200, 280, 30, 4, C_DKGREY);
    Display.fillRoundRect(20, 200, 150, 30, 4, C_GREY);
    Display.setCursor(30, 220);
    Display.print(F("Transceiver 2 FAIL"));
  }

  Display.fillRoundRect(20, 200, 280, 30, 4, C_DKGREY);
  Display.fillRoundRect(20, 200, 200, 30, 4, C_GREY);
  Display.setCursor(30, 220);
  Display.print(F("Building interface"));

  SetupBtn.init(260, 20, 100, 35, C_WHITE, C_DKGREY, C_WHITE, C_BLACK, "Setup", 0, 0, &FONT_ITEM);
  DoneBtn.init(270, 20, 90, 35, C_WHITE, C_DKGREY, C_WHITE, C_BLACK, "OK", 0, 0, &FONT_ITEM);

  sprintf(buf, "%d", RChannel);
  RChannelBtn.init(COLUMN_1, 67, 95, 40, C_GREY, C_RED, C_WHITE, C_BLACK, buf, 0, 0, &FONT_ITEM);
  sprintf(buf, "%d", WChannel);
  WChannelBtn.init(COLUMN_2, 67, 95, 40, C_GREY, C_WHITE, C_BLACK, C_BLACK, buf, 0, 0, &FONT_ITEM);
  sprintf(buf, "%d", BChannel);
  BChannelBtn.init(COLUMN_3, 67, 95, 40, C_GREY, C_BLUE, C_WHITE, C_BLACK, buf, 0, 0, &FONT_ITEM);

  // air data rate
  if (RDataRate >= ((sizeof(AirRateText) / sizeof(AirRateText[0])))) {
    RDataRate = 0;
  }
  RDataRateBtn.init(COLUMN_1, 107, 95, 40, C_WHITE, C_DKGREY, C_WHITE, C_BLACK, AirRateText[RDataRate], 0, 0, &FONT_ITEM);
  if (WDataRate >= ((sizeof(AirRateText) / sizeof(AirRateText[0])))) {
    WDataRate = 0;
  }
  WDataRateBtn.init(COLUMN_2, 107, 95, 40, C_WHITE, C_DKGREY, C_WHITE, C_BLACK, AirRateText[WDataRate], 0, 0, &FONT_ITEM);
  if (BDataRate >= ((sizeof(AirRateText) / sizeof(AirRateText[0])))) {
    BDataRate = 0;
  }
  BDataRateBtn.init(COLUMN_3, 107, 95, 40, C_WHITE, C_DKGREY, C_WHITE, C_BLACK, AirRateText[BDataRate], 0, 0, &FONT_ITEM);

  // radio power
  if (RRadioPower >= ((sizeof(HighPowerText) / sizeof(HighPowerText[0])))) {
    RRadioPower = 0;
  }
  RRadioPowerBtn.init(COLUMN_1, 147, 95, 40, C_WHITE, C_DKGREY, C_WHITE, C_BLACK, HighPowerText[RRadioPower], 0, 0, &FONT_ITEM);
  if (WRadioPower >= ((sizeof(HighPowerText) / sizeof(HighPowerText[0])))) {
    WRadioPower = 0;
  }
  WRadioPowerBtn.init(COLUMN_2, 147, 95, 40, C_WHITE, C_DKGREY, C_WHITE, C_BLACK, HighPowerText[WRadioPower], 0, 0, &FONT_ITEM);
  if (BRadioPower >= ((sizeof(HighPowerText) / sizeof(HighPowerText[0])))) {
    BRadioPower = 0;
  }
  BRadioPowerBtn.init(COLUMN_3, 147, 95, 40, C_WHITE, C_DKGREY, C_WHITE, C_BLACK, HighPowerText[BRadioPower], 0, 0, &FONT_ITEM);

  RResetBtn.init(COLUMN_1, 207, 95, 40, C_WHITE, C_RED, C_WHITE, C_BLACK, "Reset", 0, 0, &FONT_ITEM);
  WResetBtn.init(COLUMN_2, 207, 95, 40, C_WHITE, C_RED, C_WHITE, C_BLACK, "Reset", 0, 0, &FONT_ITEM);
  BResetBtn.init(COLUMN_3, 207, 95, 40, C_WHITE, C_RED, C_WHITE, C_BLACK, "Reset", 0, 0, &FONT_ITEM);

  NumberInput.init(C_BLACK, C_WHITE, C_BLUE, C_WHITE, C_DKBLUE, &FONT_ITEM);
  NumberInput.setMinMax(0.0, 69.0);
  NumberInput.enableDecimal(false);
  NumberInput.enableNegative(false);
  NumberInput.setTouchLimits(ScreenLeft, ScreenRight, ScreenTop, ScreenBottom);


  Password.init(C_GREY, C_WHITE, C_BLUE, C_WHITE, C_DKBLUE, &FONT_ITEM);
  Password.setTouchLimits(ScreenLeft, ScreenRight, ScreenTop, ScreenBottom);
  Password.enableDecimal(false);
  Password.enableNegative(false);
  Password.setMinMax(0.0, 9999.0);
  Password.hideInput();
  Password.setInitialText("Password");

  delay(500);
  Display.fillRoundRect(20, 200, 280, 30, 4, C_DKGREY);
  Display.fillRoundRect(20, 200, 280, 30, 4, C_GREY);
  Display.setCursor(30, 220);
  Display.print(F("Done..."));

  delay(500);

  Display.fillScreen(C_BLACK);

  DisplayHeader();

#ifdef DEBUG_ON
  Serial.print(F("Here we go..."));
#endif
}

/*

*/

void loop() {

  ProcessLoopTouch();

  Display.setFont(&FONT_ITEM);

  /////////////////////////////////////////////////////////////////////////
  // RED car
  if (DataPacket_0.receiveData()) {
    // found
    Display.fillCircle(COLUMN_1 - 40, ROW_1, 9, C_DKGREY);
    Display.fillCircle(COLUMN_1 - 40, ROW_2, 9, C_GREEN);
    Display.fillCircle(COLUMN_1 - 40, ROW_3, 9, C_DKGREY);
    ProcessLoopTouch();
    DataPacket_0.sendData();
    // sent
    Display.fillCircle(COLUMN_1 - 40, ROW_1, 9, C_DKGREY);
    Display.fillCircle(COLUMN_1 - 40, ROW_2, 9, C_DKGREY);
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
    DataPacket_1.sendData();
    Display.fillCircle(COLUMN_2 - 40, ROW_1, 9, C_DKGREY);
    Display.fillCircle(COLUMN_2 - 40, ROW_2, 9, C_DKGREY);
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
    DataPacket_2.sendData();
    Display.fillCircle(COLUMN_3 - 40, ROW_1, 9, C_DKGREY);
    Display.fillCircle(COLUMN_3 - 40, ROW_2, 9, C_DKGREY);
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

void DisplayHeader() {

  Display.fillRect(0, 0, 319, 40, C_WHITE);
  Display.setTextColor(C_BLACK, C_WHITE);
  Display.setFont(&FONT_HEADER);
  Display.setCursor(10, 30);
  Display.print(F("Patriot Racing"));

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

  ProcessTouch();

  if (PressIt(SetupBtn) == true) {
    Password.getInput();
    if ((Password.value) == 1515.0) {  // key pad returns float...
      Display.fillScreen(C_BLACK);
      SetupScreen();
      Display.fillScreen(C_BLACK);
      DisplayHeader();
    }
    Display.fillScreen(C_BLACK);
    DisplayHeader();
    return;
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
    BtnX = map(BtnX, ScreenLeft, ScreenRight, 320, 0);
    BtnY = map(BtnY, ScreenTop, ScreenBottom, 240, 0);

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

    if (PressIt(RChannelBtn) == true) {

      NumberInput.value = RChannel;
      NumberInput.getInput();
      RChannel = (uint8_t)NumberInput.value;
      RDirty = true;
      sprintf(buf, "%d", RChannel);
      RChannelBtn.setText(buf);
      DrawSetupScreen();
    }
    if (PressIt(WChannelBtn) == true) {

      NumberInput.value = WChannel;
      NumberInput.getInput();
      WChannel = (uint8_t)NumberInput.value;
      WDirty = true;
      sprintf(buf, "%d", WChannel);
      WChannelBtn.setText(buf);
      DrawSetupScreen();
    }
    if (PressIt(BChannelBtn) == true) {

      NumberInput.value = BChannel;
      NumberInput.getInput();
      BChannel = (uint8_t)NumberInput.value;
      BDirty = true;
      sprintf(buf, "%d", BChannel);
      BChannelBtn.setText(buf);
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
      Trans_0.SetAddressH(0);
      Trans_0.SetAddressL(0);
      Trans_0.SetSpeed(0b00011100);
      Trans_0.SetOptions(0b01000100);
      Trans_0.SetChannel(5);
      delay(50);
      Trans_0.SaveParameters(PERMANENT);
      delay(50);
#ifdef DEBUG_ON
      Trans_0.PrintParameters();
#endif
    }

    if (PressIt(WResetBtn) == true) {
      Trans_1.SetAddressH(0);
      Trans_1.SetAddressL(0);
      Trans_1.SetSpeed(0b00011100);
      Trans_1.SetOptions(0b01000100);
      Trans_1.SetChannel(15);
      delay(50);
      Trans_1.SaveParameters(PERMANENT);
      delay(50);
#ifdef DEBUG_ON
      Trans_1.PrintParameters();
#endif
    }
    if (PressIt(BResetBtn) == true) {
      Trans_2.SetAddressH(0);
      Trans_2.SetAddressL(0);
      Trans_2.SetSpeed(0b00011100);
      Trans_2.SetOptions(0b01000100);
      Trans_2.SetChannel(1);
      Trans_2.SaveParameters(PERMANENT);
      delay(50);
#ifdef DEBUG_ON
      Trans_2.PrintParameters();
#endif
    }
  }

  delay(50);

  if (RDirty) {
    Trans_0.SetChannel(RChannel);
    Trans_0.SetAirDataRate(RDataRate);
    Trans_0.SetTransmitPower(RRadioPower);
    delay(50);
    Trans_0.SaveParameters(PERMANENT);
    delay(50);
#ifdef DEBUG_ON
    delay(1000);
    Trans_2.PrintParameters();
#endif
  }
  if (WDirty) {
    Serial.println(WDataRate);
    Trans_1.SetChannel(WChannel);
    Trans_1.SetAirDataRate(WDataRate);
    Trans_1.SetTransmitPower(WRadioPower);
    delay(50);
    Trans_1.SaveParameters(PERMANENT);
    delay(50);
#ifdef DEBUG_ON
    delay(1000);
    Trans_1.PrintParameters();
#endif
  }
  if (BDirty) {
    Trans_2.SetChannel(BChannel);
    Trans_2.SetAirDataRate(BDataRate);
    Trans_2.SetTransmitPower(BRadioPower);
    delay(50);
    Trans_2.SaveParameters(PERMANENT);
    delay(50);
#ifdef DEBUG_ON
    delay(1000);
    Trans_2.PrintParameters();
#endif
  }
}

void DrawSetupScreen() {

  Display.fillScreen(C_BLACK);
  //Draw Header Text

  Display.fillRect(0, 0, 319, 40, C_WHITE);
  Display.setTextColor(C_BLACK, C_WHITE);
  Display.setFont(&FONT_HEADER);
  Display.setCursor(10, 30);
  Display.print(F("Setup"));

  DoneBtn.draw();

  RChannelBtn.draw();
  WChannelBtn.draw();
  BChannelBtn.draw();

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



// end of code
