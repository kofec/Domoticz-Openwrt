#
# Copyright (C) 2026 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=libwebem
PKG_VERSION:=2026-10-06-b7fd46e
PKG_RELEASE:=1

PKG_SOURCE_PROTO:=git
PKG_SOURCE_URL:=https://github.com/domoticz/libwebem.git
PKG_SOURCE_VERSION:=b7fd46e018fe10febd3f9af63053ca7a24427c1e
PKG_MIRROR_HASH:=skip

PKG_MAINTAINER:=David Woodhouse <dwmw2@infradead.org>
PKG_LICENSE:=GPL-3.0
PKG_LICENSE_FILES:=License.txt

PKG_BUILD_DEPENDS:=boost minizip jsoncpp jwt-cpp nlohmannjson

CMAKE_INSTALL:=1

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Build/Prepare
	$(call Build/Prepare/Default)
	# Usuwamy wewnętrzne kopie bibliotek
	rm -rf $(PKG_BUILD_DIR)/extern/jsoncpp
	rm -rf $(PKG_BUILD_DIR)/extern/jwt-cpp

	# Tworzymy link symboliczny do nagłówków systemowych jsoncpp
	# wewnątrz katalogu include projektu libwebem
	mkdir -p $(PKG_BUILD_DIR)/include/json
	ln -s $(STAGING_DIR)/usr/include/json/* $(PKG_BUILD_DIR)/include/json/ 2>/dev/null || true

	# Naprawiamy CMakeLists.txt (jsoncpp_static -> jsoncpp)
	$(SED) 's/minizip::minizip/minizip/g' $(PKG_BUILD_DIR)/CMakeLists.txt
	$(SED) 's/jsoncpp_static/jsoncpp/g' $(PKG_BUILD_DIR)/CMakeLists.txt
	$(SED) 's/jsoncpp_lib/jsoncpp/g' $(PKG_BUILD_DIR)/CMakeLists.txt
endef

# Przekazujemy ścieżki include bezpośrednio do CMake jako flagi CXX
# Dodano: -I$(STAGING_DIR)/usr/include/jsoncpp
CMAKE_OPTIONS += \
	-DCMAKE_CXX_FLAGS="$(TARGET_CXXFLAGS) -I$(STAGING_DIR)/usr/include -I$(STAGING_DIR)/usr/include/minizip -I$(STAGING_DIR)/usr/include/jsoncpp" \
	-DUSE_SYSTEM_JSONCPP=ON \
	-DUSE_SYSTEM_JWTCPP=ON \
	-DUSE_BUILTIN_JSONCPP=OFF \
	-DUSE_BUILTIN_JWTCPP=OFF \
	-DUSE_OPENSSL=ON 
#	-DBUILD_SHARED_LIBS=ON

define Package/libwebem
  BUILDONLY:=1
  SECTION:=libs
  CATEGORY:=Libraries
  TITLE:=Embedded Web Server C++ library
  URL:=https://github.com/domoticz/libwebem
  DEPENDS:=+boost +boost-system +boost-thread +minizip +jsoncpp +jwt-cpp +libopenssl
endef

define Package/libwebem/description
  A small C++ library for embedding a web server into an application.
endef

#define Package/libwebem/install
#	$(INSTALL_DIR) $(1)/usr/lib
#	$(CP) $(PKG_INSTALL_DIR)/usr/lib/libwebem.so* $(1)/usr/lib/
#endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include/libwebem
	$(CP) $(PKG_INSTALL_DIR)/usr/include/libwebem/* $(1)/usr/include/libwebem/
	# Kopiujemy plik prywatny sha1.h dla Domoticza
	$(CP) $(PKG_BUILD_DIR)/src/sha1.h $(1)/usr/include/libwebem/

	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/libwebem.a $(1)/usr/lib/

	# DODAJ TE LINIE: Kopiujemy pliki konfiguracyjne CMake dla webem
	$(INSTALL_DIR) $(1)/usr/lib/cmake/webem
	$(CP) $(PKG_INSTALL_DIR)/usr/lib/cmake/webem/* $(1)/usr/lib/cmake/webem/
endef

$(eval $(call BuildPackage,libwebem))
