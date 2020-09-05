#include <stdio.h>
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
	unsigned short col0, col1;
	unsigned char row[4];
} DXTColorBlock_t;

typedef struct
{
	unsigned short row[4];
} DXT3AlphaBlock_t;

typedef struct
{
	unsigned char alpha0, alpha1;
	unsigned char row[6];
} DXT5AlphaBlock_t;

typedef struct
{
	unsigned long Size;
	unsigned long Flags;
	unsigned long Height;
	unsigned long Width;
	unsigned long PitchLinearSize;
	unsigned long Depth;
	unsigned long MipMapCount;
	unsigned long Reserved1[11];
	unsigned long pfSize;
	unsigned long pfFlags;
	unsigned long pfFourCC;
	unsigned long pfRGBBitCount;
	unsigned long pfRMask;
	unsigned long pfGMask;
	unsigned long pfBMask;
	unsigned long pfAMask;
	unsigned long Caps1;
	unsigned long Caps2;
	unsigned long Reserved2[3];
} DDS_Header_t;

void Swap(void *byte1, void *byte2, int size)
{
	unsigned char *tmp=(unsigned char *)malloc(sizeof(unsigned char)*size);

	memcpy(tmp, byte1, size);
	memcpy(byte1, byte2, size);
	memcpy(byte2, tmp, size);

	FREE(tmp);
}

void FlipDXT1Blocks(DXTColorBlock_t *Block, int NumBlocks)
{
	int i;
	DXTColorBlock_t *ColorBlock=Block;

	for(i=0;i<NumBlocks;i++)
	{
		Swap(&ColorBlock->row[0], &ColorBlock->row[3], sizeof(unsigned char));
		Swap(&ColorBlock->row[1], &ColorBlock->row[2], sizeof(unsigned char));
		ColorBlock++;
	}
}

void FlipDXT3Blocks(DXTColorBlock_t *Block, int NumBlocks)
{
	int i;
	DXTColorBlock_t *ColorBlock=Block;
	DXT3AlphaBlock_t *AlphaBlock;

	for(i=0;i<NumBlocks;i++)
	{
		AlphaBlock=(DXT3AlphaBlock_t *)ColorBlock;

		Swap(&AlphaBlock->row[0], &AlphaBlock->row[3], sizeof(unsigned short));
		Swap(&AlphaBlock->row[1], &AlphaBlock->row[2], sizeof(unsigned short));
		ColorBlock++;

		Swap(&ColorBlock->row[0], &ColorBlock->row[3], sizeof(unsigned char));
		Swap(&ColorBlock->row[1], &ColorBlock->row[2], sizeof(unsigned char));
		ColorBlock++;
	}
}

void FlipDXT5Alpha(DXT5AlphaBlock_t *Block)
{
	unsigned long *Bits, Bits0=0, Bits1=0;

	memcpy(&Bits0, &Block->row[0], sizeof(unsigned char)*3);
	memcpy(&Bits1, &Block->row[3], sizeof(unsigned char)*3);

	Bits=((unsigned long *)&(Block->row[0]));
	*Bits&=0xff000000;
	*Bits|=(unsigned char)(Bits1>>12)&0x00000007;
	*Bits|=(unsigned char)((Bits1>>15)&0x00000007)<<3;
	*Bits|=(unsigned char)((Bits1>>18)&0x00000007)<<6;
	*Bits|=(unsigned char)((Bits1>>21)&0x00000007)<<9;
	*Bits|=(unsigned char)(Bits1&0x00000007)<<12;
	*Bits|=(unsigned char)((Bits1>>3)&0x00000007)<<15;
	*Bits|=(unsigned char)((Bits1>>6)&0x00000007)<<18;
	*Bits|=(unsigned char)((Bits1>>9)&0x00000007)<<21;

	Bits=((unsigned long *)&(Block->row[3]));
	*Bits&=0xff000000;
	*Bits|=(unsigned char)(Bits0>>12)&0x00000007;
	*Bits|=(unsigned char)((Bits0>>15)&0x00000007)<<3;
	*Bits|=(unsigned char)((Bits0>>18)&0x00000007)<<6;
	*Bits|=(unsigned char)((Bits0>>21)&0x00000007)<<9;
	*Bits|=(unsigned char)(Bits0&0x00000007)<<12;
	*Bits|=(unsigned char)((Bits0>>3)&0x00000007)<<15;
	*Bits|=(unsigned char)((Bits0>>6)&0x00000007)<<18;
	*Bits|=(unsigned char)((Bits0>>9)&0x00000007)<<21;
}

void FlipDXT5Blocks(DXTColorBlock_t *Block, int NumBlocks)
{
	DXTColorBlock_t *ColorBlock=Block;
	DXT5AlphaBlock_t *AlphaBlock;
	int i;

	for(i=0;i<NumBlocks;i++)
	{
		AlphaBlock=(DXT5AlphaBlock_t *)ColorBlock;

		FlipDXT5Alpha(AlphaBlock);
		ColorBlock++;

		Swap(&ColorBlock->row[0], &ColorBlock->row[3], sizeof(unsigned char));
		Swap(&ColorBlock->row[1], &ColorBlock->row[2], sizeof(unsigned char));
		ColorBlock++;
	}
}

void Flip(unsigned char *image, int width, int height, int size, int format)
{
	int linesize, i, j;

	if((format==32)||(format==24))
	{
		unsigned char *top, *bottom;

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
		int xblocks=width/4;
		int yblocks=height/4;

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

int DDS_Load(char *Filename, Image_t *Image)
{
	DDS_Header_t dds;
	unsigned long magic;
	FILE *stream;
	int size;

	if((stream=fopen(Filename, "rb"))==NULL)
		return 0;

	fread(&magic, sizeof(unsigned long), 1, stream);

	if(magic!=DDS_MAGIC)
	{
		fclose(stream);
		return 0;
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
				return 0;
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
				return 0;
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

	Image->Data=(unsigned char *)malloc(sizeof(unsigned char)*size);

	if(Image->Data==NULL)
		return 0;

	fread(Image->Data, sizeof(unsigned char), size, stream);
	fclose(stream);

	Flip(Image->Data, Image->Width, Image->Height, size, Image->Depth);

	return 1;
}
