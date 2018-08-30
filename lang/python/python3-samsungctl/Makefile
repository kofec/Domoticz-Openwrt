#
# Copyright (C) 2015 CZ.NIC, z. s. p. o. (https://www.nic.cz/)
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=python3-samsungctl
PKG_VERSION:=0.7.1
PKG_RELEASE:=1

PKG_SOURCE:=samsungctl-$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=https://pypi.io/packages/source/s/samsungctl
PKG_HASH:=2fc175fb730b39ebaed5d95db178a6daeec1b802146b0116ed13e7629a1bff46

PKG_BUILD_DIR:=$(BUILD_DIR)/samsungctl-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk
include ../python3-package.mk

PKG_UNPACK:=$(HOST_TAR) -C $(PKG_BUILD_DIR) --strip-components=1 -xzf $(DL_DIR)/$(PKG_SOURCE)

define Package/python3-samsungctl
  SUBMENU:=Python
  SECTION:=lang
  CATEGORY:=Languages
  TITLE:=samsungctl is a library and a command line tool for remote controlling Samsung televisions via a TCP/IP connection
  URL:=https://github.com/jstasiak/python-samsungctl
  DEPENDS+=+PACKAGE_python3-samsungctl:python3-light
  VARIANT:=python3
endef

define Package/python3-samsungctl/description
  samsungctl is a library and a command line tool for remote controlling Samsung televisions via a TCP/IP connection
endef

$(eval $(call Py3Package,python3-samsungctl))
$(eval $(call BuildPackage,python3-samsungctl))
$(eval $(call BuildPackage,python3-samsungctl-src))