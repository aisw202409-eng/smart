#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <Adafruit_NeoPixel.h>


// =====================================================
// Wi-Fi
// =====================================================

const char* WIFI_SSID = "FirstClass2.4G";
const char* WIFI_PASSWORD = "12345678";

// 고정 IP
IPAddress local_IP(10, 114, 189, 104);

// 공유기 게이트웨이
IPAddress gateway(10, 114, 184, 1);

// 서브넷
IPAddress subnet(255, 255, 252, 0);

// DNS
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(1, 1, 1, 1);


// =====================================================
// 미세먼지 API
// =====================================================

// 공개된 API 키는 재발급 후 입력하는 것을 권장
const char* API_KEY = "OaGmdvwOAz%2B7FV58ovB2TpFREKnJWAcvUqHlg63EK%2BbjEoQAAz%2FSm6pRNFU%2Fy9KOH2Mv0kgDxn0LSI3t9pzGpw%3D%3D";

// 사용할 측정소
const char* STATION_NAME = "영통동";


// =====================================================
// 미세먼지 기준
// =====================================================

// PM2.5 36 이상 = 나쁨
const int PM25_BAD = 36;


// =====================================================
// 모터 핀
// =====================================================

const int PWMA = 19;
const int PWMB = 18;

const int AIN1 = 33;
const int AIN2 = 32;

const int BIN1 = 25;
const int BIN2 = 26;

const int STBY = 5;


// =====================================================
// NeoPixel
// =====================================================

const int LED_PIN = 27;
const int LED_COUNT = 2;

Adafruit_NeoPixel strip(
  LED_COUNT,
  LED_PIN,
  NEO_GRB + NEO_KHZ800
);


// =====================================================
// 모터 속도
// =====================================================

const int motorSpeed = 90;


// =====================================================
// 웹 서버
// =====================================================

WebServer server(80);


// =====================================================
// 상태 변수
// =====================================================

float pm25 = -1;

String airStatus = "측정 전";

bool badAir = false;

bool manualMode = false;

String robotMode = "정지";


// =====================================================
// 미세먼지 측정 주기
// =====================================================

unsigned long lastAirCheck = 0;

// 5분마다 API 조회
const unsigned long AIR_INTERVAL =
  5UL * 60UL * 1000UL;


// =====================================================
// 좌우 반복 회전
// =====================================================

bool turnLeftState = true;

unsigned long lastTurnChange = 0;

// 1초마다 방향 변경
const unsigned long TURN_INTERVAL = 1000;


// =====================================================
// 모터 제어
// =====================================================

void motor(
  int leftSpeed,
  int rightSpeed,
  int a1,
  int a2,
  int b1,
  int b2
) {

  analogWrite(PWMA, leftSpeed);
  analogWrite(PWMB, rightSpeed);

  digitalWrite(AIN1, a1);
  digitalWrite(AIN2, a2);

  digitalWrite(BIN1, b1);
  digitalWrite(BIN2, b2);
}


// =====================================================
// 전진
// =====================================================

void forward() {

  motor(
    motorSpeed,
    motorSpeed,
    LOW,
    HIGH,
    HIGH,
    LOW
  );

  robotMode = "전진";
}


// =====================================================
// 후진
// =====================================================

void backward() {

  motor(
    motorSpeed,
    motorSpeed,
    HIGH,
    LOW,
    LOW,
    HIGH
  );

  robotMode = "후진";
}


// =====================================================
// 좌회전
// =====================================================

void left() {

  motor(
    motorSpeed,
    motorSpeed,
    LOW,
    HIGH,
    LOW,
    HIGH
  );

  robotMode = "좌회전";
}


// =====================================================
// 우회전
// =====================================================

void right() {

  motor(
    motorSpeed,
    motorSpeed,
    HIGH,
    LOW,
    HIGH,
    LOW
  );

  robotMode = "우회전";
}


// =====================================================
// 정지
// =====================================================

void stopMotor() {

  motor(
    0,
    0,
    LOW,
    LOW,
    LOW,
    LOW
  );

  robotMode = "정지";
}


// =====================================================
// NeoPixel
// =====================================================

void setLED(
  uint8_t r,
  uint8_t g,
  uint8_t b
) {

  for (int i = 0; i < LED_COUNT; i++) {

    strip.setPixelColor(
      i,
      strip.Color(r, g, b)
    );
  }

  strip.show();
}


// =====================================================
// PM2.5 상태
// =====================================================

String getAirStatus(float value) {

  if (value < 0) {
    return "데이터 없음";
  }

  if (value <= 15) {
    return "좋음";
  }

  if (value <= 35) {
    return "보통";
  }

  if (value <= 75) {
    return "나쁨";
  }

  return "매우나쁨";
}


// =====================================================
// 미세먼지 API
// =====================================================

bool getAirQuality() {

  if (WiFi.status() != WL_CONNECTED) {

    Serial.println("Wi-Fi 연결 안됨");

    return false;
  }


  WiFiClientSecure client;

  // 프로토타입용
  client.setInsecure();


  HTTPClient http;


  String url =
    "https://apis.data.go.kr/"
    "B552584/atmstMsrstnRltmInfo/getList";


  url += "?serviceKey=";
  url += API_KEY;

  url += "&returnType=json";

  url += "&numOfRows=1";

  url += "&pageNo=1";

  url += "&stationName=";
  url += STATION_NAME;


  Serial.println();
  Serial.println("==============================");
  Serial.println("미세먼지 API 요청");
  Serial.println("==============================");


  if (!http.begin(client, url)) {

    Serial.println("HTTP 시작 실패");

    return false;
  }


  int httpCode = http.GET();


  Serial.print("HTTP Code: ");
  Serial.println(httpCode);


  if (httpCode != HTTP_CODE_OK) {

    Serial.println("API 요청 실패");

    http.end();

    return false;
  }


  String payload = http.getString();


  Serial.println("API 응답:");
  Serial.println(payload);


  DynamicJsonDocument doc(16384);


  DeserializationError error =
    deserializeJson(doc, payload);


  if (error) {

    Serial.print("JSON 오류: ");
    Serial.println(error.c_str());

    http.end();

    return false;
  }


  JsonArray items =
    doc["response"]["body"]["items"].as<JsonArray>();


  if (
    items.isNull() ||
    items.size() == 0
  ) {

    Serial.println("측정 데이터 없음");

    http.end();

    return false;
  }


  JsonObject item = items[0];


  const char* pm25String =
    item["pm25Value"] | "-1";


  pm25 = atof(pm25String);


  airStatus =
    getAirStatus(pm25);


  badAir =
    (pm25 >= PM25_BAD);


  Serial.println();
  Serial.println("==============================");
  Serial.print("PM2.5 : ");
  Serial.println(pm25);

  Serial.print("상태 : ");
  Serial.println(airStatus);

  Serial.print("자동 동작 : ");

  if (badAir) {
    Serial.println("좌우 반복");
  }
  else {
    Serial.println("오른쪽 회전");
  }

  Serial.println("==============================");


  // =================================================
  // LED 상태
  // =================================================

  if (pm25 <= 15) {

    // 좋음 - 초록
    setLED(0, 255, 0);

  }
  else if (pm25 <= 35) {

    // 보통 - 노랑
    setLED(255, 180, 0);

  }
  else if (pm25 <= 75) {

    // 나쁨 - 빨강
    setLED(255, 0, 0);

  }
  else {

    // 매우나쁨 - 보라
    setLED(180, 0, 180);
  }


  http.end();

  return true;
}


// =====================================================
// 자동 주행
// =====================================================

void automaticRobot() {

  // 수동 모드면 자동제어하지 않음
  if (manualMode) {
    return;
  }


  // ===================================================
  // PM2.5 나쁨 이상
  // ===================================================

  if (badAir) {

    if (
      millis() - lastTurnChange
      >= TURN_INTERVAL
    ) {

      lastTurnChange = millis();

      turnLeftState =
        !turnLeftState;
    }


    if (turnLeftState) {

      left();

    }
    else {

      right();
    }


    return;
  }


  // ===================================================
  // PM2.5 정상
  // ===================================================

  right();
}


// =====================================================
// HTML
// =====================================================

void handleRoot() {

  File file =
    LittleFS.open(
      "/index.html",
      "r"
    );


  if (!file) {

    server.send(
      500,
      "text/plain",
      "index.html 파일을 찾을 수 없습니다."
    );

    return;
  }


  server.streamFile(
    file,
    "text/html"
  );


  file.close();
}


// =====================================================
// 상태 API
// =====================================================

void handleStatus() {

  String json = "{";


  json += "\"pm25\":";
  json += String(pm25, 1);


  json += ",";


  json += "\"status\":\"";
  json += airStatus;
  json += "\"";


  json += ",";


  json += "\"badAir\":";

  if (badAir) {
    json += "true";
  }
  else {
    json += "false";
  }


  json += ",";


  json += "\"robotMode\":\"";
  json += robotMode;
  json += "\"";


  json += ",";


  json += "\"manualMode\":";

  if (manualMode) {
    json += "true";
  }
  else {
    json += "false";
  }


  json += "}";


  server.send(
    200,
    "application/json",
    json
  );
}


// =====================================================
// 웹 제어
// =====================================================

void handleForward() {

  manualMode = true;

  forward();

  server.send(
    200,
    "text/plain",
    "OK"
  );
}


void handleBackward() {

  manualMode = true;

  backward();

  server.send(
    200,
    "text/plain",
    "OK"
  );
}


void handleLeft() {

  manualMode = true;

  left();

  server.send(
    200,
    "text/plain",
    "OK"
  );
}


void handleRight() {

  manualMode = true;

  right();

  server.send(
    200,
    "text/plain",
    "OK"
  );
}


void handleStop() {

  manualMode = true;

  stopMotor();

  server.send(
    200,
    "text/plain",
    "OK"
  );
}


void handleAuto() {

  manualMode = false;

  server.send(
    200,
    "text/plain",
    "OK"
  );
}


// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);


  // ===================================================
  // 모터 핀
  // ===================================================

  pinMode(PWMA, OUTPUT);
  pinMode(PWMB, OUTPUT);

  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);

  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);

  pinMode(STBY, OUTPUT);


  digitalWrite(STBY, HIGH);


  // ===================================================
  // NeoPixel
  // ===================================================

  strip.begin();

  strip.show();

  setLED(0, 0, 50);


  // ===================================================
  // 모터 정지
  // ===================================================

  stopMotor();


  // ===================================================
  // LittleFS
  // ===================================================

  if (!LittleFS.begin(true)) {

    Serial.println(
      "LittleFS 시작 실패"
    );

  }
  else {

    Serial.println(
      "LittleFS 시작 완료"
    );
  }


  // ===================================================
  // 고정 IP
  // ===================================================

  if (
    !WiFi.config(
      local_IP,
      gateway,
      subnet,
      primaryDNS,
      secondaryDNS
    )
  ) {

    Serial.println(
      "고정 IP 설정 실패"
    );
  }


  // ===================================================
  // Wi-Fi
  // ===================================================

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );


  Serial.println();

  Serial.print(
    "Wi-Fi 연결 중"
  );


  int wifiTry = 0;


  while (
    WiFi.status()
    != WL_CONNECTED
  ) {

    delay(500);

    Serial.print(".");


    wifiTry++;


    if (wifiTry > 40) {

      Serial.println();

      Serial.println(
        "Wi-Fi 연결 실패"
      );

      return;
    }
  }


  Serial.println();

  Serial.println(
    "Wi-Fi 연결 완료"
  );


  Serial.print(
    "IP 주소: "
  );

  Serial.println(
    WiFi.localIP()
  );


  // ===================================================
  // 미세먼지 최초 측정
  // ===================================================

  getAirQuality();

  lastAirCheck = millis();


  // ===================================================
  // 웹 서버
  // ===================================================

  server.on(
    "/",
    HTTP_GET,
    handleRoot
  );


  server.on(
    "/api/status",
    HTTP_GET,
    handleStatus
  );


  server.on(
    "/forward",
    HTTP_GET,
    handleForward
  );


  server.on(
    "/backward",
    HTTP_GET,
    handleBackward
  );


  server.on(
    "/left",
    HTTP_GET,
    handleLeft
  );


  server.on(
    "/right",
    HTTP_GET,
    handleRight
  );


  server.on(
    "/stop",
    HTTP_GET,
    handleStop
  );


  server.on(
    "/auto",
    HTTP_GET,
    handleAuto
  );


  server.begin();


  Serial.println();

  Serial.println(
    "=============================="
  );

  Serial.println(
    "웹 서버 시작"
  );

  Serial.print(
    "대시보드: http://"
  );

  Serial.println(
    WiFi.localIP()
  );

  Serial.println(
    "=============================="
  );
}


// =====================================================
// LOOP
// =====================================================

void loop() {

  // 웹 요청
  server.handleClient();


  // ===================================================
  // 5분마다 미세먼지 업데이트
  // ===================================================

  if (
    millis() - lastAirCheck
    >= AIR_INTERVAL
  ) {

    lastAirCheck = millis();

    getAirQuality();
  }


  // ===================================================
  // 자동 주행
  // ===================================================

  automaticRobot();


  delay(20);
}