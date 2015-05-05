#ifndef __MOCKGATE_H
#define __MOCKGATE_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>

#include "debug.h"

#define HIGH (1)
#define LOW  (0)

extern const uint8_t  RADIO_PIN;
extern const uint8_t  TARGET_MSG[5];
extern const uint16_t TARGET_BITRATE;

extern const uint8_t MOTOR_PWMA_PIN;
extern const uint8_t MOTOR_AIN1_PIN;
extern const uint8_t MOTOR_AIN2_PIN;
extern const uint8_t MOTOR_PWMB_PIN;
extern const uint8_t MOTOR_BIN1_PIN;
extern const uint8_t MOTOR_BIN2_PIN;
extern const uint8_t MOTOR_STBY_PIN;

extern const uint8_t  MOTOR_NUM_STEPS;
extern const uint16_t MOTOR_SPEED_DEFAULT;
extern const uint32_t MOTOR_PWM_MAX_FREQ;

extern sem_t code_match;
extern sem_t new_sample;


void shutdown(int8_t);

extern volatile bool   shutdown_received;
extern pthread_cond_t  shutdown_received_cond;
extern pthread_mutex_t shutdown_received_lock;

static inline void wait_for_shutdown(void)
{
    pthread_cond_wait(&shutdown_received_cond, &shutdown_received_lock);
}

#endif