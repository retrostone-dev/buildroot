###############################################################################
#
# agg
#
###############################################################################

FATRESIZE_VERSION = 1.0
FATRESIZE_SOURCE = master.tar.gz
FATRESIZE_SITE = https://github.com/ya-mouse/fatresize/archive
FATRESIZE_LICENSE = GPLv3+
FATRESIZE_LICENSE_FILES = COPYING
FATRESIZE_INSTALL_STAGING = YES
FATRESIZE_AUTORECONF = YES

FATRESIZE_DEPENDENCIES = host-pkgconf parted

$(eval $(autotools-package))
