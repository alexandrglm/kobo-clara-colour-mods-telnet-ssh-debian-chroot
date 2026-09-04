# Makefile for drm.c - Cross-compile for ARM/Kobo

CC = arm-unknown-linux-gnueabihf-gcc
CFLAGS = -static -O2 -Wall -Wextra -I$(HOME)/x-tools/arm-unknown-linux-gnueabihf/include
LDFLAGS = -static -L$(HOME)/x-tools/arm-unknown-linux-gnueabihf/lib
LIBS = -lssl -lcrypto -lzip -lxml2 -lz -lm

SOURCES = drm.c
TARGET = drm

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	scp $(TARGET) kobo:/mnt/onboard/.adds/bin/

.PHONY: all clean install
