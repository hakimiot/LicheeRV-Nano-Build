# ipc/main_app/main_app.mk
################################################################################
#
# main_app
#
################################################################################

MAIN_APP_VERSION = 1.0.0
MAIN_APP_SITE = $(BR2_EXTERNAL)/../ipc/main_app
MAIN_APP_SITE_METHOD = local

MW_PATH = $(BR2_EXTERNAL)/../middleware/v2


MAIN_APP_DEPENDENCIES += linux-pam
MAIN_APP_DEPENDENCIES += ffmpeg
MAIN_APP_DEPENDENCIES += paho-mqtt-cpp
MAIN_APP_DEPENDENCIES += cpp-httplib
MAIN_APP_DEPENDENCIES += json-for-modern-cpp
MAIN_APP_DEPENDENCIES += simpleini

define MAIN_APP_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D) \
		CC="$(TARGET_CC)" CXX="$(TARGET_CXX)" \
		CFLAGS="$(TARGET_CFLAGS)" \
		CXXFLAGS="$(TARGET_CXXFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS) --sysroot=$(STAGING_DIR) -L$(STAGING_DIR)/usr/lib"
endef

define MAIN_APP_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/main_app $(TARGET_DIR)/usr/bin/main_app
	cp -a $(@D)/files $(TARGET_DIR)/

	for lib in $(MW_PATH)/lib/*.so*; do \
		$(INSTALL) -D -m 0755 $$lib $(TARGET_DIR)/usr/lib/; \
	done
	for lib in $(MW_PATH)/lib/3rd/*.so*; do \
		$(INSTALL) -D -m 0755 $$lib $(TARGET_DIR)/usr/lib/; \
	done
endef

$(eval $(generic-package))
