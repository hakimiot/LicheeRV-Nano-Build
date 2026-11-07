# ipc/mqtt_app/mqtt_app.mk
################################################################################
#
# mqtt_app
#
################################################################################

MQTT_APP_VERSION = 1.0.0
MQTT_APP_SITE = $(BR2_EXTERNAL)/../ipc/mqtt_app
MQTT_APP_SITE_METHOD = local

MQTT_APP_DEPENDENCIES += paho-mqtt-cpp
MQTT_APP_DEPENDENCIES += cpp-httplib
MQTT_APP_DEPENDENCIES += json-for-modern-cpp
MQTT_APP_DEPENDENCIES += simpleini

define MQTT_APP_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D) \
		CC="$(TARGET_CC)" CXX="$(TARGET_CXX)" \
		CFLAGS="$(TARGET_CFLAGS)" \
		CXXFLAGS="$(TARGET_CXXFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS) --sysroot=$(STAGING_DIR) -L$(STAGING_DIR)/usr/lib"
endef

define MQTT_APP_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/mqtt_app $(TARGET_DIR)/usr/bin/mqtt_app

	for lib in $(MW_PATH)/lib/*.so*; do \
		$(INSTALL) -D -m 0755 $$lib $(TARGET_DIR)/usr/lib/; \
	done
	for lib in $(MW_PATH)/lib/3rd/*.so*; do \
		$(INSTALL) -D -m 0755 $$lib $(TARGET_DIR)/usr/lib/; \
	done
endef

$(eval $(generic-package))
