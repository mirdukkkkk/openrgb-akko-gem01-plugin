/*---------------------------------------------------------*\
| ViaKeyboard.h                                             |
|                                                           |
|   Minimal VIA (QMK raw HID) protocol client used to       |
|   drive the RGB Matrix lighting channel of a keyboard.    |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include <hidapi.h>

/*---------------------------------------------------------*\
| VIA protocol constants (QMK quantum/via.h)                |
\*---------------------------------------------------------*/
enum
{
    VIA_ID_GET_PROTOCOL_VERSION     = 0x01,
    VIA_ID_CUSTOM_SET_VALUE         = 0x07,
    VIA_ID_CUSTOM_GET_VALUE         = 0x08,
    VIA_ID_CUSTOM_SAVE              = 0x09,
    VIA_ID_UNHANDLED                = 0xFF,
};

enum
{
    VIA_CHANNEL_RGB_MATRIX          = 0x03,
};

enum
{
    VIA_RGB_MATRIX_BRIGHTNESS       = 0x01,
    VIA_RGB_MATRIX_EFFECT           = 0x02,
    VIA_RGB_MATRIX_EFFECT_SPEED     = 0x03,
    VIA_RGB_MATRIX_COLOR            = 0x04,
};

#define VIA_RAW_HID_USAGE_PAGE      0xFF60
#define VIA_RAW_HID_USAGE           0x61
#define VIA_REPORT_SIZE             32

struct ViaDeviceInfo
{
    std::string     path;
    std::string     serial;
    std::string     manufacturer;
    std::string     product;
    uint16_t        vid;
    uint16_t        pid;
};

struct RGBMatrixState
{
    uint8_t         brightness;
    uint8_t         effect;
    uint8_t         speed;
    uint8_t         hue;
    uint8_t         saturation;
};

class ViaKeyboard
{
public:
    ViaKeyboard();
    ~ViaKeyboard();

    static std::vector<ViaDeviceInfo> Enumerate(uint16_t vid, uint16_t pid);

    bool            Open(const ViaDeviceInfo& info);
    void            Close();
    bool            IsOpen();
    const ViaDeviceInfo& GetInfo() const { return info; }

    /*-----------------------------------------------------*\
    | Returns 0 on failure                                  |
    \*-----------------------------------------------------*/
    uint16_t        GetProtocolVersion();

    bool            GetRGBMatrixState(RGBMatrixState& state);

    bool            SetBrightness(uint8_t brightness);
    bool            SetEffect(uint8_t effect);
    bool            SetSpeed(uint8_t speed);
    bool            SetColor(uint8_t hue, uint8_t saturation);
    bool            Save();

private:
    bool            Transact(const std::vector<uint8_t>& request, std::size_t match_len, uint8_t* response);
    bool            GetValue(uint8_t value_id, uint8_t* out, std::size_t out_len);
    bool            SetValue(uint8_t value_id, const std::vector<uint8_t>& data);

    std::mutex      lock;
    hid_device*     dev;
    ViaDeviceInfo   info;
    unsigned int    consecutive_failures;
};
