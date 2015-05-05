//#define MOTORDEBUG

#include <stdint.h>
#include <stdbool.h>

#define MICROSTEPS 16         // 8 or 16

#define FORWARD 1
#define BACKWARD 2
#define BRAKE 3
#define RELEASE 4

#define SINGLE 1
#define DOUBLE 2
#define INTERLEAVE 3
#define MICROSTEP 4

int motor_init(uint32_t, uint8_t);
void motor_goto(int32_t);
void motor_step(int32_t);
void motor_stop(void);
void motor_set_speed(uint16_t);
uint16_t motor_get_position(void);
extern bool motor_is_moving;