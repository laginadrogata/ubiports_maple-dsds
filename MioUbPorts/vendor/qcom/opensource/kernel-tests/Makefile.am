
ACLOCAL_AMFLAGS = -I m4

topdir           = $(prefix)

BUILD_TEST=

if KERNELMODULES
if USEION
BUILD_TEST += ion
endif

if USEIPI
BUILD_TEST += ipi
endif

if USEIPC_LOGGING
BUILD_TEST += ipc_logging
endif

if USEMEMORY_PROF
BUILD_TEST += memory_prof
endif
if USEMSM_BUS
BUILD_TEST += msm_bus
endif
if USEWATCHDOG
BUILD_TEST += watchdog
endif
if USEOCMEM
BUILD_TEST += ocmem
endif
if USEMSM_IOMMU
BUILD_TEST += msm_iommu
endif
if USEBAM_DMUX_LOOPBACK
BUILD_TEST += bam_dmux_loopback
endif
if USEGLINK
BUILD_TEST += glink
endif
if USESPCOM
BUILD_TEST += spcom
endif
if USEPAGE_NOM
BUILD_TEST += page-nom
endif
endif
if USESPS
BUILD_TEST += sps
endif
if USEV4L2APPS
BUILD_TEST += v4l2apps
endif
if USEIP_ACCELERATOR
BUILD_TEST += ip_accelerator
endif
if USEHRTIMER
BUILD_TEST += hrtimer
endif
if USESWP
BUILD_TEST += swp
endif
if USEMODSIGN
BUILD_TEST += modsign
endif
if USECORESIGHT
BUILD_TEST += coresight
endif
if USEBUS_TIMEOUT
BUILD_TEST += bus-timeout
endif
if USEABNORMAL_RESET
BUILD_TEST += abnormal_reset
endif
if USESPINLOCK
BUILD_TEST += spinlock
endif
if USEQRNG
BUILD_TEST += qrng
endif
if USEQCEDEV
BUILD_TEST += qcedev
endif
SUBDIRS = $(BUILD_TEST)

