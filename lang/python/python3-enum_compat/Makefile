#
# Copyright (C) 2015 CZ.NIC, z. s. p. o. (https://www.nic.cz/)
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=python3-enum-compat
PKG_VERSION:=0.0.2
PKG_RELEASE:=1

PKG_SOURCE:=enum-compat-$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=https://pypi.io/packages/source/e/enum-compat
PKG_HASH:=939ceff18186a5762ae4db9fa7bfe017edbd03b66526b798dd8245394c8a4192

PKG_BUILD_DIR:=$(BUILD_DIR)/enum-compat-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk
include ../python3-package.mk

PKG_UNPACK:=$(HOST_TAR) -C $(PKG_BUILD_DIR) --strip-components=1 -xzf $(DL_DIR)/$(PKG_SOURCE)

define Package/python3-enum-compat
  SUBMENU:=Python
  SECTION:=lang
  CATEGORY:=Languages
  TITLE:=enum-compat is a powerful declarative and symmetrical parser and builder for binary data
  URL:=https://github.com/jstasiak/enum-compat
  DEPENDS+=+PACKAGE_python3-enum-compat:python3-light
  VARIANT:=python3
endef

define Package/python3-enum-compat/description
  enum-compat is a powerful declarative and symmetrical parser and builder for binary data
endef

$(eval $(call Py3Package,python3-enum-compat))
$(eval $(call BuildPackage,python3-enum-compat))
$(eval $(call BuildPackage,python3-enum-compat-src))