
include $(TOPDIR)/rules.mk

PKG_NAME:=ebusd
PKG_RELEASE:=1

PKG_SOURCE_URL:=https://github.com/john30/ebusd.git
PKG_SOURCE_PROTO:=git
PKG_SOURCE_DATE:=2021-12-19
PKG_SOURCE_VERSION:=13221e2fd7a38a468916a98238b2e49d97687692
PKG_FIXUP:=autoreconf

include $(INCLUDE_DIR)/package.mk

define Package/ebusd
  SECTION:=net
  CATEGORY:=Network
  TITLE:=eBUS daemon
  URL:=https://github.com/john30/ebusd
  DEPENDS:=+libpthread +librt +libstdcpp +libmosquittopp 
endef

define Package/ebusd/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/src/ebusd/ebusd $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/src/tools/ebusctl $(1)/usr/bin
endef

$(eval $(call BuildPackage,ebusd))

