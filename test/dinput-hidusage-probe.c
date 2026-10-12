/* Table check of dlls/dinput/hid_usage_type.h (patch 2879): the DirectInput type
 * of every top level HID usage. Built with the header from the wine source tree. */
#include <stdio.h>
#include <windows.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include "hid_usage_type.h"

static int failures, checks;

#define T(p, u, e) { p, u, e }
static const struct { unsigned short page, usage; unsigned long expect; const char *name; } tests[] =
{
    /* existing behaviour: gamepad and joystick, mouse/keyboard and pointer are not mapped here */
    {0x01, 0x04, DI8DEVTYPE_JOYSTICK, "joystick"},
    {0x01, 0x05, DI8DEVTYPE_GAMEPAD, "gamepad"},
    {0x01, 0x02, 0, "mouse"},
    {0x01, 0x06, 0, "keyboard"},
    {0x01, 0x01, 0, "pointer"},
    {0x01, 0x07, 0, "keypad"},
    {0x01, 0x00, 0, "undefined"},
    /* multi-axis controller */
    {0x01, 0x08, DI8DEVTYPE_1STPERSON | (DI8DEVTYPE1STPERSON_SIXDOF << 8), "multi-axis"},
    /* simulation controls */
    {0x02, 0x01, DI8DEVTYPE_FLIGHT, "flight simulation"},
    {0x02, 0x02, DI8DEVTYPE_DRIVING, "automobile simulation"},
    {0x02, 0x03, DI8DEVTYPE_DRIVING, "tank"},
    {0x02, 0x04, DI8DEVTYPE_FLIGHT, "spaceship"},
    {0x02, 0x05, DI8DEVTYPE_DRIVING, "submarine"},
    {0x02, 0x06, DI8DEVTYPE_DRIVING, "sailing"},
    {0x02, 0x07, DI8DEVTYPE_DRIVING, "motorcycle"},
    {0x02, 0x08, DI8DEVTYPE_JOYSTICK, "sports"},
    {0x02, 0x09, DI8DEVTYPE_FLIGHT, "airplane"},
    {0x02, 0x0a, DI8DEVTYPE_FLIGHT, "helicopter"},
    {0x02, 0x0b, DI8DEVTYPE_FLIGHT, "magic carpet"},
    {0x02, 0x0c, DI8DEVTYPE_DRIVING, "bicycle"},
    {0x02, 0x0d, 0, "simulation 0x0d"},
    {0x02, 0xc8, 0, "steering as top level"},
    /* game controls */
    {0x05, 0x01, DI8DEVTYPE_1STPERSON | (DI8DEVTYPE1STPERSON_UNKNOWN << 8), "3D game controller"},
    {0x05, 0x02, DI8DEVTYPE_JOYSTICK, "pinball"},
    {0x05, 0x03, DI8DEVTYPE_SCREENPOINTER | (DI8DEVTYPESCREENPTR_LIGHTGUN << 8), "gun"},
    {0x05, 0x04, 0, "game 0x04"},
    /* never game controllers */
    {0x0c, 0x01, 0, "consumer control"},
    {0x0d, 0x04, 0, "touch screen"},
    {0xff00, 0x01, 0, "vendor page"},
    {0x00, 0x04, 0, "page 0"},
    {0x09, 0x04, 0, "button page"},
};

int main(void)
{
    unsigned int i;
    for (i = 0; i < sizeof(tests) / sizeof(*tests); i++)
    {
        unsigned long got = hid_joystick_usage_type( tests[i].page, tests[i].usage );
        checks++;
        if (got != tests[i].expect)
        {
            failures++;
            printf( "FAIL  %04x:%04x %s: type %#lx, expected %#lx\n", tests[i].page, tests[i].usage,
                    tests[i].name, got, tests[i].expect );
        }
    }
    /* the DI8 device type is the low byte, the subtype the next byte */
    checks++;
    if (GET_DIDEVICE_TYPE( hid_joystick_usage_type( 0x02, 0x02 ) ) != DI8DEVTYPE_DRIVING) { failures++; printf( "FAIL  driving type mask\n" ); }
    printf( "%d checks, %d failures\n", checks, failures );
    printf( "%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS" );
    return failures != 0;
}
