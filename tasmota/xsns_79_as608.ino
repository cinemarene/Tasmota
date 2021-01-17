/*
  xsns_79_as608.ino - AS608 and R503 fingerprint sensor support for Tasmota

  Copyright (C) 2021  boaschti and Theo Arends

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef USE_AS608
/*********************************************************************************************\
 * AS608 optical and R503 capacitive Fingerprint sensor
 *
 * Uses Adafruit-Fingerprint-sensor-library with TasmotaSerial
 *
 * Changes made to Adafruit_Fingerprint.h and Adafruit_Fingerprint.cpp:
 * - Replace SoftwareSerial with TasmotaSerial
 * - Add defined(ESP32) where also defined(ESP8266)
\*********************************************************************************************/

#define XSNS_79               79

//#define USE_AS608_MESSAGES

#define D_JSON_FPRINT "FPrint"

#define D_PRFX_FP "Fp"
#define D_CMND_FP_ENROLL "Enroll"
#define D_CMND_FP_DELETE "Delete"
#define D_CMND_FP_COUNT "Count"

const char kAs608Commands[] PROGMEM = D_PRFX_FP "|" D_CMND_FP_ENROLL "|" D_CMND_FP_DELETE "|" D_CMND_FP_COUNT;

void (*const As608Commands[])(void) PROGMEM = { &CmndFpEnroll, &CmndFpDelete, &CmndFpCount };

#ifdef USE_AS608_MESSAGES
const char kAs608Messages[] PROGMEM =
  D_DONE "|" D_FP_PACKETRECIEVEERR "|" D_FP_NOFINGER "|" D_FP_IMAGEFAIL "|" D_FP_UNKNOWNERROR "|" D_FP_IMAGEMESS "|" D_FP_FEATUREFAIL "|" D_FP_NOMATCH "|"
  D_FP_NOTFOUND "|" D_FP_ENROLLMISMATCH "|" D_FP_BADLOCATION "|" D_FP_DBRANGEFAIL "|" D_FP_UPLOADFEATUREFAIL "|" D_FP_PACKETRESPONSEFAIL "|"
  D_FP_UPLOADFAIL "|" D_FP_DELETEFAIL "|" D_FP_DBCLEARFAIL "|" D_FP_PASSFAIL "|" D_FP_INVALIDIMAGE "|" D_FP_FLASHERR "|" D_FP_INVALIDREG "|"
  D_FP_ADDRCODE "|" D_FP_PASSVERIFY;

const uint8_t As608Reference[] PROGMEM = { 0, 1, 2, 3, 4, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 4, 17, 4, 18, 4, 4, 19, 4, 20, 4, 4, 4, 4, 4, 21, 22 };
#else
const char kAs608Messages[] PROGMEM = D_DONE "|" D_FP_UNKNOWNERROR "|" D_FP_NOFINGER;
#endif

#include <TasmotaSerial.h>
#include <Adafruit_Fingerprint.h>

Adafruit_Fingerprint *As608Finger;
TasmotaSerial *As608Serial;

struct AS608 {
  bool selected = false;
  bool is_R5xx = false;
  uint8_t enroll_step = 0;
  uint8_t search_step = 0;
  uint8_t model_number = 0;
} As608;

char* As608Message(char* response, uint32_t index) {
#ifdef USE_AS608_MESSAGES
  if (index > sizeof(As608Reference)) { index = 4; }
  uint32_t i = pgm_read_byte(&As608Reference[index]);
#else
  if (index > 2) { index = 1; }
  uint32_t i = index;
#endif
  return GetTextIndexed(response, TOPSZ, i, kAs608Messages);
}

void As608PublishMessage(const char* message) {
  char romram[TOPSZ];
  snprintf_P(romram, sizeof(romram), message);
  if (strlen(romram) > 0) {
    char json_name[20];
    if (As608.enroll_step) {
      strcpy_P(json_name, PSTR(D_PRFX_FP D_CMND_FP_ENROLL));
    } else {
      strcpy_P(json_name, PSTR(D_JSON_FPRINT));
    }
    Response_P(S_JSON_COMMAND_SVALUE, json_name, romram);
    MqttPublishPrefixTopicRulesProcess_P(RESULT_OR_STAT, json_name);
  }
}

void As608Init(void) {
  if (PinUsed(GPIO_AS608_RX) && PinUsed(GPIO_AS608_TX)) {
    As608Serial = new TasmotaSerial(Pin(GPIO_AS608_RX), Pin(GPIO_AS608_TX), 0);
    As608Finger = new Adafruit_Fingerprint(As608Serial, 0);

    As608Finger->begin(57600);
    if (As608Serial->hardwareSerial()) { ClaimSerial(); }

    if (As608Finger->verifyPassword()) {
      AddLog_P(LOG_LEVEL_INFO, PSTR("AS6: Fingerprint found and password correct"));
      As608ProductInformation();
      As608.selected = true;
    } else {
      AddLog_P(LOG_LEVEL_INFO, PSTR("AS6: Fingerprint not found or password incorrect"));
    }
  }
}

void As608ProductInformation() {
  As608Finger->getTemplateCount();
  AddLog_P(LOG_LEVEL_INFO, PSTR("AS6: Detected with %d fingerprint(s) stored"), As608Finger->templateCount);

  As608Finger->getParameters();
  AddLog_P(LOG_LEVEL_DEBUG, PSTR("AS6: Detected status_reg %d"), As608Finger->status_reg);
  AddLog_P(LOG_LEVEL_DEBUG, PSTR("AS6: Detected system_id %d"), As608Finger->system_id);
  AddLog_P(LOG_LEVEL_DEBUG, PSTR("AS6: Detected device_addr %d"), As608Finger->device_addr);
  AddLog_P(LOG_LEVEL_DEBUG, PSTR("AS6: Detected baud_rate %d"), As608Finger->baud_rate);

  As608Finger->getProductInformation();
  AddLog_P(LOG_LEVEL_INFO, PSTR("AS6: Fingerprint Model is %s"), As608Finger->fpmModel);
  AddLog_P(LOG_LEVEL_DEBUG, PSTR("AS6: Detected fpsBN %s"), As608Finger->fpsBN);
  AddLog_P(LOG_LEVEL_DEBUG, PSTR("AS6: Detected fpsSN %s"), As608Finger->fpsSN);
  AddLog_P(LOG_LEVEL_DEBUG, PSTR("AS6: Detected fpsHWV %d"), As608Finger->fpsHWV);
  AddLog_P(LOG_LEVEL_DEBUG, PSTR("AS6: Detected fpsModel %s"), As608Finger->fpsModel);
  AddLog_P(LOG_LEVEL_DEBUG, PSTR("AS6: Detected fpsWidth %d"), As608Finger->fpsWidth);
  AddLog_P(LOG_LEVEL_DEBUG, PSTR("AS6: Detected fpsHeight %d"), As608Finger->fpsHeight);
  AddLog_P(LOG_LEVEL_DEBUG, PSTR("AS6: Detected tplSize %d"), As608Finger->tplSize);
  AddLog_P(LOG_LEVEL_DEBUG, PSTR("AS6: Detected tplTotal %d"), As608Finger->tplTotal);

  if(strstr(As608Finger->fpmModel, "R5xx") != NULL) {
    As608.is_R5xx = true;
  } else {
    AddLog_P(LOG_LEVEL_INFO, PSTR("AS6:Fingerprint Model is AS608"));
  }
}

int As608GetFingerImage(void) {
  int p = As608Finger->getImage();
  if (p != FINGERPRINT_OK) {
    char response[TOPSZ];
    As608PublishMessage(As608Message(response, p));
  }
  return p;
}

int As608ConvertFingerImage(uint8_t slot) {
  int p = As608Finger->image2Tz(slot);
  if (p != FINGERPRINT_OK) {
    char response[TOPSZ];
    As608PublishMessage(As608Message(response, p));
  }
  return p;
}

void As608Loop(void) {
  if (!As608.enroll_step) {
    searchFinger();
  } else {
    enrollingFinger();
  }
}

/*HELPERS*/
// Search for Finger
void searchFinger() {
  uint32_t p = 0;

  switch (As608.search_step) {
    case 0:
        turnOffAllLeds();
        As608.search_step++;
      break;
    case 1:
      if(hasFinger()) {
        changeLedColor(FINGERPRINT_LED_BREATHING, 30, FINGERPRINT_LED_BLUE, 0, 0);
        As608.search_step++;
      }
      break;
    case 2:
      p = As608Finger->getImage();
      if (p != FINGERPRINT_OK) {
        flashRed();
        As608.search_step = 99;
      } else {
        uint32_t i2Tz = As608Finger->image2Tz();
        if (i2Tz != FINGERPRINT_OK) {
          flashRed();
          As608.search_step = 99;
        } else {
          As608.search_step++;
        }
      }
      break;
    case 3:
      p = As608Finger->fingerSearch();
      if (p != FINGERPRINT_OK) {
        flashRed();
        AddLog_P(LOG_LEVEL_DEBUG, PSTR("AS6: No matching finger"));
      } else {
        // Found a match
        Response_P(PSTR("{\"" D_JSON_FPRINT "\":{\"" D_JSON_ID "\":%d,\"" D_JSON_CONFIDENCE "\":%d}}"), As608Finger->fingerID, As608Finger->confidence);
        MqttPublishPrefixTopicRulesProcess_P(RESULT_OR_STAT, PSTR(D_JSON_FPRINT));
        changeLedColor(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_PURPLE, 0, 1000);
        changeLedColor(FINGERPRINT_LED_OFF, 0, FINGERPRINT_LED_PURPLE, 0, 0);
      }
      As608.search_step = 99;
    break;
    case 99:
      if(hasNoFinger()) {
        As608.search_step = 0;
      }
    break;
  }
}

void enrollingFinger() {
  uint32_t p = 0;

  switch (As608.enroll_step) {
    case 1:
      As608PublishMessage(PSTR(D_FP_ENROLL_PLACEFINGER));
      changeLedColor(FINGERPRINT_LED_BREATHING, 30, FINGERPRINT_LED_BLUE, 0, 0);
      As608.enroll_step++;
      break;
    case 2:
      if(hasFinger()) {
        changeLedColor(FINGERPRINT_LED_BREATHING, 30, FINGERPRINT_LED_PURPLE, 0, 0);
        As608.enroll_step++;
      }
      break;
    case 3:
      // get first image
      if(hasNoFinger()) As608.enroll_step = 1;
      if (As608GetFingerImage() == FINGERPRINT_OK) {
        As608.enroll_step++;
      }
      delay(250);
      break;
    case 4:
      // convert image
      if (As608ConvertFingerImage(1) == FINGERPRINT_OK) {
        As608.enroll_step++;
      } else {
        As608PublishMessage(PSTR(D_FP_ENROLL_RETRY));
        changeLedColor(FINGERPRINT_LED_FLASHING, 30, FINGERPRINT_LED_RED, 0, 1000);
        As608.enroll_step = 1;
      }
      break;
    case 5:
      changeLedColor(FINGERPRINT_LED_OFF, 0, FINGERPRINT_LED_PURPLE, 0, 0);
      As608PublishMessage(PSTR(D_FP_ENROLL_REMOVEFINGER));
      As608.enroll_step++;
      break;
    case 6:
      // Remove finger
      if(hasNoFinger()) {
        As608.enroll_step++;
      }
      break;
    case 7:
      As608PublishMessage(PSTR(D_FP_ENROLL_PLACESAMEFINGER));
      changeLedColor(FINGERPRINT_LED_BREATHING, 30, FINGERPRINT_LED_BLUE, 0, 0);
      As608.enroll_step++;
      break;
    case 8:
      // get second image of finger
      if(hasFinger()) {
        changeLedColor(FINGERPRINT_LED_BREATHING, 30, FINGERPRINT_LED_PURPLE, 0, 0);
        As608.enroll_step++;
      }
      break;
    case 9:
      // get second image of finger
      if(hasNoFinger()) As608.enroll_step = 7;
      if (As608GetFingerImage() == FINGERPRINT_OK) {
        As608.enroll_step++;
      }
      delay(250);
      break;
    case 10:
      // convert second image
      if (As608ConvertFingerImage(2) == FINGERPRINT_OK) {
        As608.enroll_step++;
      } else {
        As608PublishMessage(PSTR(D_FP_ENROLL_RETRY));
        flashRed();
        As608.enroll_step = 7;
      }
      break;
    case 11:
      // Create model
      changeLedColor(FINGERPRINT_LED_OFF, 0, FINGERPRINT_LED_PURPLE, 0, 0);
      p = As608Finger->createModel();
      if (p != FINGERPRINT_OK) {
        char response[TOPSZ];
        As608PublishMessage(As608Message(response, p));
        As608.enroll_step = 99;
        break;
      }

      // Store model
      p = As608Finger->storeModel(As608.model_number);
      char response[TOPSZ];
      As608PublishMessage(As608Message(response, p));
      if (p == FINGERPRINT_OK) {
        As608.model_number = 0;
        As608PublishMessage(PSTR(D_FP_ENROLL_REMOVEFINGER));
        changeLedColor(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_PURPLE, 0, 2000);
        changeLedColor(FINGERPRINT_LED_OFF, 0, FINGERPRINT_LED_PURPLE, 0, 0);
        As608.enroll_step++;
      } else {
        As608.enroll_step = 99;
      }
      break;
    case 12:
      if(hasNoFinger()) {
        As608.enroll_step = 0;
      }
      break;
    case 99:
      As608PublishMessage(PSTR(D_FP_ENROLL_RESTART));
      changeLedColor(FINGERPRINT_LED_FLASHING, 30, FINGERPRINT_LED_RED, 0, 1000);
      As608.enroll_step = 1;
      break;
    default:
      As608PublishMessage(PSTR(D_FP_ENROLL_ERROR));
      changeLedColor(FINGERPRINT_LED_FLASHING, 30, FINGERPRINT_LED_RED, 0, 1000);
      As608.enroll_step = 0;
      As608.model_number = 0;
      break;
    }
}

void flashRed() {
  changeLedColor(FINGERPRINT_LED_FLASHING, 30, FINGERPRINT_LED_RED, 0, 1000);
  changeLedColor(FINGERPRINT_LED_OFF, 0, FINGERPRINT_LED_RED, 0, 0);
}

bool hasFinger() {
  return As608Finger->getImage() != FINGERPRINT_NOFINGER;
}

bool hasNoFinger() {
  return As608Finger->getImage() == FINGERPRINT_NOFINGER;
}

/*********************************************************************************************\
* LEDs
\*********************************************************************************************/

void changeLedColor(uint8_t control, uint8_t speed, uint8_t coloridx, uint8_t count, uint16_t delayTime) {
  if(As608.is_R5xx) {
    As608Finger->LEDcontrol(control, speed, coloridx, count);
    delay(delayTime);
  }
}

void turnOffAllLeds() {
  changeLedColor(FINGERPRINT_LED_OFF, 0, FINGERPRINT_LED_RED, 0, 0);
  changeLedColor(FINGERPRINT_LED_OFF, 0, FINGERPRINT_LED_PURPLE, 0, 0);
  changeLedColor(FINGERPRINT_LED_OFF, 0, FINGERPRINT_LED_BLUE, 0, 0);
}

/*********************************************************************************************\
* Commands
\*********************************************************************************************/

void CmndFpEnroll(void) {
  turnOffAllLeds();
  if (As608.enroll_step) {
    if (0 == XdrvMailbox.payload) {
      // FpEnroll 0 - Stop enrollement
      As608.enroll_step = 0;
      ResponseCmndChar_P(PSTR(D_FP_ENROLL_RESET));
    } else {
      // FpEnroll - Enrollement state
      ResponseCmndChar_P(PSTR(D_FP_ENROLL_ACTIVE));
    }
  } else {
    if ((XdrvMailbox.payload > 0) && (XdrvMailbox.payload <= 128)) {
      // FpEnroll 1..128 - Start enrollement into slot x
      As608.enroll_step = 1;
      As608.model_number = XdrvMailbox.payload;
      ResponseClear();  // Will use loop start message
    } else {
      // FpEnroll - Enrollement state
      ResponseCmndChar_P(PSTR(D_FP_ENROLL_INACTIVE));
    }
  }
}

void CmndFpDelete(void) {
  turnOffAllLeds();
  changeLedColor(FINGERPRINT_LED_BREATHING, 30, FINGERPRINT_LED_RED, 0, 1000);
  if (0 == XdrvMailbox.payload) {
    // FpDelete 0 - Clear database
    As608Finger->emptyDatabase();
    As608Finger->getTemplateCount();
    if (As608Finger->templateCount) {
      ResponseCmndChar_P(PSTR(D_FP_DBCLEARFAIL));
    } else {
      ResponseCmndDone();
    }
  }
  else if ((XdrvMailbox.payload > 0) && (XdrvMailbox.payload <= 128)) {
    // FpDelete 1..128 - Delete single entry from database
    int p = As608Finger->deleteModel(XdrvMailbox.payload);
    char response[TOPSZ];
    ResponseCmndChar(As608Message(response, p));
  }
  changeLedColor(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_RED, 0, 1000);
  turnOffAllLeds();
}

void CmndFpCount(void) {
  // FpCount - Show number of slots used
  As608Finger->getTemplateCount();
  ResponseCmndNumber(As608Finger->templateCount);
}

/*********************************************************************************************\
* Interface
\*********************************************************************************************/

bool Xsns79(uint8_t function) {
  bool result = false;

  if (FUNC_INIT == function) {
    As608Init();
  }
  else if (As608.selected) {
    switch (function) {
      case FUNC_EVERY_100_MSECOND:
        As608Loop();
        break;
      case FUNC_COMMAND:
        result = DecodeCommand(kAs608Commands, As608Commands);
        break;
    }
  }
  return result;
}

#endif  // USE_AS608
