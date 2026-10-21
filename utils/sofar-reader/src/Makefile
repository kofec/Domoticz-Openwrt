CFLAGS ?= -Os
CFLAGS += -Wall -Wextra -std=gnu99
LDLIBS += -luci -lcurl

OBJS = main.o solarman.o sofar.o domoticz.o pvoutput.o

all: sofar-reader

sofar-reader: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(OBJS): solarman.h sofar.h domoticz.h pvoutput.h

clean:
	rm -f sofar-reader $(OBJS)

.PHONY: all clean
