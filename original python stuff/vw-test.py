#!/usr/bin/env python

import mraa
import time
import vw
import syslog
import numpy
import pdb
import signal



def handle_pdb(sig, frame):
  print('debug')
  pdb.Pdb().set_trace(frame)
signal.signal(signal.SIGUSR1, handle_pdb)


mraa.init()
mraa.setLogLevel(syslog.LOG_DEBUG)
mraa.setPriority(99)

for i in vw.TARGET_MSG:
  print("%10s " % bin(i)),
print("")
rate = 3000
print('starting RX, rate: ', rate)
rx = vw.rx(36, rate)

print('sampling ...')
time.sleep(3)
rx.cancel()
print('done')

# while rx.messages:
#   row = rx.get()
#   print("good: "),
#   for i in vw.TARGET_MSG:
#     print("%10s " % bin(i)),
#   print("")
#   print(" bad: "),
#   for i in row:
#     print("%10s " % bin(i)),
#   print("\n")

# print("mean was", numpy.percentile(rx.times, 90))




print("threads closed")
print("item count: ", len(rx.times))
print('resets', rx.resets)
print('oops max', rx.oops_max)
print('bit_index_max', rx.bit_index_max)

file_ = open('edges.txt', 'w')
for edge in rx.times:
  file_.write("%d\n" % edge)
file_.close()

pdb.set_trace()

exit(0)