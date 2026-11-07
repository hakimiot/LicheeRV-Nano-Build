CXX := $(CXX)
CFLAGS := $(CFLAGS)
TARGET = mqtt_app

SDK_ROOT = ../../../..
PACKAGE_ROOT = ..

COMMON_PATH = ${SDK_ROOT}/ipc/common

# 头文件目录
override CFLAGS += -Ihttp_client/include
override CFLAGS += -Imqtt_client/include
override CFLAGS += -Imqtt_response/include
override CFLAGS += -I$(PACKAGE_ROOT)/include
override CFLAGS += -I$(COMMON_PATH)/include
override CFLAGS += -I$(STAGING_DIR)/usr/include
override CFLAGS += -I$(STAGING_DIR)/usr/include/ffmpeg
override CFLAGS += -I$(STAGING_DIR)/usr/include/mqtt
override CFLAGS += -I$(STAGING_DIR)/usr/include/json-for-modern-cpp

override LDFLAGS += -lpaho-mqttpp3 -lpaho-mqtt3a

# 所有源文件
SRCS = main.cpp
SRCS += http_client/src/http_client.cpp
SRCS += mqtt_client/src/mqtt_client.cpp
SRCS += mqtt_response/src/mqtt_response.cpp
SRCS += ${COMMON_PATH}/src/device_info.cpp
SRCS += ${COMMON_PATH}/src/file_system.cpp
SRCS += ${COMMON_PATH}/src/log.cpp

OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean