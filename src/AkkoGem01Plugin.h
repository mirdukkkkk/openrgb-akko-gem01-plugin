/*---------------------------------------------------------*\
| AkkoGem01Plugin.h                                         |
|                                                           |
|   OpenRGB plugin adding support for the AKKO Gem 01       |
|   keyboard running its stock VIA firmware.                |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <QObject>
#include <QString>
#include <QtPlugin>

#include "OpenRGBPluginInterface.h"

class AkkoGem01Device;
class QLabel;
class QTimer;

class AkkoGem01Plugin : public QObject, public OpenRGBPluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OpenRGBPluginInterface_IID FILE "plugin.json")
    Q_INTERFACES(OpenRGBPluginInterface)

public:
    AkkoGem01Plugin();
    ~AkkoGem01Plugin() override;

    OpenRGBPluginInfo   GetPluginInfo() override;
    unsigned int        GetPluginAPIVersion() override;

    void                Load(OpenRGBPluginAPIInterface* plugin_api_ptr) override;
    QWidget*            GetWidget() override;
    QMenu*              GetTrayMenu() override;
    void                Unload() override;
    void                OnProfileAboutToLoad() override;
    void                OnProfileLoad(nlohmann::json profile_data) override;
    nlohmann::json      OnProfileSave() override;
    unsigned char*      OnSDKCommand(unsigned int pkt_id, unsigned char* pkt_data, unsigned int* pkt_size) override;

    void                ProfileManagerUpdated(unsigned int update_reason) override;
    void                ResourceManagerUpdated(unsigned int update_reason) override;
    void                SettingsManagerUpdated(unsigned int update_reason) override;

private:
    OpenRGBPluginAPIInterface*  api;
    AkkoGem01Device*            device;
    QLabel*                     status_label;
    QTimer*                     status_timer;
};
