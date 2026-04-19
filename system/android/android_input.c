#include <stddef.h>
#include "../../input/input.h"

void Input_Platform_Update(void)
{
	Input_t *input=Input_GetState();
	input->gamepad.connected=false;
}

void Input_PlatformInit(void)
{
}

void Input_PlatformDestroy(void)
{
}
