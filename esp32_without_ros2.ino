// ==================================================
// Project          : ESP32 without ROS2
// Date Created     : 05-04-2025
// Author           : duypham
// Description      : Low-level control using FreeRTOS for 2 motor drivers
// Target MCU       : ESP32-WROOM-32
// Framework        : Arduino (ESP32 core)
// ==================================================


#include <ModbusMaster.h>
#include <ZLAC8015D.h>
#include <Arduino.h>

// ==================================================
// HYPERPARAMETER
// ==================================================
const int rcPin2 = 17; // 1298~1805 ga CH2 | default: 1559~1560
const int rcPin4 = 18; // 1205~1685 trái phải CH4 | default: 1456~1457
const int rcPin5 = 14; // chinh auto/stop/manual

const uint8_t MODE = 3;

const uint16_t L_MS_A1 = 200; // Tang toc motor 1 (DRIVER L)
const uint16_t L_MS_A2 = 200; // Tang toc motor 2
const uint16_t L_MS_D1 = 200; // Giam toc motor 1
const uint16_t L_MS_D2 = 200; // Giam toc motor 2

const uint16_t R_MS_A1 = 200; // (DRIVER R tuong tu)
const uint16_t R_MS_A2 = 200;
const uint16_t R_MS_D1 = 200;
const uint16_t R_MS_D2 = 200;

// CH2
// FORWARD
const uint16_t bottom_forward_threshold = 1571;
const uint16_t top_forward_threshold = 1810;
// STOP
const uint16_t bottom_stop_threshold = 1540;
const uint16_t top_stop_threshold = 1570;
// BACKWARD
const uint16_t bottom_backward_threshold = 1280;
const uint16_t top_backward_threshold = 1538;

// CH4
// TURN LEFT
const uint16_t bottom_turnleft_threshold = 1470;
const uint16_t top_turnleft_threshold = 1700;
// TURN RIGHT
const uint16_t bottom_turnright_threshold = 1200;
const uint16_t top_turnright_threshold = 1448;

//CH5
const uint16_t ch5_threshold = 1500;

//FREERTOS
TaskHandle_t xTask1Handle = NULL;
TaskHandle_t xTask2Handle = NULL;

const uint16_t STACK_SIZE = 2048;
const uint8_t tskIDLE_PRIORITY = 1;
const uint8_t PRO_CPU = 0;
const uint8_t APP_CPU = 1;


// ==================================================
// PARAMETER
// ==================================================
#define ButtonPhysic 15
#define MAX485_DE 32
#define MAX485_RE_NEG  33
#define MODBUS_RX_PIN 27
#define MODBUS_TX_PIN 13

ModbusMaster nodeL;
ModbusMaster nodeR;

ZLAC8015D driverL;
ZLAC8015D driverR;

uint8_t check_mode;

int16_t rpm_left;
int16_t rpm_right;

bool buttonSOFT = false;
bool buttonPHYSIC = false;

volatile unsigned long pulseWidth2 = 1500;
volatile unsigned long pulseWidth4 = 1500;
volatile unsigned long pulseWidth5 = 0;

volatile unsigned long lastTime2 = 0;
volatile unsigned long lastTime4 = 0;
volatile unsigned long lastTime5 = 0;


// ==================================================
// TASK1
// ==================================================
void Task1(void *pvParameters) {
  for(;;) {
    noInterrupts();
    unsigned long pw2 = pulseWidth2;
    unsigned long pw4 = pulseWidth4;
    interrupts();

    if (!checkButton() && check_mode == 0){
      // CH2: FORWARD | STOP | BACKWARD
      if (pw2 > bottom_forward_threshold && pw2 < top_forward_threshold){
        int base_speed = map(pw2, 1571, 1850, 0, 175);
        rpm_left = base_speed;
      }
      else if (pw2 > bottom_stop_threshold && pw2 < top_stop_threshold){
        int base_speed = 0;
        rpm_left = base_speed;
      }
      else if (pw2 > bottom_backward_threshold && pw2 < top_backward_threshold){
        int base_speed = map(pw2, 1280, 1538, -160, 0);
        rpm_left = base_speed;
      }

      // CH4: TURN LEFT | TURN RIGHT
      if (pw4 > bottom_turnleft_threshold &&pw4 < top_turnleft_threshold){
        int turn_speed = map(pw4, 1470, 1690, 0, 80);
        rpm_left = turn_speed;
      }
      else if (pw4 > bottom_turnright_threshold && pw4 < top_turnright_threshold){
        int turn_speed = map(pw4, 1200, 1448, 80, 0);
        rpm_left = -turn_speed;
      }    
    }
    else{
      rpm_left = 0;
    }

    rpm_left = constrain(rpm_left, -125, 125);
      driverL.set_rpm(rpm_left, rpm_left);
      Serial.print("RPM LEFT: "); Serial.println(rpm_left);
      vTaskDelay(10 / portTICK_PERIOD_MS);
      int16_t fb1[2];
      driverL.get_rpm(fb1);
      Serial.print("[Driver L] Feedback RPM L:"); Serial.print(fb1[0]);
      Serial.print(" L:");Serial.println(fb1[1]);
  }
}


// ==================================================
// TASK2
// ==================================================
void Task2(void *pvParameters) {
  for(;;) {
    noInterrupts();
    unsigned long pw2 = pulseWidth2;
    unsigned long pw4 = pulseWidth4;
    interrupts();

    if (!checkButton() && check_mode == 0){
      // CH2: FORWARD | STOP | BACKWARD
      if (pw2 > bottom_forward_threshold && pw2 < top_forward_threshold){
        int base_speed = map(pw2, 1571, 1850, 0, 175);
        rpm_right = - base_speed;
      }
      else if (pw2 > bottom_stop_threshold && pw2 < top_stop_threshold){
        int base_speed = 0;
        rpm_right = base_speed;
      }
      else if (pw2 > bottom_backward_threshold && pw2 < top_backward_threshold){
        int base_speed = map(pw2, 1280, 1538, -160, 0);
        rpm_right = - base_speed;
      }

      // CH4: TURN LEFT | TURN RIGHT
      if (pw4 > bottom_turnleft_threshold &&pw4 < top_turnleft_threshold){
        int turn_speed = map(pw4, 1470, 1690, 0, 80);
        rpm_right =  turn_speed;
      }
      else if (pw4 > bottom_turnright_threshold && pw4 < top_turnright_threshold){
        int turn_speed = map(pw4, 1200, 1448, 80, 0);
        rpm_right =  -turn_speed;
      }    
    }
    else{
      rpm_right = 0;
    }
    rpm_right = constrain(rpm_right, -125, 125);
      driverR.set_rpm(rpm_right, rpm_right);
      Serial.print("RPM RIGHT: "); Serial.println(rpm_right);
      vTaskDelay(10 / portTICK_PERIOD_MS);
      int16_t fb1[2];
      driverR.get_rpm(fb1);
      Serial.print("[Driver R] Feedback RPM R:"); Serial.print(fb1[0]);
      Serial.print(" R:");Serial.println(fb1[1]);
  }
}

// ==================================================
// INTERRUPT
// ==================================================
void preTransmission() {
  digitalWrite(MAX485_RE_NEG, 1);
  digitalWrite(MAX485_DE, 1);
}

void postTransmission() {
  digitalWrite(MAX485_RE_NEG, 0);
  digitalWrite(MAX485_DE, 0);
}

void IRAM_ATTR handleInterrupt2() {
  if (digitalRead(rcPin2) == HIGH){
    lastTime2 = micros();
  }
  else{
    pulseWidth2 = micros() - lastTime2;
  }
}

void IRAM_ATTR handleInterrupt4() {
  if (digitalRead(rcPin4) == HIGH){
    lastTime4 = micros();
  }
  else{
    pulseWidth4 = micros() - lastTime4;
  }
}

void IRAM_ATTR handleInterrupt5() {
  if (digitalRead(rcPin5) == HIGH){
    lastTime5 = micros();
  }
  else{
    pulseWidth5 = micros() - lastTime5;
  }
}

bool checkButton(){
  if (pulseWidth5 < 2000 && pulseWidth5 > 1990){
    check_mode = 0;
    buttonSOFT = false;
  }
  else if (pulseWidth5 < 1500 && pulseWidth5 > 1490){
    check_mode = 1;
    buttonSOFT = true;
  }
  else if (pulseWidth5 < 1000 && pulseWidth5 > 990){
    check_mode = 2;
    buttonSOFT = false;
  }
  buttonPHYSIC = (digitalRead(ButtonPhysic) == LOW);
  // Serial.print("SOFT: "); Serial.print(buttonSOFT);
  // Serial.print(" | PHYSIC: "); Serial.print(buttonPHYSIC);
  // Serial.print(" | ANY: "); Serial.println(buttonSOFT || buttonPHYSIC);
  return buttonSOFT || buttonPHYSIC;
}

// ==================================================
// SETUP
// ==================================================
void setup() {
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8E1, MODBUS_RX_PIN, MODBUS_TX_PIN);

  pinMode(MAX485_RE_NEG, OUTPUT);
  pinMode(MAX485_DE, OUTPUT);
  digitalWrite(MAX485_RE_NEG, 0);
  digitalWrite(MAX485_DE, 0);

  pinMode(rcPin2, INPUT);
  pinMode(rcPin4, INPUT);
  pinMode(rcPin5, INPUT);
  
  pinMode(ButtonPhysic, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(rcPin2), handleInterrupt2, CHANGE);
  attachInterrupt(digitalPinToInterrupt(rcPin4), handleInterrupt4, CHANGE);
  attachInterrupt(digitalPinToInterrupt(rcPin5), handleInterrupt5, CHANGE);

  nodeL.begin(1, Serial1);
  nodeR.begin(2, Serial1);

  nodeL.preTransmission(preTransmission);
  nodeL.postTransmission(postTransmission);
  nodeR.preTransmission(preTransmission);
  nodeR.postTransmission(postTransmission);

  driverL.set_modbus(&nodeL);
  driverR.set_modbus(&nodeR);

  driverL.disable_motor();
  driverL.set_mode(MODE);
  driverL.enable_motor();
  driverL.set_accel_time(L_MS_A1, L_MS_A2);
  driverL.set_accel_time(L_MS_D1, L_MS_D2);

  driverR.disable_motor();
  driverR.enable_motor();
  driverR.set_accel_time(R_MS_A1, R_MS_A2);
  driverR.set_accel_time(R_MS_D1, R_MS_D2);

  xTaskCreatePinnedToCore(Task1, "Task1", STACK_SIZE, NULL, tskIDLE_PRIORITY, &xTask1Handle, PRO_CPU);
  xTaskCreatePinnedToCore(Task2, "Task2", STACK_SIZE, NULL, tskIDLE_PRIORITY, &xTask2Handle, APP_CPU);

}
void loop() {
  // Khong co gi dau dung nhin 
}
