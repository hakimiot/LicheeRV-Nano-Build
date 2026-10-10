################################################################################
#
# simpleini
#
################################################################################

SIMPLEINI_VERSION = 4.24
SIMPLEINI_SITE = https://github.com/brofield/simpleini/archive/refs/tags
SIMPLEINI_SOURCE = v$(SIMPLEINI_VERSION).tar.gz
SIMPLEINI_LICENSE = MIT
SIMPLEINI_LICENSE_FILES = LICENSE.txt

SIMPLEINI_INSTALL_STAGING = YES

define SIMPLEINI_INSTALL_STAGING_CMDS
	$(INSTALL) -D -m 0644 \
		$(@D)/SimpleIni.h \
		$(STAGING_DIR)/usr/include/SimpleIni.h
endef

$(eval $(generic-package))
