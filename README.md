# MockGate - Portail DYI Project

Firmware for a bench-top fake gate used to test a home gate control system [insert link to main project].

## Notes

* Targets Linux on Intel Edison
* Gate hardware interface defined in C
  * listen on radio for over the air command
  * drive a stepper motor using a dual H-bridge [insert link to sparkfun]
* Gate 'business logic' defined in ruby
  * eg. what happens when the gate remote is clicked, when someone crosses gate while it is closing, ...

## Setup
Requires `.envrc` to provide full target cross compilation environment

[rework mraa install and provide prepare step]

Prepare mruby
```bash
# mruby requires building for host as well as target
# disable .envrc to provide host build toolchain by default
direnv deny
direnv reload
script/prepare_mruby
# restore .envrc as most building targets target
direnv allow
```

Build the firmware app to be deployed
```bash
make
```

## Deploy
I used NFS to stay in sync with the target. `scp` or anything should do.