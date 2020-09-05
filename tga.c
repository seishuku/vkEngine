#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include "image.h"

#ifndef FREE
#define FREE(p) { if(p) { free(p); p=NULL; } }
#endif

void rle_read(unsigned char *row, int width, int bpp, FILE *stream)
{
	int pos=0, len, i;
	unsigned char header;

	while(pos<width)
	{
		fread(&header, sizeof(unsigned char), 1, stream);

		len=(header&0x7F)+1;

		if(header&0x80)
		{
			unsigned long buffer;

			fread(&buffer, sizeof(unsigned char), bpp, stream);

			for(i=0;i<len*bpp;i+=bpp)
				memcpy(&row[bpp*pos+i], &buffer, bpp);
		}
		else
			fread(&row[bpp*pos], sizeof(unsigned char), len*bpp, stream);

		pos+=len;
	}
}

int rle_type(unsigned char *data, unsigned short pos, unsigned short width, unsigned char bpp)
{
	if(!memcmp(data+bpp*pos, data+bpp*(pos+1), bpp))
	{
		if(!memcmp(data+bpp*(pos+1), data+bpp*(pos+2), bpp))
			return 1;
	}

	return 0;
}

void rle_write(unsigned char *row, int width, int bpp, FILE *stream)
{
    unsigned short pos=0;

	while(pos<width)
	{
		unsigned char header, len=2;

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

int TGA_Write(char *filename, Image_t *Image, int rle)
{
	FILE *stream;
	unsigned char IDLength=0;
	unsigned char ColorMapType=0, ColorMapStart=0, ColorMapLength=0, ColorMapDepth=0;
	unsigned short XOffset=0, YOffset=0, Width=Image->Width, Height=Image->Height;
	unsigned char Depth=(unsigned char)Image->Depth, ImageDescriptor=0, ImageType;

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

	fwrite(&IDLength, sizeof(unsigned char), 1, stream);
	fwrite(&ColorMapType, sizeof(unsigned char), 1, stream);
	fwrite(&ImageType, sizeof(unsigned char), 1, stream);
	fwrite(&ColorMapStart, sizeof(unsigned short), 1, stream);
	fwrite(&ColorMapLength, sizeof(unsigned short), 1, stream);
	fwrite(&ColorMapDepth, sizeof(unsigned char), 1, stream);
	fwrite(&XOffset, sizeof(unsigned short), 1, stream);
	fwrite(&XOffset, sizeof(unsigned short), 1, stream);
	fwrite(&Width, sizeof(unsigned short), 1, stream);
	fwrite(&Height, sizeof(unsigned short), 1, stream);
	fwrite(&Depth, sizeof(unsigned char), 1, stream);
	fwrite(&ImageDescriptor, sizeof(unsigned char), 1, stream);

	if(rle)
	{
		unsigned char *ptr;
		int i, bpp=Depth>>3;

		for(i=0, ptr=Image->Data;i<Height;i++, ptr+=Width*bpp)
			rle_write(ptr, Width, bpp, stream);
	}
	else
		fwrite(Image->Data, sizeof(unsigned char), Image->Width*Image->Height*(Image->Depth>>3), stream);

	fclose(stream);

	return 1;
}

int TGA_Load(char *Filename, Image_t *Image)
{
	FILE *stream=NULL;
	unsigned char *ptr;
	unsigned char IDLength;
	unsigned char ColorMapType, ImageType;
	unsigned short ColorMapStart, ColorMapLength;
	unsigned char ColorMapDepth;
	unsigned short XOffset, YOffset;
	unsigned short Width, Height;
	unsigned char Depth;
	unsigned char ImageDescriptor;
	int i, bpp;

	if((stream=fopen(Filename, "rb"))==NULL)
		return 0;

	fread(&IDLength, sizeof(unsigned char), 1, stream);
	fread(&ColorMapType, sizeof(unsigned char), 1, stream);
	fread(&ImageType, sizeof(unsigned char), 1, stream);
	fread(&ColorMapStart, sizeof(unsigned short), 1, stream);
	fread(&ColorMapLength, sizeof(unsigned short), 1, stream);
	fread(&ColorMapDepth, sizeof(unsigned char), 1, stream);
	fread(&XOffset, sizeof(unsigned short), 1, stream);
	fread(&YOffset, sizeof(unsigned short), 1, stream);
	fread(&Width, sizeof(unsigned short), 1, stream);
	fread(&Height, sizeof(unsigned short), 1, stream);
	fread(&Depth, sizeof(unsigned char), 1, stream);
	fread(&ImageDescriptor, sizeof(unsigned char), 1, stream);
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
			return 0;
	}

	switch(Depth)
	{
		case 32:
		case 24:
		case 16:
		case 8:
			bpp=Depth>>3;

			Image->Data=(unsigned char *)malloc(Width*Height*bpp);

			if(Image->Data==NULL)
				return 0;

			if(ImageType==10||ImageType==11)
			{
				for(i=0, ptr=(unsigned char *)Image->Data;i<Height;i++, ptr+=Width*bpp)
					rle_read(ptr, Width, bpp, stream);
			}
			else
				fread(Image->Data, sizeof(unsigned char), Width*Height*bpp, stream);
			break;

		default:
			fclose(stream);
			return 0;
	}

	fclose(stream);

	if(ImageDescriptor&0x20)
	{
		int Scanline=Width*bpp, Size=Scanline*Height;
		unsigned char *Buffer=(unsigned char *)malloc(Size);

		if(Buffer==NULL)
		{
			FREE(Image->Data);
			return 0;
		}

		for(i=0;i<Height;i++)
			memcpy(Buffer+(Size-(i+1)*Scanline), Image->Data+i*Scanline, Scanline);

		memcpy(Image->Data, Buffer, Size);

		FREE(Buffer);
	}

	Image->Width=Width;
	Image->Height=Height;
	Image->Depth=Depth;

	return 1;
}
