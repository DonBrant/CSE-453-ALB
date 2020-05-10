/*
Arduino Uno Pinout:
  PN532 Module:
    A4 - SDA
    A5 - SCL
  LED:
    9  - Red
    10 - Green
    11 - Blue
  Solenoid:
    8  - (+)  
*/

#include <Wire.h> // Included library for I2C communication
// The PN532 libraries in use here can be found at https://github.com/elechouse/PN532
// The PN532 module is using I2C for communication, make sure the DIP switches on the module are set appropriatley (1 = ON, 2 = OFF)
#include <PN532_I2C.h>
#include <PN532.h>
#include <NfcAdapter.h> // Part of the NDEF library (https://github.com/don/NDEF)
// Registered tags are stored in the EEPROM. The 0th byte stores the count of currently registered tags.
// The remaineder of the EEPROM stores the byte values of each registered tag with starting addresses of 1, 1 + maxUidLength, 1 + 2*maxUidLength, etc.
#include <EEPROM.h>
PN532_I2C pn532_i2c(Wire);
NfcAdapter nfc = NfcAdapter(pn532_i2c);
// Baud rate is the rate of serial communication. This value should match:
// 1. The Arduino (Windows Device Manager > Ports > Arduino Uno > Properties > Port Settings > Bits per seccond)
// 2. The Arduino serial monitor (Ctrl+Shift+M)
const long baudrate = 115200;
const int loopDelay = 5; // The general delay (in seconds) between read attempts
const int registrarDelay = 5; // The delay (in seconds) between detection of a registrar tag and the next read (the tag to be registered)
const int unlockTime = 5; // The amount of time (in seconds) the solenoid is retracted after successful authentication
// Admin UIDs
const int adminUidLength = 7; // Length in bytes of ALL admin UIDs
const unsigned int masterEntries = 1; // Number of master ID entries
const byte masterIDs[] = {
//  0x00,  0x00, 0x00, 0x00, 0x00, 0x00, 0x00 // Example entry
  };
const unsigned int registrarEntries = 1; // Number of registrar ID entries
const byte registrarIDs[] = {
//  0x00,  0x00, 0x00, 0x00, 0x00, 0x00, 0x00 // Example entry
  };
const unsigned int clearEntries = 1; // Number of clear ID entries
const byte clearIDs[] = {
//  0x00,  0x00, 0x00, 0x00, 0x00, 0x00, 0x00 // Example entry
  };
// Max UID length in bytes. Typical UID lengths are 4, 7, and (less typically) 11 bytes.
// Lowering this value will increase the number of tags that can be registered with the tradeoff of dropping support for tags with larger UIDs
const int maxUidLength = 7;
const int regCount = 0; // EEPROM address of the count of registered tags
// Hardware pins
const int ledPinR = 9;
const int ledPinG = 10;
const int ledPinB = 11;
const int solenoidPin = 8;
const int errorBlinkRate = 200;

void setup(void) {
  Serial.begin(baudrate);
  Serial.println("NFC Locker Initializing...");
  // Initialize Indicator LED
  pinMode(ledPinR, OUTPUT);
  pinMode(ledPinG, OUTPUT);
  pinMode(ledPinB, OUTPUT);
  setLed(255,0,0);
  // Initialize Solenoid pin
  pinMode(solenoidPin, OUTPUT);
  digitalWrite(solenoidPin, LOW);
  // Initialize the PN532 NFC module
  nfc.begin();
}
void loop(void) {
  setLed(0,0,0);
  delay(loopDelay * 1000);
  if (nfc.tagPresent()) {
    Serial.println("NFC tag present. Attempting read...");
    setLed(255,15,0); // Orange
    NfcTag tag = nfc.read();
    // tag.print();
    // Serial.println(tag.getUidString());
    int readUidLength = tag.getUidLength();
    // If the UID is of a valid length, continue processing.
    if (readUidLength <= maxUidLength) {
      byte readUid[readUidLength];
      tag.getUid(readUid, readUidLength);

      String readUidString = "Tag UID: ";
      for(int i = 0; i < readUidLength; i++) {
        readUidString += (String(readUid[i], (unsigned char)HEX) + " ");
      }
      Serial.println(readUidString);
      
      // If the tag is registered, open the locker
      if (idRegCheck(readUid, readUidLength)) {
        Serial.println("Registered tag present. Opening the locker...");
        digitalWrite(solenoidPin, HIGH);
        delay(unlockTime*1000);
        digitalWrite(solenoidPin, LOW);
      }
      // If tag is a master tag, open the locker
      else if (idListCheck(readUid, readUidLength, masterIDs, masterEntries)) {
        Serial.println("Master tag present. Opening the locker...");
        digitalWrite(solenoidPin, HIGH);
        delay(unlockTime*1000);
        digitalWrite(solenoidPin, LOW);
      }
      // If the tag is a registrar tag, proceed to register the next tag read
      else if (idListCheck(readUid, readUidLength, registrarIDs, registrarEntries)) {
        setLed(0, 255, 0); // Green
        Serial.println("Registrar tag present. Attempting read of tag to register...");
        delay(registrarDelay * 1000);
        while (!nfc.tagPresent()) {
          delay(loopDelay * 1000);
        }
        setLed(0, 0, 255); // Blue
        Serial.println("NFC tag present. Attempting read for registration...");
        tag = nfc.read();
        readUidLength = tag.getUidLength();
        if (readUidLength <= maxUidLength) {
          readUid[readUidLength];
          tag.getUid(readUid, readUidLength);
          
          readUidString = "Registering tag: ";
          for(int i = 0; i < readUidLength; i++) {
            readUidString += (String(readUid[i], (unsigned char)HEX) + " ");
          }
          Serial.println(readUidString);

          // If the tag is not an admin tag or a registered tag, register it
          if (!idListCheck(readUid, readUidLength, masterIDs, masterEntries) and !idListCheck(readUid, readUidLength, registrarIDs, registrarEntries) and !idListCheck(readUid, readUidLength, clearIDs, clearEntries) and !idRegCheck(readUid, readUidLength)){
            registerTag(readUid, readUidLength);
            ledBlink(0, 255, 0, errorBlinkRate, 4); // Purple
          }
          else {
            ledBlink(0, 0, 255, errorBlinkRate, 3);  // Blue
          }
        }
        else {
          Serial.println("(E102) Tag Error: Attempt to register a tag with UID length of " + String(readUidLength) + " which is longer than the allowed UID length of " + maxUidLength);
          ledBlink(0, 0, 255, errorBlinkRate, 4); // Blue
        }
      }
      // If the tag is a clear tag, proceed to clear all registered tags
      else if (idListCheck(readUid, readUidLength, clearIDs, clearEntries)) {
        setLed(255, 15, 15); // Pink
        Serial.println("Clear tag present. Clearing all registered tags...");      
        for (int i = 0 ; i < EEPROM.length() ; i++) {
          EEPROM.write(i, 0);
        }
      }
      // The tag doesn't belong to a recognized category, do nothing
      else {
        ledBlink(255, 255, 200, errorBlinkRate, 2); // White
        Serial.println("Unrecognized tag.");  
      }   
    }
    else {
      Serial.println("(E101) Tag Error: This tag has a UID length of " + String(readUidLength) + " which is longer than the allowed UID length of " + maxUidLength);
      ledBlink(255, 0, 0, errorBlinkRate, 3); // Red
    }
  }  
}
// Check if the provided uid is part of the provided idList
bool idListCheck(byte *uid, unsigned int uidLength, byte *idList, unsigned int idListEntries) {
  // Serial.println("Comparing ID to an admin ID list");
  if (uidLength != adminUidLength) {
    Serial.println("Comparison mismatch. Tag has UID length " + String(uidLength) + " which doesn't match the admin UID length of " + adminUidLength);
    return false;
  }
  // Serial.println("Admin UID Length = " + String(adminUidLength));
  // Serial.println("Tag UID Length = " + String(uidLength));
  // Serial.println("sizeof ID list = " + String(sizeof(idList)));
  // for each ID in the IDlist
  for (int i = 0; i < idListEntries; i++) {
    bool matchState = true; // No comparison has been done at this point, still potentially a match
    // compare each byte of the current ID list ID to the provided UID
    for (int j = 0; j < adminUidLength; j++) {
      // Serial.println("Comparint byte " + String(uid[j], (unsigned char)HEX) + " to " + String(idList[(i*adminUidLength) + j], (unsigned char)HEX)); 
      if (uid[j] != idList[(i*adminUidLength) + j]) {
        matchState = false; // A byte mismatch was found
        break; // No need to compare the UID to this ID list entry any further
      }
    }
    // if the match state is still true, the UID matched a list entry, return true
    if (matchState) {
      return true;
    }
    // This ID list entry wasn't a match, proceed to check the next entry
  }
  // Serial.println("Comparison mismatch. All ID list entries checked, no match found.");
  return false; // No match was found
}
// TODO: Check if the provided uid is registered in the EEPROM
bool idRegCheck(byte *uid, unsigned int uidLength) {
  int regTagCount = int(EEPROM.read(0)); // The current count of registered tags
  // for each registered tag
  for (int i = 0; i < regTagCount; i++) {
    bool matchState = true; // No comparison has been done at this point, still potentially a match
    // compare each byte of the provided UID with the registered ID
    for (int j = 0; j < uidLength; j++) {
      if (uid[j] != EEPROM.read(((i*maxUidLength) + 1) + j)) {
        matchState = false; // A byte mismatch was found
        break; // No need to compare the UID to this registered ID any further
      }
    }
    // if the match state is still true, the UID matched a registered ID, return true
    if (matchState) {
      return true;
    }
    // This registered ID wasn't a match, proceed to check the next entry
  }
  // Serial.println("Comparison mismatch. All registered IDs checked, no match found.");
  return false;
}
// Register the provided tag to the EEPROM
bool registerTag(byte *uid, unsigned int uidLength) {
  int regTagCount = int(EEPROM.read(0)); // The current count of registered tags
  int writeAddress = (maxUidLength * regTagCount) + 1; // The next starting address to write a tag to
  // Write each byte of the UID to the appropriate place in the EEPROM
  for (int i = 0; i < uidLength; i++) {
    EEPROM.write(writeAddress + i, uid[i]);
  }
  // Increment the count of registered tags
  EEPROM.write(0, EEPROM.read(0) + 1);
  Serial.println("Tag successfully registered.");
  return true;
}
// Set the RGB LED to the provided RGB value (each 'val' should be between 0 and 255)
void setLed (int rVal, int gVal, int bVal) {
  analogWrite (ledPinR, rVal);
  analogWrite (ledPinG, gVal);
  analogWrite (ledPinB, bVal);
}
// Blink the RGB LED given the provided RGB value, the time for each half cycle (time off, time on, in ms) and the number of blinks
void ledBlink (int rVal, int gVal, int bVal, int halfCycle, int blinkCount) {
  for (int i = 0; i < blinkCount; i++) {
    setLed(0, 0, 0);
    delay(halfCycle);
    setLed(rVal, gVal, bVal);
    delay(halfCycle);
  }
}
