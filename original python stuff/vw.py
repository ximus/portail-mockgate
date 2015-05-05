"""
This module provides a 313MHz/434MHz radio interface compatible
with the Virtual Wire library used on Arduinos.

It has been tested between a Pi, TI Launchpad, and Arduino Pro Mini.
"""
# 2014-08-14
# vw.py

import time
import pdb
import threading
from array import array
import math
import sys
import collections

import traceback

import mraa

TARGET_MSG = (0x58, 0x92, 0x2d, 0xd9, 0xb28)
MAX_MESSAGE_BYTES=5

MIN_BPS=50
MAX_BPS=10000

TIMEOUT='timeout'
EVENT_LEVEL='level'

class _observe_thread(threading.Thread):
   def __init__(self, mraa_gpio):
      """Initialises notifications."""
      threading.Thread.__init__(self)
      self.daemon = True
      self.gpio = mraa_gpio
      self.last_value = 0
      self.last_tick = 0
      self.start()

   def run(self):
      while 1:
         value = self.gpio.read()
         if value != self.last_value:
            tick  = int(round(time.time() * 10**6))
            if self.last_tick:
               print(tick - self.last_tick)
            self.last_tick = tick
         last_value = value

class rx():

   def __init__(self, gpio, bps=2000):
      """
      Instantiate a receiver with the Pi, the receive gpio, and
      the bits per second (bps).  The bps defaults to 2000.
      The bps is constrained to be within MIN_BPS to MAX_BPS.
      """
      port = mraa.Gpio(gpio)
      self.rxgpio = port

      if bps < MIN_BPS:
         bps = MIN_BPS
      elif bps > MAX_BPS:
         bps = MAX_BPS

      slack = 0.40
      self.mics = int( 1000000 / bps)
      slack_mics = int(slack * self.mics)
      self.min_mics = self.mics - slack_mics       # Shortest legal edge.
      self.max_mics = (self.mics + slack_mics) * 4 # Longest legal edge.

      self.timeout =  8 * self.mics / 1000 # 8 bits time in ms.
      if self.timeout < 8:
         self.timeout = 8

      self.resets = dict()
      self.preamble_count = 0
      self.oops_max = 0
      self.bit_index_max = 0
      self.oops_count = 0
      self.in_message = False
      self.times = array('I')
      self.last_tick = None
      self.good = 0
      self.bit_index = 0
      self.timer = None
      self.current_msg = array('B')
      self.messages = []
      port.mode(mraa.MODE_PULLDOWN)
      err = port.dir(mraa.DIR_IN)
      if err > 0:
         mraa.printError(err)

      self.edge_buffer = collections.deque()
      # def edge_isr(self):
      #    value = self.rxgpio.read()
      #    tick  = time.time()
      #    self.edge_buffer.append((tick, value))

      # err = port.isr(mraa.EDGE_BOTH, edge_isr, self)
      # if err > 0:
      #    mraa.printError(err)

      # worker = threading.Thread(target=self.process_loop)
      # worker.deamon = True
      # worker.start()

      _observe_thread(port)

      print('inited')

   def process_loop(self):
      while 1:
         if len(self.times) > 2000:
            print("worker out")
            exit()
         if any(self.edge_buffer):
            self._cb(EVENT_LEVEL)
            time.sleep(0)
         else:
            time.sleep(0.01)


   def _cb(self, event):
      tick, level = self.edge_buffer.popleft()
      # print(tick, level)
      # level refers to the previous level
      tick = int(round(tick * 10**6))
      level = 1 - level

      if self.last_tick is not None:

         if event == TIMEOUT:
            self.set_watchdog(0) # Switch watchdog off.
            self._insert(4, not self.last_level)
            self.good = 0
            self.reset('timeout')

         else:
            edge = int((tick - self.last_tick))
            if level:
               self.times.append(edge)

            if edge < self.min_mics:
               self.good = 0
               self.reset('edge too sort')

            elif edge > self.max_mics:
               # self._insert(4, level)

               self.good = 0
               # self.reset('edge too long')
            else:
               self.good += 1

               if self.good > 8:
                  bitlen = (100 * edge) / self.mics

                  if bitlen < 140:
                     bits = 1
                  elif bitlen < 240:
                     bits = 2
                  elif bitlen < 340:
                     bits = 3
                  else:
                     bits = 4

                  self._insert(bits, level)

      self.last_tick = tick
      self.last_level = level


   # def _insert(self, bits, level):
   #    for i in range(bits):
   #       msg_index = int(math.floor(self.bit_index / 8))
   #       byte_index = self.bit_index % 8
   #       if msg_index >= MAX_MESSAGE_BYTES:
   #          # maybe i should just scrap the message here
   #          # print(msg_index, MAX_MESSAGE_BYTES)
   #          self.message_end()
   #       # initialize new byte if necessary
   #       if len(self.current_msg) < (msg_index + 1):
   #          self.current_msg.append(0)
   #       byte = self.current_msg[msg_index]
   #       # bytes need to be inited as zero
   #       if level:
   #          byte |= (1 << byte_index)
   #       # print(self.bit_index, msg_index, byte_index)
   #       self.current_msg[msg_index] = byte
   #       self.bit_index += 1
   def _insert(self, bits, level):
      # sys.stdout.write(bits * str(level))
      for i in range(bits):
         if self.in_message:
            if self.bit_index >= len(TARGET_MSG) * 8:
               print("WIN!")
               self.reset('win')
            byte_offset  = int(math.floor(self.bit_index / 8))
            rel_index = self.bit_index % 8
            expected_level = TARGET_MSG[byte_offset] & (1 << rel_index)
            if level != expected_level:
               self.oops_count =+ 1
               if self.oops_count > self.oops_max:
                  self.oops_max = self.oops_count
            if self.oops_count > 2:
               self.reset('oops')
            if self.bit_index > self.bit_index_max:
               self.bit_index_max = self.bit_index
            self.bit_index += 1

         else:
            if not level:
               self.preamble_count =+ bits
               if self.preamble_count >= 8:
                  self.oops_count = 0
                  self.in_message = True
            else:
               self.preamble_count = 0

   def set_watchdog(self, time):
      if self.timer:
         self.timer.cancel()

      if time > 0:
         print("about to set timer")
         def on_timeout(self):
            self._cb(None, TIMEOUT)
         print("watch for time: %d" % (time/1000))
         self.timer = Timer(time/1000, on_timeout, self)

   def reset(self, cause='unspecified'):
      self.resets.setdefault(cause, 0)
      # uncomment following to reproduce segfaulting errrors silence
      # self.resets[cause].setdefault(cause, 0)
      self.resets[cause] += 1
      # print("\n")
      # traceback.print_stack()
      # self.messages.append(self.current_msg)
      self.bit_index = 0
      # self.current_msg = []
      self.in_message = False

   def get(self):
      """
      Returns the next unread message, or None if none is avaiable.
      """
      if len(self.messages):
         return self.messages.pop(0)
      else:
         return None

   def ready(self):
      """
      Returns True if there is a message available to be read.
      """
      return len(self.result)

   def cancel(self):
      """
      Cancels the wireless receiver.
      """
      print "out io"
      self.rxgpio and self.rxgpio.isrExit()
      print "out timer"
      self.timer and self.timer.cancel()
      print "out"
      self.timer = None
