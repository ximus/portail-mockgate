TARGET = bin/mockgate
LIBS = -lrt -lmraa
CC ?= gcc
# -gdwarf-2 necessary to debug gcc 4.8 output with gdb 7.4
CFLAGS += -O0 -gdwarf-2 -std=gnu99 -Wall -lm -pthread

# available flags: -DMOTORDEBUG -DENABLE_DEBUG -DUSE_RADIO
APPFLAGS = -DENABLE_DEBUG -DUSE_RADIO

USE_RUBY=1

ifeq ($(USE_RUBY),1)
	CFLAGS += -DUSE_RUBY
	CFLAGS += -I vendor/mruby/include
	CFLAGS += vendor/mruby/build/edison/lib/libmruby.a
endif

.PHONY: default all clean

default: $(TARGET)
all: default

# OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))
# HEADERS = $(wildcard *.h)
HEADERS = circular_buffer.h radio.h gate.h motor.h
OBJECTS = main.c circular_buffer.c radio.c gate.c motor.c app.c

.INTERMEDIATE: %.o

%.o: %.c $(HEADERS)
		$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
		$(CC) $(OBJECTS) $(CFLAGS) $(LIBS) $(APPFLAGS) -o $@

clean:
		-rm -f *.o
		-rm -f $(TARGET)

deploy:
	scp -Cp bin/mockgate ximus@mockgate.local:/home/ximus