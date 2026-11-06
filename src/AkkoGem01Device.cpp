/*---------------------------------------------------------*\
| AkkoGem01Device.cpp                                       |
|                                                           |
|   Bridges an AKKO Gem 01 (stock VIA firmware) to an       |
|   OpenRGB virtual RGBController.                          |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "AkkoGem01Device.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

#define LOG(level, ...) api->LogEntry(__FILE__, __LINE__, level, __VA_ARGS__)

/* LogManager levels (OpenRGB LogManager.h) */
enum { PLUGIN_LL_ERROR = 1, PLUGIN_LL_WARNING = 2, PLUGIN_LL_INFO = 3, PLUGIN_LL_VERBOSE = 4 };

/*---------------------------------------------------------*\
| Effect table, taken from Akko's VIA definition for the    |
| Gem 01 (gem_01.json). value = rgb_matrix mode index.      |
\*---------------------------------------------------------*/
struct AkkoEffect
{
    const char* name;
    uint8_t     value;
};

static const AkkoEffect akko_effects[] =
{
    { "Off",                        0  },
    { "Static",                     1  },   /* Solid Color */
    { "Alphas Mods",                2  },
    { "Gradient Up Down",           3  },
    { "Gradient Left Right",        4  },
    { "Breathing",                  5  },
    { "Band Saturation",            6  },
    { "Band Value",                 7  },
    { "Band Pinwheel Saturation",   8  },
    { "Band Pinwheel Value",        9  },
    { "Band Spiral Saturation",     10 },
    { "Band Spiral Value",          11 },
    { "Cycle All",                  12 },
    { "Cycle Left Right",           13 },
    { "Cycle Up Down",              14 },
    { "Rainbow Moving Chevron",     15 },
    { "Cycle Out In",               16 },
    { "Cycle Out In Dual",          17 },
    { "Cycle Pinwheel",             18 },
    { "Cycle Spiral",               19 },
    { "Dual Beacon",                20 },
    { "Rainbow Beacon",             21 },
    { "Rainbow Pinwheels",          22 },
    { "Raindrops",                  23 },
    { "Jellybean Raindrops",        24 },
    { "Hue Breathing",              25 },
    { "Hue Pendulum",               26 },
    { "Hue Wave",                   27 },
    { "Pixel Rain",                 28 },
    { "Pixel Flow",                 29 },
    { "Pixel Fractal",              30 },
    { "Typing Heatmap",             31 },
    { "Digital Rain",               32 },
    { "Solid Reactive Simple",      33 },
    { "Solid Reactive",             34 },
    { "Solid Reactive Wide",        35 },
    { "Solid Reactive Multiwide",   36 },
    { "Solid Reactive Cross",       37 },
    { "Solid Reactive Multicross",  38 },
    { "Solid Reactive Nexus",       39 },
    { "Solid Reactive Multinexus",  40 },
    { "Splash",                     41 },
    { "Multisplash",                42 },
    { "Solid Splash",               43 },
    { "Solid Multisplash",          44 },
    { "Close All",                  45 },
};

#define AKKO_EFFECT_COUNT   (sizeof(akko_effects) / sizeof(akko_effects[0]))
#define AKKO_EFFECT_SOLID   1

/*---------------------------------------------------------*\
| Mode index 0 is the emulated Direct mode, mode index      |
| i + 1 is akko_effects[i]                                  |
\*---------------------------------------------------------*/
#define MODE_IDX_DIRECT     0

static bool EffectHasColor(uint8_t value)
{
    /* Same rule as the VIA definition's showIf */
    return value != 0 && value != 24 && value != 28 && value != 29 && value != 32;
}

static bool EffectHasSpeed(uint8_t value)
{
    return value != 0 && value != AKKO_EFFECT_SOLID;
}

/*---------------------------------------------------------*\
| Color helpers, QMK HSV uses 0-255 for all components      |
\*---------------------------------------------------------*/
static void RGBToHSV(RGBColor color, uint8_t& h, uint8_t& s, uint8_t& v)
{
    float r = RGBGetRValue(color) / 255.0f;
    float g = RGBGetGValue(color) / 255.0f;
    float b = RGBGetBValue(color) / 255.0f;

    float max   = std::max(r, std::max(g, b));
    float min   = std::min(r, std::min(g, b));
    float delta = max - min;
    float hue   = 0.0f;

    if(delta > 0.0f)
    {
        if(max == r)
        {
            hue = std::fmod((g - b) / delta, 6.0f);
        }
        else if(max == g)
        {
            hue = ((b - r) / delta) + 2.0f;
        }
        else
        {
            hue = ((r - g) / delta) + 4.0f;
        }

        hue /= 6.0f;

        if(hue < 0.0f)
        {
            hue += 1.0f;
        }
    }

    h = (uint8_t)std::lround(hue * 255.0f) ;
    s = (uint8_t)std::lround(max > 0.0f ? (delta / max) * 255.0f : 0.0f);
    v = (uint8_t)std::lround(max * 255.0f);
}

static RGBColor HSVToRGB(uint8_t h, uint8_t s, uint8_t v)
{
    float hue = (h / 255.0f) * 6.0f;
    float sat = s / 255.0f;
    float val = v / 255.0f;

    int   sector = (int)hue % 6;
    float f      = hue - std::floor(hue);
    float p      = val * (1.0f - sat);
    float q      = val * (1.0f - sat * f);
    float t      = val * (1.0f - sat * (1.0f - f));
    float r, g, b;

    switch(sector)
    {
        case 0:  r = val; g = t;   b = p;   break;
        case 1:  r = q;   g = val; b = p;   break;
        case 2:  r = p;   g = val; b = t;   break;
        case 3:  r = p;   g = q;   b = val; break;
        case 4:  r = t;   g = p;   b = val; break;
        default: r = val; g = p;   b = q;   break;
    }

    return ToRGBColor((unsigned int)std::lround(r * 255.0f),
                      (unsigned int)std::lround(g * 255.0f),
                      (unsigned int)std::lround(b * 255.0f));
}

/*---------------------------------------------------------*/

AkkoGem01Device::AkkoGem01Device(OpenRGBPluginAPIInterface* api_ptr) :
    api(api_ptr),
    controller(nullptr),
    registered(false),
    protocol_version(0),
    sent_effect(-1),
    sent_brightness(-1),
    sent_speed(-1),
    sent_hue(-1),
    sent_saturation(-1),
    running(false),
    link_lost(false),
    status("Not started")
{
}

AkkoGem01Device::~AkkoGem01Device()
{
    Stop();
}

void AkkoGem01Device::Start()
{
    if(running)
    {
        return;
    }

    hid_init();

    running = true;
    worker  = std::thread(&AkkoGem01Device::Run, this);
}

void AkkoGem01Device::Stop()
{
    if(!running)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> guard(worker_lock);
        running = false;
    }
    worker_cv.notify_all();

    if(worker.joinable())
    {
        worker.join();
    }

    Disconnect("plugin unloaded");

    if(controller)
    {
        api->DeleteVirtualRGBController(controller);
        controller = nullptr;
    }
}

std::string AkkoGem01Device::GetStatus()
{
    std::lock_guard<std::mutex> guard(status_lock);
    return status;
}

void AkkoGem01Device::Run()
{
    std::unique_lock<std::mutex> lk(worker_lock);

    while(running)
    {
        lk.unlock();

        if(!kbd.IsOpen())
        {
            TryConnect();
        }
        else
        {
            bool present = false;

            for(const ViaDeviceInfo& info : ViaKeyboard::Enumerate(AKKO_VID, AKKO_GEM01_PID))
            {
                if(info.path == kbd.GetInfo().path)
                {
                    present = true;
                    break;
                }
            }

            if(!present)
            {
                Disconnect("device unplugged");
            }
            else if(link_lost)
            {
                Disconnect("device stopped responding");
            }
        }

        lk.lock();
        worker_cv.wait_for(lk, std::chrono::seconds(2), [this]{ return !running || link_lost; });
    }
}

bool AkkoGem01Device::TryConnect()
{
    std::vector<ViaDeviceInfo> devices = ViaKeyboard::Enumerate(AKKO_VID, AKKO_GEM01_PID);

    if(devices.empty())
    {
        std::lock_guard<std::mutex> guard(status_lock);
        status = "Keyboard not found (USB 342d:e4e3). Wireless modes are not supported.";
        return false;
    }

    const ViaDeviceInfo& info = devices.front();

    if(!kbd.Open(info))
    {
        std::lock_guard<std::mutex> guard(status_lock);
        status = "Found " + info.path + " but cannot open it. Is the udev rule installed?";
        return false;
    }

    protocol_version = kbd.GetProtocolVersion();

    if(protocol_version < 0x000B)
    {
        LOG(PLUGIN_LL_WARNING, "[AkkoGem01] %s: unsupported VIA protocol version %u", info.path.c_str(), protocol_version);
        kbd.Close();

        std::lock_guard<std::mutex> guard(status_lock);
        status = "Unsupported or no VIA response from " + info.path;
        return false;
    }

    RGBMatrixState state;

    if(!kbd.GetRGBMatrixState(state))
    {
        LOG(PLUGIN_LL_WARNING, "[AkkoGem01] %s: failed to read RGB matrix state", info.path.c_str());
        kbd.Close();

        std::lock_guard<std::mutex> guard(status_lock);
        status = "Failed to read lighting state from " + info.path;
        return false;
    }

    {
        std::lock_guard<std::mutex> guard(apply_lock);
        sent_effect     = state.effect;
        sent_brightness = state.brightness;
        sent_speed      = state.speed;
        sent_hue        = state.hue;
        sent_saturation = state.saturation;
    }

    RGBController_Setup setup;
    FillSetup(setup, state);

    if(controller == nullptr)
    {
        controller = api->CreateVirtualRGBController(&setup);
    }
    else
    {
        api->UpdateVirtualRGBController(controller, &setup);
    }

    if(controller == nullptr)
    {
        kbd.Close();
        return false;
    }

    controller->SetColor(0, HSVToRGB(state.hue, state.saturation, 255));

    link_lost = false;
    api->RegisterVirtualRGBController(controller);
    registered = true;

    LOG(PLUGIN_LL_INFO, "[AkkoGem01] Connected to %s (VIA protocol %u, effect %u, brightness %u)",
        info.path.c_str(), protocol_version, state.effect, state.brightness);

    char buf[256];
    snprintf(buf, sizeof(buf), "Connected: %s, VIA protocol %u", info.path.c_str(), protocol_version);

    std::lock_guard<std::mutex> guard(status_lock);
    status = buf;
    return true;
}

void AkkoGem01Device::Disconnect(const char* reason)
{
    if(registered && controller)
    {
        api->UnregisterVirtualRGBController(controller);
        registered = false;
        LOG(PLUGIN_LL_INFO, "[AkkoGem01] Disconnected: %s", reason);
    }

    kbd.Close();
    link_lost = false;

    std::lock_guard<std::mutex> guard(status_lock);
    status = std::string("Disconnected: ") + reason;
}

void AkkoGem01Device::FillSetup(RGBController_Setup& setup, const RGBMatrixState& state)
{
    const ViaDeviceInfo& info = kbd.GetInfo();

    setup.name          = "AKKO Gem 01";
    setup.vendor        = "AKKO";
    setup.description   = "AKKO Gem 01 (stock VIA firmware, AkkoGem01 plugin)";
    setup.location      = "HID: " + info.path;
    setup.serial        = info.serial;
    setup.version       = "VIA protocol " + std::to_string(protocol_version);
    setup.configuration = "";
    setup.type          = DEVICE_TYPE_KEYBOARD;
    setup.flags         = CONTROLLER_FLAG_VIRTUAL;
    setup.object_ptr    = this;

    RGBColor current = HSVToRGB(state.hue, state.saturation, 255);

    /*-----------------------------------------------------*\
    | Direct: whole keyboard follows LED 0                  |
    \*-----------------------------------------------------*/
    mode direct;
    direct.name         = "Direct";
    direct.value        = AKKO_MODE_DIRECT;
    direct.flags        = MODE_FLAG_HAS_PER_LED_COLOR;
    direct.color_mode   = MODE_COLORS_PER_LED;
    setup.modes.push_back(direct);

    setup.active_mode = -1;

    for(std::size_t i = 0; i < AKKO_EFFECT_COUNT; i++)
    {
        const AkkoEffect& e = akko_effects[i];

        mode m;
        m.name          = e.name;
        m.value         = e.value;

        if(e.value != 0)
        {
            m.flags            |= MODE_FLAG_HAS_BRIGHTNESS | MODE_FLAG_MANUAL_SAVE;
            m.brightness_min    = 0;
            m.brightness_max    = 255;
            m.brightness        = state.brightness;
        }

        if(EffectHasSpeed(e.value))
        {
            m.flags            |= MODE_FLAG_HAS_SPEED;
            m.speed_min         = 0;
            m.speed_max         = 255;
            m.speed             = state.speed;
        }

        if(EffectHasColor(e.value))
        {
            m.flags            |= MODE_FLAG_HAS_MODE_SPECIFIC_COLOR;
            m.colors_min        = 1;
            m.colors_max        = 1;
            m.color_mode        = MODE_COLORS_MODE_SPECIFIC;
            m.colors.push_back(current);
        }
        else
        {
            m.color_mode        = MODE_COLORS_NONE;
        }

        if(e.value == 0)
        {
            m.flags            |= MODE_FLAG_MANUAL_SAVE;
        }

        if(e.value == state.effect)
        {
            setup.active_mode = (int)setup.modes.size();
        }

        setup.modes.push_back(m);
    }

    if(setup.active_mode < 0)
    {
        setup.active_mode = MODE_IDX_DIRECT;
    }

    zone z;
    z.name          = "Keyboard";
    z.type          = ZONE_TYPE_SINGLE;
    z.leds_min      = 1;
    z.leds_max      = 1;
    z.leds_count    = 1;
    setup.zones.push_back(z);

    led l;
    l.name          = "Keyboard";
    l.value         = 0;
    setup.leds.push_back(l);

    setup.DeviceConfigureZone                           = nullptr;
    setup.DeviceUpdateLEDs                              = &AkkoGem01Device::CbUpdateLEDs;
    setup.DeviceUpdateZoneLEDs                          = &AkkoGem01Device::CbUpdateZoneLEDs;
    setup.DeviceUpdateSingleLED                         = &AkkoGem01Device::CbUpdateSingleLED;
    setup.DeviceUpdateMode                              = &AkkoGem01Device::CbUpdateMode;
    setup.DeviceSaveMode                                = &AkkoGem01Device::CbSaveMode;
    setup.DeviceUpdateZoneMode                          = nullptr;
    setup.DeviceUpdateDeviceSpecificConfiguration       = nullptr;
    setup.DeviceUpdateDeviceSpecificZoneConfiguration   = nullptr;
}

/*---------------------------------------------------------*\
| Callbacks                                                 |
\*---------------------------------------------------------*/
void AkkoGem01Device::CbUpdateLEDs(void* obj)
{
    static_cast<AkkoGem01Device*>(obj)->ApplyDirectColor();
}

void AkkoGem01Device::CbUpdateZoneLEDs(void* obj, int /*zone*/)
{
    static_cast<AkkoGem01Device*>(obj)->ApplyDirectColor();
}

void AkkoGem01Device::CbUpdateSingleLED(void* obj, int /*led*/)
{
    static_cast<AkkoGem01Device*>(obj)->ApplyDirectColor();
}

void AkkoGem01Device::CbUpdateMode(void* obj)
{
    static_cast<AkkoGem01Device*>(obj)->ApplyMode();
}

void AkkoGem01Device::CbSaveMode(void* obj)
{
    static_cast<AkkoGem01Device*>(obj)->SaveMode();
}

void AkkoGem01Device::CheckLink(bool ok)
{
    if(!ok && kbd.IsOpen())
    {
        /*-------------------------------------------------*\
        | A single unanswered command is not fatal; the     |
        | worker re-checks presence. Ask it to look now.    |
        \*-------------------------------------------------*/
        if(kbd.GetProtocolVersion() == 0)
        {
            link_lost = true;
            worker_cv.notify_all();
        }
    }
}

void AkkoGem01Device::ApplyMode()
{
    if(controller == nullptr || !kbd.IsOpen())
    {
        return;
    }

    int mode_idx = controller->GetActiveMode();

    if(mode_idx == MODE_IDX_DIRECT)
    {
        {
            std::lock_guard<std::mutex> guard(apply_lock);
            sent_effect = -1; /* force full resend */
        }
        ApplyDirectColor();
        return;
    }

    if(mode_idx < 1 || mode_idx > (int)AKKO_EFFECT_COUNT)
    {
        return;
    }

    const AkkoEffect& e   = akko_effects[mode_idx - 1];
    unsigned int      flg = controller->GetModeFlags(mode_idx);
    bool              ok  = true;

    std::lock_guard<std::mutex> guard(apply_lock);

    /*-----------------------------------------------------*\
    | User-initiated: always send everything, the keyboard  |
    | may have been changed with Fn shortcuts meanwhile     |
    \*-----------------------------------------------------*/
    ok = ok && kbd.SetEffect(e.value);
    sent_effect = e.value;

    if(flg & MODE_FLAG_HAS_MODE_SPECIFIC_COLOR)
    {
        uint8_t h, s, v;
        RGBToHSV(controller->GetModeColor(mode_idx, 0), h, s, v);

        ok = ok && kbd.SetColor(h, s);
        sent_hue        = h;
        sent_saturation = s;
    }

    if(flg & MODE_FLAG_HAS_SPEED)
    {
        uint8_t speed = (uint8_t)std::min(255u, controller->GetModeSpeed(mode_idx));
        ok = ok && kbd.SetSpeed(speed);
        sent_speed = speed;
    }

    if(flg & MODE_FLAG_HAS_BRIGHTNESS)
    {
        uint8_t brightness = (uint8_t)std::min(255u, controller->GetModeBrightness(mode_idx));
        ok = ok && kbd.SetBrightness(brightness);
        sent_brightness = brightness;
    }

    CheckLink(ok);
}

void AkkoGem01Device::ApplyDirectColor()
{
    if(controller == nullptr || !kbd.IsOpen())
    {
        return;
    }

    if(controller->GetActiveMode() != MODE_IDX_DIRECT)
    {
        return;
    }

    uint8_t h, s, v;
    RGBToHSV(controller->GetColor(0), h, s, v);

    std::lock_guard<std::mutex> guard(apply_lock);

    /*-----------------------------------------------------*\
    | Periodically resend everything in case the keyboard   |
    | was changed with Fn shortcuts                         |
    \*-----------------------------------------------------*/
    auto now = std::chrono::steady_clock::now();

    if(now - last_full_send > std::chrono::seconds(2))
    {
        sent_effect     = -1;
        sent_hue        = -1;
        sent_brightness = -1;
        last_full_send  = now;
    }

    bool ok = true;

    if(sent_effect != AKKO_EFFECT_SOLID)
    {
        ok = ok && kbd.SetEffect(AKKO_EFFECT_SOLID);
        if(ok) sent_effect = AKKO_EFFECT_SOLID;
    }

    /*-----------------------------------------------------*\
    | Hue is meaningless for black/white, keep the old one  |
    \*-----------------------------------------------------*/
    if(v > 0 && (sent_hue != h || sent_saturation != s))
    {
        ok = ok && kbd.SetColor(h, s);
        if(ok) { sent_hue = h; sent_saturation = s; }
    }

    if(sent_brightness != v)
    {
        ok = ok && kbd.SetBrightness(v);
        if(ok) sent_brightness = v;
    }

    CheckLink(ok);
}

void AkkoGem01Device::SaveMode()
{
    if(!kbd.IsOpen())
    {
        return;
    }

    /*-----------------------------------------------------*\
    | VIA setters are non-persistent, this writes the       |
    | current RGB matrix config to the keyboard's EEPROM    |
    \*-----------------------------------------------------*/
    ApplyMode();
    CheckLink(kbd.Save());
}
