#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "../system/system.h"
#include "../vulkan/vulkan.h"
#include "image.h"

static uint32_t QOI_MAGIC='q'<<24|'o'<<16|'i'<<8|'f';

#define QOI_SRGB			0
#define QOI_LINEAR			1

#define QOI_OP_INDEX		0x00
#define QOI_OP_DIFF			0x40
#define QOI_OP_LUMA			0x80
#define QOI_OP_RUN			0xC0
#define QOI_OP_RGB			0xFE
#define QOI_OP_RGBA			0xFF
#define QOI_OP_MASK			0xC0

#define QOI_HASH(C)	(C[0]*3+C[1]*5+C[2]*7+C[3]*11)

const uint8_t qoiPadding[8]={ 0, 0, 0, 0, 0, 0, 0, 1 };

static uint32_t Int32Swap(const uint32_t l)
{
	const uint8_t b1=(l>>0)&255;
	const uint8_t b2=(l>>8)&255;
	const uint8_t b3=(l>>16)&255;
	const uint8_t b4=(l>>24)&255;

	return ((uint32_t)b1<<24)+((uint32_t)b2<<16)+((uint32_t)b3<<8)+((uint32_t)b4<<0);
}

bool QOI_Load(const char *filename, VkuImage_t *image)
{
	FILE *stream=NULL;
	uint32_t magic=0;
	uint32_t width=0;
	uint32_t height=0;
	uint8_t channels=0;
	uint8_t colorspace=0;
	uint8_t b1=0, b2=0;
	uint8_t index[64][4]={ {0, 0, 0, 0} }, bytes[4]={ 0 };
	uint32_t run=0;

	if(!(stream=fopen(filename, "rb")))
		return false;

	fread(&magic, 1, 4, stream);
	magic=Int32Swap(magic);

	fread(&width, 1, 4, stream);
	width=Int32Swap(width);

	fread(&height, 1, 4, stream);
	height=Int32Swap(height);

	fread(&channels, 1, 1, stream);
	fread(&colorspace, 1, 1, stream);

	if(width==0||height==0||channels<3||channels>4||colorspace>1||magic!=QOI_MAGIC)
		return false;

	image->width=width;
	image->height=height;
	image->depth=channels<<3;
	image->data=(uint8_t *)Zone_Malloc(zone, width*height*channels);

	if(!image->data)
		return false;

	memset(index, 0, 64*4);
	bytes[0]=0;
	bytes[1]=0;
	bytes[2]=0;
	bytes[3]=255;

	for(uint32_t i=0;i<width*height*channels;i+=channels)
	{
		if(run>0)
			run--;
		else
		{
			fread(&b1, 1, 1, stream);

			if(b1==QOI_OP_RGB)
				fread(bytes, 1, 3, stream);
			else if(b1==QOI_OP_RGBA)
				fread(bytes, 1, 4, stream);
			else if((b1&QOI_OP_MASK)==QOI_OP_INDEX)
			{
				bytes[0]=index[b1][0];
				bytes[1]=index[b1][1];
				bytes[2]=index[b1][2];
				bytes[3]=index[b1][3];
			}
			else if((b1&QOI_OP_MASK)==QOI_OP_DIFF)
			{
				bytes[0]+=((b1>>4)&0x03)-2;
				bytes[1]+=((b1>>2)&0x03)-2;
				bytes[2]+=((b1>>0)&0x03)-2;
			}
			else if((b1&QOI_OP_MASK)==QOI_OP_LUMA)
			{
				fread(&b2, 1, 1, stream);

				int vg=(b1&0x3f)-32;

				bytes[0]+=vg-8+((b2>>4)&0x0f);
				bytes[1]+=vg;
				bytes[2]+=vg-8+(b2&0x0f);
			}
			else if((b1&QOI_OP_MASK)==QOI_OP_RUN)
				run=(b1&0x3f);

			index[QOI_HASH(bytes)%64][0]=bytes[0];
			index[QOI_HASH(bytes)%64][1]=bytes[1];
			index[QOI_HASH(bytes)%64][2]=bytes[2];
			index[QOI_HASH(bytes)%64][3]=bytes[3];
		}

		image->data[i+2]=bytes[0];
		image->data[i+1]=bytes[1];
		image->data[i+0]=bytes[2];

		if(channels==4)
			image->data[i+3]=bytes[3];
	}

	fclose(stream);

	return true;
}

bool QOI_Write(const char *filename, VkuImage_t *image)
{
	FILE *stream=NULL;
	uint32_t temp32, i;
	uint8_t index[64][4], px[4], pxPrev[4];
	uint8_t temp8, channels;
	uint8_t run=0;

	if(image==NULL)
		return false;

	if(!(stream=fopen(filename, "wb")))
		return false;

	// MAGIC
	temp32=Int32Swap(QOI_MAGIC);
	fwrite(&temp32, 1, sizeof(uint32_t), stream);

	// Width
	temp32=Int32Swap(image->width);
	fwrite(&temp32, 1, sizeof(uint32_t), stream);

	// Height
	temp32=Int32Swap(image->height);
	fwrite(&temp32, 1, sizeof(uint32_t), stream);

	// Channels
	channels=image->depth>>3;
	fwrite(&channels, 1, sizeof(uint8_t), stream);

	// Colorspace
	temp8=QOI_SRGB;
	fwrite(&temp8, 1, sizeof(uint8_t), stream);

	memset(index, 0, sizeof(index));

	pxPrev[0]=0;
	pxPrev[1]=0;
	pxPrev[2]=0;
	pxPrev[3]=255;

	px[0]=0;
	px[1]=0;
	px[2]=0;
	px[3]=255;

	for(i=0;i<(uint32_t)image->width*image->height*channels;i+=channels)
	{
		px[0]=image->data[i+2];
		px[1]=image->data[i+1];
		px[2]=image->data[i+0];

		if(channels==4)
		{
			px[3]=image->data[i+3];
		}

		if(px[0]==pxPrev[0]&&px[1]==pxPrev[1]&&px[2]==pxPrev[2]&&px[3]==pxPrev[3])
		{
			run++;

			if(run==62)
			{
				temp8=QOI_OP_RUN|(run-1);
				fwrite(&temp8, 1, sizeof(uint8_t), stream);
				run=0;
			}
		}
		else
		{
			int indexPos;

			if(run>0)
			{
				temp8=QOI_OP_RUN|(run-1);
				fwrite(&temp8, 1, sizeof(uint8_t), stream);
				run=0;
			}

			indexPos=QOI_HASH(px)%64;

			if(index[indexPos][0]==px[0]&&index[indexPos][1]==px[1]&&index[indexPos][2]==px[2]&&index[indexPos][3]==px[3])
			{
				temp8=QOI_OP_INDEX|indexPos;
				fwrite(&temp8, 1, sizeof(uint8_t), stream);
			}
			else
			{
				index[indexPos][0]=px[0];
				index[indexPos][1]=px[1];
				index[indexPos][2]=px[2];
				index[indexPos][3]=px[3];

				if(px[3]==pxPrev[3])
				{
					int8_t vr=px[0]-pxPrev[0];
					int8_t vg=px[1]-pxPrev[1];
					int8_t vb=px[2]-pxPrev[2];

					int8_t vg_r=vr-vg;
					int8_t vg_b=vb-vg;

					if(vr>-3&&vr<2&&vg>-3&&vg<2&&vb>-3&&vb<2)
					{
						temp8=QOI_OP_DIFF|(vr+2)<<4|(vg+2)<<2|(vb+2);
						fwrite(&temp8, 1, sizeof(uint8_t), stream);
					}
					else if(vg_r>-9&&vg_r<8&&vg>-33&&vg<32&&vg_b>-9&&vg_b<8)
					{
						temp8=QOI_OP_LUMA|(vg+32);
						fwrite(&temp8, 1, sizeof(uint8_t), stream);
						temp8=(vg_r+8)<<4|(vg_b+8);
						fwrite(&temp8, 1, sizeof(uint8_t), stream);
					}
					else
					{
						temp8=QOI_OP_RGB;
						fwrite(&temp8, 1, sizeof(uint8_t), stream);
						fwrite(&px[0], 1, sizeof(uint8_t), stream);
						fwrite(&px[1], 1, sizeof(uint8_t), stream);
						fwrite(&px[2], 1, sizeof(uint8_t), stream);
					}
				}
				else
				{
					temp8=QOI_OP_RGBA;
					fwrite(&temp8, 1, sizeof(uint8_t), stream);
					fwrite(&px[0], 1, sizeof(uint8_t), stream);
					fwrite(&px[1], 1, sizeof(uint8_t), stream);
					fwrite(&px[2], 1, sizeof(uint8_t), stream);
					fwrite(&px[3], 1, sizeof(uint8_t), stream);
				}
			}
		}

		pxPrev[0]=px[0];
		pxPrev[1]=px[1];
		pxPrev[2]=px[2];
		pxPrev[3]=px[3];
	}

	fwrite(qoiPadding, 1, sizeof(qoiPadding), stream);

	fclose(stream);

	return true;
}
