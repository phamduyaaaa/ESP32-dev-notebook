#include <ModbusMaster.h>
#include <ZLAC8015D.h>
#include <stdint.h>
#include <micro_ros_arduino.h>
#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <std_msgs/msg/int32.h>

#define LED_PIN 13

#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){error_loop();}}
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){}}

std_msgs__msg__Int32 ch5_msg;
rcl_publisher_t publisher;
rclc_executor_t executor;
rclc_support_t support;
rcl_allocator_t allocator;
rcl_node_t node;
rcl_timer_t timer;
const int rcPin5 = 14;

int bottom_threshold_auto = 900;
int top_threshold_auto = 1100;
int bottom_threshold_stop = 1400;
int top_threshold_stop = 1600;
int bottom_threshold_manual = 1900;
int top_threshold_manual = 2100;

volatile unsigned long pulseWidth5 = 0;
volatile unsigned long lastTime5 = 0;

void IRAM_ATTR handleInterrupt6(){
  if (digitalRead(rcPin5) == HIGH){
    lastTime5 = micros();
  }
  else{
    pulseWidth5 = micros() - lastTime5;
  }
}

int convert_ch5_value(){
  int x = pulseWidth5;
  if(bottom_threshold_stop < x && x < top_threshold_stop){
    x = 2;
  }
  else if(bottom_threshold_manual < x && x < top_threshold_manual){
    x = 1;
  }
  else if(bottom_threshold_auto < x && x < top_threshold_auto){
    x = 3;
  }
  return x;
}

void error_loop(){
  while(1){
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    delay(100);
  }
}

void ch5_callback(rcl_timer_t * timer, int64_t last_call_time){
  if(timer != NULL){
    ch5_msg.data = convert_ch5_value();
    RCSOFTCHECK(rcl_publish(&publisher, &ch5_msg, NULL));
  }
}

void setup() {
  // put your setup code here, to run once:
  set_microros_transports();
  pinMode(rcPin5, INPUT);
  
  attachInterrupt(digitalPinToInterrupt(rcPin5), handleInterrupt6, CHANGE);

  delay(500);

  allocator = rcl_get_default_allocator();
  RCCHECK(rclc_support_init(&support, 0, NULL, &allocator));
  RCCHECK(rclc_node_init_default(&node, "esp32_node", "", &support));
  RCCHECK(rclc_publisher_init_default(
    &publisher,
    &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32), "esp32_control"));
  
  const unsigned int timer_timeout = 1000;
  RCCHECK(rclc_timer_init_default(
    &timer,
    &support,
    RCL_MS_TO_NS(timer_timeout),
    ch5_callback));
  RCCHECK(rclc_executor_init(&executor, &support.context, 1, &allocator));
  RCCHECK(rclc_executor_add_timer(&executor, &timer));
}

void loop() {
  RCSOFTCHECK(rclc_executor_spin_some(&executor, RCL_MS_TO_NS(100)));
  delay(100);
}
