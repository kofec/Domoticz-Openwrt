#
# Copyright (C) 2015 CZ.NIC, z. s. p. o. (https://www.nic.cz/)
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=python3-construct
PKG_VERSION:=2.10.56
PKG_RELEASE:=1

PKG_SOURCE:=construct-$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=https://pypi.io/packages/source/c/construct
PKG_HASH:=97ba13edcd98546f10f7555af41c8ce7ae9d8221525ec4062c03f9adbf940661

PKG_BUILD_DIR:=$(BUILD_DIR)/construct-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk
include ../python3-package.mk

PKG_UNPACK:=$(HOST_TAR) -C $(PKG_BUILD_DIR) --strip-components=1 -xzf $(DL_DIR)/$(PKG_SOURCE)

define Package/python3-construct
  SUBMENU:=Python
  SECTION:=lang
  CATEGORY:=Languages
  TITLE:=Construct is a powerful declarative and symmetrical parser and builder for binary data
  URL:=https://github.com/jstasiak/python-construct
  DEPENDS+=+PACKAGE_python3-construct:python3-light
  VARIANT:=python3
endef

define Package/python3-construct/description
  Construct is a powerful declarative and symmetrical parser and builder for binary data
endef

$(eval $(call Py3Package,python3-construct))
$(eval $(call BuildPackage,python3-construct))
$(eval $(call BuildPackage,python3-construct-src))
