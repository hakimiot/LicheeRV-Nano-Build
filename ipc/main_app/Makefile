CXX := $(CXX)
CFLAGS := $(CFLAGS)
TARGET = main_app

SDK_ROOT = ../../../..
PACKAGE_ROOT = ..

MW_PATH = $(SDK_ROOT)/middleware/v2
INTERDRV_PATH = $(SDK_ROOT)/osdrv/interdrv/v2

MW_LIB = $(MW_PATH)/lib
MW_3RD_LIB = $(MW_PATH)/lib/3rd

# 头文件目录
override CFLAGS += -Icapture/include
override CFLAGS += -Icommon/include
override CFLAGS += -Idevice/include
override CFLAGS += -Ilog/include
override CFLAGS += -Iserver/include
override CFLAGS += -Istreamer/include
override CFLAGS += -Ivideo/include
override CFLAGS += -I$(STAGING_DIR)/usr/include
override CFLAGS += -I$(STAGING_DIR)/usr/include/ffmpeg
override CFLAGS += -I$(STAGING_DIR)/usr/include/mqtt
override CFLAGS += -I$(STAGING_DIR)/usr/include/json-for-modern-cpp
override CFLAGS += -I$(MW_PATH)/include
override CFLAGS += -I$(MW_PATH)/sample/common
override CFLAGS += -I$(MW_PATH)/modules/sys/include
override CFLAGS += -I$(MW_PATH)/modules/vpu/include
override CFLAGS += -I$(MW_PATH)/modules/isp/include/sg200x
override CFLAGS += -I$(INTERDRV_PATH)/include/common/uapi
override CFLAGS += -I$(INTERDRV_PATH)/include/chip/mars/uapi/

override LDFLAGS += -lpam
override LDFLAGS += -lavcodec -lavformat -lavutil -lswscale
override LDFLAGS += -lpaho-mqttpp3 -lpaho-mqtt3a
override LDFLAGS += $(MW_LIBS)
override LDFLAGS += -L$(MW_PATH)/lib -L$(MW_PATH)/lib/3rd \
                    -Wl,--start-group \
                    -lvpu -lsns_full \
                    -lvenc -lvdec \
                    -lcvi_bin -lcvi_bin_isp \
                    -lsys -lisp -lawb -lae -laf -lisp_algo \
                    -lsample \
                    -lini -lpthread -lm -latomic \
                    -Wl,--end-group

# 所有源文件
SRCS = main.cpp
SRCS += capture/src/camera_capture.cpp
SRCS += common/src/random.cpp
SRCS += device/src/device_safe.cpp
SRCS += device/src/file_system.cpp
SRCS += log/src/mainapp_log.cpp
SRCS += server/src/http_server_app.cpp
SRCS += server/src/http_server_media.cpp
SRCS += server/src/http_server_stream.cpp
SRCS += server/src/http_server_user.cpp
SRCS += server/src/http_server.cpp
SRCS += streamer/src/rtmp_streamer.cpp
SRCS += video/src/video_writer.cpp
SRCS += video/src/video_reader.cpp

OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean