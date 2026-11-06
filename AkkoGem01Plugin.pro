#-----------------------------------------------------------#
# AKKO Gem 01 plugin for OpenRGB 1.0 (plugin API v5)        #
#-----------------------------------------------------------#
QT       += core gui widgets
TEMPLATE  = lib
CONFIG   += plugin c++17 silent
TARGET    = OpenRGBAkkoGem01Plugin

#-----------------------------------------------------------#
# Version info                                              #
#-----------------------------------------------------------#
PLUGIN_VERSION = 0.1.0
GIT_COMMIT = $$system(git -C $$PWD rev-parse --short HEAD 2> /dev/null)
DEFINES += AKKO_PLUGIN_VERSION=\\\"$$PLUGIN_VERSION\\\" \
           AKKO_PLUGIN_COMMIT=\\\"$$GIT_COMMIT\\\"

#-----------------------------------------------------------#
# OpenRGB headers (git submodule, tag release_1.0)          #
#-----------------------------------------------------------#
INCLUDEPATH += \
    src \
    OpenRGB \
    OpenRGB/RGBController \
    OpenRGB/dependencies/json

HEADERS += \
    OpenRGB/OpenRGBPluginInterface.h \
    src/AkkoGem01Plugin.h \
    src/AkkoGem01Device.h \
    src/ViaKeyboard.h

DISTFILES += src/plugin.json

SOURCES += \
    src/AkkoGem01Plugin.cpp \
    src/AkkoGem01Device.cpp \
    src/ViaKeyboard.cpp

#-----------------------------------------------------------#
# hidapi                                                    #
#-----------------------------------------------------------#
unix:!macx {
    CONFIG    += link_pkgconfig
    PKGCONFIG += hidapi-hidraw
}

macx {
    CONFIG    += link_pkgconfig
    PKGCONFIG += hidapi
}

win32 {
    LIBS += -lhidapi
}

#-----------------------------------------------------------#
# make install -> per-user OpenRGB plugin directory         #
#-----------------------------------------------------------#
unix:!macx {
    target.path = $$(HOME)/.config/OpenRGB/plugins
    INSTALLS += target
}
