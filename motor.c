#include <pthread.h>
#include <time.h>
#include <stdbool.h>
#include <unistd.h>

#include "mraa.h"
#include "motor.h"
#include "app.h"

#if (MICROSTEPS == 8)
uint8_t microstepcurve[] = {0, 50, 98, 142, 180, 212, 236, 250, 255};
#elif (MICROSTEPS == 16)
uint8_t microstepcurve[] = {0, 25, 50, 74, 98, 120, 141, 162, 180, 197, 212, 225, 236, 244, 250, 253, 255};
#endif


int8_t          motor_direction;
static uint16_t revsteps; // # steps per revolution
static uint8_t  current_step;
static uint8_t  step_style;
static int32_t  current_pos;
static int32_t  target_pos;
static bool     target_changed;
bool            motor_is_moving;

uint32_t steppingcounter;
uint32_t usperstep;

mraa_pwm_context pinPWMA;
mraa_pwm_context pinPWMB;
mraa_gpio_context pinAIN1;
mraa_gpio_context pinAIN2;
mraa_gpio_context pinBIN1;
mraa_gpio_context pinBIN2;
mraa_gpio_context pinSTBY;


static inline void pwm_write(mraa_pwm_context pwm, float value) {
    // while debugging noticed alot of consecutive calls with matching
    // args, should maybe not write unless value !=
    int err = MRAA_SUCCESS;
    // printf("write pwm %f\n", value);
    err = mraa_pwm_write(pwm, value);
    if (err != MRAA_SUCCESS) {
        mraa_result_print(err);
    }
}

static inline mraa_pwm_context init_pwm(int pin, uint32_t freq)
{
    mraa_pwm_context pwm = mraa_pwm_init(pin);
    if (pwm == NULL) {
        printf("failed to init motor pwm on pin %d\n", pin);
        return NULL;
    }
    int period = 1000000/freq;
    DEBUG(
        "init motor pwm at pin %d to %d Hz (period of %dus)\n",
        pin, freq, period
    );
    int err = MRAA_SUCCESS;
    err = mraa_pwm_period_us(pwm, period);
    err = mraa_pwm_enable(pwm, 1);
    if (err != MRAA_SUCCESS) {
        printf("error initializing pwm at pin %d :\n", pin);
        mraa_result_print(err);
        return NULL;
    }
    pwm_write(pwm, 0);
    return pwm;
}

static void *motor_loop(void *arg);

int motor_init(uint32_t freq, uint8_t style)
{
    current_step = 0;
    target_pos = 0;
    current_pos = 0;
    motor_direction = 0;
    usperstep = 0;
    revsteps = MOTOR_NUM_STEPS;
    target_changed = false;
    motor_is_moving = false;
    step_style = style;

    int err = MRAA_SUCCESS;

    // ideally should check all those pin and pwm values are non NULL
    pinPWMA = init_pwm(MOTOR_PWMA_PIN, freq);
    pinPWMB = init_pwm(MOTOR_PWMB_PIN, freq);
    if (pinPWMA == NULL || pinPWMB == NULL) {
        return 1;
    }
    mraa_pwm_enable(pinPWMA, 0);
    mraa_pwm_enable(pinPWMB, 0);
    mraa_pwm_enable(pinPWMA, 1);
    mraa_pwm_enable(pinPWMB, 1);
    pwm_write(pinPWMA, 0);
    pwm_write(pinPWMB, 0);

    pinAIN1 = mraa_gpio_init(MOTOR_AIN1_PIN);
    pinAIN2 = mraa_gpio_init(MOTOR_AIN2_PIN);
    pinBIN1 = mraa_gpio_init(MOTOR_BIN1_PIN);
    pinBIN2 = mraa_gpio_init(MOTOR_BIN2_PIN);
    pinSTBY = mraa_gpio_init(MOTOR_STBY_PIN);

    err = mraa_gpio_dir(pinAIN1, MRAA_GPIO_OUT);
    err = mraa_gpio_dir(pinAIN2, MRAA_GPIO_OUT);
    err = mraa_gpio_dir(pinBIN1, MRAA_GPIO_OUT);
    err = mraa_gpio_dir(pinBIN2, MRAA_GPIO_OUT);
    err = mraa_gpio_dir(pinSTBY, MRAA_GPIO_OUT);

    err = mraa_gpio_write(pinSTBY, HIGH);


    if ( err != MRAA_SUCCESS ) {
        mraa_result_print(err);
    }

    motor_set_speed(MOTOR_SPEED_DEFAULT);

    static pthread_t motor_thread;
    static struct sched_param param;
    param.sched_priority = 10;
    static int policy = SCHED_FIFO;
    pthread_setschedparam(motor_thread, policy, &param);

    err = pthread_create(&motor_thread, NULL, motor_loop, NULL);
    // err = pthread_detach(motor_thread);
    if(err)
    {
        fprintf(stderr, "error creating motor thread return code: %d\n", err);
    }

    return err;
}

static void teardown(void *arg)
{
    printf("shuting down motor ...\n");
    int err = MRAA_SUCCESS;
    err = mraa_gpio_write(pinAIN1, LOW);
    err = mraa_gpio_write(pinAIN2, LOW);
    err = mraa_gpio_write(pinBIN1, LOW);
    err = mraa_gpio_write(pinBIN2, LOW);
    err = mraa_gpio_write(pinSTBY, LOW);
    err = mraa_pwm_close(pinPWMA);
    err = mraa_pwm_close(pinPWMB);

    if ( err != MRAA_SUCCESS ) {
        printf("error in motor cleanup:\n");
        mraa_result_print(err);
    }
}

static void step(uint16_t, uint8_t, uint8_t);

static void *motor_loop(void *arg)
{
    pthread_cleanup_push(teardown, NULL);

    while (!shutdown_received)
    {
        if (current_pos != target_pos)
        {
            target_changed = false;
            motor_is_moving = true;
            DEBUG("motor has locked on a new target=%d, current=%d\n", target_pos, current_pos);

            int8_t dir;
            if (target_pos > current_pos) {
                dir = FORWARD;
            } else if (target_pos < current_pos) {
                dir = BACKWARD;
            } else {
                dir = BRAKE;
            }
            motor_direction = dir;
            step(abs(target_pos - current_pos), dir, step_style);
        }
        else
        {
            motor_is_moving = false;
            // not sure what ideal sleep time should be
            // need a proper sync variable, semaphore
            // usleep(1000);
            sched_yield();
        }
    }

    // passing 1 has no meaning, just satisfy function
    pthread_cleanup_pop(1);
    return NULL;
}

/* TB6612FNG Control Functions
     Source: H-SW Control Functions found in the Datasheet on Page 4
             https://www.sparkfun.com/datasheets/Robotics/TB6612FNG.pdf
     Pin#  = IN1    IN2
     Logic = inPin1 inPin2  Mode
              0     0       OFF (High Impedance)
              0     1       BACKWARD (CCW)
              1     0       FORWARD  (CW)
              1     1       Short brake
*/
static inline uint8_t single_step(uint8_t dir, uint8_t style)
{
    uint8_t ocrb, ocra;

    ocra = ocrb = 255;


    // next determine what sort of stepping procedure we're up to
    if (style == SINGLE) {
        if ((current_step / (MICROSTEPS / 2)) % 2) { // we're at an odd step, weird
            if (dir == FORWARD) {
                current_step += MICROSTEPS / 2;
            }
            else {
                current_step -= MICROSTEPS / 2;
            }
        } else {           // go to the next even step
            if (dir == FORWARD) {
                current_step += MICROSTEPS;
            }
            else {
                current_step -= MICROSTEPS;
            }
        }
    } else if (style == DOUBLE) {
        if (! (current_step / (MICROSTEPS / 2) % 2)) { // we're at an even step, weird
            if (dir == FORWARD) {
                current_step += MICROSTEPS / 2;
            } else {
                current_step -= MICROSTEPS / 2;
            }
        } else {           // go to the next odd step
            if (dir == FORWARD) {
                current_step += MICROSTEPS;
            } else {
                current_step -= MICROSTEPS;
            }
        }
    } else if (style == INTERLEAVE) {
        if (dir == FORWARD) {
            current_step += MICROSTEPS / 2;
        } else {
            current_step -= MICROSTEPS / 2;
        }
    }

    if (style == MICROSTEP) {
        if (dir == FORWARD) {
            current_step++;
        } else {
            // BACKWARDS
            current_step--;
        }

        current_step += MICROSTEPS * 4;
        current_step %= MICROSTEPS * 4;

        ocra = ocrb = 0;
        if ( (current_step >= 0) && (current_step < MICROSTEPS)) {
            ocra = microstepcurve[MICROSTEPS - current_step];
            ocrb = microstepcurve[current_step];
        } else if  ( (current_step >= MICROSTEPS) && (current_step < MICROSTEPS * 2)) {
            ocra = microstepcurve[current_step - MICROSTEPS];
            ocrb = microstepcurve[MICROSTEPS * 2 - current_step];
        } else if  ( (current_step >= MICROSTEPS * 2) && (current_step < MICROSTEPS * 3)) {
            ocra = microstepcurve[MICROSTEPS * 3 - current_step];
            ocrb = microstepcurve[current_step - MICROSTEPS * 2];
        } else if  ( (current_step >= MICROSTEPS * 3) && (current_step < MICROSTEPS * 4)) {
            ocra = microstepcurve[current_step - MICROSTEPS * 3];
            ocrb = microstepcurve[MICROSTEPS * 4 - current_step];
        }
    }

    current_step += MICROSTEPS * 4;
    current_step %= MICROSTEPS * 4;

    if (dir == FORWARD) {
        current_pos += 1;
    }
    else {
        current_pos -= 1;
    }

    #ifdef MOTORDEBUG
      // DEBUG("current step=%d\n", current_step);
      // DEBUG("  pwmA=%d\n", ocra);
      // DEBUG("  pwmB=%d\n", ocrb);
    #endif

    pwm_write(pinPWMA, (float) ocra / 255);
    pwm_write(pinPWMB, (float) ocrb / 255);

    uint8_t latch_state = 0; // all motor pins to 0

    switch (current_step/(MICROSTEPS/2)) {
        case 0:
          latch_state |= 0x1; // energize coil 1 only
          break;
        case 1:
          latch_state |= 0x3; // energize coil 1+2
          break;
        case 2:
          latch_state |= 0x2; // energize coil 2 only
          break;
        case 3:
          latch_state |= 0x6; // energize coil 2+3
          break;
        case 4:
          latch_state |= 0x4; // energize coil 3 only
          break;
        case 5:
          latch_state |= 0xC; // energize coil 3+4
          break;
        case 6:
          latch_state |= 0x8; // energize coil 4 only
          break;
        case 7:
          latch_state |= 0x9; // energize coil 1+4
          break;
    }

    if (latch_state & 0x1) {
       // Serial.println(AIN2pin);
        mraa_gpio_write(pinAIN2, HIGH);
    } else {
        mraa_gpio_write(pinAIN2, LOW);
    }
    if (latch_state & 0x2) {
        mraa_gpio_write(pinBIN1, HIGH);
    // Serial.println(BIN1pin);
    } else {
        mraa_gpio_write(pinBIN1, LOW);
    }
    if (latch_state & 0x4) {
        mraa_gpio_write(pinAIN1, HIGH);
    // Serial.println(AIN1pin);
    } else {
        mraa_gpio_write(pinAIN1, LOW);
    }
    if (latch_state & 0x8) {
        mraa_gpio_write(pinBIN2, HIGH);
    // Serial.println(BIN2pin);
    } else {
        mraa_gpio_write(pinBIN2, LOW);
    }

    return current_step;
}

static void step(uint16_t steps, uint8_t dir, uint8_t style)
{
    uint32_t uspers = usperstep;
    uint8_t ret = 0;

    if (style == INTERLEAVE) {
        uspers /= 2;
    }
    else if (style == MICROSTEP) {
        uspers /= MICROSTEPS;
#ifdef MOTORDEBUG
        DEBUG("moving by steps=%d, dir=%d\n", steps, dir);
#endif
    }

    while (steps-- && !target_changed && !shutdown_received) {
        //Serial.println("step!"); Serial.println(uspers);
        ret = single_step(dir, style);
        usleep(uspers);
        steppingcounter += (uspers % 1000);
        if (steppingcounter >= 1000) {
          usleep(1000);
          steppingcounter -= 1000;
        }
    }
    if (style == MICROSTEP) {
        while ((ret != 0) && (ret != MICROSTEPS)) {
            ret = single_step(dir, style);
            usleep(uspers);
            steppingcounter += (uspers % 1000);
            if (steppingcounter >= 1000) {
              usleep(1000);
              steppingcounter -= 1000;
            }
        }
    }
}

static inline int32_t logical_steps_to_real(int32_t steps, uint8_t style)
{
    if (style == MICROSTEP) {
        steps *= MICROSTEPS;
    }
    return steps;
}

static inline void set_target_pos(int32_t value)
{
    target_pos = logical_steps_to_real(value, step_style);
    target_changed = true;
}

void motor_step(int32_t steps)
{
    set_target_pos(current_pos + steps);
}

void motor_goto(int32_t to_step)
{
    set_target_pos(to_step);
}

void motor_stop(void)
{
    set_target_pos(current_pos);
}

uint16_t motor_get_position(void) {
    return current_pos;
}

// revs/min
void motor_set_speed(uint16_t rpm) {
    DEBUG("steps per rev: %d\n", revsteps);
    DEBUG("RPM: %d\n", rpm);
    usperstep = 60000000 / ((uint32_t)revsteps * (uint32_t)rpm);
    steppingcounter = 0;
    DEBUG("us per step: %dus\n", usperstep);
}