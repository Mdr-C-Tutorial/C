SUMMARY = "MDR 的纯软件 FIFO 字符设备"
LICENSE = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/GPL-2.0-only;md5=801f80980d171dd6425610833a22dbe6"

SRC_URI = "file://mdr_fifo.c file://Makefile"

S = "${UNPACKDIR}"

inherit module

RPROVIDES:${PN} += "kernel-module-mdr-fifo"
