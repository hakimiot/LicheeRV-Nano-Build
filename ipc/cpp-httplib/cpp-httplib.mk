################################################################################
#
# cpp-httplib
#
################################################################################

CPP_HTTPLIB_VERSION = 0.18.0
CPP_HTTPLIB_SITE = $(call github,yhirose,cpp-httplib,v$(CPP_HTTPLIB_VERSION))
CPP_HTTPLIB_LICENSE = MIT
CPP_HTTPLIB_LICENSE_FILES = LICENSE

CPP_HTTPLIB_INSTALL_STAGING = YES

define CPP_HTTPLIB_INSTALL_STAGING_CMDS
	$(INSTALL) -D -m 0644 \
		$(@D)/httplib.h \
		$(STAGING_DIR)/usr/include/httplib.h
endef

$(eval $(generic-package))
