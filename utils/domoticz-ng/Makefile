#
# Copyright (C) 2016 - 2020  Stijn Tintel <stijn@linux-ipv6.be>
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=domoticz
PKG_VERSION_MAJOR:=2025.1
PKG_VERSION_PATCH:=16732
PKG_COMMIT:=5db6fd8b2ad055b22b111ae90443cebc404f60a5
PKG_VERSION:=$(PKG_VERSION_MAJOR).$(PKG_VERSION_PATCH)
PKG_RELEASE:=1

ifeq ($(PKG_COMMIT),)
PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_URL:=https://github.com/domoticz/domoticz/archive/$(PKG_VERSION)/$(PKG_SOURCE)
PKG_HASH:=355057204437e295e3e3147623026481b500059ef24f59b59e3e8793237ac0b2
else
PKG_SOURCE:=$(PKG_NAME)-$(PKG_COMMIT).tar.gz
PKG_SOURCE_URL:=https://github.com/domoticz/domoticz/archive/$(PKG_COMMIT)/
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_COMMIT)
PKG_HASH:=d09e6899651f49dd05bf7de8938cc49393be2b90b1caf1cba982b88d37d8aff4
endif

PKG_LICENSE:=GPL-3.0
PKG_LICENSE_FILES:=License.txt

PKG_BUILD_DEPENDS:=minizip cereal DOMOTICZ-NG_PYTHON3:python3
PKG_BUILD_PARALLEL:=1
PKG_FIXUP:=autoreconf
PKG_USE_MIPS16:=0

PKG_CONFIG_DEPENDS := \
        CONFIG_PACKAGE_DOMOTICZ-NG \
        CONFIG_DOMOTICZ-NG_PYTHON3 \
        CONFIG_DOMOTICZ-NG_OPENZWAVE \
        CONFIG_DOMOTICZ-NG_TELLDUS

CMAKE_INSTALL:=1
TARGET_CPPFLAGS += -I$(STAGING_DIR)/usr/include/picojson

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/domoticz-ng
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=Open Source Home Automation System
  URL:=http://domoticz.com/
  MAINTAINER:=David Woodhouse <dwmw2@infradead.org>
  USERID:=domoticz=6144:domoticz=6144
  MENU:=1
  DEPENDS:= \
        +boost \
        +boost-date_time \
        +boost-system \
        +boost-thread \
        +jsoncpp \
        +libcurl \
        +DOMOTICZ-NG_PYTHON3:python3 \
        +DOMOTICZ-NG_OPENZWAVE:libopenzwave \
        +DOMOTICZ-NG_TELLDUS:telldus-core  \
        +minizip \
        +lua5.3 \
        +libfmt \
        +libmosquitto \
        +libopenssl \
        +libsqlite3 \
        +libstdcpp \
        +zlib
endef

define Package/domoticz-ng/config
        source "$(SOURCE)/Config.in"
endef

define Package/domoticz-ng/description
  Domoticz is a Home Automation System that lets you monitor and configure various devices like: Lights, Switches, various sensors/meters like Temperature, Rain, Wind, UV, Electra, Gas, Water and much more. Notifications/Alerts can be sent to any mobile device.
endef

CMAKE_OPTIONS += \
        -DBUILD_SHARED_LIBS=yes \
        -DBoost_NO_BOOST_CMAKE=yes \
        -DGIT_SUBMODULE=no \
        -DCMAKE_BUILD_TYPE=Release \
        -DUSE_BUILTIN_MQTT=no \
        -DUSE_BUILTIN_SQLITE=no \
        -DUSE_BUILTIN_JSONCPP=no \
        -DUSE_BUILTIN_MINIZIP=no \
        -DUSE_BUILTIN_JWTCPP=no \
        -DUSE_BUILTIN_LIBFMT=no \
        -DUSE_LUA_STATIC=no \
        -DUSE_STATIC_BOOST=no \
        -DUSE_STATIC_LIBSTDCXX=no \
        -DUSE_STATIC_OPENZWAVE=no \
        -DUSE_OPENSSL_STATIC=no \
        -DWITH_LIBUSB=no

TARGET_CFLAGS += -flto
TARGET_CXXFLAGS += -DWITH_GPIO -flto

ifneq ($(CONFIG_DOMOTICZ-NG_PYTHON3),)
CMAKE_OPTIONS += -DUSE_PYTHON=yes
else
CMAKE_OPTIONS += -DUSE_PYTHON=no
endif

ifneq ($(CONFIG_DOMOTICZ-NG_OPENZWAVE),)
CMAKE_OPTIONS += -DUSE_OPENZWAVE=yes
else
CMAKE_OPTIONS += -DUSE_OPENZWAVE=no
endif

ifneq ($(CONFIG_DOMOTICZ-NG_TELLDUS),)
CMAKE_OPTIONS += -DUSE_TELLDUS=yes
else
CMAKE_OPTIONS += -DUSE_TELLDUS=no
endif

define Build/Prepare
        $(call Build/Prepare/Default)
        # Fix APPVERSION/APPDATE since we don't build from a git tree
        sed -i 's|#define APPVERSION.[^\]*\\|#define APPVERSION $(PKG_VERSION_PATCH)\\|' \
                 $(PKG_BUILD_DIR)/getgit.cmake
        COMMITDATE=`tar tvfz $(DL_DIR)/$(PKG_SOURCE) --full-time | sed 's/.* \(20..-..-.. ..:..:..\) domoticz-.*/\1/;q'`; \
        COMMITTS=`date --date="$$$${COMMITDATE}" +%s`; \
        sed -i "s/#define APPDATE.*/#define APPDATE $$$${COMMITTS}\\\n\")/" $(PKG_BUILD_DIR)/getgit.cmake
ifneq ($(PKG_COMMIT),)
        sed -i "s/#define APPHASH.[^n]*n/#define APPHASH \"$(shell echo $(PKG_COMMIT) | cut -c1-8)\"\\\n/" $(PKG_BUILD_DIR)/getgit.cmake
        sed -i "s/#define APPHASH /#define APPHASH \\\/" $(PKG_BUILD_DIR)/getgit.cmake
        sed -i "s/$(shell echo $(PKG_COMMIT) | cut -c1-8)/$(shell echo $(PKG_COMMIT) | cut -c1-8)\\\/" $(PKG_BUILD_DIR)/getgit.cmake
endif

        # Remove unwanted scripts
        cd $(PKG_BUILD_DIR)/scripts && rm -rf \
                buienradar_rain_example.pl \
                _domoticz_main* \
                download_update.sh \
                dzVents/{.gitignore,documentation,examples,generated_scripts,data} \
                dzVents/runtime/{integration-tests,misc/smoothing.xlsx,tests} \
                logrotate/ \
                lua_parsers/example* \
                lua/*demo.lua \
                python/ \
                readme.txt \
                restart_domoticz \
                templates/All.Python \
                update_domoticz \
                domoticz.conf
        # Remove *.md
        cd $(PKG_BUILD_DIR) && $(FIND) -name '*.md' -delete
endef

define Package/domoticz-ng/install
        $(INSTALL_DIR) $(1)/etc/config $(1)/etc/hotplug.d/tty $(1)/etc/init.d $(1)/etc/domoticz/plugins
        $(INSTALL_BIN) ./files/domoticz.hotplug $(1)/etc/hotplug.d/tty/domoticz
        $(INSTALL_BIN) ./files/domoticz.init $(1)/etc/init.d/domoticz
        $(INSTALL_CONF) ./files/domoticz.config $(1)/etc/config/domoticz
        $(INSTALL_DIR) $(1)/usr/share/domoticz $(1)/usr/bin
        $(CP) $(PKG_INSTALL_DIR)/usr/dzVents $(1)/usr/share/domoticz/dzVents
        # Config for OZW
        # $(CP) $(PKG_INSTALL_DIR)/usr/Config $(1)/usr/share/domoticz/Config
        $(CP) $(PKG_INSTALL_DIR)/usr/scripts $(1)/etc/domoticz/scripts
        ln -sf /var/lib/domoticz/dzVents/generated_scripts $(1)/etc/domoticz/scripts/dzVents
        ln -sf /var/lib/domoticz/dzVents/data $(1)/etc/domoticz/scripts/dzVents
        $(CP) $(PKG_INSTALL_DIR)/usr/www $(1)/usr/share/domoticz/
        $(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/domoticz $(1)/usr/bin/domoticz
        # compress static web content
        find $(1)/usr/share/domoticz/www -name "*.css" -exec gzip -9 {} \;
        find $(1)/usr/share/domoticz/www -name "*.js" -exec gzip -9 {} \;
        gzip -9 $(1)/usr/share/domoticz/www/*.html
        gzip -9 $(1)/usr/share/domoticz/www/secpanel/*.html
        find $(1)/usr/share/domoticz/www/views -name "*.html" -exec sh -c 'grep -q "<\!--#embed" {} || gzip -9 {}' \;
endef

define Package/domoticz-ng/conffiles
/etc/config/domoticz
/var/lib/domoticz/
/etc/domoticz/
endef

$(eval $(call BuildPackage,domoticz-ng))