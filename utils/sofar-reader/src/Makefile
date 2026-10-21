CFLAGS ?= -Os
CFLAGS += -Wall -Wextra -std=gnu99

# domoticz.c uses solarman_connect() for its plain TCP connection
COMMON = domoticz.o pvoutput.o http.o uciutil.o solarman.o
SOFAR_OBJS = main.o sofar.o $(COMMON)
GOODWE_OBJS = goodwe_main.o goodwe.o sems.o $(COMMON)

all: sofar-reader goodwe-reader

sofar-reader: $(SOFAR_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(SOFAR_OBJS) -luci -lcurl

goodwe-reader: $(GOODWE_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(GOODWE_OBJS) -luci -lcurl -ljson-c -lcrypto -lm

$(SOFAR_OBJS) $(GOODWE_OBJS): $(wildcard *.h)

clean:
	rm -f sofar-reader goodwe-reader *.o

.PHONY: all clean
