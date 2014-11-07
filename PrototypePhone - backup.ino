/**************************************************************************************************

                Coded and written by Pusalieth aka Jake Pring
                          Licensed under the GPLv2

    This code running on an Arduino combined with the GPRS shield from Seeedstudio, and the
  4.3" tft touch screen from Adafrit, using the RA8875 driver from Adafruit, it will produce
  a functioning phone

  Current Features:
  Place calls, receive calls, hang up calls
  Dialing numbers and displayed on-screen
  In call DTMF Tones
  Read incoming data from GPRS shield, and provide functions and executions based on data
  Display prompt for incoming texts
  Display Time in a human readable format including AM/PM
  Seperate and Defined Screens with functions and touch values per screen (ie. Phone vs SMS)
  Read individual texts defined/sorted by index
  Delete SMS
  Caller ID
  Display Missed Calls
  Passcode


************************************************************************************************************/






//==================================================================================================================================================

//   DECLARATIONS AND INITIALIZATIONS

//==================================================================================================================================================


#include <SPI.h>
#include "Adafruit_GFX.h"
#include "Adafruit_RA8875.h"

// Define pins used for RA8875 driver
#define RA8875_INT 3
#define RA8875_CS 10
#define RA8875_RESET 9

// function call tft to use RA8875
Adafruit_RA8875 tft = Adafruit_RA8875(RA8875_CS, RA8875_RESET);

// Initialize touch screen place values
uint16_t tx, ty;

// Keep track of time intervals
unsigned long int timelast = 0;
unsigned long int sinceLastTouch = 0;

// Call Buffer
char callCharBuf[20] = {'A', 'T', 'D'};

// Place value of number dialed sequence
int place = 3;

// Text Messaging array, store from bufGPRS, start and finish of text message read from GPRS
char textCharBuf[160];
String textBuf = "";
String incomingMessageIndex = "01";
boolean viewMessage = false;
boolean textReadStarted = false;
boolean textReady = false;


// resultStr defines the functions of the phone
String resultStr = "";
String oldResultStr = "";

// Store incoming data from GPRS shield
String bufGPRS = "";

// Keep track of whether in call or not
boolean inCall = false;

// Keep track of missed Calls
int countRings = 0;

// ASCII version of 0
int missedCalls = 48;

// Keep track of time elapsed since last call for time (defined per 60 seconds)
unsigned long int every60 = 0;
boolean displayTime = false;

// Define where on the phone the User is and what part of the phone functions to jump to
String whereAmI = "";
boolean atHomeScreen = false;
boolean atPhoneScreen = false;
boolean atTextMessageScreen = false;
boolean atAnswerScreen = false;
boolean atStartUpScreen = false;
boolean atPasscodeScreen = true;

// Display On or Off with button
unsigned long int lastPush = 0;
boolean displaySleep = false;

// passcode set the length and declare variables in ASCII, int 0 starts at 48
String realPasscode = "1234";
String passcode = "";
int passcodePlace = 0;

//==================================================================================================================================================

//   END OF DECLARATIONS AND INITIALIZATIONS

//==================================================================================================================================================









void setup() {
  //==================================================================================================================================================

  //   MAIN SETUP

  //==================================================================================================================================================

  // Start serial from Arduino to USB
  Serial.begin(19200);
  // Start serial from GPRS to Arduino
  Serial1.begin(1200);

  // Initialize the RA8875 driver
  Serial.println("RA8875 start");
  if (!tft.begin(RA8875_480x272)) {
    Serial.println("RA8875 Not Found!");
    while (1);
  }
  Serial.println("Found RA8875");

  // Turn display backlight and PWM output from driver
  tft.displayOn(true);
  tft.GPIOX(true);
  tft.PWM1config(true, RA8875_PWM_CLK_DIV1024);
  tft.PWM1out(255);

  // Set pin for touch resistance of tft display
  pinMode(RA8875_INT, INPUT_PULLUP);
  digitalWrite(RA8875_INT, HIGH);
  tft.touchEnable(true);

  // Flip the vertical and horizontal scanning
  tft.scanV_flip(true);
  tft.scanH_flip(true);

  // Display Boot screen until phone ready
  atStartUpScreen = true;
  tft.textMode();
  tft.textRotate(true);
  tft.textColor(RA8875_WHITE, RA8875_BLACK);
  tft.textEnlarge(2);
  delay(5);
  tft.textSetCursor(75, 20);
  tft.textWrite("Booting...");

  // Update time from network
  Serial1.write("AT+CLTS=1\r\n");
  for (int i = 0; i < 30; i++);

  // Make sure Caller ID is on
  Serial1.write("AT+CLIP=1\r\n");
  for (int i = 0; i < 30; i++);

  pinMode(12, INPUT_PULLUP);
  digitalWrite(12, HIGH);
}
//==================================================================================================================================================

//   END OF SETUP

//==================================================================================================================================================









void loop() {
  //==================================================================================================================================================

  //   MAIN PROGRAM

  //==================================================================================================================================================

  oldResultStr = resultStr;

  //=========================
  //      SERIAL READS
  //=========================
  // When data is coming from GPRS store into bufGPRS
  //noInterrupts();
  if (Serial1.available()) {
    bufGPRS += (char)Serial1.read();
  } //interrupts();

  // When data is coming from USB write to GPRS shield
  if (Serial.available()) {
    Serial1.write(Serial.read());
  }

  //=========================
  //     GPRS FUNCTIONS
  //=========================
  if ((int)bufGPRS.charAt(0) == 255)
    bufGPRS = "";
  if (bufGPRS.substring(0, bufGPRS.length() - 2) == "Call Ready" && bufGPRS.substring(bufGPRS.length() - 2, bufGPRS.length()) == "\r\n")
    resultStr = "Call Ready";
  if (bufGPRS.substring(0, 5) == "+CLIP" && bufGPRS.substring(bufGPRS.length() - 2, bufGPRS.length()) == "\r\n")
    resultStr = "Caller ID";
  if (bufGPRS.substring(0, bufGPRS.length() - 2) == "NO CARRIER" && bufGPRS.substring(bufGPRS.length() - 2, bufGPRS.length()) == "\r\n")
    resultStr = "No Carrier";
  if (bufGPRS.substring(0, 5) == "+CMTI" && bufGPRS.substring(bufGPRS.length() - 2, bufGPRS.length()) == "\r\n")
    resultStr = "Incoming Text";
  if (bufGPRS.substring(0, bufGPRS.length() - 2) == "NORMAL POWER DOWN" && bufGPRS.substring(bufGPRS.length() - 2, bufGPRS.length()) == "\r\n")
    resultStr = "NORMAL POWER DOWN";
  if (bufGPRS.substring(0, 6) == "+CCLK : " && bufGPRS.substring(bufGPRS.length() - 2, bufGPRS.length()) == "\r\n")
    resultStr = "Time";
  if (bufGPRS.substring(0, bufGPRS.length() - 2) == "RING" && bufGPRS.substring(bufGPRS.length() - 2, bufGPRS.length()) == "\r\n") {
    resultStr = "Ringing";
    atAnswerScreen = true;
    atHomeScreen = false;
    atPhoneScreen = false;
    atTextMessageScreen = false;
  }
  if (bufGPRS.substring(0, bufGPRS.length() - 2) == "OK" && bufGPRS.substring(bufGPRS.length() - 2, bufGPRS.length()) == "\r\n")
    resultStr = "OK";
  if (bufGPRS.substring(0, 6) == "+CMGR : " && bufGPRS.substring(bufGPRS.length() - 2, bufGPRS.length()) == "\r\n")
    resultStr = "Text Read";




  //===========================================================================================================================

  //  Defined functions per screen

  //===========================================================================================================================




  //================================================================================================
  // BOOT SCREEN
  //================================================================================================
  if (atStartUpScreen == true) {

    //=========================
    //      CALL READY
    //=========================
    if (resultStr == "Call Ready" && oldResultStr != resultStr) {
      // clear number display and number pad
      tft.graphicsMode();
      tft.fillScreen(RA8875_BLACK);

      // Display to user that phone is ready for use
      tft.textMode();
      tft.textColor(RA8875_WHITE, RA8875_BLACK);
      tft.textEnlarge(2);
      delay(2);
      tft.textSetCursor(75, 40);
      tft.textWrite("Ready!");
      delay(4000);
      atHomeScreen = false;
      atPhoneScreen = false;
      atTextMessageScreen = false;
      atStartUpScreen = false;
      atPasscodeScreen = true;
      drawPasscodeScreen();
      Serial1.write("AT+CCLK?\r\n");
      every60 = millis();
      displayTime = true;
    }

    //=========================
    //       SIM900 OFF
    //=========================
    if (resultStr == "NORMAL POWER DOWN" && oldResultStr != resultStr) {

      // Display to User GPRS shield is off
      tft.textMode();
      tft.textColor(RA8875_WHITE, RA8875_BLACK);
      tft.textEnlarge(2);
      delay(2);
      tft.textSetCursor(250, 50);
      tft.textWrite("SIM900m");
      tft.textSetCursor(300, 50);
      tft.textWrite("POWERED");
      tft.textSetCursor(350, 50);
      tft.textWrite("  OFF");
    }
  }

  if (atPasscodeScreen != true) {
    //================================================================================================
    //   HOME SCREEN
    //================================================================================================
    if (atHomeScreen == true) {

      //=========================
      //          TIME
      //=========================
      if (millis() >= (every60 + 60000)) {
        Serial1.write("AT+CCLK?\r\n");
        every60 = millis();
        displayTime = true;
      }
      if (resultStr == "Time" && displayTime == true) {

        // Display to User GPRS shield is off
        tft.textMode();
        tft.textColor(RA8875_WHITE, RA8875_BLACK);
        tft.textEnlarge(2);
        delay(2);
        tft.textSetCursor(150, 80);

        // Determine the start and finish of body for Caller id number
        int time_Start = bufGPRS.indexOf('\"');
        int time_End = bufGPRS.indexOf('-');

        /*
        // Make sure that each time call is at the 00 mark
        if (millis() < 120000) {
        int firstDec = bufGPRS.charAt(time_End - 2)
        int countTo_60 = 0;
        while (bufGPRS.substring(time_End - 2, time_End) < "60") {
        countTo_60++;
        }
        every60 += 60000 - (countTo_60 * 1000);
        }*/

        // store data and input to array
        char time[20];

        //=========================
        //      MONTH FORMAT
        //=========================
        // (default year:month:day hour:minute:second)
        if (bufGPRS.charAt(time_Start + 4) == '0' && bufGPRS.charAt(time_Start + 5) == '1') {
          time[0] = 'J';
          time[1] = 'a';
          time[2] = 'n';
        }
        if (bufGPRS.charAt(time_Start + 4) == '0' && bufGPRS.charAt(time_Start + 5) == '2') {
          time[0] = 'F';
          time[1] = 'e';
          time[2] = 'b';
        }
        if (bufGPRS.charAt(time_Start + 4) == '0' && bufGPRS.charAt(time_Start + 5) == '3') {
          time[0] = 'M';
          time[1] = 'a';
          time[2] = 'r';
        }
        if (bufGPRS.charAt(time_Start + 4) == '0' && bufGPRS.charAt(time_Start + 5) == '4') {
          time[0] = 'A';
          time[1] = 'p';
          time[2] = 'r';
        }
        if (bufGPRS.charAt(time_Start + 4) == '0' && bufGPRS.charAt(time_Start + 5) == '5') {
          time[0] = 'M';
          time[1] = 'a';
          time[2] = 'y';
        }
        if (bufGPRS.charAt(time_Start + 4) == '0' && bufGPRS.charAt(time_Start + 5) == '6') {
          time[0] = 'J';
          time[1] = 'u';
          time[2] = 'n';
        }
        if (bufGPRS.charAt(time_Start + 4) == '0' && bufGPRS.charAt(time_Start + 5) == '7') {
          time[0] = 'J';
          time[1] = 'u';
          time[2] = 'l';
        }
        if (bufGPRS.charAt(time_Start + 4) == '0' && bufGPRS.charAt(time_Start + 5) == '8') {
          time[0] = 'A';
          time[1] = 'u';
          time[2] = 'g';
        }
        if (bufGPRS.charAt(time_Start + 4) == '0' && bufGPRS.charAt(time_Start + 5) == '9') {
          time[0] = 'S';
          time[1] = 'e';
          time[2] = 'p';
        }
        if (bufGPRS.charAt(time_Start + 4) == '1' && bufGPRS.charAt(time_Start + 5) == '0') {
          time[0] = 'O';
          time[1] = 'c';
          time[2] = 't';
        }
        if (bufGPRS.charAt(time_Start + 4) == '1' && bufGPRS.charAt(time_Start + 5) == '1') {
          time[0] = 'N';
          time[1] = 'o';
          time[2] = 'v';
        }
        if (bufGPRS.charAt(time_Start + 4) == '1' && bufGPRS.charAt(time_Start + 5) == '2') {
          time[0] = 'D';
          time[1] = 'e';
          time[2] = 'c';
        }
        time[3] = ',';

        //=========================
        //      Day FORMAT
        //=========================
        time[4] = bufGPRS.charAt(time_Start + 7);
        time[5] = bufGPRS.charAt(time_Start + 8);
        time[6] = ' ';

        //=========================
        //      YEAR FORMAT
        //=========================
        time[7] = '2';
        time[8] = '0';
        time[9] = bufGPRS.charAt(time_Start + 1);
        time[10] = bufGPRS.charAt(time_Start + 2);
        time[11] = ' ';
        time[12] = ' ';

        //=========================
        //      HOUR FORMAT
        //=========================
        time[13] = bufGPRS.charAt(time_Start + 10);
        time[14] = bufGPRS.charAt(time_Start + 11);

        //=========================
        //        AM & PM
        //=========================
        if (time[13] >= '1' && time[14] >= '3') {
          time[13] -= 1;
          time[13] = (char)time[13];
          time[14] -= 2;
          time[14] = (char)time[14];
        }
        if (time[13] >= '1' && time[14] >= '2') {
          time[18] = 'p';
          time[19] = 'm';
        }
        else {
          time[18] = 'a';
          time[19] = 'm';
        }

        //=========================
        //      MINUTE FORMAT
        //=========================
        time[15] = bufGPRS.charAt(time_Start + 12);
        time[16] = bufGPRS.charAt(time_Start + 13);
        time[17] = bufGPRS.charAt(time_Start + 14);

        //=========================
        //     WRITE TO SCREEN
        //=========================
        tft.textSetCursor(20, 0);
        tft.writeCommand(RA8875_MRWC);
        for (int i = 0; i < 20; i++) {
          tft.writeData(time[i]);
          delay(1);
        }
        displayTime = false;
      }

      //=========================
      //      MISSED CALLS
      //=========================
      if (missedCalls > 48) {
        tft.textMode();
        tft.textRotate(true);
        tft.textColor(RA8875_WHITE, RA8875_BLACK);
        tft.textEnlarge(1);
        delay(2);
        tft.textSetCursor(270, 15);
        tft.textWrite("Missed Calls ");
        for (int i = 0; i < 1; i++) {
          tft.writeData((char)missedCalls);
          delay(1);
        }
      }

      //=========================
      //      NEW MESSAGE
      //=========================
      if (resultStr == "Incoming Text" && oldResultStr != resultStr) {

        // clear number display and number pad
        tft.graphicsMode();
        tft.fillRect(160, 0, 100, 279, RA8875_BLACK);

        // Prompt User new message received
        tft.textMode();
        tft.textRotate(true);
        tft.textColor(RA8875_WHITE, RA8875_BLACK);
        tft.textEnlarge(1);
        delay(2);
        tft.textSetCursor(160, 20);
        tft.textWrite("New Message ");

        //=========================
        //    NEW MESSAGE INDEX
        //=========================
        if (bufGPRS.indexOf(',') + 2 == '\r')
          incomingMessageIndex[1] = bufGPRS.charAt(bufGPRS.indexOf(',') + 1);
        else
          incomingMessageIndex = bufGPRS.substring(bufGPRS.indexOf(',') + 1, bufGPRS.length() - 2);

        //=========================
        //      DISPLAY INDEX
        //=========================
        tft.writeCommand(RA8875_MRWC);
        for (int i = 0; i < 2; i++) {
          tft.writeData(incomingMessageIndex[i]);
          delay(1);
        }

        //=========================
        //   VIEW MESSAGE PROMPT
        //=========================
        tft.textMode();
        tft.textRotate(true);
        tft.textColor(RA8875_WHITE, RA8875_BLACK);
        tft.textEnlarge(1);
        delay(2);
        tft.textSetCursor(210, 40);
        tft.textWrite("View Message");
        viewMessage = true;
      }
    }

    //================================================================================================
    //   TEXT MESSAGE SCREEN
    //================================================================================================
    if (atTextMessageScreen == true) {

      //=========================
      //      VIEW MESSAGE
      //=========================
      if (textReady == true) {
        textBuf.toCharArray(textCharBuf, textBuf.length());

        // Prepare to display message
        tft.textMode();
        tft.textColor(RA8875_WHITE, RA8875_BLACK);
        tft.textEnlarge(1);

        // Display Index Number
        tft.textSetCursor(30, 110);
        tft.writeCommand(RA8875_MRWC);
        for (int i = 0; i < 2; i++) {
          tft.writeData(incomingMessageIndex[i]);
          delay(1);
        }

        // Define the length number to display on-screen
        int receivingNumStart = textBuf.indexOf(',');
        int receivingNumEnd = (textBuf.indexOf(',', receivingNumStart + 1));

        // Display new message receiving number
        tft.textSetCursor(130, 20);
        tft.writeCommand(RA8875_MRWC);
        for (int i = receivingNumStart + 3; i < receivingNumEnd - 1; i++) {
          tft.writeData(textCharBuf[i]);
          delay(1);
        }

        // Define the length of message to display on-screen
        int messageStart = textBuf.indexOf('\r');
        int messageEnd = (textBuf.indexOf('\r', messageStart + 1));

        // Display Message Body
        tft.textSetCursor(175, 0);
        tft.writeCommand(RA8875_MRWC);
        for (int i = messageStart + 2; i < messageEnd; i++) {
          tft.writeData(textCharBuf[i]);
          delay(1);
        }
        textReady = false;
        textBuf = "";
      }
    }
  }

  //================================================================================================
  //   ANSWER SCREEN
  //================================================================================================
  if (atAnswerScreen == true) {

    //=========================
    //  DISPLAY Incoming Call
    //=========================
    if (resultStr == "Ringing" && oldResultStr != resultStr) {
      if (countRings == 0) {
        drawAnswerScreen();
      }
      tft.textMode();
      tft.textColor(RA8875_WHITE, RA8875_BLACK);
      tft.textEnlarge(1);
      delay(2);
      tft.textSetCursor(75, 20);
      tft.textWrite("Incoming Call");
      resultStr = "";
      countRings++;
    }

    //=========================
    //    DISPLAY CALLER ID
    //=========================
    if (resultStr == "Caller ID" && oldResultStr != resultStr) {
      // Determine the start and finish of body for Caller id number
      int callerID_Start = bufGPRS.indexOf('\"');
      int callerID_End = bufGPRS.indexOf('\"', callerID_Start + 1);

      // store data and input to array
      char callerID[bufGPRS.length()];
      bufGPRS.toCharArray(callerID, bufGPRS.length());

      // Display number on screen
      tft.textMode();
      tft.textColor(RA8875_WHITE, RA8875_BLACK);
      tft.textEnlarge(1);
      delay(2);
      tft.textSetCursor(125, 20);
      tft.writeCommand(RA8875_MRWC);
      for (int i = callerID_Start + 1; i < callerID_End; i++) {
        tft.writeData(callerID[i]);
        delay(1);
      }
    }

    //=========================
    //     MISSED CALLS
    //=========================
    if (countRings > 1 && resultStr == "No Carrier" && oldResultStr != resultStr) {
      missedCalls++;
      countRings = 0;
    }
  }
  //=========================
  //       CALL LOST
  //=========================
  if (resultStr == "No Carrier" && oldResultStr != resultStr) {

    if (atPasscodeScreen == true) {
      atPhoneScreen = false;
      drawPasscodeScreen();
    }
    else {
      atHomeScreen = true;
      atPhoneScreen = false;
      atTextMessageScreen = false;
      atAnswerScreen = false;
      drawHomeScreen();
    }
  }

  //===========================================================================================================================

  //  END OF SCREEN FUNCTIONS

  //===========================================================================================================================





  //===========================================================================================================================

  //  BACKGROUND TASKS

  //===========================================================================================================================

  //=========================
  //    NOTIFICATION LEDS
  //=========================
  if (missedCalls > 48) {
    for (int i = 0; i < 255; i++) {
      analogWrite(4, i / 10);
      Serial.print("This is i");
      Serial.print(i);
      Serial.print("\r\n");
    }
    delay(100);
    for (int i = 255; i > 0; i--)
      analogWrite(4, i / 10);
  }
  else
    digitalWrite(4, LOW);
  if (viewMessage)
    digitalWrite(5, HIGH);
  else
    digitalWrite(5, LOW);

  //=========================
  //  DISPLAY POWER BUTTON
  //=========================
  if (millis() >= (lastPush + 350)) {
    while (!digitalRead(12)) {
      lastPush = millis();
      if (displaySleep == true) {
        tft.displayOn(true);
        tft.PWM1out(255);
        tft.touchEnable(true);
        displaySleep = false;
        delay(10);
        break;
      } else {
        tft.displayOn(false);
        tft.PWM1out(0);
        tft.touchEnable(false);
        displaySleep = true;
        drawPasscodeScreen();
        atHomeScreen = false;
        atPhoneScreen = false;
        atTextMessageScreen = false;
        atAnswerScreen = false;
        atStartUpScreen = false;
        atPasscodeScreen = true;
        drawPasscodeScreen();
        break;
      }
    }
  }

  //===========================================================================================================================

  //  TOUCH FUNCTIONS/AREAS DEFINED PER SCREEN

  //===========================================================================================================================

  // Enable graphics mode for touch input
  tft.graphicsMode();

  // Set scale for touch values
  float xScale = 1200.0F / tft.width();
  float yScale = 1200.0F / tft.height();

  if (!digitalRead(RA8875_INT)) {
    if (tft.touched()) {
      printTouchValues();

      //sinceLastTouch = millis();

      tft.touchRead(&tx, &ty);
      if (millis() >= (timelast + 250)) {

        //================================================================================================
        //  PASSCODE SCREEN
        //================================================================================================
        if (atPasscodeScreen == true) {

          //=========================
          //          #1
          //=========================
          if (tx > 682 && tx < 784 && ty > 190 && ty < 305) {
            tx = 0;
            ty = 0;
            passcode += "1";
            passcodePlace++;
            tft.textMode();
            tft.textColor(RA8875_WHITE, RA8875_BLACK);
            tft.textEnlarge(2);
            delay(2);
            switchPasscodePlace();
            tft.textWrite("*");
          }

          //=========================
          //          #2
          //=========================
          if (tx > 682 && tx < 784 && ty > 443 && ty < 577) {
            tx = 0;
            ty = 0;
            passcode += "2";
            passcodePlace++;
            tft.textMode();
            tft.textColor(RA8875_WHITE, RA8875_BLACK);
            tft.textEnlarge(2);
            delay(2);
            switchPasscodePlace();
            tft.textWrite("*");
          }

          //=========================
          //          #3
          //=========================
          if (tx > 682 && tx < 784 && ty > 690 && ty < 872) {
            tx = 0;
            ty = 0;
            passcode += "3";
            passcodePlace++;
            tft.textMode();
            tft.textColor(RA8875_WHITE, RA8875_BLACK);
            tft.textEnlarge(2);
            delay(2);
            switchPasscodePlace();
            tft.textWrite("*");
          }

          //=========================
          //          #4
          //=========================
          if (tx > 523 && tx < 627 && ty > 190 && ty < 305) {
            tx = 0;
            ty = 0;
            passcode += "4";
            passcodePlace++;
            tft.textMode();
            tft.textColor(RA8875_WHITE, RA8875_BLACK);
            tft.textEnlarge(2);
            delay(2);
            switchPasscodePlace();
            tft.textWrite("*");
          }

          //=========================
          //          #5
          //=========================
          if (tx > 523 && tx < 627 && ty > 443 && ty < 577) {
            tx = 0;
            ty = 0;
            passcode += "5";
            passcodePlace++;
            tft.textMode();
            tft.textColor(RA8875_WHITE, RA8875_BLACK);
            tft.textEnlarge(2);
            delay(2);
            switchPasscodePlace();
            tft.textWrite("*");
          }

          //=========================
          //          #6
          //=========================
          if (tx > 523 && tx < 627 && ty > 690 && ty < 872) {
            tx = 0;
            ty = 0;
            passcode += "6";
            passcodePlace++;
            tft.textMode();
            tft.textColor(RA8875_WHITE, RA8875_BLACK);
            tft.textEnlarge(2);
            delay(2);
            switchPasscodePlace();
            tft.textWrite("*");
          }

          //=========================
          //          #7
          //=========================
          if (tx > 348 && tx < 454 && ty > 190 && ty < 305) {
            tx = 0;
            ty = 0;
            passcode += "7";
            passcodePlace++;
            tft.textMode();
            tft.textColor(RA8875_WHITE, RA8875_BLACK);
            tft.textEnlarge(2);
            delay(2);
            switchPasscodePlace();
            tft.textWrite("*");
          }

          //=========================
          //          #8
          //=========================
          if (tx > 348 && tx < 454 && ty > 443 && ty < 577) {
            tx = 0;
            ty = 0;
            passcode += "8";
            passcodePlace++;
            tft.textMode();
            tft.textColor(RA8875_WHITE, RA8875_BLACK);
            tft.textEnlarge(2);
            delay(2);
            switchPasscodePlace();
            tft.textWrite("*");
          }

          //=========================
          //          #9
          //=========================
          if (tx > 348 && tx < 454 && ty > 690 && ty < 872) {
            tx = 0;
            ty = 0;
            passcode += "9";
            passcodePlace++;
            tft.textMode();
            tft.textColor(RA8875_WHITE, RA8875_BLACK);
            tft.textEnlarge(2);
            delay(2);
            switchPasscodePlace();
            tft.textWrite("*");
          }

          //=========================
          //           <-
          //=========================
          if (tx > 207 && tx < 284 && ty > 170 && ty < 324) {
            tx = 0;
            ty = 0;
            passcode[passcodePlace - 1] = NULL;
            tft.textMode();
            tft.textColor(RA8875_WHITE, RA8875_BLACK);
            tft.textEnlarge(2);
            delay(2);
            switchPasscodePlace();
            tft.textWrite(" ");
            passcodePlace--;
          }

          //=========================
          //          #0
          //=========================
          if (tx > 207 && tx < 284 && ty > 443 && ty < 577) {
            tx = 0;
            ty = 0;
            passcode += "0";
            passcodePlace++;
            tft.textMode();
            tft.textColor(RA8875_WHITE, RA8875_BLACK);
            tft.textEnlarge(2);
            delay(2);
            switchPasscodePlace();
            tft.textWrite("*");
          }

          //=========================
          //           OK
          //=========================
          if (tx > 207 && tx < 284 && ty > 672 && ty < 894) {
            tx = 0;
            ty = 0;
            char passcodeChar[passcode.length()];
            passcode.toCharArray(passcodeChar, passcode.length());
            Serial.write("passcode entered is ");
            Serial.write(passcodeChar);
            Serial.write("\r\n");
            if (passcode == realPasscode) {
              atHomeScreen = true;
              atPasscodeScreen = false;
              atTextMessageScreen = false;
              atPhoneScreen = false;
              atAnswerScreen = false;
              drawHomeScreen();
              Serial1.write("AT+CCLK?\r\n");
              every60 = millis();
              displayTime = true;
            } else {
              tft.graphicsMode();
              tft.fillRect(20, 20, 80, 200, RA8875_BLACK);
              tft.textMode();
              tft.textEnlarge(1);
              delay(2);
              tft.textColor(RA8875_WHITE, RA8875_RED);
              tft.textSetCursor(30, 25);
              tft.textWrite("Wrong Passcode");
              delay(3000);
              drawPasscodeScreen();
            }
            passcode = "";
            passcodePlace = 0;
          }
        }

        if (atPasscodeScreen != true) {
          //================================================================================================
          //  HOME SCREEN TOUCH
          //================================================================================================
          if (atHomeScreen == true) {

            //=========================
            //      PHONE BUTTON
            //=========================
            if (tx > 128 && tx < 300 && ty > 204 && ty < 440) {
              tx = 0;
              ty = 0;
              missedCalls = 48;
              drawPhoneScreen();
              atPhoneScreen = true;
              atHomeScreen = false;
              atTextMessageScreen = false;
              atAnswerScreen = false;
            }

            //=========================
            //      SMS BUTTON
            //=========================
            if (tx > 128 && tx < 300 && ty > 580 && ty < 815) {
              tx = 0;
              ty = 0;
              missedCalls = 48;
              bufGPRS = "AT+CMGR=";
              bufGPRS += incomingMessageIndex;
              bufGPRS += "\r\n";
              bufGPRS.toCharArray(callCharBuf, 16);
              Serial1.write(callCharBuf, bufGPRS.length());
              drawTextMessageScreen();
              atTextMessageScreen = true;
              atHomeScreen = false;
              atPhoneScreen = false;
              atAnswerScreen = false;
              viewMessage = false;
            }
            /*
            //=========================
            //   CLEAR NEW MESSAGES
            //=========================
            if (finger dragged over missed messages left to right) {
            }

            //=========================
            //   CLEAR MISSED CALLS
            //=========================
            if (finger dragged over missed calls left to right) {
            tft.fillRect(270, 20, 40, 259, RA8875_BLACK);
            oldMissedCalls = missedCalls;
            }
            */

            //=========================
            //    VIEW NEW MESSAGE
            //=========================
            if (viewMessage == true) {
              if (tx > 490 && tx < 576 && ty > 163 && ty < 756) {
                tx = 0;
                ty = 0;
                // During user prompt for new message send data if touched
                bufGPRS = "AT+CMGR=";
                bufGPRS += incomingMessageIndex;
                bufGPRS += "\r\n";
                bufGPRS.toCharArray(callCharBuf, 16);
                Serial1.write(callCharBuf, bufGPRS.length());
                drawTextMessageScreen();
                atTextMessageScreen = true;
                atHomeScreen = false;
                atPhoneScreen = false;
                atAnswerScreen = false;
                viewMessage = false;
              }
            }
          }

          //================================================================================================
          //  TEXT MESSAGE SCREEN TOUCH
          //================================================================================================
          if (atTextMessageScreen == true) {

            //=========================
            //          SEND
            //=========================
            if (tx > 56 && tx < 159 && ty > 130 && ty < 353) {
              tx = 0;
              ty = 0;
              drawMessagePad();
            }

            //=========================
            //      HOME BUTTON
            //=========================
            if (tx > 56 && tx < 159 && ty > 383 && ty < 604) {
              tx = 0;
              ty = 0;
              atHomeScreen = true;
              atPhoneScreen = false;
              atTextMessageScreen = false;
              atAnswerScreen = false;
              drawHomeScreen();
              Serial1.write("AT+CCLK?\r\n");
              every60 = millis();
              displayTime = true;
            }

            //=========================
            //        DELETE
            //=========================
            if (tx > 56 && tx < 159 && ty > 671 && ty < 881) {
              tx = 0;
              ty = 0;
              String sendDelete = "AT+CMGD=";
              sendDelete += incomingMessageIndex;
              sendDelete += "\r\n";
              char sendCharDelete[sendDelete.length()];
              sendDelete.toCharArray(sendCharDelete, sendDelete.length());
              Serial1.write(sendCharDelete);
              drawTextMessageScreen();
              tft.textMode();
              tft.textColor(RA8875_WHITE, RA8875_BLACK);
              tft.textEnlarge(1);
              delay(2);
              tft.textSetCursor(150, 20);
              tft.textWrite("Message Deleted");
              delay(3000);
              drawTextMessageScreen();
              if (incomingMessageIndex.charAt(1) == '9') {
                incomingMessageIndex[0] += 1;
                incomingMessageIndex[0] = (char)incomingMessageIndex[0];
                incomingMessageIndex[1] = '0';
              }
              else {
                incomingMessageIndex[1] += 1;
                incomingMessageIndex[1] = (char)incomingMessageIndex[1];
              }
              String sendRead = "AT+CMGR=";
              sendRead += incomingMessageIndex;
              sendRead += "\r\n";
              char sendCharRead[sendRead.length()];
              sendRead.toCharArray(sendCharRead, sendRead.length());
              Serial1.write(sendCharRead);
            }

            //=========================
            //      NEXT BUTTON
            //=========================
            if (tx > 876 && tx < 952 && ty > 731 && ty < 843) {
              tx = 0;
              ty = 0;
              if (incomingMessageIndex.charAt(1) == '9') {
                incomingMessageIndex[0] += 1;
                incomingMessageIndex[0] = (char)incomingMessageIndex[0];
                incomingMessageIndex[1] = '0';
              }
              else {
                incomingMessageIndex[1] += 1;
                incomingMessageIndex[1] = (char)incomingMessageIndex[1];
              }
              String sendRead = "AT+CMGR=";
              sendRead += incomingMessageIndex;
              sendRead += "\r\n";
              char sendCharRead[sendRead.length()];
              sendRead.toCharArray(sendCharRead, sendRead.length());
              Serial1.write(sendCharRead);
            }

            //=========================
            //    PREVIOUS BUTTON
            //=========================
            if (tx > 718 && tx < 803 && ty > 731 && ty < 843) {
              tx = 0;
              ty = 0;
              if (incomingMessageIndex.charAt(0) > '0' && incomingMessageIndex.charAt(1) == '0') {
                incomingMessageIndex[0] -= 1;
                incomingMessageIndex[0] = (char)incomingMessageIndex[0];
                incomingMessageIndex[1] = '9';
              }
              else {
                incomingMessageIndex[1] -= 1;
                incomingMessageIndex[1] = (char)incomingMessageIndex[1];
              }
              String sendRead = "AT+CMGR=";
              sendRead += incomingMessageIndex;
              sendRead += "\r\n";
              char sendCharRead[sendRead.length()];
              sendRead.toCharArray(sendCharRead, sendRead.length());
              Serial1.write(sendCharRead);
            }
          }
        }

        //================================================================================================
        //  ANSWER SCREEN TOUCH
        //================================================================================================
        if (atAnswerScreen == true) {

          //=========================
          //      ANSWER BUTTON
          //=========================
          if (tx > 119 && tx < 311 && ty > 196 && ty < 456) {
            tx = 0;
            ty = 0;
            Serial1.write("ATA\r\n");
            inCall = true;
            tft.fillRect(75, 20, 150, 250, RA8875_BLACK);
            tft.textMode();
            tft.textColor(RA8875_WHITE, RA8875_BLACK);
            tft.textEnlarge(2);
            delay(2);
            tft.textSetCursor(75, 20);
            tft.textWrite("Answered");
            delay(1000);
            atPhoneScreen = true;
            atHomeScreen = false;
            atTextMessageScreen = false;
            atAnswerScreen = false;
            drawPhoneScreen();
            countRings = 0;
          }

          //=========================
          //      IGNORE BUTTON
          //=========================
          if (tx > 119 && tx < 311 && ty > 568 && ty < 826) {
            tx = 0;
            ty = 0;
            inCall = false;
          }
        }

        //================================================================================================
        //  PHONE SCREEN TOUCH
        //================================================================================================
        if (atPhoneScreen == true) {

          //=========================
          //          #1
          //=========================
          if (tx > 682 && tx < 784 && ty > 190 && ty < 305) {
            tx = 0;
            ty = 0;
            callCharBuf[place] = '1';
            place++;
            tft.textMode();
            tft.textRotate(true);
            tft.textColor(RA8875_WHITE, RA8875_BLACK);
            tft.textEnlarge(2);
            delay(2);
            switchPlace();
            tft.textWrite("1");

            // In call DTMF Tone
            if (inCall == true)
              Serial1.write("AT+VTS=1\r\n");
          }

          //=========================
          //          #2
          //=========================
          if (tx > 682 && tx < 784 && ty > 443 && ty < 577) {
            tx = 0;
            ty = 0;
            callCharBuf[place] = '2';
            place++;
            tft.textMode();
            tft.textRotate(true);
            tft.textColor(RA8875_WHITE, RA8875_BLACK);
            tft.textEnlarge(2);
            delay(2);
            switchPlace();
            tft.textWrite("2");

            // In call DTMF Tone
            if (inCall == true)
              Serial1.write("AT+VTS=2\r\n");
          }

          //=========================
          //          #3
          //=========================
          if (tx > 682 && tx < 784 && ty > 690 && ty < 872) {
            tx = 0;
            ty = 0;
            callCharBuf[place] = '3';
            place++;
            tft.textMode();
            tft.textRotate(true);
            tft.textColor(RA8875_WHITE, RA8875_BLACK);
            tft.textEnlarge(2);
            delay(2);
            switchPlace();
            tft.textWrite("3");

            // In call DTMF Tone
            if (inCall == true)
              Serial1.write("AT+VTS=3\r\n");
          }

          //=========================
          //          #4
          //=========================
          if (tx > 523 && tx < 627 && ty > 190 && ty < 305) {
            tx = 0;
            ty = 0;
            callCharBuf[place] = '4';
            place++;
            tft.textMode();
            tft.textRotate(true);
            tft.textColor(RA8875_WHITE, RA8875_BLACK);
            tft.textEnlarge(2);
            delay(2);
            switchPlace();
            tft.textWrite("4");

            // In call DTMF Tone
            if (inCall == true)
              Serial1.write("AT+VTS=4\r\n");
          }

          //=========================
          //          #5
          //=========================
          if (tx > 523 && tx < 627 && ty > 443 && ty < 577) {
            tx = 0;
            ty = 0;
            callCharBuf[place] = '5';
            place++;
            tft.textMode();
            tft.textRotate(true);
            tft.textColor(RA8875_WHITE, RA8875_BLACK);
            tft.textEnlarge(2);
            delay(2);
            switchPlace();
            tft.textWrite("5");

            // In call DTMF Tone
            if (inCall == true)
              Serial1.write("AT+VTS=5\r\n");
          }

          //=========================
          //          #6
          //=========================
          if (tx > 523 && tx < 627 && ty > 690 && ty < 872) {
            tx = 0;
            ty = 0;
            callCharBuf[place] = '6';
            place++;
            tft.textMode();
            tft.textRotate(true);
            tft.textColor(RA8875_WHITE, RA8875_BLACK);
            tft.textEnlarge(2);
            delay(2);
            switchPlace();
            tft.textWrite("6");

            // In call DTMF Tone
            if (inCall == true)
              Serial1.write("AT+VTS=6\r\n");
          }

          //=========================
          //          #7
          //=========================
          if (tx > 348 && tx < 454 && ty > 190 && ty < 305) {
            tx = 0;
            ty = 0;
            callCharBuf[place] = '7';
            place++;
            tft.textMode();
            tft.textRotate(true);
            tft.textColor(RA8875_WHITE, RA8875_BLACK);
            tft.textEnlarge(2);
            delay(2);
            switchPlace();
            tft.textWrite("7");

            // In call DTMF Tone
            if (inCall == true)
              Serial1.write("AT+VTS=7\r\n");
          }

          //=========================
          //          #8
          //=========================
          if (tx > 348 && tx < 454 && ty > 443 && ty < 577) {
            tx = 0;
            ty = 0;
            callCharBuf[place] = '8';
            place++;
            tft.textMode();
            tft.textRotate(true);
            tft.textColor(RA8875_WHITE, RA8875_BLACK);
            tft.textEnlarge(2);
            delay(2);
            switchPlace();
            tft.textWrite("8");

            // In call DTMF Tone
            if (inCall == true)
              Serial1.write("AT+VTS=8\r\n");
          }

          //=========================
          //          #9
          //=========================
          if (tx > 348 && tx < 454 && ty > 690 && ty < 872) {
            tx = 0;
            ty = 0;
            callCharBuf[place] = '9';
            place++;
            tft.textMode();
            tft.textRotate(true);
            tft.textColor(RA8875_WHITE, RA8875_BLACK);
            tft.textEnlarge(2);
            delay(2);
            switchPlace();
            tft.textWrite("9");

            // In call DTMF Tone
            if (inCall == true)
              Serial1.write("AT+VTS=9\r\n");
          }

          //=========================
          //           *
          //=========================
          if (tx > 207 && tx < 284 && ty > 190 && ty < 305) {
            tx = 0;
            ty = 0;
            callCharBuf[place] = '*';
            place++;
            tft.textMode();
            tft.textRotate(true);
            tft.textColor(RA8875_WHITE, RA8875_BLACK);
            tft.textEnlarge(2);
            delay(2);
            switchPlace();
            tft.textWrite("*");

            // In call DTMF Tone
            if (inCall == true)
              Serial1.write("AT+VTS=*\r\n");
          }

          //=========================
          //          #0
          //=========================
          if (tx > 207 && tx < 284 && ty > 443 && ty < 577) {
            tx = 0;
            ty = 0;
            callCharBuf[place] = '0';
            place++;
            tft.textMode();
            tft.textRotate(true);
            tft.textColor(RA8875_WHITE, RA8875_BLACK);
            tft.textEnlarge(2);
            delay(2);
            switchPlace();
            tft.textWrite("0");

            // In call DTMF Tone
            if (inCall == true)
              Serial1.write("AT+VTS=0\r\n");
          }

          //=========================
          //           #
          //=========================
          if (tx > 207 && tx < 284 && ty > 690 && ty < 872) {
            tx = 0;
            ty = 0;
            callCharBuf[place] = '#';
            place++;
            tft.textMode();
            tft.textRotate(true);
            tft.textColor(RA8875_WHITE, RA8875_BLACK);
            tft.textEnlarge(2);
            delay(2);
            switchPlace();
            tft.textWrite("#");

            // In call DTMF Tone
            if (inCall == true)
              Serial1.write("AT+VTS=#\r\n");
          }

          if (atPasscodeScreen != true) {
            //=========================
            //          CALL
            //=========================
            if (tx > 56 && tx < 159 && ty > 130 && ty < 353) {
              tx = 0;
              ty = 0;
              Serial.print("Place at Call 0");
              Serial.print(place);
              Serial.print("\r\n");
              callCharBuf[place] = ';';
              Serial.print("Place at Call 1");
              Serial.print(place);
              Serial.print("\r\n");
              callCharBuf[place + 1] = '\r';
              Serial.print("Place at Call 2");
              Serial.print(place);
              Serial.print("\r\n");
              callCharBuf[place + 2] = '\n';
              Serial.print("Place at Call 3");
              Serial.print(place);
              Serial.print("\r\n");
              Serial.print("callCharBuf ready");
              Serial.print(callCharBuf);
              Serial.print("\r\n");
              Serial1.write(callCharBuf);
              inCall = true;
              tft.textMode();
              tft.textColor(RA8875_WHITE, RA8875_BLACK);
              tft.textEnlarge(2);
              delay(2);
              tft.textSetCursor(20, 20);
              tft.textWrite("Calling...");
              delay(4000);
              tft.graphicsMode();
              tft.fillRect(0, 0, 75, 279, RA8875_BLACK);
              place = 3;
            }

            //=========================
            //      HOME BUTTON
            //=========================
            if (tx > 56 && tx < 159 && ty > 383 && ty < 604) {
              tx = 0;
              ty = 0;
              atHomeScreen = true;
              atPhoneScreen = false;
              atTextMessageScreen = false;
              atAnswerScreen = false;
              drawHomeScreen();
              Serial1.write("AT+CCLK?\r\n");
              every60 = millis();
              displayTime = true;
            }
          }

          //=========================
          //       END/HANG UP
          //=========================
          if (tx > 56 && tx < 159 && ty > 671 && ty < 881) {
            tx = 0;
            ty = 0;
            inCall = false;
            Serial1.write("ATH\r\n");
            tft.textMode();
            tft.textColor(RA8875_WHITE, RA8875_BLACK);
            tft.textEnlarge(2);
            delay(2);
            tft.textSetCursor(20, 15);
            tft.textWrite("Call Ended");
            delay(2000);
            tft.graphicsMode();
            tft.fillRect(0, 0, 75, 279, RA8875_BLACK);
            place = 3;
            if (atPasscodeScreen == true) {
              atPhoneScreen = false;
              drawPasscodeScreen();
            }
          }
        }
        timelast = millis();
      }
    }
  }

  //================================================================================================
  //  CLEAR CALL BUFFER AT 17 CHARACTERS
  //================================================================================================
  if (inCall != true) {
    if (place > 17) {
      place = 3;

      // clear number display
      tft.graphicsMode();
      tft.fillScreen(RA8875_BLACK);

      // Alert User of to long of string
      tft.textMode();
      tft.textRotate(true);
      tft.textColor(RA8875_WHITE, RA8875_BLACK);
      tft.textEnlarge(1);
      delay(2);
      tft.textSetCursor(0, 100);
      tft.textWrite("Alert!");
      tft.textSetCursor(35, 20);
      tft.textWrite("17 characters or less");
      delay(3000);
      drawPhoneScreen();
    }
  }

  //================================================================================================
  //  PRINT SERIAL DATA AT END OF EACH STATEMENT OR REQUEST
  //================================================================================================
  // Print data coming from GPRS then clear
  if (bufGPRS.substring(bufGPRS.length() - 2, bufGPRS.length()) == "\r\n") {
    Serial.print(bufGPRS);
    if (resultStr == "Text Read") {
      textBuf += bufGPRS;
      textReadStarted = true;
    }
    else if (resultStr == "OK" && textReadStarted == true)
      textReady = true;
    bufGPRS = "";
  }
}
//==================================================================================================================================================

//   END OF MAIN PROGRAM

//==================================================================================================================================================






//==================================================================================================================================================

//   FUNCTIONS CALLS

//==================================================================================================================================================
void switchPlace() {
  switch (place) {
    case 3:
      tft.textSetCursor(20, 0);
      break;
    case 4:
      tft.textSetCursor(20, 20);
      break;
    case 5:
      tft.textSetCursor(20, 40);
      break;
    case 6:
      tft.textSetCursor(20, 60);
      break;
    case 7:
      tft.textSetCursor(20, 80);
      break;
    case 8:
      tft.textSetCursor(20, 100);
      break;
    case 9:
      tft.textSetCursor(20, 140);
      break;
    case 10:
      tft.textSetCursor(20, 120);
      break;
    case 11:
      tft.textSetCursor(20, 140);
      break;
    case 12:
      tft.textSetCursor(20, 160);
      break;
    case 13:
      tft.textSetCursor(20, 180);
      break;
    case 14:
      tft.textSetCursor(20, 200);
      break;
    case 15:
      tft.textSetCursor(20, 220);
      break;
    case 16:
      tft.textSetCursor(20, 240);
      break;
    case 17:
      tft.textSetCursor(20, 260);
      break;
  }
}
void drawHomeScreen() {
  tft.graphicsMode();
  tft.fillScreen(RA8875_BLACK);
  tft.fillCircle(400, 70, 45, RA8875_WHITE);
  tft.fillCircle(400, 198, 45, RA8875_WHITE);
  tft.textMode();
  tft.textEnlarge(1);
  delay(2);
  tft.textColor(RA8875_BLACK, RA8875_WHITE);
  tft.textSetCursor(385, 30);
  tft.textWrite("Phone");
  tft.textColor(RA8875_BLACK, RA8875_WHITE);
  tft.textSetCursor(385, 176);
  tft.textWrite("SMS");
}
void drawPhoneScreen() {
  tft.graphicsMode();
  tft.fillScreen(RA8875_BLACK);
  tft.fillRect(425, 0, 54, 80, RA8875_GREEN);
  tft.fillRect(425, 96, 54, 80, RA8875_BLUE);
  tft.fillRect(425, 189, 54, 80, RA8875_RED);
  tft.textMode();
  tft.textColor(RA8875_WHITE, RA8875_BLACK);
  tft.textEnlarge(2);
  delay(2);
  tft.textSetCursor(100, 25);
  tft.textWrite("1");
  tft.textSetCursor(100, 125);
  tft.textWrite("2");
  tft.textSetCursor(100, 225);
  tft.textWrite("3");
  tft.textSetCursor(185, 25);
  tft.textWrite("4");
  tft.textSetCursor(185, 125);
  tft.textWrite("5");
  tft.textSetCursor(185, 225);
  tft.textWrite("6");
  tft.textSetCursor(270, 25);
  tft.textWrite("7");
  tft.textSetCursor(270, 125);
  tft.textWrite("8");
  tft.textSetCursor(270, 225);
  tft.textWrite("9");
  tft.textSetCursor(350, 30);
  tft.textWrite("*");
  tft.textSetCursor(350, 125);
  tft.textWrite("0");
  tft.textSetCursor(350, 225);
  tft.textWrite("#");
  tft.textEnlarge(1);
  tft.textColor(RA8875_WHITE, RA8875_GREEN);
  tft.textSetCursor(437, 8);
  tft.textWrite("Call");
  tft.textColor(RA8875_WHITE, RA8875_BLUE);
  tft.textSetCursor(437, 104);
  tft.textWrite("Home");
  tft.textColor(RA8875_WHITE, RA8875_RED);
  tft.textSetCursor(437, 206);
  tft.textWrite("End");
}
void drawTextMessageScreen() {
  tft.graphicsMode();
  tft.fillScreen(RA8875_BLACK);
  tft.fillRect(425, 0, 54, 80, RA8875_GREEN);
  tft.fillRect(425, 96, 54, 80, RA8875_BLUE);
  tft.fillRect(425, 189, 54, 80, RA8875_RED);
  tft.fillTriangle(60, 205, 15, 230, 60, 255, RA8875_WHITE);
  tft.fillTriangle(85, 205, 130, 230, 85, 255, RA8875_WHITE);
  tft.textMode();
  tft.textEnlarge(1);
  delay(2);
  tft.textColor(RA8875_WHITE, RA8875_BLACK);
  tft.textSetCursor(30, 0);
  tft.textWrite("Index ");
  tft.textSetCursor(100, 20);
  tft.textWrite("From #");
  tft.textColor(RA8875_WHITE, RA8875_GREEN);
  tft.textSetCursor(437, 8);
  tft.textWrite("New");
  tft.textColor(RA8875_WHITE, RA8875_BLUE);
  tft.textSetCursor(437, 104);
  tft.textWrite("Home");
  tft.textColor(RA8875_WHITE, RA8875_RED);
  tft.textSetCursor(437, 206);
  tft.textWrite("Del");
}
void drawAnswerScreen () {
  tft.graphicsMode();
  tft.fillScreen(RA8875_BLACK);
  tft.fillCircle(400, 70, 55, RA8875_GREEN);
  tft.fillCircle(400, 198, 55, RA8875_RED);
  tft.textMode();
  tft.textEnlarge(1);
  delay(2);
  tft.textColor(RA8875_WHITE, RA8875_GREEN);
  tft.textSetCursor(385, 23);
  tft.textWrite("Answer");
  tft.textColor(RA8875_WHITE, RA8875_RED);
  tft.textSetCursor(385, 148);
  tft.textWrite("Ignore");
}
void drawMessagePad() {
  tft.graphicsMode();
  tft.fillScreen(RA8875_BLACK);
  tft.fillRect(0, 220, 59, 59, RA8875_GREEN);
  tft.fillRect(425, 220, 54, 59, RA8875_RED);
  tft.textMode();
  tft.textRotate(false);
  tft.textEnlarge(0);
  delay(2);
  tft.textSetCursor(0, 30);
  tft.textWrite("Q");
  tft.textSetCursor(10, 30);
  tft.textWrite("W");
  tft.textSetCursor(20, 30);
  tft.textWrite("E");
  tft.textSetCursor(30, 30);
  tft.textWrite("R");
  tft.textSetCursor(40, 30);
  tft.textWrite("T");
  tft.textSetCursor(50, 30);
  tft.textWrite("Y");
  tft.textSetCursor(60, 30);
  tft.textWrite("U");
  tft.textSetCursor(70, 30);
  tft.textWrite("I");
  tft.textSetCursor(80, 30);
  tft.textWrite("O");
  tft.textSetCursor(90, 30);
  tft.textWrite("P");
  tft.textSetCursor(0, 40);
  tft.textWrite("A");
  tft.textSetCursor(10, 40);
  tft.textWrite("S");
  tft.textSetCursor(20, 40);
  tft.textWrite("D");
  tft.textSetCursor(30, 40);
  tft.textWrite("F");
  tft.textSetCursor(40, 40);
  tft.textWrite("G");
  tft.textSetCursor(50, 40);
  tft.textWrite("H");
  tft.textSetCursor(60, 40);
  tft.textWrite("J");
  tft.textSetCursor(70, 40);
  tft.textWrite("K");
  tft.textSetCursor(80, 40);
  tft.textWrite("L");
  tft.textSetCursor(90, 40);
  tft.textWrite("; ");
  tft.textSetCursor(0, 50);
  tft.textWrite("Z");
  tft.textSetCursor(10, 50);
  tft.textWrite("X");
  tft.textSetCursor(20, 50);
  tft.textWrite("C");
  tft.textSetCursor(30, 50);
  tft.textWrite("V");
  tft.textSetCursor(40, 50);
  tft.textWrite("B");
  tft.textSetCursor(50, 50);
  tft.textWrite("N");
  tft.textSetCursor(60, 50);
  tft.textWrite("M");
  tft.textSetCursor(70, 50);
  tft.textWrite(", ");
  tft.textSetCursor(80, 50);
  tft.textWrite(".");
  tft.textSetCursor(90, 50);
  tft.textWrite(" / ");
  tft.textRotate(true);
}
void drawPasscodeScreen() {
  tft.graphicsMode();
  tft.fillScreen(RA8875_BLACK);
  tft.textMode();
  tft.textColor(RA8875_WHITE, RA8875_BLACK);
  tft.textEnlarge(2);
  delay(2);
  tft.textSetCursor(100, 25);
  tft.textWrite("1");
  tft.textSetCursor(100, 125);
  tft.textWrite("2");
  tft.textSetCursor(100, 225);
  tft.textWrite("3");
  tft.textSetCursor(185, 25);
  tft.textWrite("4");
  tft.textSetCursor(185, 125);
  tft.textWrite("5");
  tft.textSetCursor(185, 225);
  tft.textWrite("6");
  tft.textSetCursor(270, 25);
  tft.textWrite("7");
  tft.textSetCursor(270, 125);
  tft.textWrite("8");
  tft.textSetCursor(270, 225);
  tft.textWrite("9");
  tft.textSetCursor(350, 125);
  tft.textWrite("0");
  tft.textColor(RA8875_WHITE, RA8875_RED);
  tft.textSetCursor(350, 25);
  tft.textWrite("<-");
  tft.textColor(RA8875_WHITE, RA8875_GREEN);
  tft.textSetCursor(350, 200);
  tft.textWrite("OK");
  tft.textColor(RA8875_WHITE, RA8875_BLUE);
  tft.textEnlarge(1);
  delay(2);
  tft.textSetCursor(437, 21);
  tft.textWrite("Enter Passcode");
}
void switchPasscodePlace() {
  switch (passcodePlace) {
    case 0:
      tft.textSetCursor(20, 0);
      break;
    case 1:
      tft.textSetCursor(20, 20);
      break;
    case 2:
      tft.textSetCursor(20, 40);
      break;
    case 3:
      tft.textSetCursor(20, 60);
      break;
    case 4:
      tft.textSetCursor(20, 80);
      break;
    case 5:
      tft.textSetCursor(20, 100);
      break;
    case 6:
      tft.textSetCursor(20, 140);
      break;
    case 7:
      tft.textSetCursor(20, 120);
      break;
    case 8:
      tft.textSetCursor(20, 140);
      break;
    case 9:
      tft.textSetCursor(20, 160);
      break;
    case 10:
      tft.textSetCursor(20, 180);
      break;
    case 11:
      tft.textSetCursor(20, 200);
      break;
    case 12:
      tft.textSetCursor(20, 220);
      break;
    case 13:
      tft.textSetCursor(20, 240);
      break;
    case 14:
      tft.textSetCursor(20, 260);
      break;
  }
}
void printTouchValues() {
  if (tx > 50 && ty > 50) {
    Serial.print("Touched tX position is |");
    Serial.print(tx);
    Serial.print("|\r\n");
    Serial.print("Touched tY position is |");
    Serial.print(ty);
    Serial.print("|\r\n");
  }
}
//==================================================================================================================================================

//   END OF FUNCTION CALLS

//==================================================================================================================================================