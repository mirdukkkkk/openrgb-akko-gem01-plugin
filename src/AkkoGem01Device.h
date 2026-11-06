/*---------------------------------------------------------*\
| AkkoGem01Device.h                                         |
|                                                           |
|   Bridges an AKKO Gem 01 (stock VIA firmware) to an       |
|   OpenRGB virtual RGBController.                          |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

#include "OpenRGBPluginInterface.h"
#include "ViaKeyboard.h"

#define AKKO_VID            0x342D
#define AKKO_GEM01_PID      0xE4E3

/*---------------------------------------------------------*\
| Mode value used for the emulated "Direct" mode (whole     |
| keyboard follows one OpenRGB color, implemented with the  |
| firmware's Solid Color effect)                            |
\*---------------------------------------------------------*/
#define AKKO_MODE_DIRECT    -1

class AkkoGem01Device
{
public:
    AkkoGem01Device(OpenRGBPluginAPIInterface* api);
    ~AkkoGem01Device();

    void            Start();
    void            Stop();

    std::string     GetStatus();

private:
    /*-----------------------------------------------------*\
    | Hotplug / detection thread                            |
    \*-----------------------------------------------------*/
    void            Run();
    bool            TryConnect();
    void            Disconnect(const char* reason);
    void            FillSetup(RGBController_Setup& setup, const RGBMatrixState& state);

    /*-----------------------------------------------------*\
    | RGBController_Virtual callbacks                       |
    \*-----------------------------------------------------*/
    static void     CbUpdateLEDs(void* obj);
    static void     CbUpdateZoneLEDs(void* obj, int zone);
    static void     CbUpdateSingleLED(void* obj, int led);
    static void     CbUpdateMode(void* obj);
    static void     CbSaveMode(void* obj);

    void            ApplyMode();
    void            ApplyDirectColor();
    void            SaveMode();
    void            CheckLink(bool ok);

    OpenRGBPluginAPIInterface*  api;
    RGBControllerInterface*     controller;
    bool                        registered;

    ViaKeyboard                 kbd;
    uint16_t                    protocol_version;

    /*-----------------------------------------------------*\
    | Last values written to the keyboard, to avoid         |
    | resending unchanged settings (Direct mode can be      |
    | driven at high frame rates by effect plugins)         |
    \*-----------------------------------------------------*/
    std::mutex                  apply_lock;
    int                         sent_effect;
    int                         sent_brightness;
    int                         sent_speed;
    int                         sent_hue;
    int                         sent_saturation;
    std::chrono::steady_clock::time_point last_full_send;

    std::thread                 worker;
    std::mutex                  worker_lock;
    std::condition_variable     worker_cv;
    std::atomic<bool>           running;
    std::atomic<bool>           link_lost;

    std::mutex                  status_lock;
    std::string                 status;
};
