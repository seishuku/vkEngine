#ifndef __IMAGE_H__
#define __IMAGE_H__

// Special image format 'fourcc'
#define IMAGE_DXT1										('D'|('X'<<8)|('T'<<16)|('1'<<24))
#define IMAGE_DXT3										('D'|('X'<<8)|('T'<<16)|('3'<<24))
#define IMAGE_DXT5										('D'|('X'<<8)|('T'<<16)|('5'<<24))

// Image flags
#define IMAGE_NONE										0x00000000
#define IMAGE_MIPMAP									0x00000002
#define IMAGE_NEAREST									0x00000004
#define IMAGE_BILINEAR									0x00000008
#define IMAGE_TRILINEAR									0x00000010
#define IMAGE_NORMALMAP									0x00000020
#define IMAGE_NORMALIZE									0x00000040
#define IMAGE_RGBE										0x00000080
#define IMAGE_CUBEMAP_ANGULAR							0x00000100
#define IMAGE_CLAMP_U									0x00000400
#define IMAGE_CLAMP_V									0x00000800
#define IMAGE_CLAMP										(IMAGE_CLAMP_U|IMAGE_CLAMP_V)
#define IMAGE_REPEAT_U									0x00004000
#define IMAGE_REPEAT_V									0x00008000
#define IMAGE_REPEAT									(IMAGE_REPEAT_U|IMAGE_REPEAT_V)

#include <vulkan/vulkan.h>
#include <sys/types.h>

typedef struct
{
	uint32_t Width, Height, Depth;
	uint8_t *Data;
	uint32_t mipLevels;

	VkSampler sampler;
	VkImage image;
	VkImageLayout imageLayout;
	VkDeviceMemory deviceMemory;
	VkImageView view;
} Image_t;

bool DDS_Load(const char *Filename, Image_t *Image);
bool TGA_Load(const char *Filename, Image_t *Image);
bool TGA_Write(const char *filename, Image_t *Image, bool rle);
bool QOI_Load(const char *Filename, Image_t *Image);

unsigned int Image_Upload(Image_t *Image, char *Filename, unsigned long Flags);

#endif
