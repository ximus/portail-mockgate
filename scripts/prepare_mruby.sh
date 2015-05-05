#!/usr/bin/env bash

# mruby git repo --> vendor directory
mkdir -p vendor
git clone --depth=1 https://github.com/mruby/mruby.git vendor/mruby
cd vendor/mruby
git pull


# overwrite default build config with ours
cp -f ../../mruby_build_config.rb build_config.rb

./minirake clean

cwd=$(pwd)

# Toolchain env targets host
./minirake $cwd/bin/mrbc
./minirake $cwd/build/host/lib/libmruby.a

# this breaks the whole direnv idea,
# but I couldn't get direnv to work in this script
source /opt/poky-edison/1.6.1/environment-setup-core2-32-poky-linux
# Toolchain env targets target
./minirake $cwd/build/edison/lib/libmruby.a