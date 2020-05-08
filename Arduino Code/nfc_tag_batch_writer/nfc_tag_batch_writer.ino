/*

Arduino Uno Pinout:
  I2C devices:
    A4 - SDA
    A5 - SCL
  MicroSD Breakout:
    11 - MOSI
    12 - MISO
    13 - SCK
     4 - CS
    
*/

#include <Wire.h> // Included library for I2C communication
// The PN532 libraries in use here can be found at https://github.com/elechouse/PN532
#include <PN532_I2C.h>
#include <PN532.h>
#include <NfcAdapter.h> // Part of the NDEF library (https://github.com/don/NDEF)
// The PN532 is using I2C for communication, make sure the DIP switches on the module are appropriatley (1 = ON, 2 = OFF)
PN532_I2C pn532_i2c(Wire);
NfcAdapter nfc = NfcAdapter(pn532_i2c);
// Adafruit libraries used for the Adafruit HT15K33 Backpack 7-segment display, available through Arduino library manager (Ctrl+Shit+I)
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"
// The MicroSD Card breakout board utilizes the built in SD and SPI libraries
#include <SPI.h>
#include <SD.h>
// Baud rate is the rate of serial communication. This value should match:
// 1. The Arduino (Windows Device Manager > Ports > Arduino Uno > Properties > Port Settings > Bits per seccond)
// 2. The Arduino serial monitor (Ctrl+Shift+M)
const long baudrate = 115200;
const int dispBright = 5; // The brightness of the 7-segment display. Value between 0 (dark) and 15 (bright)
const int dispBlink = 0; // The blink rate of the display. 0 = no blink, 1 = fast blink, 2 = medium blink, 3 = slow blink
const int cs = 4; // The pin used for the MicroSD breakout board's chip select (CS)
const String keyFileName = "keys.csv"; // The name of the CSV file containing the NFC keys to be written. Should be stored in the root of the SD card
const int writeDelay = 5; // The added delay (in seconds) between write attempts after a successful write
const int loopDelay = 5; // The general delay (in seconds) between write attempts
File keyCSV; // Global declaration of the File object repesenting the keys CSV file
String queuedKey; // Global declaration of the String representing the key queued for writing
bool writeSuccess = false; // Determines if the loop should queue up the next key
int keyCount = 1; // The row number associated with the current key
 
Adafruit_7segment dispMatrix = Adafruit_7segment();

void setup(void) {
  Serial.begin(baudrate);
  Serial.println("NFC Tag Batch Writer");
  // Initialize the PN532 NFC module
  nfc.begin();
  // Initialize the Adafruit HT15K33 Backpack 7-segment display
  dispMatrix.begin(0x70);
  dispMatrix.setBrightness(dispBright);
  dispMatrix.blinkRate(dispBlink);
  dispMatrix.writeDigitRaw(0, 55);  // N
  dispMatrix.writeDigitRaw(1, 113); // F
  dispMatrix.writeDigitRaw(3, 57);  // C
  dispMatrix.writeDisplay();
  // Initialize the SD card
  if (!SD.begin(cs)) {
    Serial.println("(E101) SD Error: Failed to initialize SD card");
    dispMatrix.print(101);
    dispMatrix.writeDigitRaw(0, 121); // E
    dispMatrix.writeDisplay();
    while(1);
  }
  Serial.println("SD Card initialized");
  // Check if the file containing the NFC keys to be written exists
  if (!SD.exists(keyFileName)) {
    Serial.println("(E102) File Error: \"" + keyFileName + "\" not found");
    dispMatrix.print(102);
    dispMatrix.writeDigitRaw(0, 121); // E
    dispMatrix.writeDisplay();
    while(1);
  }
  Serial.println("\"" + keyFileName + "\" found");
  // Open the file containing the NFC keys to be written
  keyCSV = SD.open(keyFileName);
  if (!keyCSV) {
    Serial.println("(E103) File Error: \"" + keyFileName + "\" couldn't be opened");
    dispMatrix.print(103);
    dispMatrix.writeDigitRaw(0, 121); // E
    dispMatrix.writeDisplay();
    keyCSV.close();
    while(1);
  }
  Serial.println("\"" + keyFileName + "\" opened");
  queuedKey = keyCSV.readStringUntil(','); // Queue up the first key as the string stored in the first column first row cell
  if (queuedKey.length() == 0) {
    Serial.println("(E104) File Error: \"" + keyFileName + "\" is empty");
    dispMatrix.print(104);
    dispMatrix.writeDigitRaw(0, 121); // E
    dispMatrix.writeDisplay();
    keyCSV.close();
    while(1);
  }
  Serial.println("Key " + String(keyCount) + " queued: " + queuedKey);
  dispMatrix.print(keyCount);
  dispMatrix.writeDisplay();
}

void loop(void) {
  delay(loopDelay * 1000);

  // the previous loop ended in a successful write, queue up the next key
  if (writeSuccess) {
    Serial.println("Successful write.");
    String rowRemainder = keyCSV.readStringUntil('\n');
    queuedKey = keyCSV.readStringUntil(',');
    if (queuedKey.length() == 0) {
      Serial.println("EOF");
      dispMatrix.writeDigitRaw(0, 94);  // d
      dispMatrix.writeDigitRaw(1, 63);  // O
      dispMatrix.writeDigitRaw(3, 55);  // N
      dispMatrix.writeDigitRaw(4, 121); // E
      dispMatrix.writeDisplay();
      keyCSV.close();
      while(1);
    }
    keyCount += 1;
    writeSuccess = false;
    Serial.println("Key " + String(keyCount) + " queued: " + queuedKey);
    dispMatrix.print(keyCount);
    dispMatrix.writeDisplay();
    delay(writeDelay * 1000);
  }

  if (nfc.tagPresent()) {
    Serial.println("NFC tag present. Attempting write...");
    NdefMessage message = NdefMessage();
    message.addTextRecord(queuedKey);
    writeSuccess = nfc.write(message);
    if (!writeSuccess) {
      Serial.println("A tag was detected but the write failed. Attempting another write...");
    }
  }


//  Serial.println("\nPlace an unformatted Mifare Classic tag on the reader.");
//  if (nfc.tagPresent()) {
//
//    bool success = nfc.format();
//    if (success) {
//      Serial.println("\nSuccess, tag formatted as NDEF.");
//    } else {
//      Serial.println("\nFormat failed.");
//    }
//
//  }

}
