#include <esp_camera.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <BluetoothSerial.h>
#include <math.h>
#include <Preferences.h>
#include <SD.h>
#include <FS.h>

// CAMERA CONFIG (Please uncomment one what you use.)
//#define CAMERA_MODEL_WROVER_KIT
//#define CAMERA_MODEL_ESP_EYE
//#define CAMERA_MODEL_M5STACK_PSRAM
//#define CAMERA_MODEL_M5STACK_V2_PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE
//#define CAMERA_MODEL_M5STACK_ESP32CAM
#define CAMERA_MODEL_AI_THINKER
//#define CAMERA_MODEL_TTGO_T_JOURNAL

#include "camera_pins.h"

// HTTP CONFIG (Not Recommend to Change)
#define STRING_BOUNDARY "123456789000000000000987654321"
#define STRING_MULTIHEAD02 "Content-Disposition: form-data; name=\"upfile\"; filename=\"espcam.jpg\""
#define STRING_MULTIHEAD03 "Content-Type: image/jpg"
#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */

// PIN CONFIG
#define LED_PIN_LIGHT 4
#define CDS_PIN 12
#define BTN_PIN 2

#define SD_SCK  14
#define SD_MOSI 15
#define SD_SS 13
#define SD_MISO  2

// BLUETOOTH CONFIG
#define BLUETOOTH_ID "IntervalCamXV"

// EEPROM COINFIG
#define EEP_NAME_SPACE "UserSettings"
#define EEP_WIFI_UPLOAD "USE_WIFI"
#define EEP_SD_SAVE "USE_SD"
#define EEP_BRIGHTNESS "BRIGHT"
#define EEP_SLEEP "SLEEP"
#define EEP_CRITERIA "CDS_CRITERIA"
#define EEP_SSID "WIFI_SSID"
#define EEP_PASS "WIFI_PASS"
#define EEP_URL "WIFI_URL"
#define EEP_FRAMESIZE "FRAMESIZE"

// EEPROM SD FILE NAME
#define SD_NAME_SPACE "SD"
#define SD_COUNT "SC"

Preferences preferences;

// EEPROM 
struct USERCONFIG{
  bool enable_wifi;
  bool enable_sd;
  String wifi_ssid;
  String wifi_pass;
  int sleep_time_sec;
  String post_url;
  int cds_criteria;
  float brightness;
  int framesize;
};

struct USERCONFIG uc;

// true: goto blutooth mode 
// false: goto camera mode
bool SETMODE = false;
int wifiRetry = 0;
WiFiMulti wifiMulti;
BluetoothSerial blue;
char btChar[512];
int btCharIdx = 0;
bool CAM_READY = true;
bool SerialOut = false;

const char BLUE_RECV_START = '#';
const char BLUE_RECV_FIN = ';';
const char BLUE_SET_SEP = '/';

void setup() {
  if(SerialOut) Serial.begin(115200);
  if(SerialOut) Serial.println();

  print_wakeup_reason();
  eepLoad();
  esp_sleep_enable_timer_wakeup(uc.sleep_time_sec * uS_TO_S_FACTOR);

  if(SerialOut) Serial.println("[INIT] User Pins...");
  pinSetting();  
  
  if(SerialOut) Serial.println("[INIT] Cam...");
  cameraSetUp();

  if(SerialOut) Serial.println("[INIT] Wifi...");
  wifiRetry = 0;
  wifiMulti.addAP(uc.wifi_ssid.c_str(), uc.wifi_pass.c_str());
    
  if(SerialOut) Serial.println("[INIT] Completed.");
}

void eepLoad(){
  if(SerialOut) Serial.println("---- EEPROM Loading START ----");
  preferences.begin(EEP_NAME_SPACE, false);
  uc.enable_wifi    = preferences.getBool(EEP_WIFI_UPLOAD);
  uc.enable_sd      = preferences.getBool(EEP_SD_SAVE);
  uc.wifi_ssid      = preferences.getString(EEP_SSID);
  uc.wifi_pass      = preferences.getString(EEP_PASS);
  uc.sleep_time_sec = preferences.getInt(EEP_SLEEP);
  uc.post_url       = preferences.getString(EEP_URL);
  uc.cds_criteria   = preferences.getInt(EEP_CRITERIA);
  uc.brightness     = preferences.getFloat(EEP_BRIGHTNESS);
  uc.framesize     = preferences.getInt(EEP_FRAMESIZE);
  preferences.end();
  if(SerialOut) Serial.println("---- EEPROM Loading END ----");
  
  if(SerialOut) Serial.println("---- User Settings START ----");
  printUserConfig();
  if(SerialOut) Serial.println("---- Validating the Settings ----");
  if(validateSetting() == false){
    if(SerialOut) Serial.println("---- Corrected User Settings after validation ----");
    printUserConfig();
  }
  if(SerialOut) Serial.println("---- User Settings END ----");
}

bool validateSetting(){
  bool result = true;
  if(uc.enable_wifi && uc.wifi_ssid == ""){
    uc.enable_wifi = false;
    result = false;
    if(SerialOut) Serial.println("Wifi SSID is undefined. Set Wifi OFF.");
  }
  if(uc.enable_wifi && uc.post_url == ""){
    uc.enable_wifi = false;
    result = false;
    if(SerialOut) Serial.println("Wifi POST URL is undefined. Set Wifi OFF.");
  }
  if(uc.sleep_time_sec < 10){
    uc.sleep_time_sec = 10;
    result = false;
    if(SerialOut) Serial.println("Sleep time should be >=10 sec. Set 10 sec.");
  }
  if(uc.cds_criteria <= 0){
    uc.cds_criteria = 4096 ;
    result = false;
    if(SerialOut) Serial.println("CDS Criteria should be >=0 value. Set 4096.");
  }
  if(uc.cds_criteria > 4095){
    uc.cds_criteria = 4096 ;
    result = false;
    if(SerialOut) Serial.println("CDS Max value is grater than 4095. Turn CDS off. (=4096)");
  }
  if(uc.brightness < -2 || uc.brightness > 2){
    uc.brightness = 0;
    result = false;
    if(SerialOut) Serial.println("Camera Brightness should be -2.0 <= val <= 2.0. Set 0.");
  }
  if(result){
    if(SerialOut) Serial.println("Validation OK.");
  }
  return result;
}

void eepWrite(){
  if(SerialOut) Serial.println("---- EEPROM Writing START ----");
  preferences.begin(EEP_NAME_SPACE, false);
  preferences.putBool(EEP_WIFI_UPLOAD, uc.enable_wifi);
  preferences.putBool(EEP_SD_SAVE, uc.enable_sd);
  preferences.putString(EEP_SSID, uc.wifi_ssid);
  preferences.putString(EEP_PASS, uc.wifi_pass);
  preferences.putInt(EEP_SLEEP, uc.sleep_time_sec);
  preferences.putString(EEP_URL, uc.post_url);
  preferences.putInt(EEP_CRITERIA, uc.cds_criteria);
  preferences.putFloat(EEP_BRIGHTNESS, uc.brightness);
  preferences.putInt(EEP_FRAMESIZE, uc.framesize);
  preferences.end();
  printUserConfig();
  if(SerialOut) Serial.println("---- EEPROM Writing END ----");
}

void printUserConfig(){
  if(SerialOut) Serial.print(" - enable_wifi: ");
  if(SerialOut) Serial.println(uc.enable_wifi);
  if(SerialOut) Serial.print(" - enable_sd: ");
  if(SerialOut) Serial.println(uc.enable_sd);
  if(SerialOut) Serial.print(" - wifi_ssid: ");
  if(SerialOut) Serial.println(uc.wifi_ssid);
  if(SerialOut) Serial.print(" - wifi_pass: ");
  if(SerialOut) Serial.println(uc.wifi_pass);
  if(SerialOut) Serial.print(" - sleep_time_sec: ");
  if(SerialOut) Serial.println(uc.sleep_time_sec);
  if(SerialOut) Serial.print(" - post_url: ");
  if(SerialOut) Serial.println(uc.post_url);
  if(SerialOut) Serial.print(" - cds_criteria: ");
  if(SerialOut) Serial.println(uc.cds_criteria);
  if(SerialOut) Serial.print(" - brightness: ");
  if(SerialOut) Serial.println(uc.brightness);
  if(SerialOut) Serial.print(" - framesize: ");
  if(SerialOut) Serial.println(getFrameSizeStr());
}

void cameraSetUp(){
  CAM_READY = false; 
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = getFrameSize();
  config.jpeg_quality = 14; // 10:max quality  63:min quality
  if(psramFound()){
    if(SerialOut) Serial.println("psramFound");
    config.fb_count = 2;
  } else {
    if(SerialOut) Serial.println("not psramFound");
    config.fb_count = 1;
  }

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    if(SerialOut) Serial.printf("Camera init failed with error 0x%x", err);
    if(SerialOut) Serial.println("");
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  // set brightness (default value is a little dark than actual)
  s->set_brightness(s, uc.brightness);
  // drop down frame size for higher initial frame rate
  s->set_framesize(s, config.frame_size);

  CAM_READY = true;
}

framesize_t getFrameSize(){
  switch(uc.framesize){
    case 1: 
      return FRAMESIZE_QVGA;
    case 2: 
      return FRAMESIZE_CIF;
    case 3: 
      return FRAMESIZE_VGA;
    case 4: 
      return FRAMESIZE_SVGA;
    case 5: 
      return FRAMESIZE_XGA;
    case 6: 
      return FRAMESIZE_SXGA;
    case 7: 
      return FRAMESIZE_UXGA;
    default:
     return FRAMESIZE_VGA;
  }
}

String getFrameSizeStr(){
  switch(uc.framesize){
    case 1: 
      return "1:FRAMESIZE_QVGA";
    case 2: 
      return "2:FRAMESIZE_CIF";
    case 3: 
      return "3:FRAMESIZE_VGA";
    case 4: 
      return "4:FRAMESIZE_SVGA";
    case 5: 
      return "5:FRAMESIZE_XGA";
    case 6: 
      return "6:FRAMESIZE_SXGA";
    case 7: 
      return "7:FRAMESIZE_UXGA";
    default:
     return "DEFAULT:FRAMESIZE_VGA";
  }
}


void pinSetting(){
  // Please put your gpio assign here
  pinMode(LED_PIN_LIGHT, OUTPUT);
  pinMode(BTN_PIN, INPUT);
  digitalWrite(LED_PIN_LIGHT, LOW);
  delay(20);
  
  if(digitalRead(BTN_PIN) == LOW){
    if(SerialOut) Serial.println("[INIT] BLUETOOTH MODE ON");
    brinkLed(3);
    digitalWrite(LED_PIN_LIGHT, LOW);
    blue.begin(BLUETOOTH_ID);
    SETMODE = true;
  }else{
    if(SerialOut) Serial.println("[INIT] INTERVAL CAM MODE ON");
    blue.begin(BLUETOOTH_ID); // To Find PSRAM
    SETMODE = false;
  }
}

void offPins(){
  digitalWrite(LED_PIN_LIGHT, LOW);
  delay(50);
}

void captureAndPost(){
  bool light = cds();
  if(light){
    digitalWrite(LED_PIN_LIGHT,HIGH);
    delay(1000);
  }
  // Capture BMP
  if(SerialOut) Serial.println("[CAM] Cam start");
  delay(200);
  camera_fb_t *fb = NULL;
  
  if(SerialOut) Serial.println("[CAM] Cam capturing...");
  fb = esp_camera_fb_get();
  if (!fb){
    if(SerialOut) Serial.println("[CAM] Capture failed");
    return;
  }
  delay(100);

  // convert to jpg
  if(SerialOut) Serial.println("[CAM] JPG converting...");
  uint8_t * jpg = NULL;
  size_t jpg_len = 0;
  bool converted = frame2bmp(fb, &jpg, &jpg_len);
  esp_camera_fb_return(fb);
  if(!converted){
    if(SerialOut) Serial.println("[CAM] JPG Convert Failed");
    return;
  }
  digitalWrite(LED_PIN_LIGHT,LOW);
  delay(100);

  // SD Saving
  if(uc.enable_sd){
    sdStoring(fb);
  }

  // Wifi Connecting
  if(uc.enable_wifi){ // WIFI START
    wifiPosting(fb->buf, fb->len);
  }
  
  free(jpg);
}

void sdStoring(camera_fb_t *fb){
  if(SerialOut) Serial.println("[SD] SD Card Storing...");

  // Generate File Number with EEPROM
  preferences.begin(SD_NAME_SPACE,false);
  int sc = preferences.getInt(SD_COUNT);
  char fileNum[5];
  sprintf(fileNum, "%04d", sc);
  if(sc == 9999){
    sc = 0;
  }else{
    sc++;
  }
  preferences.putInt(SD_COUNT, sc);
  preferences.end();

  // Storing the File
  String filename = "/IWC_";
  filename += fileNum;
  filename += ".jpg";
  if(SerialOut) Serial.print("[SD] File Path: ");
  if(SerialOut) Serial.println(filename);
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_SS);
  SD.begin(SD_SS);
  File file = SD.open(filename.c_str(), FILE_WRITE);
  file.write(fb->buf, fb->len); 
  file.close();
  delay(300);
  SD.end();
  
  if(SerialOut) Serial.println("[SD] SD Card Storing Completed.");
}

void wifiPosting(uint8_t * jpg, size_t jpg_len){
  for(int i=0;i<3;i++){
    if(SerialOut) Serial.println("[HTTP] Wifi connecting...");
    if((wifiMulti.run() == WL_CONNECTED)) {
      if(SerialOut) Serial.println("[HTTP] Posting the picture...");
      HTTPClient http;
      http.begin(uc.post_url.c_str());

      // Set Multipart Headers
      String stConType ="";
      stConType += "multipart/form-data; boundary=";
      stConType += STRING_BOUNDARY;
      http.addHeader("Content-Type",stConType);

      String stMHead="";
      stMHead += "--";
      stMHead += STRING_BOUNDARY;
      stMHead += "\r\n";
      stMHead += STRING_MULTIHEAD02;
      stMHead += "\r\n";
      stMHead += STRING_MULTIHEAD03;
      stMHead += "\r\n";
      stMHead += "\r\n";

      String stMTail="";
      stMTail += "\r\n";
      stMTail += "--";
      stMTail += STRING_BOUNDARY;
      stMTail += "--";
      stMTail += "\r\n";
      stMTail += "\r\n";  

      if(SerialOut) Serial.println("-----------------------------------");
      if(SerialOut) Serial.println(uc.post_url);
      if(SerialOut) Serial.println(stConType);
      if(SerialOut) Serial.println(stMHead);
      if(SerialOut) Serial.println(stMTail);
      if(SerialOut) Serial.println(jpg_len);
      if(SerialOut) Serial.println("-----------------------------------");
    
      uint32_t iNumMHead = stMHead.length();
      uint32_t iNumMTail = stMTail.length();
      uint32_t iNumTotalLen = iNumMHead + iNumMTail + jpg_len;
      uint8_t *uiB = (uint8_t *)malloc(sizeof(uint8_t)*iNumTotalLen);
    
      for(int i=0; i < iNumMHead; i++){
        uiB[0+i] = stMHead[i];
      }
      for(int i=0; i < jpg_len; i++){
        uiB[iNumMHead+i] = jpg[i];
      }
      uint32_t offset = iNumMHead + jpg_len;
      for(int i=0; i < iNumMTail; i++){
        uiB[offset + i] = stMTail[i];
      }
    
      if(SerialOut) Serial.println("[HTTP] Posting");
      int httpCode = http.POST(uiB, iNumTotalLen);
      if(httpCode >= 200 || httpCode < 300){
        if(SerialOut) Serial.println("[HTTP] Posted");
        brinkLed(2);
      }else{
        if(SerialOut) Serial.println("[HTTP] Get Error");
      }    
      free(uiB);
      http.end();
      if(SerialOut) Serial.println("[HTTP] Wifi End");
      return;
    }else{
      delay(4000);
      continue;
    }
  } // Wifi END
}

bool cds(){
  if(SerialOut) Serial.println("[CDS] Reading...");
  int val = analogRead(CDS_PIN);
  if(SerialOut) Serial.print("[CDS] Value -> ");
  if(SerialOut) Serial.println(val);
  if(SerialOut) Serial.print("[CDS] cds_criteria -> ");
  if(SerialOut) Serial.println(uc.cds_criteria);
  return val > uc.cds_criteria;
}

void brinkLed(int t){
  for(int i=0;i<t;i++){
    digitalWrite(LED_PIN_LIGHT, LOW);
    delay(150);
    digitalWrite(LED_PIN_LIGHT, HIGH);
    delay(150);
  }
  digitalWrite(LED_PIN_LIGHT, LOW);
}

void deepSleep(){
  if(SerialOut) Serial.print("[SLEEP] Getting into Deep sleep ");
  if(SerialOut) Serial.print(uc.sleep_time_sec);
  if(SerialOut) Serial.println("sec...");
  if(SerialOut) Serial.flush(); 
  esp_deep_sleep_start();
}

void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();
  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0     : if(SerialOut) Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1     : if(SerialOut) Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER    : if(SerialOut) Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : if(SerialOut) Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP      : if(SerialOut) Serial.println("Wakeup caused by ULP program"); break;
    default : if(SerialOut) Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}

void blueToothSetting(){
  if(blue.available()){
    int rp = blue.read();
    if(rp == BLUE_RECV_FIN){
      printBtCharInSerial();
      execBTCommand();
      btCharIdx = -1;
    }else if(rp == BLUE_RECV_START){
      btCharIdx = -1;
    }else{
      btCharIdx++;
      btChar[btCharIdx] = rp;
    }
  }
  
  delay(50);
}

void printBtCharInSerial(){
  if(SerialOut) Serial.print("printBtCharInSerial: idx is ");
  if(SerialOut) Serial.println(btCharIdx);
  for(int i=0;i<=btCharIdx;i++){
    if(SerialOut) Serial.print(btChar[i]);
  }
  if(SerialOut) Serial.println();
}

void execBTCommand(){
  /** GETTER **/
  if(startsWith("GET:SETTINGS")){
    blue.print("#");
    blue.print("BRIGHTNESS:");
    blue.print(round(uc.brightness * 10));
    blue.print(",");
    
    blue.print("CDS_CRITERIA:");
    blue.print(uc.cds_criteria);
    blue.print(",");
    
    blue.print("SLEEP_TIME:");
    blue.print(uc.sleep_time_sec);
    blue.print(",");
    
    blue.print("FRAMESIZE:");
    blue.print(uc.framesize);
    blue.print(",");
    
    blue.print("USESD:");
    if(uc.enable_sd){
      blue.print(1);
    }else{
      blue.print(0);
    }
    blue.print(",");
    
    blue.print("USEWIFI:");
    if(uc.enable_wifi){
      blue.print(1);
    }else{
      blue.print(0);
    }
    blue.print(",");
    
    blue.print("WIFISSID:");
    blue.print(uc.wifi_ssid);
    blue.print(",");
    
    blue.print("WIFIPASS:");
    blue.print(uc.wifi_pass);
    blue.print(",");
    
    blue.print("WIFIURL:");
    blue.print(uc.post_url);
    blue.println(";");
    return;
  }
  
  /** SETTER **/
  if(startsWith("SET:BRIGHTNESS")){
    uc.brightness = getIntVal() / 10;
    if(SerialOut) Serial.print("SET:BRIGHTNESS => ");
    if(SerialOut) Serial.println(uc.brightness);
    blue.println("OK:BRIGHTNESS");
    return;
  }
  if(startsWith("SET:CDS_CRITERIA")){
    uc.cds_criteria = getIntVal();
    if(SerialOut) Serial.print("SET:CDS_CRITERIA => ");
    if(SerialOut) Serial.println(uc.cds_criteria);
    blue.println("OK:CDS_CRITERIA");
    return;
  }
  if(startsWith("SET:SLEEP_TIME")){
    uc.sleep_time_sec = getIntVal();
    if(SerialOut) Serial.print("SET:SLEEP => ");
    if(SerialOut) Serial.println(uc.sleep_time_sec);
    blue.println("OK:SLEEP_TIME");
    return;
  }
  if(startsWith("SET:FRAMESIZE")){
    uc.framesize = getIntVal();
    if(SerialOut) Serial.print("SET:FRAMESIZE => ");
    if(SerialOut) Serial.println(getFrameSizeStr());
    blue.println("OK:FRAMESIZE");
    return;
  }
  if(startsWith("SET:USESD")){
    int flg = getIntVal();
    if(SerialOut) Serial.print("SET:USESD => ");
    if(SerialOut) Serial.println(flg);
    if(flg == 1){
      uc.enable_sd = true;
    }else{
      uc.enable_sd = false;
    }
    blue.println("OK:USESD");
    return;
  }
  if(startsWith("SET:USEWIFI")){
    int flg = getIntVal();
    if(SerialOut) Serial.print("SET:USEWIFI => ");
    if(SerialOut) Serial.println(flg);
    if(flg == 1){
      uc.enable_wifi = true;
    }else{
      uc.enable_wifi = false;
    }
    blue.println("OK:USEWIFI");
    return;
  }
  if(startsWith("SET:WIFISSID")){
    uc.wifi_ssid = getStrVal();
    if(SerialOut) Serial.print("SET:WIFISSID => ");
    if(SerialOut) Serial.println(uc.wifi_ssid);
    blue.println("OK:WIFISSID");
    return;
  }
  if(startsWith("SET:WIFIPASS")){
    uc.wifi_pass = getStrVal();
    if(SerialOut) Serial.print("SET:WIFIPASS => ");
    if(SerialOut) Serial.println(uc.wifi_pass);
    blue.println("OK:WIFIPASS");
    return;
  }
  if(startsWith("SET:WIFIURL")){
    uc.post_url = getStrVal();
    if(SerialOut) Serial.print("SET:WIFIURL => ");
    if(SerialOut) Serial.println(uc.post_url);
    blue.println("OK:WIFIURL");
    return;
  }

  if(startsWith("SET:RESET_FILECOUNT")){  
    preferences.begin(SD_NAME_SPACE,false);
    preferences.putInt(SD_COUNT, 0);
    preferences.end();
    blue.println("OK:RESET_FILECOUNT");
    return;
  }
  if(startsWith("SET:COMMIT")){
    eepWrite();
    blue.println("OK:COMMIT");
    return;
  }

  // DEBUG CAMTEST
  if(startsWith("CMD:TEST")){
    if(SerialOut) Serial.println("CMD:TEST START");
    if(validateSetting() == false){
      if(SerialOut) Serial.println("---- Corrected User Settings after validation ----");
      printUserConfig();
    }
    captureAndPost();
    offPins();
    blue.println("OK:TEST");
    return;
  }

  // COMMAND
  if(startsWith("CMD:REBOOT")){
    blue.println("OK:REBOOT");
    ESP.restart();
    return;
  }

  if(SerialOut) Serial.println("Undefined BT Command");
  return;
}

bool startsWith(const char* compStr){
  int len = strlen(compStr);
  if(btCharIdx + 1 < len){
    return false;
  }
  for(int i=0;i<len;i++){
    if(btChar[i] != compStr[i]){
      return false;
    }
  }
  return true;
}

int getIntVal(){
  boolean getnext = false;
  int rt = 0;
  for(int i=0;i<=btCharIdx;i++){
    if(getnext){
      rt = getIntValSub(rt, btChar[i]);
      continue;
    }else if(btChar[i] == BLUE_SET_SEP){
      getnext = true;
      continue;
    }
  }
  return rt;
}

int getIntValSub(int current, char next){
  if(next == '0'){
    return current * 10;
  }else if(next == '1'){
    return current * 10 + 1;
  }else if(next == '2'){
    return current * 10 + 2;
  }else if(next == '3'){
    return current * 10 + 3;
  }else if(next == '4'){
    return current * 10 + 4;
  }else if(next == '5'){
    return current * 10 + 5;
  }else if(next == '6'){
    return current * 10 + 6;
  }else if(next == '7'){
    return current * 10 + 7;
  }else if(next == '8'){
    return current * 10 + 8;
  }else if(next == '9'){
    return current * 10 + 9;
  }
  return current;
}

String getStrVal(){
  String rt = "";
  boolean getnext = false;
  for(int i=0;i<=btCharIdx;i++){
    if(getnext){
      rt += btChar[i];
      continue;
    }else if(btChar[i] == BLUE_SET_SEP){
      getnext = true;
      continue;
    }
  }
  return rt;
}

void loop() {
  if(SETMODE){
    blueToothSetting();
  }else{
    if(CAM_READY){
      captureAndPost();
    }else{
      if(SerialOut) Serial.println("CAM is not ready. Skip to deep sleep.");
    }
    offPins();
    deepSleep();
  }
}
