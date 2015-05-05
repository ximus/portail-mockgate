#include <stdint.h>

const uint8_t  RADIO_PIN = 31; // GP44
const uint8_t  TARGET_MSG[5] = {0xb2, 0xd9, 0x2d, 0x92, 0x58};
// const uint8_t  TARGET_MSG[5] = {0x58, 0x92, 0x2d, 0xd9, 0xb2};
const uint16_t TARGET_BITRATE = 3000;

const uint8_t MOTOR_PWMA_PIN = 20; // PWM0
const uint8_t MOTOR_AIN1_PIN = 33; // GP48
const uint8_t MOTOR_AIN2_PIN = 46; // GP47
const uint8_t MOTOR_PWMB_PIN = 14; // PWM1
const uint8_t MOTOR_BIN1_PIN = 48; // GP15
const uint8_t MOTOR_BIN2_PIN = 36; // GP14
const uint8_t MOTOR_STBY_PIN = 47; // GP49

const uint8_t  MOTOR_NUM_STEPS     = 200;
const uint16_t MOTOR_SPEED_DEFAULT = 30;    // rpm
const uint32_t MOTOR_PWM_MAX_FREQ  = 1600;  // Hz