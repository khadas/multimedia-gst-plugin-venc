#
# Makefile for libgstamljpegenc.so
#
#

TARGET = libgstamljpegenc.so

OUT_DIR ?= .
$(info "OUT_DIR : $(OUT_DIR)")

LOCAL_PATH := $(shell pwd)
SRCS = $(wildcard *.c)

OBJS = $(patsubst %c, %o, $(SRCS))

CFLAGS = -Wall -Wextra -fPIC
CFLAGS += $(shell $(PKG_CONFIG) --cflags gstreamer-1.0 gstreamer-base-1.0 gstreamer-allocators-1.0 gstreamer-video-1.0)
LDFLAGS += $(shell $(PKG_CONFIG) --libs gstreamer-1.0 gstreamer-base-1.0 gstreamer-allocators-1.0 gstreamer-video-1.0)

LDFLAGS += -L$(STAGING_DIR)/usr/lib/ -ljpegenc_api -lge2d -lgstdrmbufferpool -lgstdrmallocator
CFLAGS += \
	-I$(STAGING_DIR)/usr/include/ \
    -I$(LOCAL_PATH)/../common/


# build
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(patsubst %, $(OUT_DIR)/%, $^) $(LDFLAGS) -shared -Wl,-soname,$(TARGET) -o $(OUT_DIR)/$@

%.o: %.c
	$(CC) -c $(CFLAGS) $^ -o $(OUT_DIR)/$@

.PHONY: clean install uninstall

clean:
	rm -f $(patsubst %, $(OUT_DIR)/%, $(OBJS))
	rm -f $(OUT_DIR)/$(TARGET)

install:
	cp $(OUT_DIR)/$(TARGET) $(TARGET_DIR)/usr/lib/gstreamer-1.0/

uninstall:
	rm $(TARGET_DIR)/usr/lib/gstreamer-1.0/$(TARGET)
