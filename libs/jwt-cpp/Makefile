include $(TOPDIR)/rules.mk

PKG_NAME:=jwt-cpp
PKG_VERSION:=0.7.1
PKG_RELEASE:=1

PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=https://github.com/Thalhammer/$(PKG_NAME)/archive/refs/tags/v$(PKG_VERSION)
PKG_HASH:=e52f247d5e62fac5da6191170998271a70ce27f747f2ce8fde9b09f96a5375a4
PKG_SOURCE_SUBDIR:=$(PKG_NAME)-$(PKG_VERSION)

CMAKE_INSTALL:=1
#CMAKE_OPTIONS += -DJWT_BUILD_EXAMPLES=OFF
#CMAKE_OPTIONS += -DJWT_DISABLE_PICOJSON=ON

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/jwt-cpp
  SECTION:=libs
  CATEGORY:=Libraries
  TITLE:=jwt-cpp – header-only JWT library for C++
  URL:=https://github.com/Thalhammer/jwt-cpp
  DEPENDS:=+libstdcpp
endef

define Package/jwt-cpp/description
  A header-only C++ library for creating and verifying JSON Web Tokens (JWT).
endef

define Build/InstallDev
        $(INSTALL_DIR) $(1)/usr/include/jwt-cpp
        $(CP) -a $(PKG_BUILD_DIR)/include/jwt-cpp/. $(1)/usr/include/jwt-cpp/
endef

define Package/jwt-cpp/install
        true
endef

$(eval $(call BuildPackage,jwt-cpp))
