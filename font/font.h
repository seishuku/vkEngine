#ifndef __FONT_H__
#define __FONT_H__

void Font_Print(VkCommandBuffer CommandBuffer, uint32_t Eye, float x, float y, char *string, ...);
void Font_Destroy(void);

#endif
