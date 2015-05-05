/* Blinky test using mraa */

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>

#include "mraa.h"
#include "motor.h"
#if USE_RADIO
#include "radio.h"
#endif
#include "app.h"

#if USE_RUBY
#include "mruby.h"
#include "mruby/string.h"
#include "mruby/compile.h"
#include "ruby_methods.c"
#endif


volatile bool   shutdown_received = false;
pthread_cond_t  shutdown_received_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t shutdown_received_lock = PTHREAD_MUTEX_INITIALIZER;

void shutdown(int8_t err)
{
    if (err) {
        printf("exit with error\n");
    }
    shutdown_received = true;
    pthread_cond_broadcast(&shutdown_received_cond);
}

void sig_handler(int signo)
{
    if ( signo == SIGINT ) {
        printf("Closing ...\n");
        shutdown(0);
        // cancel main thread
        pthread_exit(0);
    } else {
        printf("unhandled signal %d\n", signo);
    }
}

#if USE_RUBY
void ruby_run(char path[])
{
    mrb_state *mrb = mrb_open();

    struct RClass *mockgate = mrb_define_module(mrb, "MockGate");
    mrb_define_class_method(mrb, mockgate, "on_signal", ruby_on_signal, MRB_ARGS_REQ(1));

    struct RClass *motor = mrb_define_module(mrb, "Motor");
    mrb_define_class_method(mrb, motor, "goto", ruby_motor_goto, MRB_ARGS_REQ(1));

    FILE *f = fopen(path, "r");
    mrb_value obj = mrb_load_file(mrb, f);

    // report any exception
    if ( mrb->exc ) {
        obj = mrb_funcall(mrb, mrb_obj_value(mrb->exc), "inspect", 0);
        fwrite(RSTRING_PTR(obj), RSTRING_LEN(obj), 1, stdout);
        putc('\n', stdout);
    }

    while (!shutdown_received) {
        // make sure a radio callback was set in ruby
        if (!mrb_test(ruby_on_signal_cb)) {
            break;
        }
        sem_wait(&code_match);
        mrb_yield_argv(mrb, ruby_on_signal_cb, 0, NULL);
    }

    wait_for_shutdown();

    printf("closing ruby ...\n");
    mrb_close(mrb);
}
#endif



int main(int argc, char *argv[])
{
    mraa_init();
    mraa_set_log_level(7);

    int err;
    /* Create signal handler so we can exit gracefully */
    signal(SIGINT, sig_handler);

    err = motor_init(MOTOR_PWM_MAX_FREQ, MICROSTEP);
    if (err) {
        printf("error initializing motor, exiting ...\n");
        shutdown(err);
    }

    #if USE_RADIO
    err = radio_up();
    if(err) {
        fprintf(stderr, "Could not start radio. code=%d", err);
        shutdown(err);
    }
    #endif

    #if USE_RUBY
    if (argc == 2) {
        ruby_run(argv[1]);
    }
    else {
        printf("error: missing ruby script path\n");
        printf("usage: mockgate [path/to/script.rb]\n");
        shutdown(1);
    }
    #else
    while (!shutdown_received) {
        sem_wait(&code_match);
        printf("CODE MATCH!\n");
        sleep(1);
    }
    #endif

    #if USE_RADIO
    radio_down();
    #endif

    shutdown(0);
    // TODO: look into this
    // i don't get why but this allows motor thread to teardown
    pthread_exit(0);

    return err;
}