#
# Copyright (C) 2019 CZ.NIC, z. s. p. o. (https://www.nic.cz/)
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=python-ciso8601
PKG_VERSION:=2.1.3
PKG_RELEASE:=1

PYPI_NAME:=ciso8601
PKG_HASH:=bdbb5b366058b1c87735603b23060962c439ac9be66f1ae91e8c7dbd7d59e262

PKG_MAINTAINER:=Josef Schlehofer <pepe.schlehofer@gmail.com>
PKG_LICENSE:=Apache-2.0
PKG_LICENSE_FILES:=LICENSE.md

include ../pypi.mk
include $(INCLUDE_DIR)/package.mk
include ../python3-package.mk

define Package/python3-ciso8601
  SECTION:=lang
  CATEGORY:=Languages
  SUBMENU:=Python
  TITLE:=converts ISO 8601 or RFC 3339 date time strings into Python datetime objects
  URL:=https://github.com/closeio/ciso8601
  DEPENDS:= \
	  +python3-light \
	  +python3-requests \
	  +python3-zeroconf
  VARIANT:=python3
endef

define Package/python3-ciso8601/description
  Ciso8601 converts ISO 8601 or RFC 3339 date time strings into Python datetime objects.
endef

$(eval $(call Py3Package,python3-ciso8601))
$(eval $(call BuildPackage,python3-ciso8601))
$(eval $(call BuildPackage,python3-ciso8601-src))
