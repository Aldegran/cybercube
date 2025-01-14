#include <WiFi.h>
#include <ESPmDNS.h>
#include <AsyncTCP.h>
#include <SPIFFS.h>
#include <SPI.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <SPIFFSEditor.h>

#define USE_MIDI true
#define USE_FLASHES true

#define IDLE_TIME 300000

#define LCD_DC 14
#define LCD_BL 13
#define LCD_RST 12

#define GAME_MODE_INIT 0
#define GAME_MODE_START 1
#define GAME_MODE_CONNECT_TOP 2
#define GAME_MODE_CONNECT_BOX 3
#define GAME_MODE_CONNECT_LCD 4
#define GAME_MODE_WAIT_BUTTON 5
#define GAME_MODE_WAIT_ANIMATION 14
#define GAME_MODE_WAIT_CAPSULE 6
#define GAME_MODE_CAPSULE_BEGIN 7
#define GAME_MODE_CAPSULE_END 8
#define GAME_MODE_CAPSULE_END_WAIT 17
#define GAME_MODE_CAPSULE_READ 9
#define GAME_MODE_CAPSULE_FAIL_READ 10
#define GAME_MODE_CAPSULE_GAME 11
#define GAME_MODE_CAPSULE_GAME_FAIL 12
#define GAME_MODE_CAPSULE_GAME_OK 13
#define GAME_MODE_IDLE 15
#define GAME_MODE_OTA 16

#define SOUND_SOME 1
#define SOUND_IDLE 2
#define SOUND_FAIL 3
#define SOUND_WAKEUP 4//
#define SOUND_NOISE 5//
#define SOUND_ELECTRICAL_NOICE 6
#define SOUND_CALCULATING 7
#define SOUND_HACKING 8
#define SOUND_BUTTON_WAIT 9
#define SOUND_HACKING_FAIL 10
#define SOUND_INSERT 11
/*
1-1 - апдейт
2-7 - ожидание
3-8 - неудача
4-9 - пробуждение
5-10 - шипение
6-11- электрический разряд
7-12 - вычисление
8-2 - взлом системы
9-3 - капсула внутри
10-4 - неудачный взлом
11-5 - момент вставки капсулы / извлечения
12-6
*/

extern void setLeds();

AsyncWebServer HTTPserver(80);

String ssid = "MYSTERIOUS-NEW";
String password = "0631603132";
String ap_ssid = "CyberCube";
String ap_password = "";

TaskHandle_t Task0;
TaskHandle_t Task1;
TaskHandle_t Task2;
TaskHandle_t Task3;

typedef struct {
  int value[3];
  int delta[3];
} __attribute__((packed, aligned(1))) SettingsStruct;
SettingsStruct encoderData;


typedef struct {
  int lineColorChanceUp;
  int lineChaosUp;
  int lineChanceUp;
  int lineCycleDown;
  int userRadiusDown;
  int lineStepUp;
  int levels;
  int volume;
} __attribute__((packed, aligned(1))) GameSettingsStruct;
GameSettingsStruct gameSettings;

typedef struct {
  byte lineColorChance[2];
  byte lineChance[2];
  byte lineChaos[2];
  byte positionBonus[2];
  byte colorBonus[2];
  int maxScore;
  byte userRadius;
  byte lineCycle;
  byte lineStep;
} __attribute__((packed, aligned(1))) RFIDSettingsStruct;
RFIDSettingsStruct RFIDSettings;

typedef struct {
  byte cylinderTop;
  byte cylinderBottom;
  byte cylinderStatus;
  byte cylinderConnection;
  byte LCDConnection;
  byte DFPlayerConnection;
  byte extendersConnection;
  byte stopEXT;
  byte RFIDok;
  String RFID;
} __attribute__((packed, aligned(1))) ConnectorsStatusStruct;
ConnectorsStatusStruct ConnectorsStatus;

typedef struct {
  byte IDLE;
  byte beginCapsule;
  byte endCapsule;
  byte capsuleFail;
} __attribute__((packed, aligned(1))) AnimationsStruct;
AnimationsStruct Animations;

typedef void (*ButtonCallback) ();
unsigned long timers[3] = { 0, 0, 0 };
byte gameMode = 0;
byte mode = GAME_MODE_INIT;
int LCDDelay = 0;
extern byte inited[6];

void emptyFunction() {}
ButtonCallback BackButtonCallback = emptyFunction;

bool modeAP() {
  Serial.println(F("WiFi mode AP\n"));
  if (ap_ssid.length()) {
    IPAddress apIP(192, 168, 0, 1);
    IPAddress GateWayIP(0, 0, 0, 0);
    IPAddress netMsk(255, 255, 255, 0);

    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, GateWayIP, netMsk);
    WiFi.softAP(ap_ssid.c_str(), ap_password.c_str());
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println(F("======================================================="));
  SPI.begin();

  ConnectorsStatus.cylinderTop = false;
  ConnectorsStatus.cylinderBottom = false;
  ConnectorsStatus.cylinderStatus = false;
  ConnectorsStatus.cylinderConnection = false;
  ConnectorsStatus.LCDConnection = false;
  ConnectorsStatus.DFPlayerConnection = false;
  ConnectorsStatus.extendersConnection = false;
  ConnectorsStatus.stopEXT = false;
  ConnectorsStatus.RFIDok = false;
  xTaskCreatePinnedToCore(taskCore0, "Core0", 10000, NULL, 0, &Task0, 0); //wifi
  delay(500);
  xTaskCreatePinnedToCore(taskCore1, "Core1", 10000, NULL, 0, &Task1, 1); //encoder, led, WS
  delay(500);
  xTaskCreatePinnedToCore(taskCore2, "Core2", 10000, NULL, 0, &Task2, 0); //display
}

void taskCore2(void* parameter) { //display
  Serial.print(F("<<< Task2 running on core "));
  Serial.println(xPortGetCoreID());
  /*{
    RFIDSettings.lineColorChance[0] = 50;
    RFIDSettings.lineColorChance[1] = 50;
    RFIDSettings.lineChance[0] = 10;
    RFIDSettings.lineChance[1] = 10;
    RFIDSettings.lineChaos[0] = 20;
    RFIDSettings.lineChaos[1] = 20;
    RFIDSettings.positionBonus[0] = 2;
    RFIDSettings.positionBonus[1] = 9;
    RFIDSettings.colorBonus[0] = 2;
    RFIDSettings.colorBonus[1] = 9;
    RFIDSettings.maxScore = 1000;
    RFIDSettings.userRadius = 15;
    RFIDSettings.lineCycle = 6;
    RFIDSettings.lineStep = 2;
  }*/
  for (;;) {

    if (mode == GAME_MODE_CONNECT_BOX && ConnectorsStatus.LCDConnection == 2) {
      delay(LCDDelay);
      ConnectorsStatus.stopEXT = true;
      displaySetup();
      ConnectorsStatus.stopEXT = false;
      ConnectorsStatus.LCDConnection = true;
    }
    if (ConnectorsStatus.LCDConnection == true && mode >= GAME_MODE_CONNECT_LCD) {
      displayLoop();
    }
    /*for (byte i = 0; i < 3; i++) {
      if (encoderData.delta[i]) {
        Serial.printf("Encoder %d = %d\r\n", i, encoderData.value[i]);
        encoderData.delta[i] = 0;
      }
    }*/
  }
}

void taskCore0(void* parameter) { //wifi
  Serial.print("<<< Task0 running on core ");
  Serial.println(xPortGetCoreID());
  if (!FSInit()) {
    Serial.println(F("An Error has occurred while mounting SPIFFS"));
  } else {
    Serial.println(F("Load config file"));
    loadConfig();
  }
  serialConsileInit();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  byte c = 0;
  while (WiFi.waitForConnectResult() != WL_CONNECTED && c < 5) {
    Serial.println("Connection Failed!");
    delay(5000);
    c++;
    //ESP.restart();
  }
  if (WiFi.waitForConnectResult() != WL_CONNECTED) modeAP();
  ArduinoOTA.onStart([]() {
    mode = GAME_MODE_OTA;
    soundStop();
    statusChanged();
    displayOta(-1);
    })
    .onEnd([]() {
      soundStop();
      Serial.println("\nEnd");
      })
      .onProgress([](unsigned int progress, unsigned int total) {
        byte p = progress / (total / 100);
        Serial.printf("Progress: %u%%\r", p);
        displayOta(p);
        })
        .onError([](ota_error_t error) {
          Serial.printf("Error[%u]: ", error);
          if (error == OTA_AUTH_ERROR)
            Serial.println("Auth Failed");
          else if (error == OTA_BEGIN_ERROR)
            Serial.println("Begin Failed");
          else if (error == OTA_CONNECT_ERROR)
            Serial.println("Connect Failed");
          else if (error == OTA_RECEIVE_ERROR)
            Serial.println("Receive Failed");
          else if (error == OTA_END_ERROR)
            Serial.println("End Failed");
          });

        ArduinoOTA.begin();

        Serial.println("Ready");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        SPIFFS.begin();
        HTTPserver.addHandler(new SPIFFSEditor(SPIFFS, "admin", "admin"));
        HTTPserver.begin();
        for (;;) {
          ArduinoOTA.handle();
          consoleLoop();
        }
}

void taskCore1(void* parameter) { //encoder, led, WS
  Serial.print(F("<<< Task1 running on core "));
  Serial.println(xPortGetCoreID());
  encoderSetup();
  setupExtender();
  ledSetup();
  MIDISetup();
  mode = GAME_MODE_START;
  statusChanged();
  for (;;) {
    someNoise();
    if (mode != GAME_MODE_IDLE) extenderLoop();
    if (mode == GAME_MODE_START && ConnectorsStatus.cylinderConnection) { // надели верх
      mode = GAME_MODE_CONNECT_TOP;
      statusChanged();
      MIDIPlayRandom();
    }
    if (mode == GAME_MODE_CONNECT_TOP) { // подключили верх
      if (!setupBoxExtenders()) {
        delay(5000);
        if (!ConnectorsStatus.cylinderConnection) { // сняли верх
          mode = GAME_MODE_START;
          statusChanged();
        }
      } else {
        mode = GAME_MODE_CONNECT_BOX;
        cylinderLight(false);
        statusChanged();
      }
    }
    /*if (mode == GAME_MODE_CONNECT_BOX && ConnectorsStatus.cylinderStatus == false) { //поменяли коннектор
      inited[0] = false;
      inited[2] = false;
      inited[3] = false;
      inited[4] = false;
      mode = GAME_MODE_CONNECT_TOP;
      statusChanged();
    }*/
    if (mode == GAME_MODE_CONNECT_BOX && ConnectorsStatus.LCDConnection == true) {
      mode = GAME_MODE_CONNECT_LCD;
      statusChanged();
    }
    if (mode == GAME_MODE_WAIT_BUTTON && checkButton()) { // нажали кнопку
      ledFlash();
      cylinderLight(true);
      while (checkButton()) delay(10);
      mode = GAME_MODE_WAIT_ANIMATION;
      timers[2] = millis();
      statusChanged();
    }

    if (mode == GAME_MODE_WAIT_CAPSULE && ConnectorsStatus.cylinderTop) { // вставили верх капсулы
      mode = GAME_MODE_CAPSULE_BEGIN;
      timers[2] = millis();
      statusChanged();
    }
    ///
    if (mode == GAME_MODE_WAIT_BUTTON) {

    }
    ///
    if (mode == GAME_MODE_CAPSULE_BEGIN && millis() - timers[2] > IDLE_TIME * 2) { //долго впихиваем капсулу или передумали
      mode = GAME_MODE_WAIT_CAPSULE;
      timers[2] = millis();
      statusChanged();
    }
    if (mode == GAME_MODE_CAPSULE_BEGIN && ConnectorsStatus.cylinderBottom) { // встравили низ капсулы
      mode = GAME_MODE_CAPSULE_END_WAIT;
      timers[2] = millis();
      statusChanged();
    }
    if (mode == GAME_MODE_CAPSULE_END_WAIT) { // встравили низ капсулы
      if (ConnectorsStatus.cylinderStatus) {
        cylinderLight(false);
        mode = GAME_MODE_CAPSULE_END;
        statusChanged();
        soundPlay(SOUND_INSERT, false);
      } else if (millis() - timers[2] > 5000) {
        mode = GAME_MODE_CAPSULE_FAIL_READ;
        soundPlay(SOUND_FAIL, false);
        statusChanged();
      }
    }
    if (mode == GAME_MODE_CAPSULE_END) {
      for (byte i = 0; i < 10;i++) {
        Serial.println(F("Read RFID data..."));
        if (readRFID()) break;
        delay(1000);
      }
      Serial.println(F("Read RFID complete"));
      if (ConnectorsStatus.RFIDok) {
        if (!readRFIDFile()) {
          mode = GAME_MODE_CAPSULE_FAIL_READ;
          statusChanged();
          return;
        }
      } else {
        ConnectorsStatus.RFIDok = false;
        mode = GAME_MODE_CAPSULE_FAIL_READ;
        statusChanged();
        return;
      }
      mode = GAME_MODE_CAPSULE_READ;
      showUser(false);
      setLeds();
      statusChanged();
    }

    //if (mode == GAME_MODE_CAPSULE_GAME) { // игра
    //  ledLoop();
    encoderLoop();
    //}
    if (mode == GAME_MODE_CAPSULE_GAME_OK || mode == GAME_MODE_CAPSULE_GAME_FAIL) { // после игры
      if (ConnectorsStatus.cylinderTop) { //ждём пока вытащит
        mode = GAME_MODE_CONNECT_LCD;
        timers[2] = millis();
        statusChanged();
      }
    }
    if (mode == GAME_MODE_WAIT_CAPSULE || mode == GAME_MODE_WAIT_BUTTON) {
      if (millis() - timers[2] > IDLE_TIME) { // долго ждали капсулу или кнопку
        mode = GAME_MODE_IDLE;
        Animations.IDLE = false;
        soundStop();
        cylinderLight(false);
        statusChanged();
        delay(10);
        extIDLE();
        ledIDLE();
      }
    }
    if (mode == GAME_MODE_IDLE && checkButton()) { // проснулись
      while (checkButton()) delay(10);
      mode = GAME_MODE_WAIT_ANIMATION;
      MIDIPlayRandom();
      cylinderLight(true);
      extDemo();
      timers[2] = millis();
      statusChanged();
    }
  }
}

void loop() {
  vTaskDelete(NULL);
}

int userColor(byte n) {
  if (n == 44)return 0;
  byte c = n % 7;
  if (c == 6)return n / 7 + 1;
  else if (c < 2) return n / 7;
  return 6;
}

void statusChanged() {
  Serial.print(F("STATUS: ["));
  Serial.print(mode);
  switch (mode) {
  case GAME_MODE_INIT: Serial.println(F("] GAME_MODE_INIT")); break;
  case GAME_MODE_START: Serial.println(F("] GAME_MODE_START")); break;
  case GAME_MODE_CONNECT_TOP: Serial.println(F("] GAME_MODE_CONNECT_TOP")); break;
  case GAME_MODE_CONNECT_BOX: Serial.println(F("] GAME_MODE_CONNECT_BOX")); break;
  case GAME_MODE_CONNECT_LCD: Serial.println(F("] GAME_MODE_CONNECT_LCD")); break;
  case GAME_MODE_WAIT_BUTTON: Serial.println(F("] GAME_MODE_WAIT_BUTTON")); break;
  case GAME_MODE_WAIT_ANIMATION: Serial.println(F("] GAME_MODE_WAIT_ANIMATION")); break;
  case GAME_MODE_WAIT_CAPSULE: Serial.println(F("] GAME_MODE_WAIT_CAPSULE")); break;
  case GAME_MODE_CAPSULE_BEGIN: Serial.println(F("] GAME_MODE_CAPSULE_BEGIN")); break;
  case GAME_MODE_CAPSULE_END: Serial.println(F("] GAME_MODE_CAPSULE_END")); break;
  case GAME_MODE_CAPSULE_END_WAIT: Serial.println(F("] GAME_MODE_CAPSULE_END_WAIT")); break;
  case GAME_MODE_CAPSULE_READ: Serial.println(F("] GAME_MODE_CAPSULE_READ")); break;
  case GAME_MODE_CAPSULE_FAIL_READ: Serial.println(F("] GAME_MODE_CAPSULE_FAIL_READ")); break;
  case GAME_MODE_CAPSULE_GAME: Serial.println(F("] GAME_MODE_CAPSULE_GAME")); break;
  case GAME_MODE_CAPSULE_GAME_FAIL: Serial.println(F("] GAME_MODE_CAPSULE_GAME_FAIL")); break;
  case GAME_MODE_CAPSULE_GAME_OK: Serial.println(F("] GAME_MODE_CAPSULE_GAME_OK")); break;
  case GAME_MODE_IDLE: Serial.println(F("] GAME_MODE_IDLE")); break;
  case GAME_MODE_OTA: Serial.println(F("] GAME_MODE_OTA")); break;
  }
}
