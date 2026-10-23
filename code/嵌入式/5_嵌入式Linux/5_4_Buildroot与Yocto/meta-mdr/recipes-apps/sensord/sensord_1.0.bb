SUMMARY = "MDR 的传感器轮询服务"
DESCRIPTION = "周期读取数据源并输出统计；SIGHUP 重载配置，SIGTERM 干净退出。"
LICENSE = "CLOSED"

SRC_URI = "file://sensord.c \
           file://evl.c \
           file://evl.h \
           file://sdnotify.c \
           file://sdnotify.h \
           file://CMakeLists.txt \
           file://sensord.conf \
           file://S50sensord"

S = "${UNPACKDIR}"

inherit cmake update-rc.d

INITSCRIPT_NAME = "S50sensord"
INITSCRIPT_PARAMS = "defaults 50"

do_install:append() {
    install -d ${D}${sysconfdir}
    install -m 0644 ${UNPACKDIR}/sensord.conf ${D}${sysconfdir}/sensord.conf
}

FILES:${PN} += "${sysconfdir}/sensord.conf"
