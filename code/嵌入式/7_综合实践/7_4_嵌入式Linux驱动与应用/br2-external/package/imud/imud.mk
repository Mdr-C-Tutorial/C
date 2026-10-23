################################################################################
#
# imud —— 读取 /dev/mdr_imu、计算倾角、经 MQTT 发布的用户态服务
#
################################################################################

IMUD_VERSION = 1.0
IMUD_SITE = $(BR2_EXTERNAL_MDR_PATH)/../imud
IMUD_SITE_METHOD = local
IMUD_LICENSE = CC-BY-NC-SA-4.0
IMUD_DEPENDENCIES = coremqtt host-cmake
IMUD_CONF_OPTS = -DCMAKE_BUILD_TYPE=Release

define IMUD_INSTALL_CONF
	$(INSTALL) -D -m 0644 $(IMUD_PKGDIR)/imud.conf \
		$(TARGET_DIR)/etc/imud.conf
endef

define IMUD_INSTALL_INIT_SYSV
	$(INSTALL) -D -m 0755 $(IMUD_PKGDIR)/S60imud \
		$(TARGET_DIR)/etc/init.d/S60imud
endef

IMUD_POST_INSTALL_TARGET_HOOKS += IMUD_INSTALL_CONF

$(eval $(cmake-package))
