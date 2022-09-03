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

void rle_read(uint8_t *row, int32_t width, int32_t bpp, FILE *stream)
{
	int32_t pos=0, len, i;
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

bool rle_type(uint8_t *data, uint16_t pos, uint16_t width, uint8_t bpp)
{
	if(!memcmp(data+bpp*pos, data+bpp*(pos+1), bpp))
	{
		if(!memcmp(data+bpp*(pos+1), data+bpp*(pos+2), bpp))
			return true;
	}

	return false;
}

void rle_write(uint8_t *row, int32_t width, int32_t bpp, FILE *stream)
{
    uint16_t pos=0;

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

bool TGA_Write(const char *filename, Image_t *Image, bool rle)
{
	FILE *stream;
	uint8_t IDLength=0;
	uint8_t ColorMapType=0, ColorMapStart=0, ColorMapLength=0, ColorMapDepth=0;
	uint16_t XOffset=0, YOffset=0, Width=Image->Width, Height=Image->Height;
	uint8_t Depth=(uint8_t)Image->Depth, ImageDescriptor=0, ImageType;

	switch(Image->Depth)
	{
		case 32:
		case 24:
		case 16:
			ImageType=rle?10:2;
			break;

		case 8:
			ImageType=rle?11:3;
			break;

		default:
			return 0;
	}

	if((stream=fopen(filename, "wb"))==NULL)
		return 0;

	fwrite(&IDLength, sizeof(uint8_t), 1, stream);
	fwrite(&ColorMapType, sizeof(uint8_t), 1, stream);
	fwrite(&ImageType, sizeof(uint8_t), 1, stream);
	fwrite(&ColorMapStart, sizeof(uint16_t), 1, stream);
	fwrite(&ColorMapLength, sizeof(uint16_t), 1, stream);
	fwrite(&ColorMapDepth, sizeof(uint8_t), 1, stream);
	fwrite(&XOffset, sizeof(uint16_t), 1, stream);
	fwrite(&XOffset, sizeof(uint16_t), 1, stream);
	fwrite(&Width, sizeof(uint16_t), 1, stream);
	fwrite(&Height, sizeof(uint16_t), 1, stream);
	fwrite(&Depth, sizeof(uint8_t), 1, stream);
	fwrite(&ImageDescriptor, sizeof(uint8_t), 1, stream);

	if(rle)
	{
		uint8_t *ptr;
		int32_t i, bpp=Depth>>3;

		for(i=0, ptr=Image->Data;i<Height;i++, ptr+=Width*bpp)
			rle_write(ptr, Width, bpp, stream);
	}
	else
		fwrite(Image->Data, sizeof(uint8_t), Image->Width*Image->Height*(Image->Depth>>3), stream);

	fclose(stream);

	return true;
}

bool TGA_Load(const char *Filename, Image_t *Image)
{
	FILE *stream=NULL;
	uint8_t *ptr;
	uint8_t IDLength;
	uint8_t ColorMapType, ImageType;
	uint16_t ColorMapStart, ColorMapLength;
	uint8_t ColorMapDepth;
	uint16_t XOffset, YOffset;
	uint16_t Width, Height;
	uint8_t Depth;
	uint8_t ImageDescriptor;
	int32_t i, bpp;

	if((stream=fopen(Filename, "rb"))==NULL)
		return false;

	fread(&IDLength, sizeof(uint8_t), 1, stream);
	fread(&ColorMapType, sizeof(uint8_t), 1, stream);
	fread(&ImageType, sizeof(uint8_t), 1, stream);
	fread(&ColorMapStart, sizeof(uint16_t), 1, stream);
	fread(&ColorMapLength, sizeof(uint16_t), 1, stream);
	fread(&ColorMapDepth, sizeof(uint8_t), 1, stream);
	fread(&XOffset, sizeof(uint16_t), 1, stream);
	fread(&YOffset, sizeof(uint16_t), 1, stream);
	fread(&Width, sizeof(uint16_t), 1, stream);
	fread(&Height, sizeof(uint16_t), 1, stream);
	fread(&Depth, sizeof(uint8_t), 1, stream);
	fread(&ImageDescriptor, sizeof(uint8_t), 1, stream);
	fseek(stream, IDLength, SEEK_CUR);

	switch(ImageType)
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

	switch(Depth)
	{
		case 32:
		case 24:
		case 16:
		case 8:
			bpp=Depth>>3;

			Image->Data=(uint8_t *)Zone_Malloc(Zone, Width*Height*bpp);

			if(Image->Data==NULL)
				return false;

			if(ImageType==10||ImageType==11)
			{
				for(i=0, ptr=(uint8_t *)Image->Data;i<Height;i++, ptr+=Width*bpp)
					rle_read(ptr, Width, bpp, stream);
			}
			else
				fread(Image->Data, sizeof(uint8_t), Width*Height*bpp, stream);
			break;

		default:
			fclose(stream);
			return false;
	}

	fclose(stream);

	if(!(ImageDescriptor&0x20))
	{
		int32_t Scanline=Width*bpp, Size=Scanline*Height;
		uint8_t *Buffer=(uint8_t *)Zone_Malloc(Zone, Size);

		if(Buffer==NULL)
		{
			Zone_Free(Zone, Image->Data);
			return false;
		}

		for(i=0;i<Height;i++)
			memcpy(Buffer+(Size-(i+1)*Scanline), Image->Data+i*Scanline, Scanline);

		memcpy(Image->Data, Buffer, Size);

		Zone_Free(Zone, Buffer);
	}

	Image->Width=Width;
	Image->Height=Height;
	Image->Depth=Depth;

	return true;
}
