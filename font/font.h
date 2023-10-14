#ifndef __FONT_H__
#define __FONT_H__

#include "../vulkan/vulkan.h"

float Font_CharacterBaseWidth(const char ch);
float Font_StringBaseWidth(const char *string);
void Font_Print(float size, float x, float y, char *string, ...);
void Font_Draw(uint32_t Index);
void Font_Destroy(void);

#endif
