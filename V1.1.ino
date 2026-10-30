#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "RTClib.h"

// 屏幕尺寸设置
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define BUZZER_PIN 14
#define BTN_MENU 32
#define BTN_PLUS 33
#define BTN_SWITCH 25
#define BTN_OK 26
#define FACE_NORMAL 0 //后期表情补充
#define FACE_SLEEP 1
#define FACE_WAKING 2
// 创建屏幕和时钟对象
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
RTC_DS3231 rtc;
int mode=0;
int now_hour=0;
int now_minute=0;
int alarm_hour=23;
int alarm_minute=58;
int temp_hour=0;
int temp_minute=0;
int switch_turn=0;
int transitionType = 0;
int PetState = FACE_NORMAL; //后期表情的补充
bool is_ringing=false;
bool alarm_dismissed=false;
unsigned long lastBtnTime = 0;
unsigned long lastDisplayTime= 0;
unsigned long transitionStart = 0;
unsigned long lastRingTime = 0;
unsigned long last_active_time = 0;
void setup() {
  Serial.begin(115200);
  delay(500); // 等待串口准备好
  Serial.println("\n=== 系统启动自检 ===");

  // 启动 I2C 总线（指定 ESP32 的 D21 和 D22）
  Wire.begin(21, 22);

  //  防烧自检 1：屏幕
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("危险：OLED 屏幕初始化失败！");
    Serial.println("请立刻拔掉 USB，检查屏幕的 VCC/GND 是否接反，SDA/SCL 是否松动！");
    while(1) { delay(100); } // 死循环停机，保护硬件
  }
  Serial.println("OLED 屏幕自检通过");

  // 防烧自检 2：时钟模块
  if (!rtc.begin()) {
    Serial.println("危险：DS3231 时钟初始化失败！");
    Serial.println("请立刻拔掉 USB，检查时钟模块的线是否插对（VCC不能接反）！");
    while(1) { delay(100); } // 死循环停机，保护硬件
  }
  Serial.println("DS3231 时钟自检通过");

  // 强制校准一次时间（把电脑编译的时间写进模块，防止看到 2000 年）
  // 注意：这行代码只有在每次重新烧录时才会执行一次
  //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  Serial.println("时间已校准为电脑当前时间！");
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BTN_MENU, INPUT_PULLUP);
  pinMode(BTN_PLUS, INPUT_PULLUP);
  pinMode(BTN_SWITCH, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);

  tone(BUZZER_PIN, 1000, 100);

  // 屏幕初始画面
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(20, 24);
  display.println("(=OwO=)");
  display.display();
  tone(BUZZER_PIN,523,100);
  delay(150);
  tone(BUZZER_PIN,659,100);
  delay(150);
  tone(BUZZER_PIN,784,150);
  delay(600); // 显示1秒的开机画面
   
  //统一定义初始时间位置
  lastDisplayTime = millis();
  lastRingTime = millis();       // 记下这次响的时间
  transitionStart = millis();
  last_active_time = millis();
  lastBtnTime = millis();

}

void loop() {
  checkButton();
  DateTime now = rtc.now();
  if(millis()-lastDisplayTime >= 100){
    updatePetState();
  // ====== 1. 过渡动画优先 ======
  if (transitionType != 0) {
    display.clearDisplay();
    display.setTextSize(2); 
    display.setTextColor(SSD1306_WHITE);
    
    if (transitionType == 1) {
      display.setCursor(16, 24); // (=o_o=)? 有8个字符，X给16+
      display.print("(=o_o=)?");
    } else {
      display.setCursor(22, 24); // (=^_^=) 有7个字符，X给22
      display.print("(=^_^=)");
    }
    
    display.display();
    
    // 时间到了，切模式
    if (millis() - transitionStart >= 1000) {
      if (transitionType == 1) mode = 1;
      if (transitionType == 2) mode = 0;
      transitionType = 0; // 退出过渡
    }
  } 
  // ====== 2. 正常模式 0 ======
  else if (mode == 0) {    
      // 读取时间
      // 清屏
      display.clearDisplay();
      if(is_ringing){
        drawPetFace();
      }
      
      else{
        // 显示时间 (大字)
        display.setTextSize(2);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(15, 10); // 坐标 (X, Y)
        
        char timeStr[10];
        sprintf(timeStr, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
        display.print(timeStr);
        
        // 显示日期 (小字)
        display.setTextSize(1);
        display.setCursor(25, 50); 
        
        char dateStr[15];
        sprintf(dateStr, "%04d-%02d-%02d", now.year(), now.month(), now.day());
        display.print(dateStr);
        drawPetFace();
        }
      display.display();
      }
    else if (mode==1){
      display.clearDisplay();
      if((millis()/500)%2==0){  // 清屏
    
      // 显示时间 (大字)
      display.setTextSize(2);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(15, 10); // 坐标 (X, Y)
    
      char tempStr[8];
      sprintf(tempStr, "%02d:%02d",temp_hour,temp_minute);
      display.print(tempStr);
      // ====== 新增：光标提示 ======
      display.setTextSize(1);  // 用小字
      if (switch_turn == 0) {
        display.setCursor(20, 30); // 在小时(左边)下方画箭头
        display.print("^^");
      } else {
        display.setCursor(50, 30); // 在分钟(右边)下方画箭头
        display.print("^^");
      }
      }
      display.display();
    }
  }
// 1. 触发条件：只要到了设定的小时和分钟，且秒数是 0 的那一瞬间
if (now.hour() == alarm_hour && now.minute() == alarm_minute) {
    if (!is_ringing && !alarm_dismissed) {
        is_ringing = true;
        Serial.println("闹钟时间到了！");
    }
}
// 2. 如果正在响铃（这个状态会一直保持，直到你按OK），蜂鸣器就发声
if (is_ringing) {
  // 每隔 500 毫秒（响200 + 停300）敲一次门
  if (millis() - lastRingTime >= 500) {
    lastRingTime = millis();
    tone(BUZZER_PIN, 1500, 200);   // 每次只响 200 毫秒！
  }
} else {
  noTone(BUZZER_PIN);              // 如果不响了，立刻闭嘴
}

// 3. 如果时间已经过了这一分钟（不再是闹钟时间），自动解锁，为明天做准备
if (now.hour() != alarm_hour || now.minute() != alarm_minute) {
    is_ringing=false;
    alarm_dismissed = false; 
    noTone(BUZZER_PIN);
}
}

void checkButton(){
  if (is_ringing && btnPressed(BTN_OK)) {
      Serial.println("按下了：OK");
      is_ringing = false;
      alarm_dismissed = true; // 加上锁
      noTone(BUZZER_PIN);     // 闭嘴
      lastRingTime = 0;
      Serial.println("闹钟已静音");
  }
  if (checkWakeUp()){
    return;
  }
  if (mode==0 && btnPressed(BTN_MENU) && transitionType==0) {
  Serial.println("按下了：MENU键");
  temp_hour=alarm_hour;
  temp_minute=alarm_minute;
  tone(BUZZER_PIN, 1000, 60); // 先响个低音
  delay(60);
  noTone(BUZZER_PIN);
  tone(BUZZER_PIN, 1500, 60); // 再响个高音
  delay(60);
  noTone(BUZZER_PIN);
  transitionType=1;
  mode=1;
  }
  if (mode==1 && btnPressed(BTN_SWITCH)) {
    Serial.println("按下了：SWITCH键");
    switch_turn++;
    if (switch_turn>=2){
    switch_turn=0;
    }
     // 【音效修改】：极短的清脆音
    tone(BUZZER_PIN, 1200, 50); 
    delay(50);
    noTone(BUZZER_PIN);
    }
  if (mode==1 && switch_turn==0 && btnPressed(BTN_PLUS)){
      Serial.println("按下了：PLUS键小时");
      temp_hour++;
      if (temp_hour>=24){
    temp_hour=0;
  }
      }
  if (mode==1 && switch_turn==1 && btnPressed(BTN_PLUS)){
    Serial.println("按下了：PLUS键分钟");
    temp_minute++;
    if (temp_minute>=60){
    temp_minute=0;
  }
    }
  if(mode==1 && btnPressed(BTN_OK) && transitionType==0){
  Serial.println("按下了：OK");
  alarm_hour=temp_hour;
  alarm_minute=temp_minute;
   // 【音效修改】：下行音（嘟-滴）
  tone(BUZZER_PIN, 1500, 60); 
  delay(60);
  noTone(BUZZER_PIN);
  tone(BUZZER_PIN, 1000, 80); 
  delay(80);
  noTone(BUZZER_PIN);
  transitionType= 2;
  mode=0;
}
}


void updatePetState(){
  if (mode != 0)return;
  if (PetState == 0 && (millis() - last_active_time >= 15000)){
    PetState = 1;
  }
  if (PetState == 2 && (millis() - last_active_time >= 1500)){
    PetState = 0;
  }
}


void drawPetFace(){
  if (is_ringing){
    display.setTextSize(2);
    display.setCursor(16,24);
    if ((millis()/ 500 % 2 == 0)){
      display.print("(=OWO=)!!");
    }
    return;
  }
  DateTime currentTime =  rtc.now();
  display.setTextSize(1);
  display.setCursor(40,32);
  if (PetState == 2){
    display.print("(=O_O=)!!");
  }else if (PetState == 1){
    display.print("(=u_u=)zZ");
  }else if (currentTime.hour() >= 23 || currentTime.hour() < 6){
    display.print("(=UwU=)zZ");
  }else  if (PetState == 0){
    display.print("(=^w^=)");
  }
}

bool btnPressed(int pin){
  if (millis()-lastBtnTime<200) {
    return false;
    }
   else if (digitalRead(pin)==LOW){
    delay(50);
    lastBtnTime = millis();
    return true;
    }
   return false;
} 

bool checkWakeUp(){
  if(PetState == 1){
    if(digitalRead(BTN_MENU) == LOW || digitalRead(BTN_OK) == LOW ||
       digitalRead(BTN_PLUS) == LOW || digitalRead(BTN_SWITCH) == LOW){
        PetState = 2;
        last_active_time = millis();
        tone(BUZZER_PIN , 1500 ,100);
        delay(100);
        noTone(BUZZER_PIN);
        delay(100);
        return true;
       }
  }
  return false;
}