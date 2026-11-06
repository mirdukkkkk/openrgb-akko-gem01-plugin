/*---------------------------------------------------------*\
| akko_via_cli.cpp                                          |
|                                                           |
|   Tiny command line tool for testing the VIA lighting     |
|   protocol layer used by the plugin.                      |
|                                                           |
|   akko_via_cli get                                        |
|   akko_via_cli effect <0-45>                              |
|   akko_via_cli brightness <0-255>                         |
|   akko_via_cli speed <0-255>                              |
|   akko_via_cli color <hue 0-255> <sat 0-255>              |
|   akko_via_cli save                                       |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ViaKeyboard.h"

int main(int argc, char** argv)
{
    if(argc < 2)
    {
        fprintf(stderr, "usage: %s get|effect N|brightness N|speed N|color H S|save\n", argv[0]);
        return 2;
    }

    hid_init();

    auto devs = ViaKeyboard::Enumerate(0x342D, 0xE4E3);

    if(devs.empty())
    {
        fprintf(stderr, "AKKO Gem 01 raw HID interface not found\n");
        return 1;
    }

    ViaKeyboard kbd;

    if(!kbd.Open(devs[0]))
    {
        fprintf(stderr, "cannot open %s (permissions? install udev/60-akko-gem01.rules)\n", devs[0].path.c_str());
        return 1;
    }

    const char* cmd = argv[1];
    bool ok = false;

    if(!strcmp(cmd, "get"))
    {
        RGBMatrixState s;
        uint16_t ver = kbd.GetProtocolVersion();
        ok = ver != 0 && kbd.GetRGBMatrixState(s);

        if(ok)
        {
            printf("path:       %s\nproduct:    %s %s\nprotocol:   %u\neffect:     %u\nbrightness: %u\nspeed:      %u\nhue/sat:    %u/%u\n",
                   devs[0].path.c_str(), devs[0].manufacturer.c_str(), devs[0].product.c_str(),
                   ver, s.effect, s.brightness, s.speed, s.hue, s.saturation);
        }
    }
    else if(!strcmp(cmd, "effect") && argc > 2)     ok = kbd.SetEffect((uint8_t)atoi(argv[2]));
    else if(!strcmp(cmd, "brightness") && argc > 2) ok = kbd.SetBrightness((uint8_t)atoi(argv[2]));
    else if(!strcmp(cmd, "speed") && argc > 2)      ok = kbd.SetSpeed((uint8_t)atoi(argv[2]));
    else if(!strcmp(cmd, "color") && argc > 3)      ok = kbd.SetColor((uint8_t)atoi(argv[2]), (uint8_t)atoi(argv[3]));
    else if(!strcmp(cmd, "save"))                   ok = kbd.Save();
    else
    {
        fprintf(stderr, "unknown command\n");
        return 2;
    }

    if(!ok)
    {
        fprintf(stderr, "command failed\n");
        return 1;
    }

    return 0;
}
