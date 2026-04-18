#include <Arduino.h>
#include <U8g2lib.h>


// oled 的引脚
#define SCL_PIN     22
#define SDA_PIN     21

// 两个指示灯的引脚
#define LED_PIN_1   26
#define LED_PIN_2   27

// 蜂鸣器的引脚
#define PIEZO_PIN   18

// 按钮的引脚
#define BUTTON_PIN  19

// 按钮的等待时间
#define BUTTON_TIMEOUT 200
#define DEBOUNCE_TIME 20

// 电位器的引脚 
#define POTE_PIN 34

#define DIGITAL_PART_NUM 7
#define DIGITAL_POSI_NUM 3


// 声明函数
void draw_line();
void show_menu(int cursor);
void print_msg_on_oled(const char *msg[], int len, int cursor);
int isOnClick();
void IRAM_ATTR shutdown_piezo();
void IRAM_ATTR shutdown_led1();
void IRAM_ATTR shutdown_led2();
void led_on(int led_pin, hw_timer_t *timer);
int adjust_val(int n);
void print_msg_on_oled(const char *msg[], int len, int cursor);

// piezo 定时器
hw_timer_t *timer_piezo = NULL;
// led 定时器
hw_timer_t *timer_led = NULL;
hw_timer_t *timer_led2 = NULL;

// 按钮状态
enum ButtonState {
  BUTTON_IDLE,
  WAIT_RELEASE,
  WAIT_DOUBLE,
  WAIT_DOUBLE_RELEASE
};
ButtonState currentState = BUTTON_IDLE;

// 屏幕状态
enum ScreenState {
  MAIN,
  ADJUST,
  ADJUST_VOICE,
  ADJUST_BRIGHTNESS
};
ScreenState currentScreen = MAIN;


// 初始化亮度以及音量
static int brightness = 128;
static int voiceness = 128;


// 实例化 u8g2 对象
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, SCL_PIN, SDA_PIN, -1);

// 定义菜单列表
const char *menu_list[] = {
  "adjust voice",
  "adjust brightness",
};
const char *menu_adjust_view[] = {
  "To adjust voice",
  "To adjust brightness",
};
int menu_count = sizeof(menu_list) / sizeof(menu_list[0]);

// 打印信息
void print_msg_on_oled(const char *msg[], int len, int cursor)
{
  u8g2.clearBuffer();
  u8g2.drawStr(0, 12, "Menu");
  for (int i = 0; i < len; i ++)
  {
    if (i == cursor)
    {
      u8g2.drawStr(0, 12 + ( i + 1 ) * 12 + 10, ">");
      u8g2.drawStr(20, 12 + ( i + 1 ) * 12 + 10, msg[i]);
    }
    else {
      u8g2.drawStr(0, 12 + ( i + 1 ) * 12 + 10, msg[i]);
    }
  }
}

void print_adjust_msg_on_oled(const char *msg[], int num)
{
  u8g2.clearBuffer();
  u8g2.drawStr(0, 12, menu_list[num]);
  draw_line();
  u8g2.drawStr(0, 12 + 12 + 10, msg[num]);
  u8g2.sendBuffer();
}
// 绘制标题与选项的分割线
void draw_line()
{
  u8g2.drawLine(0, 16, 128, 16);
}

// 绘制菜单
void show_menu(int cursor)
{
  
  u8g2.firstPage();
  do {
    print_msg_on_oled(menu_list, menu_count, cursor);
    draw_line();
  } while (u8g2.nextPage());
}


// 判断按钮是否被点击
unsigned long lastActionTime = 0;
int isOnClick()
{
  bool reading = digitalRead(BUTTON_PIN);
  unsigned long now = millis();
  int result = 0;

  switch (currentState)
  {
    case BUTTON_IDLE:
      if (reading == LOW)
      {
        lastActionTime = now;
        currentState = WAIT_RELEASE;
      }
      break;

    case WAIT_RELEASE:
      if (reading == HIGH)
      {
        if (now - lastActionTime > DEBOUNCE_TIME)
        {
          currentState = WAIT_DOUBLE;
          lastActionTime = now;
        }
        else
        {
          currentState = BUTTON_IDLE;
        }
      }
      break;

    case WAIT_DOUBLE:
      if (reading == LOW)
      {
        if (now - lastActionTime > DEBOUNCE_TIME)
        {
          result = 2;
          currentState = WAIT_DOUBLE_RELEASE;
        }
      }
      else if (now - lastActionTime > BUTTON_TIMEOUT)
      {
        result = 1;
        currentState = BUTTON_IDLE;
      }
      break;
    
    case WAIT_DOUBLE_RELEASE:
      if (reading == HIGH)
      {
        currentState = BUTTON_IDLE;
      }
  }
  return result;

}

// 指示灯关闭
void IRAM_ATTR shutdown_led1()
{
  // digitalWrite(LED_PIN_1, LOW);
  ledcWrite(1, 0);
  timerAlarmDisable(timer_led);
}
void IRAM_ATTR shutdown_led2()
{
  ledcWrite(2, 0);
  timerAlarmDisable(timer_led);
}

void led_on(int led_pin, hw_timer_t *timer)
{
  
  // digitalWrite(led_pin, HIGH);
  ledcWrite(led_pin == LED_PIN_1 ? 1 : 2, brightness);
  timerWrite(timer, 0);
  timerAlarmEnable(timer);
}

// 蜂鸣器静音
void IRAM_ATTR shutdown_piezo()
{
  ledcWrite(0, 0);
  timerAlarmDisable(timer_piezo);
}

// piezo 发声
void piezo_beep()
{
    ledcWrite(0, voiceness);
    timerWrite(timer_piezo, 0);
    timerAlarmEnable(timer_piezo);
}

// 调节数值
int adjust_val(int n)
{
  int val = map(analogRead(POTE_PIN), 0, 4095, 0, n);
  val = constrain(val, 0, n);
  return val;
}

// led 常亮
void led_always_on()
{
  int val = adjust_val(255);
  ledcWrite(1, val);
  ledcWrite(2, val);
  brightness = val;
}

// piezo 常响
void piezo_always_on()
{
  int val = adjust_val(255);
  ledcWrite(0, val);
  voiceness = val;
}

// 手动关闭 led
void led_manual_off()
{
  ledcWrite(1, 0);
  ledcWrite(2, 0);
}

// 手动关闭 piezo
void piezo_manual_off()
{
  ledcWrite(0, 0);
}

void setup()
{
  // 串口调试
  Serial.begin(115200);

  // 初始化 OLED 显示屏
  u8g2.begin();
  u8g2.setFont(u8g2_font_wqy12_t_gb2312b);

  // 初始化指示灯
  pinMode(LED_PIN_1, OUTPUT);
  pinMode(LED_PIN_2, OUTPUT);

  // 初始化按钮
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // PWM 初始化piezo
  ledcSetup(0, 2000, 8);
  ledcAttachPin(PIEZO_PIN, 0);

  // PWM 初始化指示灯
  ledcSetup(1, 2000, 8);
  ledcAttachPin(LED_PIN_1, 1);
  ledcSetup(2, 2000, 8);
  ledcAttachPin(LED_PIN_2, 2);

  timer_piezo = timerBegin(0, 80, true);
  timer_led = timerBegin(1, 80, true);
  timer_led2 = timerBegin(2, 20, true);
  
  // 设置定时器中断
  timerAttachInterrupt(timer_piezo, &shutdown_piezo, true);
  timerAlarmWrite(timer_piezo, 200*1000, false);

  timerAttachInterrupt(timer_led, &shutdown_led1, true);
  timerAlarmWrite(timer_led, 200*1000, false);

  timerAttachInterrupt(timer_led2, &shutdown_led2, true);
  timerAlarmWrite(timer_led2, 500*1000, false);

}



void loop()
{
  static int cursor = -1;
  int checkButton = isOnClick();

  switch (currentScreen)
  {
    case MAIN:
      if (checkButton == 1)
      {
        led_on(LED_PIN_1, timer_led);
        piezo_beep();
        cursor ++;
        if (cursor > 1) cursor = 0; 
        show_menu(cursor);
      }
      else if (checkButton == 2)
      {
        led_on(LED_PIN_2, timer_led2);
        piezo_beep();
        print_adjust_msg_on_oled(menu_adjust_view, cursor);
        currentScreen = ADJUST;
      }
      break;
    case ADJUST:
      switch (cursor)
      {
        case 0:
          if (!checkButton)
          {
            piezo_always_on();
          }
          else if (checkButton == 1)
          {
            piezo_manual_off();
            currentScreen = MAIN;
            show_menu(cursor);
          }
          break;
        case 1:
        if (!checkButton)
        {
          led_always_on();
        }
        else if (checkButton == 1)
        {
          led_manual_off();
          currentScreen = MAIN;
          show_menu(cursor);
        }
          break;
      }
  }
  // checkButton = 0;
}