/*---------------------------------------------------------*\
| ViaKeyboard.cpp                                           |
|                                                           |
|   Minimal VIA (QMK raw HID) protocol client used to       |
|   drive the RGB Matrix lighting channel of a keyboard.    |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include "ViaKeyboard.h"

#include <chrono>
#include <cstring>
#include <cwchar>

static std::string WideToString(const wchar_t* w)
{
    if(w == nullptr)
    {
        return "";
    }

    std::string out;
    std::mbstate_t state{};
    char buf[8];

    for(; *w; w++)
    {
        std::size_t n = std::wcrtomb(buf, *w, &state);

        if(n == static_cast<std::size_t>(-1))
        {
            out += '?';
        }
        else
        {
            out.append(buf, n);
        }
    }

    return out;
}

ViaKeyboard::ViaKeyboard() : dev(nullptr), consecutive_failures(0)
{
}

ViaKeyboard::~ViaKeyboard()
{
    Close();
}

std::vector<ViaDeviceInfo> ViaKeyboard::Enumerate(uint16_t vid, uint16_t pid)
{
    std::vector<ViaDeviceInfo> result;

    hid_device_info* devs = hid_enumerate(vid, pid);

    for(hid_device_info* cur = devs; cur != nullptr; cur = cur->next)
    {
        if(cur->usage_page != VIA_RAW_HID_USAGE_PAGE || cur->usage != VIA_RAW_HID_USAGE)
        {
            continue;
        }

        ViaDeviceInfo info;
        info.path           = cur->path ? cur->path : "";
        info.serial         = WideToString(cur->serial_number);
        info.manufacturer   = WideToString(cur->manufacturer_string);
        info.product        = WideToString(cur->product_string);
        info.vid            = cur->vendor_id;
        info.pid            = cur->product_id;

        result.push_back(info);
    }

    hid_free_enumeration(devs);

    return result;
}

bool ViaKeyboard::Open(const ViaDeviceInfo& new_info)
{
    std::lock_guard<std::mutex> guard(lock);

    if(dev)
    {
        hid_close(dev);
        dev = nullptr;
    }

    dev = hid_open_path(new_info.path.c_str());

    if(dev == nullptr)
    {
        return false;
    }

    info                    = new_info;
    consecutive_failures    = 0;

    /*-----------------------------------------------------*\
    | Throw away anything already queued on this handle     |
    \*-----------------------------------------------------*/
    uint8_t junk[64];
    while(hid_read_timeout(dev, junk, sizeof(junk), 0) > 0)
    {
    }

    return true;
}

void ViaKeyboard::Close()
{
    std::lock_guard<std::mutex> guard(lock);

    if(dev)
    {
        hid_close(dev);
        dev = nullptr;
    }
}

bool ViaKeyboard::IsOpen()
{
    std::lock_guard<std::mutex> guard(lock);

    return dev != nullptr;
}

/*---------------------------------------------------------*\
| Send one request and wait for the matching response.      |
|                                                           |
| QMK/VIA echoes the request header in its response. Other  |
| programs that have the same hidraw node open (the VIA web |
| app, another OpenRGB instance...) receive every input     |
| report too, so we must not assume that the next report is |
| ours: anything that does not echo our header is skipped.  |
\*---------------------------------------------------------*/
bool ViaKeyboard::Transact(const std::vector<uint8_t>& request, std::size_t match_len, uint8_t* response)
{
    if(dev == nullptr || request.empty() || request.size() > VIA_REPORT_SIZE)
    {
        return false;
    }

    uint8_t out[VIA_REPORT_SIZE + 1];
    memset(out, 0, sizeof(out));
    out[0] = 0x00; /* report ID, raw HID uses unnumbered reports */
    memcpy(&out[1], request.data(), request.size());

    for(int attempt = 0; attempt < 2; attempt++)
    {
        if(hid_write(dev, out, sizeof(out)) < 0)
        {
            consecutive_failures++;
            return false;
        }

        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);

        while(true)
        {
            auto now = std::chrono::steady_clock::now();

            if(now >= deadline)
            {
                break;
            }

            int timeout_ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();

            uint8_t in[64];
            int     len = hid_read_timeout(dev, in, sizeof(in), timeout_ms < 1 ? 1 : timeout_ms);

            if(len < 0)
            {
                consecutive_failures++;
                return false;
            }

            if(len == 0)
            {
                break;
            }

            if(in[0] == VIA_ID_UNHANDLED)
            {
                /*-----------------------------------------*\
                | Firmware does not support this command    |
                \*-----------------------------------------*/
                consecutive_failures = 0;
                return false;
            }

            if((std::size_t)len >= match_len && memcmp(in, request.data(), match_len) == 0)
            {
                if(response)
                {
                    memcpy(response, in, VIA_REPORT_SIZE);
                }

                consecutive_failures = 0;
                return true;
            }
        }
    }

    consecutive_failures++;
    return false;
}

uint16_t ViaKeyboard::GetProtocolVersion()
{
    std::lock_guard<std::mutex> guard(lock);

    uint8_t resp[VIA_REPORT_SIZE];

    if(!Transact({VIA_ID_GET_PROTOCOL_VERSION}, 1, resp))
    {
        return 0;
    }

    return (uint16_t)((resp[1] << 8) | resp[2]);
}

bool ViaKeyboard::GetValue(uint8_t value_id, uint8_t* out, std::size_t out_len)
{
    std::lock_guard<std::mutex> guard(lock);

    uint8_t resp[VIA_REPORT_SIZE];

    if(!Transact({VIA_ID_CUSTOM_GET_VALUE, VIA_CHANNEL_RGB_MATRIX, value_id}, 3, resp))
    {
        return false;
    }

    memcpy(out, &resp[3], out_len);
    return true;
}

bool ViaKeyboard::SetValue(uint8_t value_id, const std::vector<uint8_t>& data)
{
    std::lock_guard<std::mutex> guard(lock);

    std::vector<uint8_t> req = {VIA_ID_CUSTOM_SET_VALUE, VIA_CHANNEL_RGB_MATRIX, value_id};
    req.insert(req.end(), data.begin(), data.end());

    return Transact(req, 3, nullptr);
}

bool ViaKeyboard::GetRGBMatrixState(RGBMatrixState& state)
{
    uint8_t color[2];

    if(!GetValue(VIA_RGB_MATRIX_BRIGHTNESS,   &state.brightness, 1)) return false;
    if(!GetValue(VIA_RGB_MATRIX_EFFECT,       &state.effect,     1)) return false;
    if(!GetValue(VIA_RGB_MATRIX_EFFECT_SPEED, &state.speed,      1)) return false;
    if(!GetValue(VIA_RGB_MATRIX_COLOR,        color,             2)) return false;

    state.hue           = color[0];
    state.saturation    = color[1];

    return true;
}

bool ViaKeyboard::SetBrightness(uint8_t brightness)
{
    return SetValue(VIA_RGB_MATRIX_BRIGHTNESS, {brightness});
}

bool ViaKeyboard::SetEffect(uint8_t effect)
{
    return SetValue(VIA_RGB_MATRIX_EFFECT, {effect});
}

bool ViaKeyboard::SetSpeed(uint8_t speed)
{
    return SetValue(VIA_RGB_MATRIX_EFFECT_SPEED, {speed});
}

bool ViaKeyboard::SetColor(uint8_t hue, uint8_t saturation)
{
    return SetValue(VIA_RGB_MATRIX_COLOR, {hue, saturation});
}

bool ViaKeyboard::Save()
{
    std::lock_guard<std::mutex> guard(lock);

    return Transact({VIA_ID_CUSTOM_SAVE, VIA_CHANNEL_RGB_MATRIX}, 2, nullptr);
}
