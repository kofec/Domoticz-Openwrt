#
# Copyright (C) 2015 CZ.NIC, z. s. p. o. (https://www.nic.cz/)
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=python3-android_backup
PKG_VERSION:=0.2.0
PKG_RELEASE:=1

PKG_SOURCE:=android_backup-$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=https://pypi.io/packages/source/a/android_backup
PKG_HASH:=864b6a9f8e2dda7a3af3726df7439052d35781c5f7d50dd771d709293d158b97

PKG_BUILD_DIR:=$(BUILD_DIR)/android_backup-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk
include ../python3-package.mk

PKG_UNPACK:=$(HOST_TAR) -C $(PKG_BUILD_DIR) --strip-components=1 -xzf $(DL_DIR)/$(PKG_SOURCE)

define Package/python3-android_backup
  SUBMENU:=Python
  SECTION:=lang
  CATEGORY:=Languages
  TITLE:=Unpack and repack android backups
  URL:=https://github.com/jstasiak/python-android_backup
  DEPENDS+=+PACKAGE_python3-android_backup:python3-light
  VARIANT:=python3
endef

define Package/python3-android_backup/description
  Unpack and repack android backups
endef

$(eval $(call Py3Package,python3-android_backup))
$(eval $(call BuildPackage,python3-android_backup))
$(eval $(call BuildPackage,python3-android_backup-src))