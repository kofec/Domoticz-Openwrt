include $(TOPDIR)/rules.mk

PKG_NAME:=oscam
PKG_RELEASE:=1
# Dodaj PKG_VERSION, bo używasz go w kodzie, a wcześniej go brakowało!
PKG_VERSION:=$(PKG_SOURCE_DATE)-$(PKG_SOURCE_VERSION)

PKG_SOURCE_URL:=https://git.streamboard.tv/common/oscam
PKG_SOURCE_PROTO:=git
PKG_SOURCE_DATE:=2024-05-10
PKG_SOURCE_VERSION:=6d9801f2c605d7656d7e6b6fde5c8210b28e9d2d
PKG_MIRROR_HASH:=38cfaa55886807ed1ff74097bb823c0ddf01fdba18766cbcb4b19cad3498aca4

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/oscam
  SECTION:=utils
  CATEGORY:=Utilities
  DEPENDS:=+libopenssl +libusb-1.0 +libpcsclite +librt +libpthread +libdvbcsa
  TITLE:=OSCam conditional access module
endef

# Przenieś generowanie wersji tutaj - wykona się tylko raz
define Build/Prepare
	$(call Build/Prepare/Default)
	(cd $(PKG_BUILD_DIR); echo "$(PKG_VERSION)" > version.txt)
endef

# Użyj CMAKE_OPTIONS zamiast config.sh dla czystości procesu
CMAKE_OPTIONS += \
	-DCS_CONFDIR=/etc/oscam \
	-DUSE_LIBUSB=ON \
	-DUSE_PCSC=ON \
	-DHAVE_LIBDVBCSA=ON \
	-DIPV6SUPPORT=OFF \
	-DCS_SVN_VERSION="$(PKG_SOURCE_VERSION)"

define Package/oscam/install
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/oscam.init $(1)/etc/init.d/oscam

	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/oscam $(1)/usr/bin/

	$(INSTALL_DIR) $(1)/etc/oscam
	$(CP) ./files/oscam.{server,conf,user} $(1)/etc/oscam/
endef

$(eval $(call BuildPackage,oscam))
