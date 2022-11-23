
include $(TOPDIR)/rules.mk

PKG_NAME:=ebusd
PKG_RELEASE:=1

PKG_SOURCE_URL:=https://github.com/john30/ebusd.git
PKG_SOURCE_PROTO:=git
PKG_SOURCE_DATE:=2022-11-14
PKG_SOURCE_VERSION:=bc3e6110c7f6e12d9912e09d061c9dd5f335de39
PKG_FIXUP:=autoreconf

include $(INCLUDE_DIR)/package.mk

define Package/ebusd
  SECTION:=net
  CATEGORY:=Network
  TITLE:=eBUS daemon
  URL:=https://github.com/john30/ebusd
  DEPENDS:=+libpthread +librt +libstdcpp +libmosquittopp +libopenssl
endef

define Package/ebusd/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/src/ebusd/ebusd $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/src/tools/ebusctl $(1)/usr/bin
endef

$(eval $(call BuildPackage,ebusd))

