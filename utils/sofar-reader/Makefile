include $(TOPDIR)/rules.mk

PKG_NAME:=sofar-reader
PKG_VERSION:=1.0.0
PKG_RELEASE:=1

PKG_LICENSE:=GPL-2.0-only
PKG_MAINTAINER:=kofec

include $(INCLUDE_DIR)/package.mk

define Package/sofar-reader
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=Sofar inverter reader (Solarman LSW-3 logger) for Domoticz
  DEPENDS:=+libuci
endef

define Package/sofar-reader/description
  Reads Sofar K-TLX inverter registers through a Solarman LSW-3/LSE
  data logger (protocol V5, TCP 8899) and pushes the values to Domoticz
  over its HTTP JSON API. One small C daemon per inverter, configured
  in /etc/config/sofar-reader. Replaces the Python Sofar_LSW3 scripts.
endef

define Package/sofar-reader/conffiles
/etc/config/sofar-reader
endef

define Package/sofar-reader/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sofar-reader $(1)/usr/sbin/
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/sofar-reader.init $(1)/etc/init.d/sofar-reader
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) ./files/sofar-reader.config $(1)/etc/config/sofar-reader
endef

$(eval $(call BuildPackage,sofar-reader))
