################################################################################
#
# sensord
#
################################################################################

SENSORD_VERSION = 1.0
SENSORD_SITE = $(BR2_EXTERNAL_MDR_PATH)/package/sensord/src
SENSORD_SITE_METHOD = local
SENSORD_LICENSE = CC-BY-NC-SA-4.0
SENSORD_CONF_OPTS = -DCMAKE_BUILD_TYPE=Release

define SENSORD_INSTALL_CONF
	$(INSTALL) -D -m 0644 $(SENSORD_PKGDIR)/sensord.conf \
		$(TARGET_DIR)/etc/sensord.conf
endef

define SENSORD_INSTALL_INIT_SYSV
	$(INSTALL) -D -m 0755 $(SENSORD_PKGDIR)/S50sensord \
		$(TARGET_DIR)/etc/init.d/S50sensord
endef

SENSORD_POST_INSTALL_TARGET_HOOKS += SENSORD_INSTALL_CONF

$(eval $(cmake-package))