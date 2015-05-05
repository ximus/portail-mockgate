static mrb_value ruby_on_signal_cb;

static mrb_value ruby_on_signal(mrb_state *mrb, mrb_value value)
{
    mrb_get_args(mrb, "&", &ruby_on_signal_cb);
    return ruby_on_signal_cb;
}

static mrb_value ruby_motor_goto(mrb_state *mrb, mrb_value value)
{
    mrb_int steps;
    mrb_get_args(mrb, "i", &steps);

    motor_goto(steps);
    return mrb_fixnum_value(steps);
}