#------------------------------------------------------------------------------
# Copyright (c) 2004-2010 Atheros Communications Inc.
# All rights reserved.
#
# 
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#
#
#
# Author(s): ="Atheros"
#------------------------------------------------------------------------------

ccflags-y += -I$(obj)/include
ccflags-y += -I$(obj)/include/common
ccflags-y += -I$(obj)/wlan/include
ccflags-y += -I$(obj)/os/linux/include
ccflags-y += -I$(obj)/os
ccflags-y += -I$(obj)/bmi/include
ccflags-y += -I$(obj)/include/common/AR6002/hw4.0
ccflags-y += -I$(obj)/hif/sdio/linux_sdio/include

ccflags-y += -DLINUX 
ccflags-y += -DDEBUG 
ccflags-y += -D__KERNEL__ 
ccflags-y += -D__linux__
ccflags-y += -DTCMD 
ccflags-y += -DSEND_EVENT_TO_APP 
ccflags-y += -DUSER_KEYS 
ccflags-y += -DNO_SYNC_FLUSH 
ccflags-y += -DWAPI_ENABLE 
ccflags-y += -DANDROID_ENV 
ccflags-y += -DSOFTMAC_USED 
ccflags-y += -DREGION_CODE_FILE_USED 
ccflags-y += -DKERNEL_2_6

obj-$(CONFIG_ATH6K_X10) := ar6000.o

ar6000-y += hif/sdio/linux_sdio/src/hif.o 
ar6000-y += htc2/AR6000/ar6k.o 
ar6000-y += htc2/AR6000/ar6k_events.o 
ar6000-y += htc2/htc_send.o 
ar6000-y += htc2/htc_recv.o 
ar6000-y += htc2/htc_services.o 
ar6000-y += htc2/htc.o 
ar6000-y += bmi/src/bmi.o 
ar6000-y += os/linux/ar6000_drv.o 
ar6000-y += os/linux/ar6000_raw_if.o 
ar6000-y += os/linux/netbuf.o 
ar6000-y += os/linux/wireless_ext.o 
ar6000-y += os/linux/ioctl.o 
ar6000-y += os/linux/engine.o 
ar6000-y += miscdrv/common_drv.o 
ar6000-y += miscdrv/credit_dist.o 
ar6000-y += wmi/wmi.o 
ar6000-y += wlan/src/wlan_node.o 
ar6000-y += wlan/src/wlan_recv_beacon.o 
ar6000-y += wlan/src/wlan_utils.o 
ar6000-y += os/linux/eeprom.o 
#os/linux/ar6000.o 
