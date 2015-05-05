


void
mrb_c_extension_example_gem_init(mrb_state* mrb) {
    struct RClass *class_cextension = mrb_define_module(mrb, "MockGate");
    mrb_define_class_method(mrb, class_cextension, "on_signal", mrb_on_signal, MRB_ARGS_NONE());
}

void
mrb_c_extension_example_gem_final(mrb_state* mrb) {
}