#
# Makefile for libgstamlh265venc.so
#
#

TARGET = libgstamlh265venc.so


LOCAL_PATH := $(shell pwd)
SRCS = $(wildcard *.c)
SRCS += $(wildcard $(LOCAL_PATH)/../common/*.c)

OBJS = $(patsubst %c, %o, $(SRCS))

CFLAGS = -Wall -Wextra -fPIC
CFLAGS += $(shell $(PKG_CONFIG) --cflags gstreamer-1.0 gstreamer-base-1.0 gstreamer-allocators-1.0 gstreamer-video-1.0)
LDFLAGS += $(shell $(PKG_CONFIG) --libs gstreamer-1.0 gstreamer-base-1.0 gstreamer-allocators-1.0 gstreamer-video-1.0)

LDFLAGS += -L$(STAGING_DIR)/usr/lib/ -lion -lvphevcodec -lge2d -lgstdrmbufferpool -lgstdrmallocator
CFLAGS += \
	-I$(STAGING_DIR)/usr/include/ \
    -I$(LOCAL_PATH)/../common/


# build
all: $(TARGET)


$(TARGET): $(OBJS)
	$(CC) $^ $(LDFLAGS) -shared -o $@

%.o: %.c
	$(CC) -c $(CFLAGS) $^ -o $@

.PHONY: clean install uninstall

clean:
	rm -f $(OBJS)
	rm -f $(TARGET)

install:
	cp $(TARGET) $(TARGET_DIR)/usr/lib/gstreamer-1.0/

uninstall:
	rm $(TARGET_DIR)/usr/lib/gstreamer-1.0/$(TARGET)
