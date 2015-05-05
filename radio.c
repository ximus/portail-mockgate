#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#include <math.h>

#include "mraa.h"

#include "circular_buffer.h"
#include "app.h"


static const uint16_t TARGET_BIT_COUNT = sizeof(TARGET_MSG) * 8;

sem_t code_match;
sem_t new_sample;

const char *byte_to_binary(int x)
{
    static char b[9];
    b[0] = '\0';

    int z;
    for (z = 128; z > 0; z >>= 1)
    {
        strcat(b, ((x & z) == z) ? "1" : "0");
    }

    return b;
}


typedef struct {
    uint16_t level;
    uint16_t duration;
} sample_t;

#define SAMPLE_Q_SIZE   (5000)
sample_t sample_buffer[SAMPLE_Q_SIZE];
circ_buffer_t sample_q = CIRC_BUFFER_INIT(SAMPLE_Q_SIZE);

static inline void sample_push(sample_t sample)
{
    if (circ_buffer_is_full(&sample_q))
    {
        printf("out of sample buffer space\n");
        shutdown(1);
        return;
    }
    int cursor = circ_buffer_add(&sample_q);
    sample_buffer[cursor] = sample;
    sem_post(&new_sample);
}

static inline sample_t sample_read(void)
{
    sem_wait(&new_sample);
    int cursor = circ_buffer_get(&sample_q);
    return sample_buffer[cursor];
}

float target_edge_us;
float min_edge_us;

uint16_t match_cursor = 0;
bool is_matching = false;
uint16_t oops_cout = 0;

void reset_match(void) {
    is_matching = false;
    match_cursor = 0;
    oops_cout = 0;
    // printf("$");
}

void on_new_level(uint8_t level, uint16_t bitlen)
{
    #if DEBUG_RADIO
    for (int i = 0; i < bitlen; ++i)
    {
        printf("%d", level);
    }
    printf("\n");
    #endif
    if (is_matching)
    {
        for (uint16_t i = 0; i < bitlen; ++i)
        {
            // printf("%d\n", level);
            // printf("%d   bitlen=%d i=%d cursor=%d \n", level, bitlen, i, match_cursor);
            uint8_t byte_offset = match_cursor / 8;
            uint8_t bit_offset = 7 - (match_cursor % 8);
            uint8_t expected_level = (TARGET_MSG[byte_offset] & (1 << bit_offset)) >> bit_offset;
            // printf("%s 0x%X\n", byte_to_binary(TARGET_MSG[i]), TARGET_MSG[i]);
            // printf("byte=%d bit=%d expect=%d got=%d\n",byte_offset, bit_offset, expected_level, level );
            // printf("%d %d\n", byte_offset, bit_offset);
            // printf("%d %d\n",level, expected_level);

            if (level != expected_level) {
                if (oops_cout++ > 2) {
                    reset_match();
                    return;
                }
            }

            if (match_cursor >= TARGET_BIT_COUNT - 1) {
                sem_post(&code_match);
                reset_match();
                return;
            }
            match_cursor++;

            // printf("\n");
        }
    }
    else {
        if (!level && bitlen >= 8) {
            is_matching = true;
            // printf("^");
        }
    }
}

static void *observe_loop(void *arg) {
    mraa_result_t r = MRAA_SUCCESS;

    mraa_gpio_context gpio;
    gpio = mraa_gpio_init(RADIO_PIN);
    if ( gpio == NULL ) {
        fprintf(stderr, "Error opening GPIO\n");
        exit(1);
    }

    /* Set GPIO direction to out */
    r = mraa_gpio_dir(gpio, MRAA_GPIO_IN);
    if ( r != MRAA_SUCCESS ) {
        mraa_result_print(r);
    }

    /* Set GPIO direction to out */
    r = mraa_gpio_mode(gpio, MRAA_GPIO_PULLDOWN);
    if ( r != MRAA_SUCCESS) {
        mraa_result_print(r);
    }

    /* Turn LED off and on forever until SIGINT (Ctrl+c) */
    unsigned        last_val = mraa_gpio_read(gpio);
    struct timespec last_time = {0,0};

    unsigned int this_val;
    struct timespec this_time;

    // keep polling gpio for change
    while (!shutdown_received) {
        this_val = mraa_gpio_read(gpio);
        if (this_val != last_val) {
            clock_gettime(CLOCK_MONOTONIC_RAW, &this_time);
            if (last_time.tv_nsec > 0) {
                sample_t sample = {
                    .level = last_val,
                    .duration = ((this_time.tv_nsec - last_time.tv_nsec)/1000)
                };
                sample_push(sample);
            }
            last_time = this_time;
        }
        last_val = this_val;
    }

    r = mraa_gpio_isr_exit(gpio);
    if ( r != MRAA_SUCCESS ) {
        mraa_result_print(r);
    }

    /* Clean up GPIO and exit */
    r = mraa_gpio_close(gpio);
    if ( r != MRAA_SUCCESS ) {
        mraa_result_print(r);
    }

    return NULL;
}

static void *grep_loop(void *arg)
{
    #if DEBUG_RADIO
    printf("target code is\n");
    printf("  hex: ");
    for (int i = 0; i < sizeof(TARGET_MSG); ++i) {
        printf("  (0x%X)", TARGET_MSG[i]);
    }
    printf("\n  bin: ");
    for (int i = 0; i < sizeof(TARGET_MSG); ++i) {
        printf("%s", byte_to_binary(TARGET_MSG[i]));
    }
    printf("\nwatching for a match ...\n");
    #endif

    while (!shutdown_received)
    {
        sample_t sample = sample_read();
        // printf("%d\n", sample.duration);
        if (sample.duration > min_edge_us) {
            // printf("%f\n", ((float) sample.duration) / target_edge_us);
            // printf("%f\n", sample.duration / target_edge_us);
            uint16_t bitlen = (uint16_t) roundf(
                ((float) sample.duration) / target_edge_us
            );
            // printf("%d %d %d\n", bitlen, sample.duration, sample.level );
            if (bitlen > 0) {
                on_new_level(sample.level,bitlen);
            } else {
                // reset_match();
            }
        }
    }



    return NULL;
}

void init_code_match(uint16_t bitrate)
{
    target_edge_us = 1000000 / bitrate;
    float slack = 0.40;
    float slack_us = target_edge_us * slack;
    min_edge_us = target_edge_us - slack_us;
}


static pthread_t grep_thread;
static pthread_t observe_thread;

int radio_up(void)
{
    init_code_match(TARGET_BITRATE);
    sem_init(&code_match, 0, 0);
    sem_init(&new_sample, 0, 0);

    int err;
    err = pthread_create(&observe_thread, NULL, observe_loop, NULL);
    if(err) {
        fprintf(stderr, "Error - pthread_create() return code: %d\n", err);
    }

    err = pthread_create(&grep_thread, NULL, grep_loop, NULL);
    if(err)
    {
        fprintf(stderr, "Error - pthread_create() return code: %d\n", err);
    }

    return err;
}

void radio_down(void)
{
    // noop
}