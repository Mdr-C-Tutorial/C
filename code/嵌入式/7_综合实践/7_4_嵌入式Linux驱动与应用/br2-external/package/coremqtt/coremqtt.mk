################################################################################
#
# coremqtt —— coreMQTT 2.3.1，只装头文件与静态库
#
# coreMQTT 没有自己的构建系统：三个 .c 文件编成静态库，
# 头文件、库与配置头都装进 staging，imud 编译时从 sysroot 里找。
#
################################################################################

COREMQTT_VERSION = v2.3.1
COREMQTT_SITE = $(call github,FreeRTOS,coreMQTT,$(COREMQTT_VERSION))
COREMQTT_LICENSE = MIT
COREMQTT_LICENSE_FILES = LICENSE
COREMQTT_INSTALL_STAGING = YES

# 配置头只有一份，放在 imud/ 下，库和应用的编译都用它：
# MQTT_RECV_POLLING_TIMEOUT_MS、MQTT_SUB_UNSUB_MAX_VECTORS 这些宏
# 在库内部（core_mqtt.c）里使用，只在应用侧定义不起作用。
COREMQTT_CONFIG_SRC = $(BR2_EXTERNAL_MDR_PATH)/../imud/core_mqtt_config.h
COREMQTT_CFLAGS = $(TARGET_CFLAGS) -O2 -I$(@D)/config

define COREMQTT_BUILD_CMDS
	$(INSTALL) -D -m 0644 $(COREMQTT_CONFIG_SRC) \
		$(@D)/config/core_mqtt_config.h
	$(TARGET_CC) $(COREMQTT_CFLAGS) -c -o $(@D)/core_mqtt.o \
		-I$(@D)/source/include -I$(@D)/source/interface \
		$(@D)/source/core_mqtt.c
	$(TARGET_CC) $(COREMQTT_CFLAGS) -c -o $(@D)/core_mqtt_serializer.o \
		-I$(@D)/source/include -I$(@D)/source/interface \
		$(@D)/source/core_mqtt_serializer.c
	$(TARGET_CC) $(COREMQTT_CFLAGS) -c -o $(@D)/core_mqtt_state.o \
		-I$(@D)/source/include -I$(@D)/source/interface \
		$(@D)/source/core_mqtt_state.c
	$(TARGET_AR) rcs $(@D)/libcoreMQTT.a \
		$(@D)/core_mqtt.o $(@D)/core_mqtt_serializer.o \
		$(@D)/core_mqtt_state.o
endef

define COREMQTT_INSTALL_STAGING_CMDS
	$(INSTALL) -D -m 0644 $(@D)/libcoreMQTT.a \
		$(STAGING_DIR)/usr/lib/libcoreMQTT.a
	mkdir -p $(STAGING_DIR)/usr/include/coreMQTT
	$(INSTALL) -m 0644 $(@D)/source/include/*.h \
		$(STAGING_DIR)/usr/include/coreMQTT/
	$(INSTALL) -m 0644 $(@D)/source/interface/*.h \
		$(STAGING_DIR)/usr/include/coreMQTT/
	$(INSTALL) -m 0644 $(@D)/config/core_mqtt_config.h \
		$(STAGING_DIR)/usr/include/coreMQTT/
endef

$(eval $(generic-package))