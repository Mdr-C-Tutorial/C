################################################################################
#
# mdr-fifo
#
################################################################################

MDR_FIFO_VERSION = 1.0
MDR_FIFO_SITE = $(BR2_EXTERNAL_MDR_PATH)/package/mdr-fifo/src
MDR_FIFO_SITE_METHOD = local
MDR_FIFO_LICENSE = GPL-2.0
MDR_FIFO_LICENSE_FILES = mdr_fifo.c

$(eval $(kernel-module))
$(eval $(generic-package))