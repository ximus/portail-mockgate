#!/usr/bin/env python

import mraa
import time
import pdb

# mraa.init()

mraa.setLogLevel(5)

# Setup
x = mraa.Gpio(36)
x.dir(mraa.DIR_IN)

def test(args):
  print('before')
  hasdf()
  print('after')
  print(args)

x.isr(mraa.EDGE_BOTH, test, 123)

time.sleep(123)

p2 = mraa.Gpio(48)
p2.dir(mraa.DIR_OUT)

while True:
  p2.write(not p2.read())
  time.sleep(2)