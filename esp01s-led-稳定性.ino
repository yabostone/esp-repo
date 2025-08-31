/*
 * ESP8266四路LED控制代码最终版
 * 修复问题：明确区分GPIO4（蓝色状态灯）与其他LED
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Ticker.h>
#define PLATFORM "ESP8266"

// 网络配置
#define DEFAULT_SSID "linklink24"
#define DEFAULT_PASS "2a0bq4za1r"

ESP8266WebServer server(80);
Ticker wifiCheckTicker;

// 硬件配置
const int ledPins[] = {0, 1, 2, 3}; // GPIO0, GPIO1(TX), GPIO2, GPIO3(RX) - 普通LED
const int STATUS_LED = 4;          // GPIO4（蓝色LED - 专门用于状态指示）
bool ledStates[] = {false, false, false, false}; // 仅针对普通LED的状态
unsigned long lastBlinkTime = 0;
bool wifiConnected = false;
bool connectionInProgress = false;
unsigned long lastReconnectAttempt = 0;
const unsigned long RECONNECT_INTERVAL = 30000;

// 低电平有效配置（仅适用于普通LED）
#define ACTIVE_LOW true

void printNetworkInfo() {
  Serial.println("\n=== 网络状态 ===");
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());
  Serial.print("IP地址: ");
  Serial.println(WiFi.localIP());
  Serial.print("信号强度: ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");
  Serial.println("=================");
}

const char* htmlTemplate = R"raw(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    .btn {margin:10px; padding:15px 30px; color:white; 
          text-decoration:none; border-radius:8px; transition:0.3s;
          display:inline-block; width:200px;}
    .btn-on {background:#4CAF50;}
    .btn-off {background:#555;}
    .btn:hover {opacity:0.8;}
    .warning {color: red; font-weight: bold;}
    .led-status {display:inline-block; width:20px; height:20px; border-radius:50%; margin:0 10px; background:%s;}
  </style>
</head>
<body style="text-align:center;padding:20px">
<h2>%s LED控制--E5-openEuler--64GB</h2>
<p>WiFi状态: %s <span class="led-status"></span></p>
%s
<hr>
<p>系统运行时间: %.1f 分钟</p>
<div class="warning">
<p>GPIO使用注意事项：</p>
<ul style="text-align:left">
<li>GPIO0 - 需要上拉电阻，下载模式时不可用</li>
<li>GPIO1(TX) - 串口发送引脚，控制时可能影响日志输出</li>
<li>GPIO2 - 需要保持高电平启动</li>
<li>GPIO3(RX) - 串口接收引脚，控制时可能影响设备通信</li>
<li>GPIO4 - 专用状态指示灯（蓝色）</li>
</ul>
</div>
</body></html>
)raw";

String generateContent() {
  String buttons;
  for(int i=0; i<4; i++){
    buttons += "<a href='/led" + String(i+1) + "' class='btn ";
    buttons += ledStates[i] ? "btn-on'>ON" : "btn-off'>OFF";
    buttons += " - LED" + String(i+1) + " (GPIO" + String(ledPins[i]) + ")</a>";
  }
  
  String wifiStatus = wifiConnected ? "已连接" : "连接中...";
  String ledColor = wifiConnected ? "#0080FF" : "#999999"; // 蓝色/灰色
  
  float uptime = millis()/60000.0;
  char buffer[1400];
  snprintf(buffer, sizeof(buffer), htmlTemplate, ledColor.c_str(), PLATFORM, wifiStatus.c_str(), buttons.c_str(), uptime);
  return String(buffer);
}

void handleRoot() {
  server.send(200, "text/html", generateContent());
}

// LED控制（仅处理普通LED）
void handleLED() {
  String path = server.uri();
  int ledIndex = path.charAt(4) - '1';
  
  if(ledIndex >=0 && ledIndex <=3){
    ledStates[ledIndex] = !ledStates[ledIndex];
    
    // 仅对普通LED应用低电平有效逻辑
    if (ACTIVE_LOW) {
      digitalWrite(ledPins[ledIndex], ledStates[ledIndex] ? LOW : HIGH);
    } else {
      digitalWrite(ledPins[ledIndex], ledStates[ledIndex] ? HIGH : LOW);
    }
    
    Serial.printf("[%lu] 普通LED%d (GPIO%d) 状态: %s\n", 
                 millis(), ledIndex+1, ledPins[ledIndex], 
                 ledStates[ledIndex] ? "ON" : "OFF");
  }
  handleRoot();
}

// 专门更新GPIO4蓝色状态灯
void updateStatusLED() {
  // GPIO4是专用蓝色LED，不需要应用ACTIVE_LOW逻辑
  // 当WiFi连接时保持常亮（低电平50%亮度）
  if (wifiConnected) {
    analogWrite(STATUS_LED, 512); // 50%亮度（PWM值512/1023）
    Serial.printf("[%lu] 蓝色状态灯: 常亮(50%%)\n", millis());
  } 
  // 未连接时以1秒周期闪烁（500ms开/500ms关）
  else {
    if (millis() - lastBlinkTime > 500) {
      int newState = !digitalRead(STATUS_LED);
      digitalWrite(STATUS_LED, newState);
      lastBlinkTime = millis();
      Serial.printf("[%lu] 蓝色状态灯: %s\n", millis(), newState ? "亮" : "灭");
    }
  }
}

void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    if (wifiConnected) {
      Serial.printf("\n[%lu] 检测到网络断开!\n", millis());
    }
    wifiConnected = false;
    
    if (!connectionInProgress && (millis() - lastReconnectAttempt > RECONNECT_INTERVAL)) {
      Serial.printf("[%lu] 触发自动重连...\n", millis());
      setupNetwork();
    }
  }
}

void setupNetwork() {
  if (connectionInProgress) return;
  
  connectionInProgress = true;
  Serial.printf("\n[%lu] 开始网络连接...\n", millis());
  Serial.print("目标SSID: ");
  Serial.println(DEFAULT_SSID);
  
  WiFi.hostname("ESP01s-MachineCtrl-" + String(ESP.getChipId(), HEX));
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(100);
  
  bool previousState = wifiConnected;
  wifiConnected = false;
  int retryCount = 0;
  
  do {
    if(retryCount > 0) {
      Serial.printf("\n[%lu] 第%d次重试连接...\n", millis(), retryCount);
    }
    
    WiFi.begin(DEFAULT_SSID, DEFAULT_PASS);
    
    unsigned long connectionStart = millis();
    while(WiFi.status() != WL_CONNECTED) {
      updateStatusLED();  // 更新蓝色状态灯
      ESP.wdtFeed();      // 喂狗
      
      if(millis() - connectionStart > 20000) {
        Serial.printf("\n[%lu] 本次连接超时，准备重试\n", millis());
        break;
      }
      delay(100);
    }

    if(WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      updateStatusLED();  // 连接成功后更新蓝色状态灯
      Serial.printf("\n连接成功!\n");
      break;
    }
    
    retryCount++;
    Serial.printf("\n[状态] 当前WiFi状态码: %d\n", WiFi.status());
    
    // 连接失败时的视觉反馈（蓝色LED短暂闪烁）
    for(int i=0; i<3; i++){
      digitalWrite(STATUS_LED, LOW);
      delay(300);
      ESP.wdtFeed();
      digitalWrite(STATUS_LED, HIGH);
      delay(700);
      ESP.wdtFeed();
    }
    
    WiFi.disconnect(true);
    delay(5000);
  } while(retryCount < 5);

  lastReconnectAttempt = millis();
  connectionInProgress = false;
  
  if(wifiConnected) {
    printNetworkInfo();
  } else {
    Serial.printf("\n[%lu] 连接失败，将在30秒后重试\n", millis());
  }
}

void setup() {
  // 初始化普通LED（GPIO0-3）
  for(int i=0; i<4; i++){
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], ACTIVE_LOW ? HIGH : LOW); // 初始关闭状态
    Serial.printf("初始化普通LED (GPIO%d): 关闭\n", ledPins[i]);
  }
  
  // 初始化蓝色状态灯（GPIO4）
  pinMode(STATUS_LED, OUTPUT);
  analogWriteRange(1023); // 设置PWM范围
  digitalWrite(STATUS_LED, HIGH); // 初始关闭状态
  Serial.printf("初始化蓝色状态灯 (GPIO4): 关闭\n");
  
  // 串口初始化
  Serial.begin(115200);
  delay(500);
  Serial.printf("\n\n=== %s 四路LED控制系统启动 ===\n", PLATFORM);
  Serial.println("固件版本: 3.0 (状态灯专用版)");
  Serial.println("编译时间: " __DATE__ " " __TIME__);
  Serial.println("说明: GPIO4为专用蓝色状态指示灯");

  setupNetwork();

  server.on("/", handleRoot);
  server.on("/led1", handleLED); // 控制GPIO0
  server.on("/led2", handleLED); // 控制GPIO1
  server.on("/led3", handleLED); // 控制GPIO2
  server.on("/led4", handleLED); // 控制GPIO3
  // 注意：没有设置GPIO4的控制端点
  server.begin();
  
  Serial.printf("[%lu] HTTP服务器已启动\n", millis());

  wifiCheckTicker.attach(10, checkWiFiConnection);
  ESP.wdtEnable(5000);
}

void loop() {
  ESP.wdtFeed();
  server.handleClient();
  updateStatusLED();  // 持续更新蓝色状态灯
  
  // 监控网络状态
  static unsigned long lastCheck = 0;
  if(millis() - lastCheck > 60000){
    lastCheck = millis();
    
    if(wifiConnected){
      Serial.printf("[%lu] 网络状态: 已连接 (RSSI: %d dBm)\n", millis(), WiFi.RSSI());
    } else {
      Serial.printf("[%lu] 网络状态: 断开\n", millis());
    }
    
    // 打印所有LED状态
    Serial.print("当前LED状态: ");
    for(int i=0; i<4; i++){
      Serial.printf("LED%d(%s) ", i+1, ledStates[i] ? "ON" : "OFF");
    }
    Serial.println();
  }
}
