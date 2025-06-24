include $(TOPDIR)/rules.mk

PKG_NAME:=picojson
PKG_VERSION:=2021-01-17
PKG_RELEASE:=1

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/kazuho/picojson.git
PKG_SOURCE_VERSION:=111c9be5188f7350c2eac9ddaedd8cca3d7bf394
PKG_MIRROR_HASH:=6b3f1505fed40609d403d912550ee666f8bba3f97885d3fe6c11344ab8875f13
#PKG_MIRROR_HASH:=skip

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/picojson
  SECTION:=libs
  CATEGORY:=Libraries
  TITLE:=PicoJSON - a tiny C++ JSON parser
  URL:=https://github.com/kazuho/picojson
endef

define Build/Compile
endef

define Build/InstallDev
        $(INSTALL_DIR) $(1)/usr/include/picojson
        $(CP) $(PKG_BUILD_DIR)/../picojson-$(PKG_VERSION)/picojson.h $(1)/usr/include/picojson/
endef

define Package/picojson/install
        $(INSTALL_DIR) $(1)/usr/include/picojson
        $(CP) $(PKG_BUILD_DIR)/../picojson-$(PKG_VERSION)/picojson.h $(1)/usr/include/picojson/
endef

$(eval $(call BuildPackage,picojson))
