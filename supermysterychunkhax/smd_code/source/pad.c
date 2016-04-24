#include "pad.h"

u32 padWait()
{
    u32 pad_state_old = HID_STATE;
    while(true)
    {
        svcSleepThread(1000*1000);
        u32 pad_state = HID_STATE;
        if (pad_state ^ pad_state_old)
            return ~pad_state;
    }
}

u32 padGet()
{
    u32 pad_state = HID_STATE;
    return ~pad_state;
}
