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
override CFLAGS += -Iinclude
override CFLAGS += -I$(STAGING_DIR)/usr/include/ffmpeg
override CFLAGS += -I$(STAGING_DIR)/usr/include/opencv4
override CFLAGS += -I$(STAGING_DIR)/usr/include/mqtt
override CFLAGS += -I$(MW_PATH)/include
override CFLAGS += -I$(MW_PATH)/sample/common
override CFLAGS += -I$(MW_PATH)/modules/sys/include
override CFLAGS += -I$(MW_PATH)/modules/vpu/include
override CFLAGS += -I$(MW_PATH)/modules/isp/include/sg200x
override CFLAGS += -I$(INTERDRV_PATH)/include/common/uapi
override CFLAGS += -I$(INTERDRV_PATH)/include/chip/mars/uapi/

override LDFLAGS += -lopencv_core -lopencv_imgproc -lopencv_imgcodecs -lopencv_videoio
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
SRCS = main.cpp \
       src/camera_capture.cpp \
       src/file_system.cpp \
       src/image_handle.cpp \
       src/mainapp_log.cpp \
       src/rtmp_streamer.cpp \
       src/video_writer.cpp \

OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean