/*
 * ESP8266四路LED控制代码优化版
 * 修改说明：
 * 1. 修复GPIO初始化问题
 * 2. 修正LED控制逻辑（低电平有效）
 * 3. 增强GPIO使用安全提示
 * 4. 优化看门狗配置
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#define PLATFORM "ESP8266"

// 网络配置
#define DEFAULT_SSID "PDCN"
#define DEFAULT_PASS "Aes12345678"

ESP8266WebServer server(80);

// 硬件配置
const int ledPins[] = {0, 1, 2, 3}; // GPIO0, GPIO1(TX), GPIO2, GPIO3(RX)
const int STATUS_LED = 4;          // GPIO4
bool ledStates[] = {false, false, false, false};
unsigned long connectionStart;

// 低电平有效配置
#define ACTIVE_LOW true  // 设置为true表示低电平点亮LED

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
  </style>
</head>
<body style="text-align:center;padding:20px">
<h2>%s LED控制</h2>
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
  
  float uptime = millis()/60000.0;
  char buffer[1200];
  snprintf(buffer, sizeof(buffer), htmlTemplate, PLATFORM, buttons.c_str(), uptime);
  return String(buffer);
}

void handleRoot() {
  server.send(200, "text/html", generateContent());
}

void handleLED() {
  String path = server.uri();
  int ledIndex = path.charAt(4) - '1';
  
  if(ledIndex >=0 && ledIndex <=3){
    ledStates[ledIndex] = !ledStates[ledIndex];
    digitalWrite(ledPins[ledIndex], 
                (ACTIVE_LOW ? !ledStates[ledIndex] : ledStates[ledIndex]) ? HIGH : LOW);
  }
  handleRoot();
}

void setupNetwork() {
  Serial.printf("\n[%lu] 开始网络连接...\n", millis());
  Serial.print("目标SSID: ");
  Serial.println(DEFAULT_SSID);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(DEFAULT_SSID, DEFAULT_PASS);
  
  connectionStart = millis();
  bool ledState = HIGH;
  
  while(WiFi.status() != WL_CONNECTED) {
    if(millis() - connectionStart > 20000){
      Serial.println("\n连接超时! 可能原因:");
      Serial.println("1. 密码错误");
      Serial.println("2. 信号强度不足");
      Serial.println("3. 2.4GHz网络不可用");
      Serial.println("4. 路由器限制连接");
      while(1) {
        digitalWrite(STATUS_LED, !digitalRead(STATUS_LED));
        delay(200);
      }
    }
    digitalWrite(STATUS_LED, ledState = !ledState);
    delay(250);
    Serial.print(".");
  }
  
  digitalWrite(STATUS_LED, HIGH);
  Serial.printf("\n[%lu] 连接成功!\n", millis());
  printNetworkInfo();
}

void setup() {
  // 硬件初始化
  for(int i=0; i<4; i++){
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], ACTIVE_LOW ? HIGH : LOW); // 初始关闭状态
  }
  pinMode(STATUS_LED, OUTPUT);
  
  // 串口初始化
  Serial.begin(115200);
  delay(500);
  Serial.printf("\n\n=== %s 四路LED控制系统启动 ===\n", PLATFORM);
  Serial.println("固件版本: 2.3");
  Serial.println("编译时间: " __DATE__ " " __TIME__);
  Serial.println("警告：GPIO1和GPIO3可能影响串口通信！");

  // 网络连接
  setupNetwork();

  // 服务器配置
  server.on("/", handleRoot);
  server.on("/led1", handleLED);
  server.on("/led2", handleLED);
  server.on("/led3", handleLED);
  server.on("/led4", handleLED);
  server.begin();
  
  Serial.printf("[%lu] HTTP服务器已启动\n", millis());

  // 看门狗配置
  ESP.wdtEnable(5000);  // 5秒看门狗
}

void loop() {
  ESP.wdtFeed();
  static unsigned long lastCheck = 0;
  if(millis() - lastCheck > 5000){
    lastCheck = millis();
    if(WiFi.status() != WL_CONNECTED){
      Serial.println("检测到网络断开，尝试重新连接...");
      ESP.restart();
    }
  }
  server.handleClient();
}
