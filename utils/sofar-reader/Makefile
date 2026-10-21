include $(TOPDIR)/rules.mk

PKG_NAME:=sofar-reader
PKG_VERSION:=1.1.0
PKG_RELEASE:=1

PKG_LICENSE:=GPL-2.0-only
PKG_MAINTAINER:=kofec

include $(INCLUDE_DIR)/package.mk

# One source tree, two packages: build only the selected programs, so a
# sofar-reader build does not need json-c and OpenSSL headers.
MAKE_FLAGS += \
	$(if $(CONFIG_PACKAGE_sofar-reader),sofar-reader) \
	$(if $(CONFIG_PACKAGE_goodwe-reader),goodwe-reader)

define Package/sofar-reader
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=Sofar inverter reader (Solarman LSW-3 logger) for Domoticz
  DEPENDS:=+libuci +libcurl
endef

define Package/sofar-reader/description
  Reads Sofar K-TLX inverter registers through a Solarman LSW-3/LSE
  data logger (protocol V5, TCP 8899) and pushes the values to Domoticz
  over its HTTP JSON API, optionally also to PVOutput. One small C daemon
  per inverter, configured in /etc/config/sofar-reader. Replaces the
  Python Sofar_LSW3 scripts.
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

define Package/goodwe-reader
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=GoodWe inverter reader (LAN Modbus, SEMS+) for Domoticz
  DEPENDS:=+libuci +libcurl +libjson-c +libopenssl
endef

define Package/goodwe-reader/description
  Reads a GoodWe DT family inverter (e.g. GW3000-XS) in the LAN over
  Modbus RTU on UDP 8899, falls back to the SEMS+ cloud when it does not
  answer, and pushes the values to Domoticz and PVOutput. Once a day it
  fills gaps of yesterday in PVOutput from the SEMS+ power curve. Sleeps
  at night using sunrise/sunset from Domoticz. Configured in
  /etc/config/goodwe-reader. Replaces the Python monitor_Goodwe.py.
endef

define Package/goodwe-reader/conffiles
/etc/config/goodwe-reader
endef

define Package/goodwe-reader/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/goodwe-reader $(1)/usr/sbin/
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/goodwe-reader.init $(1)/etc/init.d/goodwe-reader
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) ./files/goodwe-reader.config $(1)/etc/config/goodwe-reader
endef

$(eval $(call BuildPackage,sofar-reader))
$(eval $(call BuildPackage,goodwe-reader))
