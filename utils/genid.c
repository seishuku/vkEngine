#include <stdint.h>

// Super simple ID generator.
// Minor issue: Where I use this, I also use UINT32_MAX as an invalid ID,
// which this can potentially generate... Though unlikely, so I'm not too worried about it.

uint32_t GenID(void)
{
    static uint32_t Static_ID=0;
    return Static_ID++;
}
