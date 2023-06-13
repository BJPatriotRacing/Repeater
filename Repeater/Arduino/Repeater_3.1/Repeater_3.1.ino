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

  connectivity map
  ________________

  Teensy 3.2   device
  A0
  A1
  A2
  A3
  A4        SDA on 0.96 OLED display
  A5        SCL on 0.96 OLED display
  A6
  0
  1
  2       BLUE led
  3       GREEN led
  4       RED led
  5       LO button (INPUT_PULLUP)
  6       ST button (INPUT_PULLUP)
  7       UP button (INPUT_PULLUP)
  8       RX on the E44
  9       M1 on the E44
  10      M0 on the E44
  11      TX on the E44
  12      AUX the E44
  13


  libraries
  OLED https://github.com/greiman/SSD1306Ascii

*/

//Include Libs
#include "PatriotRacing_Utilities.h"    // custom utilities definition
#include "SoftwareSerial.h"
#include <SSD1306AsciiAvrI2c.h>
#include <EEPROM.h>
#include "EBYTE.h"
#include <EasyTransfer.h>                // needed to ensure wireless data is sent correctly (MCU's can pack structs differently)

#define FONT Arial_bold_14

  #define debug

//Define Pins
#define PIN_BLUE  2
#define PIN_GREEN 3
#define PIN_RED   4
#define PIN_DN    5
#define PIN_ST    6
#define PIN_UP    7
#define PIN_M0    10
#define PIN_M1    9
#define PIN_RX    11
#define PIN_TX    8
#define PIN_AUX   12

bool KeepIN = true;
bool EditMode = true;
bool TransOK = false;
int Channel = 0;
int RadioPower = 0;
int AirDataRate = 0;
int PitDisplayColorID = 0;
byte DataSource = 0;
byte Row = 0;
unsigned long CurTime, PreTime;
int i = 0;
uint16_t PacketSize;
byte DataID;
byte b;

SoftwareSerial ESerial(PIN_RX, PIN_TX);

EBYTE Trans(&ESerial, PIN_M0, PIN_M1, PIN_AUX);

EasyTransfer DataPacket;

//create the structure for transceiver transmissions
Transceiver Data;

SSD1306AsciiAvrI2c Display;

void setup() {

  //Pin Modes
  pinMode(PIN_BLUE, OUTPUT);
  pinMode(PIN_GREEN, OUTPUT);
  pinMode(PIN_RED, OUTPUT);
  pinMode(PIN_DN, INPUT_PULLUP);
  pinMode(PIN_ST, INPUT_PULLUP);
  pinMode(PIN_UP, INPUT_PULLUP);

  delay(50);
  Serial.begin(9600);
  Serial.println(F("Starting"));
#ifdef debug

#endif

  ESerial.begin(9600);


  // some cute boot up indicator
  digitalWrite(PIN_BLUE, HIGH);
  delay(100);
  digitalWrite(PIN_BLUE, LOW);
  delay(100);
  digitalWrite(PIN_GREEN, HIGH);
  delay(100);
  digitalWrite(PIN_GREEN, LOW);
  delay(100);
  digitalWrite(PIN_RED, HIGH);
  delay(100);
  digitalWrite(PIN_RED, LOW);

  // fire up display
  Display.begin(&Adafruit128x64, 0x3c);

  //Splash Screen
  Display.setFont(FONT);
  Display.clear();
  Display.set1X();

  Display.setCursor(4, 0);
  Display.print(F("Patriot"));
  Display.setCursor(4, 2);
  Display.print(F("Racing"));
  Display.setCursor(4, 4);
  Display.print(F("Relay Station v3.1"));

  delay(100);
  Display.clear();

  if ((digitalRead(PIN_ST) == LOW)  || (digitalRead(PIN_UP) == LOW) || (digitalRead(PIN_DN) == LOW)) {
    Display.clear();
    Display.setCursor(4, 0);
    Display.print(F("Resetting"));
    Display.setCursor(4, 2);
    Display.print(F("EBYTE"));
    RestoreEBYTEDefaults();
    Display.clear();
  }

  Display.setCursor(4, 0);
  Display.print(F("Starting..."));

  Display.setCursor(4, 2);
  Display.print(F("Read Params"));

  GetParameters();

#ifdef debug
  Serial.println(F("Trans.init()"));
#endif

  Display.setCursor(4, 4);
  Display.print(F("Init E44"));
  // iniialize the E44-TTL-100
  delay(50);
  TransOK = Trans.init();
  Display.setCursor(4, 6);

  if (TransOK == true) {
    Display.print(F("Trns: OK"));
#ifdef debug
    Serial.println(F("Trans OK"));
#endif
  }
  else {
    Display.print(F("Trns: FAIL"));
#ifdef debug
    Serial.println(F("Trans FAIL"));
#endif

  }


#ifdef debug
  Serial.println(F("TRANSCEIVER PARAMETERS"));
  Trans.PrintParameters();
#endif

  delay(10);

  Channel = Trans.GetChannel();
  RadioPower = Trans.GetTransmitPower();
  AirDataRate = Trans.GetAirDataRate();

#ifdef debug
  Serial.print("Channel ");
  Serial.println(Channel);
  Serial.print("RadioPower ");
  Serial.println(HighPowerText[RadioPower]);
  Serial.print("AirDataRate ");
  Serial.println(AirRateText[AirDataRate]);
#endif

  DataPacket.begin(details(Data), &ESerial);

  delay(100);

  Display.clear();

  PacketSize = sizeof(Data);

  Display.setFont(FONT);

  // ClearRadio();

}

void loop() {

  CurTime = millis();

  if (DataPacket.receiveData()) {

    PreTime = CurTime;

    digitalWrite(PIN_BLUE, LOW);
    digitalWrite(PIN_RED, LOW);
    digitalWrite(PIN_GREEN, HIGH);

    // use a longer delay for shorter transmit speeds
    // otherwise buffer gets corrupt

    delay(10);

    DataID = Data.ID_RPM >> 12;
    DataSource = Data.LAPSPEED_SOURCE & 0b1111;

    // test to make sure data came from car and not another repeater
    if (DataSource == 15) {

      Display.clear();

      Display.setCursor(5, 0);
      Display.print(F("Car - "));
      Display.print(CarText[DataID]);
      delay(50);

#ifdef debug
      //     Serial.println(F("Found signal"));
#endif

      digitalWrite(PIN_BLUE, HIGH);
      digitalWrite(PIN_GREEN, LOW);

      Display.setCursor(5, 2);
      Display.clearToEOL();
      Display.setCursor(5, 2);
      Display.print(F("Reading"));

#ifdef debug
      //   Serial.println(F("Sending data"));
#endif

      Data.LAPSPEED_SOURCE = (Data.LAPSPEED_SOURCE & 0b1111111111110000 ) | (0b1111 & PitDisplayColorID); // car is data source 0

#ifdef debug
      //   Serial.println(F("got source"));
#endif

      DataPacket.sendData();

      Display.setCursor(5, 4);
      Display.clearToEOL();
      Display.setCursor(5, 4);
      Display.print(F("Sending"));
      delay(10);

      digitalWrite(PIN_BLUE, LOW);
      digitalWrite(PIN_BLUE, LOW);
      digitalWrite(PIN_GREEN, LOW);

      Display.setCursor(5, 6);
      Display.clearToEOL();
      Display.setCursor(5, 6);
      Display.print(F("Waiting"));

    }

    if ((digitalRead(PIN_UP) == LOW) & (digitalRead(PIN_DN) == LOW)) {
      Display.clear();

      ShowMenu();
      ClearRadio();
    }


  }

  else   {

    if ((CurTime - PreTime) > 4000) {
      Display.clear();
      digitalWrite(PIN_RED, HIGH);
      Display.setCursor(5, 0);
      Display.print(F("Signal..."));
      Display.setCursor(5, 2);
      Display.print(F("Searching"));
      delay(200);

      digitalWrite(PIN_RED, LOW);
      Display.setCursor(5, 0);
      Display.print(F("Signal..."));
      Display.setCursor(5, 2);
      Display.clearToEOL();
      delay(200);

    }
#ifdef debug
    // Serial.println(F("Searching"));
#endif
  }

  if ((digitalRead(PIN_UP) == LOW) & (digitalRead(PIN_DN) == LOW)) {
    Display.clear();
    ShowMenu();
    ClearRadio();
  }


}

void DisplayMenu() {

  Display.clear();

  Display.setFont(FONT);

  if (Row < 8) {
    Display.setCursor(10, 0);
    Display.print(F("Exit"));
    Display.setCursor(10, 2);
    Display.print(F("Channel"));
    Display.setCursor(10, 4);
    Display.print(F("Power"));
    Display.setCursor(10, 6);
    Display.print(F("Rate"));
    Display.setCursor(80, 2);
    Display.print(Channel);
    Display.setCursor(80, 4);
    Display.print(HighPowerText[RadioPower]);
    Display.setCursor(80, 6);
    Display.print(AirRateText[AirDataRate]);

  }
  else if (Row == 8) {
    //Row = 6;
    Display.setCursor(10, 0);
    Display.print(F("Exit"));
    Display.setCursor(10, 2);
    Display.print(F("Power"));
    Display.setCursor(10, 4);
    Display.print(F("Rate"));
    Display.setCursor(10, 6);
    Display.print(F("Color"));
    Display.setCursor(80, 2);
    Display.print(HighPowerText[RadioPower]);
    Display.setCursor(80, 4);
    Display.print(AirRateText[AirDataRate]);
    Display.setCursor(80, 6);

#ifdef debug
    Serial.println(PitDisplayColorID);
    Serial.println(DisplayColor[PitDisplayColorID]);
#endif
    Display.print(DisplayColor[PitDisplayColorID]);
  }

  Display.setCursor(0, 0); Display.print(F("  ")); // exit
  Display.setCursor(0, 2); Display.print(F("  ")); // channel
  Display.setCursor(0, 4); Display.print(F("  ")); // power
  Display.setCursor(0, 6); Display.print(F("  ")); // rate
  Display.setCursor(0, Row); Display.print(F(">"));


}

void ShowMenu() {

  bool Dirty = false;

  KeepIN = true;
  Row = 0;

  ESerial.flush();

  DisplayMenu();
  Display.setCursor(0, Row);
  Display.print(F(">"));



  while (digitalRead(PIN_ST) == LOW) {
    delay(10);
  }

  while (((digitalRead(PIN_UP) == LOW) | (digitalRead(PIN_DN) == LOW))) {}

  while (KeepIN) {

    if (digitalRead(PIN_UP) == LOW) {
      if (Row == 0) {
        Row = 8;
      }
      else {
        Row -= 2;
      }

      delay(200);

      DisplayMenu();
    }

    if (digitalRead(PIN_DN) == LOW) {

      if (Row == 8) {
        Row = 0;
      }
      else {
        Row += 2;
      }

      DisplayMenu();
      delay(200);
    }

    if (digitalRead(PIN_ST) == LOW) {

      if (Row == 0) {
        KeepIN = false;

#ifdef debug
        Serial.print("CurrentChannel "); Serial.println(Channel);
        Serial.print("RadioPower "); Serial.println(RadioPower);
        Serial.print("TransmitSpeed "); Serial.println(AirDataRate);
#endif

        Display.clear();

      }
      else if (Row == 2) {
        EditMode = true;
        Dirty = true;
        while (digitalRead(PIN_ST) == LOW) {
        }
        while (EditMode) {

          Display.setCursor(0, Row); Display.print(F("-"));
          if (digitalRead(PIN_DN) == LOW)  {

            if (Channel <= 0) {
              Channel = 31;
            }
            else {
              Channel--;
            }
            DisplayMenu();
            //Display.setCursor(80, 2); Display.clearToEOL(); Display.print(Channel);
            delay(200);
          }
          else if (digitalRead(PIN_ST) == LOW) {
            Display.setCursor(0, Row); Display.print(F(">"));
            DisplayMenu();
            //Display.setCursor(80, 2); Display.clearToEOL(); Display.print(Channel);
            EditMode = false;
            delay(200);
          }
          if (digitalRead(PIN_UP) == LOW) {
            if (Channel > 31) {
              Channel = 0;
            }
            else {
              Channel++;
            }
            DisplayMenu();
            //Display.setCursor(80, 2); Display.clearToEOL(); Display.print(Channel);
            delay(200);
          }
        }
      }

      else if (Row == 4) {
        EditMode = true;
        Dirty = true;
        while (digitalRead(PIN_ST) == LOW) {

        }
        while (EditMode) {

          Display.setCursor(0, Row); Display.print(F("-"));
          if (digitalRead(PIN_DN) == LOW)  {
            if (RadioPower <= 0) {
              RadioPower = 3;
            }
            else {
              RadioPower--;
            }
            DisplayMenu();
            //Display.setCursor(80, 4); Display.clearToEOL(); Display.print(PowerText[RadioPower]);
            delay(200);
          }
          else if (digitalRead(PIN_ST) == LOW) {
            Display.setCursor(0, Row); Display.print(F(">"));
            EditMode = false;
            delay(200);
          }
          if (digitalRead(PIN_UP) == LOW)  {
            RadioPower++;
            if (RadioPower > 3) {
              RadioPower = 0;
            }
            DisplayMenu();
            //Display.setCursor(80, 4); Display.clearToEOL(); Display.print(PowerText[RadioPower]);
            delay(200);
          }
        }
      }
      else if (Row == 6) {
        EditMode = true;
        Dirty = true;
        while (digitalRead(PIN_ST) == LOW) {
        }
        while (EditMode) {

          Display.setCursor(0, Row); Display.print(F("-"));
          if (digitalRead(PIN_DN) == LOW) {

            if (AirDataRate <= 0) {
              AirDataRate = 5;
            }
            else {
              AirDataRate--;
            }
            DisplayMenu();
            //Display.setCursor(80, 6); Display.clearToEOL(); Display.print(RateText[AirDataRate]);
            delay(200);
          }
          else if (digitalRead(PIN_ST) == LOW) {
            Display.setCursor(0, Row); Display.print(F(">"));
            EditMode = false;

            delay(200);
          }
          if (digitalRead(PIN_UP) == LOW)  {
            if (AirDataRate > 5) {
              AirDataRate = 0;
            }
            else {
              AirDataRate++;
            }
            DisplayMenu();
            //Display.setCursor(80, 6); Display.clearToEOL(); Display.print(RateText[AirDataRate]);
            delay(200);
          }
        }
      }
      else if (Row == 8) {
        EditMode = true;
        while (digitalRead(PIN_ST) == LOW) {
        }
        while (EditMode) {
          Display.setCursor(0, 6); Display.print(F("-"));

          if (digitalRead(PIN_DN) == LOW) {

            if (PitDisplayColorID <= 0) {
              PitDisplayColorID = 7;
            }
            else {
              PitDisplayColorID--;
            }
#ifdef debug
            Serial.println(PitDisplayColorID);
            Serial.println(DisplayColor[PitDisplayColorID]);
#endif
            DisplayMenu();

            delay(200);
          }
          else if (digitalRead(PIN_ST) == LOW) {
            Display.setCursor(0, 6); Display.print(F(">"));
            EditMode = false;

            delay(200);
          }
          if (digitalRead(PIN_UP) == LOW)  {



            PitDisplayColorID++;
#ifdef debug
            Serial.println(PitDisplayColorID);
            Serial.println(DisplayColor[PitDisplayColorID]);
#endif
            if (PitDisplayColorID > 7) {
              PitDisplayColorID = 0;
            }
            DisplayMenu();

            delay(200);
          }
        }
      }
      delay(500);
    }
  }


  EEPROM.put(40, PitDisplayColorID);


#ifdef debug
  Serial.println(F("Programming Transceiver..."));
#endif

  if (Dirty) {
    Display.clear();
    Display.setCursor(5, 0);
    Display.print(F("Resetting"));
    Display.setCursor(5, 2);
    Display.print(F("Wait"));
    Trans.SetChannel(Channel);
    Trans.SetAirDataRate(AirDataRate);
    Trans.SetTransmitPower(RadioPower);
    delay(100);
    Trans.SaveParameters(PERMANENT);
    delay(100);

    TransOK = Trans.init();
    delay(100);
    if (TransOK) {
      Display.setCursor(5, 4);
      Display.print(F("Trans OK"));
      delay(500);
    }
    else {
      Display.setCursor(5, 4);
      Display.print(F("Trans FAIL"));
      delay(2000);
    }

    Display.clear();
  }

#ifdef debug
  Serial.println(F("Programming Transceiver Complete..."));
  Trans.PrintParameters();
  Serial.println(F(" "));
#endif

}


void GetParameters() {

  int temp;

  // let's see if we have a new teensy installed
  // if we do create some defaults in the eeprom
  EEPROM.get(0, temp);

  if (temp != 1) {
    // new programmer reset the whole thing
#ifdef debug
    Serial.println(F("resetting EEPROM data"));
#endif
    for (i = 0; i < 255; i++) {
      EEPROM.put(i, 0);
      delay(10);
    }
    // now set some defaults
    // 1 will be our "programmed" flag
    temp = 1;

    PitDisplayColorID = 0; // 9600 baud air data rate
    EEPROM.put(40, PitDisplayColorID);
  }

  EEPROM.get(40, PitDisplayColorID);

#ifdef debug
  Serial.println(F("Printing EEPROM Parameters"));
  Serial.print(F("Pit Display Color ID "));
  Serial.println(PitDisplayColorID);
  Serial.print(F("Pit Display Color "));
  Serial.println(DisplayColor[PitDisplayColorID]);
  Serial.print(F("Pit Display Color value "));
  Serial.println(DisplayValues[PitDisplayColorID]);
#endif

}

void ClearRadio() {

  Trans.Reset();
  /*
    // clear the buffer
    if (ESerial.available() > 0) {
      b = 0;
      while (b != 255) {
        b = ESerial.read();
      }
    }

    delay(500);
  */

}

void RestoreEBYTEDefaults() {

  Display.setCursor(5, 0);
  Display.print(F("Emergency"));
  Display.setCursor(5, 2);
  Display.print(F("Transceiver"));
  Display.setCursor(5, 4);
  Display.print(F("Reset"));

  // Trans.Reset();
  Trans.SetAddressH(0);
  Trans.SetAddressL(0);
  Trans.SetSpeed(0b00011000);
  Trans.SetChannel(0);
  Trans.SetOptions(0b01000100);

  Trans.SaveParameters(PERMANENT);

  delay(50);
  TransOK = Trans.init();

  if (TransOK) {
    Display.setCursor(5, 6);
    Display.print(F("Trans OK"));
  }
  else {
    Display.setCursor(5, 6);
    Display.print(F("Trans FAIL"));
    delay(5000);
  }

  Display.clear();


#ifdef debug
  Serial.println(F("TRANSCEIVER RESET"));
  Trans.PrintParameters();
#endif

}
