#ifndef __FONT_H__
#define __FONT_H__

void Font_Print(VkCommandBuffer cmd, float x, float y, char *string, ...);
void Font_Destroy(void);

#endif
