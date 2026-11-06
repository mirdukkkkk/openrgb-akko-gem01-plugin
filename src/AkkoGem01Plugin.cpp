/*---------------------------------------------------------*\
| AkkoGem01Plugin.cpp                                       |
|                                                           |
|   OpenRGB plugin adding support for the AKKO Gem 01       |
|   keyboard running its stock VIA firmware.                |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "AkkoGem01Plugin.h"
#include "AkkoGem01Device.h"

#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#ifndef AKKO_PLUGIN_VERSION
#define AKKO_PLUGIN_VERSION "0.1.0"
#endif

#ifndef AKKO_PLUGIN_COMMIT
#define AKKO_PLUGIN_COMMIT ""
#endif

AkkoGem01Plugin::AkkoGem01Plugin() :
    api(nullptr),
    device(nullptr),
    status_label(nullptr),
    status_timer(nullptr)
{
}

AkkoGem01Plugin::~AkkoGem01Plugin()
{
    Unload();
}

OpenRGBPluginInfo AkkoGem01Plugin::GetPluginInfo()
{
    OpenRGBPluginInfo info;

    info.Name            = "AKKO Gem 01";
    info.Description     = "Lighting control for the AKKO Gem 01 keyboard (stock VIA firmware)";
    info.Version         = AKKO_PLUGIN_VERSION;
    info.Commit          = AKKO_PLUGIN_COMMIT;
    info.URL             = "https://github.com/mirdukkkkk/openrgb-akko-gem01-plugin";
    info.Location        = OPENRGB_PLUGIN_LOCATION_INFORMATION;
    info.Label           = "AKKO Gem 01";
    info.TabIconString   = "Keyboard";
    info.ProtocolVersion = 0;

    return info;
}

unsigned int AkkoGem01Plugin::GetPluginAPIVersion()
{
    return OPENRGB_PLUGIN_API_VERSION;
}

void AkkoGem01Plugin::Load(OpenRGBPluginAPIInterface* plugin_api_ptr)
{
    api    = plugin_api_ptr;
    device = new AkkoGem01Device(api);
    device->Start();
}

QWidget* AkkoGem01Plugin::GetWidget()
{
    QWidget*     widget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(widget);

    QLabel* title = new QLabel("<b>AKKO Gem 01</b> &mdash; stock VIA firmware bridge", widget);
    status_label  = new QLabel(widget);
    status_label->setWordWrap(true);
    status_label->setTextInteractionFlags(Qt::TextSelectableByMouse);

    QLabel* notes = new QLabel(
        "The stock firmware only exposes whole-keyboard lighting through VIA: "
        "effect, brightness, speed and color. Per-key colors are not possible "
        "without custom firmware.<br><br>"
        "<b>Direct</b> mode makes the whole keyboard follow one OpenRGB color, "
        "so it works with effect plugins and profiles.<br>"
        "Changes are temporary until you press <b>Save To Device</b>.",
        widget);
    notes->setWordWrap(true);

    layout->addWidget(title);
    layout->addWidget(status_label);
    layout->addWidget(notes);
    layout->addStretch(1);

    status_timer = new QTimer(widget);
    connect(status_timer, &QTimer::timeout, widget, [this]()
    {
        if(device && status_label)
        {
            status_label->setText(QString::fromStdString(device->GetStatus()));
        }
    });
    status_timer->start(1000);

    if(device)
    {
        status_label->setText(QString::fromStdString(device->GetStatus()));
    }

    connect(widget, &QObject::destroyed, this, [this]()
    {
        status_label = nullptr;
        status_timer = nullptr;
    });

    return widget;
}

QMenu* AkkoGem01Plugin::GetTrayMenu()
{
    return nullptr;
}

void AkkoGem01Plugin::Unload()
{
    if(status_timer)
    {
        status_timer->stop();
    }

    if(device)
    {
        device->Stop();
        delete device;
        device = nullptr;
    }
}

void AkkoGem01Plugin::OnProfileAboutToLoad()
{
}

void AkkoGem01Plugin::OnProfileLoad(nlohmann::json /*profile_data*/)
{
}

nlohmann::json AkkoGem01Plugin::OnProfileSave()
{
    return nlohmann::json();
}

unsigned char* AkkoGem01Plugin::OnSDKCommand(unsigned int /*pkt_id*/, unsigned char* /*pkt_data*/, unsigned int* pkt_size)
{
    if(pkt_size)
    {
        *pkt_size = 0;
    }
    return nullptr;
}

void AkkoGem01Plugin::ProfileManagerUpdated(unsigned int /*update_reason*/)
{
}

void AkkoGem01Plugin::ResourceManagerUpdated(unsigned int /*update_reason*/)
{
}

void AkkoGem01Plugin::SettingsManagerUpdated(unsigned int /*update_reason*/)
{
}
