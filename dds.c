#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <malloc.h>
#include "image.h"

#ifndef FREE
#define FREE(p) { if(p) { free(p); p=NULL; } }
#endif

#define DDS_FOURCC 0x00000004
#define DDS_RGB 0x00000040
#define DDS_RGBA 0x00000041
#define DDS_MAGIC ('D'|('D'<<8)|('S'<<16)|(' '<<24))
#define DDS_DXT1 ('D'|('X'<<8)|('T'<<16)|('1'<<24))
#define DDS_DXT3 ('D'|('X'<<8)|('T'<<16)|('3'<<24))
#define DDS_DXT5 ('D'|('X'<<8)|('T'<<16)|('5'<<24))

typedef struct
{
	uint16_t col0, col1;
	uint8_t row[4];
} DXTColorBlock_t;

typedef struct
{
	uint16_t row[4];
} DXT3AlphaBlock_t;

typedef struct
{
	uint8_t alpha0, alpha1;
	uint8_t row[6];
} DXT5AlphaBlock_t;

typedef struct
{
	uint32_t Size;
	uint32_t Flags;
	uint32_t Height;
	uint32_t Width;
	uint32_t PitchLinearSize;
	uint32_t Depth;
	uint32_t MipMapCount;
	uint32_t Reserved1[11];
	uint32_t pfSize;
	uint32_t pfFlags;
	uint32_t pfFourCC;
	uint32_t pfRGBBitCount;
	uint32_t pfRMask;
	uint32_t pfGMask;
	uint32_t pfBMask;
	uint32_t pfAMask;
	uint32_t Caps1;
	uint32_t Caps2;
	uint32_t Reserved2[3];
} DDS_Header_t;

void Swap(void *byte1, void *byte2, int32_t size)
{
	uint8_t *tmp=(uint8_t *)malloc(sizeof(uint8_t)*size);

	memcpy(tmp, byte1, size);
	memcpy(byte1, byte2, size);
	memcpy(byte2, tmp, size);

	FREE(tmp);
}

void FlipDXT1Blocks(DXTColorBlock_t *Block, int32_t NumBlocks)
{
	int32_t i;
	DXTColorBlock_t *ColorBlock=Block;

	for(i=0;i<NumBlocks;i++)
	{
		Swap(&ColorBlock->row[0], &ColorBlock->row[3], sizeof(uint8_t));
		Swap(&ColorBlock->row[1], &ColorBlock->row[2], sizeof(uint8_t));
		ColorBlock++;
	}
}

void FlipDXT3Blocks(DXTColorBlock_t *Block, int32_t NumBlocks)
{
	int32_t i;
	DXTColorBlock_t *ColorBlock=Block;
	DXT3AlphaBlock_t *AlphaBlock;

	for(i=0;i<NumBlocks;i++)
	{
		AlphaBlock=(DXT3AlphaBlock_t *)ColorBlock;

		Swap(&AlphaBlock->row[0], &AlphaBlock->row[3], sizeof(uint16_t));
		Swap(&AlphaBlock->row[1], &AlphaBlock->row[2], sizeof(uint16_t));
		ColorBlock++;

		Swap(&ColorBlock->row[0], &ColorBlock->row[3], sizeof(uint8_t));
		Swap(&ColorBlock->row[1], &ColorBlock->row[2], sizeof(uint8_t));
		ColorBlock++;
	}
}

void FlipDXT5Alpha(DXT5AlphaBlock_t *Block)
{
	uint32_t *Bits, Bits0=0, Bits1=0;

	memcpy(&Bits0, &Block->row[0], sizeof(uint8_t)*3);
	memcpy(&Bits1, &Block->row[3], sizeof(uint8_t)*3);

	Bits=((uint32_t *)&(Block->row[0]));
	*Bits&=0xff000000;
	*Bits|=(uint8_t)(Bits1>>12)&0x00000007;
	*Bits|=(uint8_t)((Bits1>>15)&0x00000007)<<3;
	*Bits|=(uint8_t)((Bits1>>18)&0x00000007)<<6;
	*Bits|=(uint8_t)((Bits1>>21)&0x00000007)<<9;
	*Bits|=(uint8_t)(Bits1&0x00000007)<<12;
	*Bits|=(uint8_t)((Bits1>>3)&0x00000007)<<15;
	*Bits|=(uint8_t)((Bits1>>6)&0x00000007)<<18;
	*Bits|=(uint8_t)((Bits1>>9)&0x00000007)<<21;

	Bits=((uint32_t *)&(Block->row[3]));
	*Bits&=0xff000000;
	*Bits|=(uint8_t)(Bits0>>12)&0x00000007;
	*Bits|=(uint8_t)((Bits0>>15)&0x00000007)<<3;
	*Bits|=(uint8_t)((Bits0>>18)&0x00000007)<<6;
	*Bits|=(uint8_t)((Bits0>>21)&0x00000007)<<9;
	*Bits|=(uint8_t)(Bits0&0x00000007)<<12;
	*Bits|=(uint8_t)((Bits0>>3)&0x00000007)<<15;
	*Bits|=(uint8_t)((Bits0>>6)&0x00000007)<<18;
	*Bits|=(uint8_t)((Bits0>>9)&0x00000007)<<21;
}

void FlipDXT5Blocks(DXTColorBlock_t *Block, int32_t NumBlocks)
{
	DXTColorBlock_t *ColorBlock=Block;
	DXT5AlphaBlock_t *AlphaBlock;
	int32_t i;

	for(i=0;i<NumBlocks;i++)
	{
		AlphaBlock=(DXT5AlphaBlock_t *)ColorBlock;

		FlipDXT5Alpha(AlphaBlock);
		ColorBlock++;

		Swap(&ColorBlock->row[0], &ColorBlock->row[3], sizeof(uint8_t));
		Swap(&ColorBlock->row[1], &ColorBlock->row[2], sizeof(uint8_t));
		ColorBlock++;
	}
}

void Flip(uint8_t *image, int32_t width, int32_t height, int32_t size, int32_t format)
{
	int32_t linesize, i, j;

	if((format==32)||(format==24))
	{
		uint8_t *top, *bottom;

		linesize=size/height;

		top=image;
		bottom=top+(size-linesize);

		for(i=0;i<(height>>1);i++)
		{
			Swap(bottom, top, linesize);

			top+=linesize;
			bottom-=linesize;
		}
	}
	else
	{
		DXTColorBlock_t *top;
		DXTColorBlock_t *bottom;
		int32_t xblocks=width/4;
		int32_t yblocks=height/4;

		switch(format)
		{
			case IMAGE_DXT1:
				linesize=xblocks*8;

				for(j=0;j<(yblocks>>1);j++)
				{
					top=(DXTColorBlock_t *)(image+j*linesize);
					bottom=(DXTColorBlock_t *)(image+(((yblocks-j)-1)*linesize));

					FlipDXT1Blocks(top, xblocks);
					FlipDXT1Blocks(bottom, xblocks);
					Swap(bottom, top, linesize);
				}
				break;

			case IMAGE_DXT3:
				linesize=xblocks*16;

				for(j=0;j<(yblocks>>1);j++)
				{
					top=(DXTColorBlock_t *)(image+j*linesize);
					bottom=(DXTColorBlock_t *)(image+(((yblocks-j)-1)*linesize));

					FlipDXT3Blocks(top, xblocks);
					FlipDXT3Blocks(bottom, xblocks);
					Swap(bottom, top, linesize);
				}
				break;

			case IMAGE_DXT5:
				linesize=xblocks*16;

				for(j=0;j<(yblocks>>1);j++)
				{
					top=(DXTColorBlock_t *)(image+j*linesize);
					bottom=(DXTColorBlock_t *)(image+(((yblocks-j)-1)*linesize));

					FlipDXT5Blocks(top, xblocks);
					FlipDXT5Blocks(bottom, xblocks);
					Swap(bottom, top, linesize);
				}
				break;

			default:
				return;
		}
	}
}

bool DDS_Load(const char *Filename, Image_t *Image)
{
	DDS_Header_t dds;
	uint32_t magic;
	FILE *stream;
	int32_t size;

	if((stream=fopen(Filename, "rb"))==NULL)
		return false;

	fread(&magic, sizeof(uint32_t), 1, stream);

	if(magic!=DDS_MAGIC)
	{
		fclose(stream);
		return false;
	}

	fread(&dds, sizeof(DDS_Header_t), 1, stream);

	if(dds.pfFlags&DDS_FOURCC)
	{
		switch(dds.pfFourCC)
		{
			case DDS_DXT1:
				Image->Depth=IMAGE_DXT1;
				break;

			case DDS_DXT3:
				Image->Depth=IMAGE_DXT3;
				break;

			case DDS_DXT5:
				Image->Depth=IMAGE_DXT5;
				break;

			default:
				fclose(stream);
				return false;
		}
	}
	else
	{
		if(dds.pfFlags==DDS_RGBA&&dds.pfRGBBitCount==32)
			Image->Depth=32;
		else
		{
			if(dds.pfFlags==DDS_RGB&&dds.pfRGBBitCount==24)
				Image->Depth=24;
			else
			{
				fclose(stream);
				return false;
			}
		}
	}

	Image->Width=dds.Width;
	Image->Height=dds.Height;

	if((Image->Depth==32)||(Image->Depth==24))
		size=Image->Width*Image->Height*(Image->Depth>>3);
	else
	{
		if(Image->Depth==IMAGE_DXT1)
			size=((Image->Width+3)>>2)*((Image->Height+3)>>2)*8;
		else
			size=((Image->Width+3)>>2)*((Image->Height+3)>>2)*16;
	}

	Image->Data=(uint8_t *)malloc(sizeof(uint8_t)*size);

	if(Image->Data==NULL)
		return false;

	fread(Image->Data, sizeof(uint8_t), size, stream);
	fclose(stream);

	Flip(Image->Data, Image->Width, Image->Height, size, Image->Depth);

	return true;
}
