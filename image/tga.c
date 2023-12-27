#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <malloc.h>
#include <string.h>
#include "../system/system.h"
#include "../vulkan/vulkan.h"
#include "image.h"

#ifndef FREE
#define FREE(p) { if(p) { free(p); p=NULL; } }
#endif

static void rle_read(uint8_t *row, const uint32_t width, const uint8_t bpp, FILE *stream)
{
	uint32_t pos=0, len, i;
	uint8_t header;

	while(pos<width)
	{
		fread(&header, sizeof(uint8_t), 1, stream);

		len=(header&0x7F)+1;

		if(header&0x80)
		{
			uint32_t buffer;

			fread(&buffer, sizeof(uint8_t), bpp, stream);

			for(i=0;i<len*bpp;i+=bpp)
				memcpy(&row[bpp*pos+i], &buffer, bpp);
		}
		else
			fread(&row[bpp*pos], sizeof(uint8_t), len*bpp, stream);

		pos+=len;
	}
}

static bool rle_type(uint8_t *data, const uint32_t pos, const uint32_t width, const uint8_t bpp)
{
	if(!memcmp(data+bpp*pos, data+bpp*(pos+1), bpp))
	{
		if(!memcmp(data+bpp*(pos+1), data+bpp*(pos+2), bpp))
			return true;
	}

	return false;
}

static void rle_write(uint8_t *row, const uint32_t width, const uint8_t bpp, FILE *stream)
{
    uint32_t pos=0;

	while(pos<width)
	{
		uint8_t header, len=2;

		if(rle_type(row, pos, width, bpp))
		{
			if(pos==width-1)
				len=1;
			else
			{
				while(pos+len<width)
				{
					if(memcmp(row+bpp*pos, row+bpp*(pos+len), bpp))
						break;

					len++;

					if(len==128)
						break;
				}
			}

			header=(len-1)|0x80;

			fwrite(&header, 1, 1, stream);
			fwrite(row+bpp*pos, bpp, 1, stream);
		}
		else
		{
			if(pos==width-1)
				len=1;
			else
			{
				while(pos+len<width)
				{
					if(rle_type(row, pos+len, width, bpp))
						break;

					len++;

					if(len==128)
						break;
				}
			}

			header=len-1;

			fwrite(&header, 1, 1, stream);
			fwrite(row+bpp*pos, bpp*len, 1, stream);
		}

		pos+=len;
	}
}

bool TGA_Write(const char *filename, VkuImage_t *image, bool rle)
{
	FILE *stream;
	uint8_t IDLength=0;
	uint8_t colorMapType=0, colorMapStart=0, colorMapLength=0, colorMapDepth=0;
	uint16_t xOffset=0, yOffset=0, width=image->width, height=image->height;
	uint8_t depth=(uint8_t)image->depth, imageDescriptor=0x20, imageType;

	switch(image->depth)
	{
		case 32:
		case 24:
		case 16:
			imageType=rle?10:2;
			break;

		case 8:
			imageType=rle?11:3;
			break;

		default:
			return 0;
	}

	if((stream=fopen(filename, "wb"))==NULL)
		return 0;

	fwrite(&IDLength, sizeof(uint8_t), 1, stream);
	fwrite(&colorMapType, sizeof(uint8_t), 1, stream);
	fwrite(&imageType, sizeof(uint8_t), 1, stream);
	fwrite(&colorMapStart, sizeof(uint16_t), 1, stream);
	fwrite(&colorMapLength, sizeof(uint16_t), 1, stream);
	fwrite(&colorMapDepth, sizeof(uint8_t), 1, stream);
	fwrite(&xOffset, sizeof(uint16_t), 1, stream);
	fwrite(&yOffset, sizeof(uint16_t), 1, stream);
	fwrite(&width, sizeof(uint16_t), 1, stream);
	fwrite(&height, sizeof(uint16_t), 1, stream);
	fwrite(&depth, sizeof(uint8_t), 1, stream);
	fwrite(&imageDescriptor, sizeof(uint8_t), 1, stream);

	if(rle)
	{
		uint8_t *ptr;
		int32_t i, bpp=depth>>3;

		for(i=0, ptr=image->data;i<height;i++, ptr+=width*bpp)
			rle_write(ptr, width, bpp, stream);
	}
	else
		fwrite(image->data, sizeof(uint8_t), image->width*image->height*(image->depth>>3), stream);

	fclose(stream);

	return true;
}

bool TGA_Load(const char *filename, VkuImage_t *image)
{
	FILE *stream=NULL;
	uint8_t IDLength=0;
	uint8_t colorMapType=0, imageType=0;
	uint16_t colorMapStart=0, colorMapLength=0;
	uint8_t colorMapDepth=0;
	uint16_t xOffset=0, yOffset=0, width=0, height=0;
	uint8_t depth=0, imageDescriptor=0, bpp;

	if((stream=fopen(filename, "rb"))==NULL)
		return false;

	fread(&IDLength, sizeof(uint8_t), 1, stream);
	fread(&colorMapType, sizeof(uint8_t), 1, stream);
	fread(&imageType, sizeof(uint8_t), 1, stream);
	fread(&colorMapStart, sizeof(uint16_t), 1, stream);
	fread(&colorMapLength, sizeof(uint16_t), 1, stream);
	fread(&colorMapDepth, sizeof(uint8_t), 1, stream);
	fread(&xOffset, sizeof(uint16_t), 1, stream);
	fread(&yOffset, sizeof(uint16_t), 1, stream);
	fread(&width, sizeof(uint16_t), 1, stream);
	fread(&height, sizeof(uint16_t), 1, stream);
	fread(&depth, sizeof(uint8_t), 1, stream);
	fread(&imageDescriptor, sizeof(uint8_t), 1, stream);
	fseek(stream, IDLength, SEEK_CUR);

	switch(imageType)
	{
		case 11:
		case 10:
		case 3:
		case 2:
			break;

		default:
			fclose(stream);
			return false;
	}

	switch(depth)
	{
		case 32:
		case 24:
		case 16:
		case 8:
			bpp=depth>>3;

			image->data=(uint8_t *)Zone_Malloc(zone, width*height*bpp);

			if(image->data==NULL)
				return false;

			if(imageType==10||imageType==11)
			{
				uint8_t *ptr=(uint8_t *)image->data;

				for(uint32_t i=0;i<height;i++, ptr+=width*bpp)
					rle_read(ptr, width, bpp, stream);
			}
			else
				fread(image->data, sizeof(uint8_t), width*height*bpp, stream);
			break;

		default:
			fclose(stream);
			return false;
	}

	fclose(stream);

	if(!(imageDescriptor&0x20))
	{
		int32_t scanline=width*bpp, size=scanline*height;
		uint8_t *buffer=(uint8_t *)Zone_Malloc(zone, size);

		if(buffer==NULL)
		{
			Zone_Free(zone, image->data);
			return false;
		}

		for(uint32_t i=0;i<height;i++)
			memcpy(buffer+(size-(i+1)*scanline), image->data+i*scanline, scanline);

		memcpy(image->data, buffer, size);

		Zone_Free(zone, buffer);
	}

	image->width=width;
	image->height=height;
	image->depth=depth;

	return true;
}
