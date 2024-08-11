/*
	Copyright 2020 Matt Williams/NitroGL
	Image texture loading for Vulkan, based on my OpenGL code.

	TODO:
		manual mipmaps?
		more comments?
*/

#include <malloc.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../system/system.h"
#include "../vulkan/vulkan.h"
#include "../math/math.h"
#include "image.h"

// _MakeNormalMap, internal function used by Image_Upload.
// Make a normal map from a greyscale height map (or the first channel of an RGB/RGBA image).
// 
// This is done by applying a Sobel kernel filter in the X and Y directions, Z=1.0, then normalizing that.
static bool _MakeNormalMap(VkuImage_t *image)
{
	const uint32_t Channels=image->depth>>3;
	const float OneOver255=1.0f/255.0f;
	const float KernelX[9]=
	{
		 1.0f,  0.0f, -1.0f,
		 2.0f,  0.0f, -2.0f,
		 1.0f,  0.0f, -1.0f
	};
	const float KernelY[9]=
	{
		-1.0f, -2.0f, -1.0f,
		 0.0f,  0.0f,  0.0f,
		 1.0f,  2.0f,  1.0f
	};

	// Check if input image is a valid bit depth.
	if(image->depth!=32&&image->depth!=8)
		return false;

	// Allocate memory for output data.
	uint16_t *buffer=(uint16_t *)Zone_Malloc(zone, sizeof(uint16_t)*image->width*image->height*4);

	// Check that memory allocated.
	if(buffer==NULL)
		return false;

	// Loop over all pixels of input image.
	for(uint32_t y=0;y<image->height;y++)
	{
		for(uint32_t x=0;x<image->width;x++)
		{
			vec3 n={ 0.0f, 0.0f, 1.0f };

			// Loop over the kernel(s) to convolve the image.
			for(uint32_t yy=0;yy<3;yy++)
			{
				const int oy=min(image->height-1, y+yy);

				for(uint32_t xx=0;xx<3;xx++)
				{
					const int ox=min(image->width-1, x+xx);
					const float Pixel=(float)image->data[Channels*(oy*image->width+ox)]*OneOver255;

					n.x+=KernelX[yy*3+xx]*Pixel;
					n.y+=KernelY[yy*3+xx]*Pixel;
				}
			}

			// Resulting convolution is the "normal" after normalizing.
			Vec3_Normalize(&n);

			// Precalculate destination image index
			const uint32_t Index=4*(y*image->width+x);

			// Set destination image RGB to the normal value,
			//   scaled and biased to fit the 16bit/channel unsigned integer image format
			buffer[Index+0]=(uint16_t)(65535.0f*(0.5f*n.x+0.5f));
			buffer[Index+1]=(uint16_t)(65535.0f*(0.5f*n.y+0.5f));
			buffer[Index+2]=(uint16_t)(65535.0f*(0.5f*n.z+0.5f));

			// If the input image was an RGBA image, copy over the alpha channel from that, otherwise alpha=1.0.
			if(Channels==4)
				buffer[Index+3]=(uint16_t)(image->data[Index+3]<<8);
			else
				buffer[Index+3]=65535;
		}
	}

	// Reset input color depth to the new format
	image->depth=64;

	// Free input image data and set it's pointer to the new data.
	Zone_Free(zone, image->data);
	image->data=(unsigned char *)buffer;

	return true;
}

// _Normalize, internal function used by Image_Upload.
// Takes an 8bit/channel RGB(A) image and re-normalizes it into a 16bit/channel image.
static bool _Normalize(VkuImage_t *image)
{
	uint32_t channels=image->depth>>3;
	const float oneOver255=1.0f/255.0f;

	if(image->depth!=32)
		return false;

	uint16_t *buffer=(uint16_t *)Zone_Malloc(zone, sizeof(uint16_t)*image->width*image->height*4);

	if(buffer==NULL)
		return false;

	for(uint32_t i=0;i<image->width*image->height;i++)
	{
		// Initialize the normal vector with the image data
		vec3 n=Vec3((float)image->data[channels*i+2]*oneOver255,
					(float)image->data[channels*i+1]*oneOver255,
					(float)image->data[channels*i+0]*oneOver255);

		// scale/bias BGR unorm -> Normal float, and normalize it
		n=Vec3_Subs(Vec3_Muls(n, 2.0f), 1.0f);
		Vec3_Normalize(&n);

		// scale/bias normal back into a 16bit/channel unorm image
		n=Vec3_Adds(Vec3_Muls(n, 0.5f), 0.5f);
		buffer[4*i+0]=(uint16_t)(65535.0f*n.x);
		buffer[4*i+1]=(uint16_t)(65535.0f*n.y);
		buffer[4*i+2]=(uint16_t)(65535.0f*n.z);

		// Pass along alpha channel if original image was RGBA
		if(channels==4)
			buffer[4*i+3]=(uint16_t)(image->data[4*i+3]<<8);
		else
			buffer[4*i+3]=65535;
	}

	// Reset new color depth
	image->depth=64;

	// Free original image and reset the buffer pointer
	Zone_Free(zone, image->data);
	image->data=(uint8_t *)buffer;

	return true;
}

// _RGBE2Float, internal function used by Image_Upload
// Takes in an 8bit/channel RGBE encoded image and outputs a
//   32bit float/channel RGB image, mainly used for HDR images.
static bool _RGBE2Float(VkuImage_t *image)
{
	float *buffer=(float *)Zone_Malloc(zone, sizeof(float)*image->width*image->height*4);

	if(buffer==NULL)
		return false;

	for(uint32_t i=0;i<image->width*image->height;i++)
	{
		unsigned char *rgbe=&image->data[4*i];
		float *rgb=&buffer[4*i];

		// This is basically rgb*exponent to reconstruct floating point pixels from 8bit/channel image.
		if(rgbe[3])
		{
			float f=1.0f;
			// Exponent is stored in the alpha channel
			int8_t e=rgbe[3]-(128+8);

			// f=exp2(e)
			if(e>0)
			{
				for(uint8_t i=0;i<e;i++)
					f*=2.0f;
			}
			else
			{
				for(uint8_t i=0;i<-e;i++)
					f/=2.0f;
			}

			// Bias and multiple
			rgb[0]=((float)rgbe[0]+0.5f)*f;
			rgb[1]=((float)rgbe[1]+0.5f)*f;
			rgb[2]=((float)rgbe[2]+0.5f)*f;

			// RGBE images can't have an alpha channel, but we're outputting in RGBA anyway.
			rgb[3]=1.0f;
		}
		else
			rgb[0]=rgb[1]=rgb[2]=rgb[3]=0.0f;
	}

	// Reset color depth to new format
	image->depth=128;

	// Free and reset data pointer
	Zone_Free(zone, image->data);
	image->data=(uint8_t *)buffer;

	return true;
}

// _Resize, internal function not currently used.
// Does a bilinear resize of an image, destination struct width/height set image size.
//
// TODO: Source/destination color depths *could* be different,
//         maybe make this do a format conversion as well?
static bool _Resize(VkuImage_t *src, VkuImage_t *dst)
{
	float fx, fy, hx, hy, lx, ly;
	float xPercent, yPercent;
	float total[4], Sum;
	uint32_t iy, ix;

	// Destination format must match source format.
	if(dst->depth!=src->depth)
		return false;

	dst->data=(uint8_t *)Zone_Malloc(zone, (size_t)dst->width*dst->height*(dst->depth>>3));

	if(dst->data==NULL)
		return false;

	float sx=(float)src->width/dst->width;
	float sy=(float)src->height/dst->height;

	for(uint32_t y=0;y<dst->height;y++)
	{
		if(src->height>dst->height)
		{
			fy=((float)y+0.5f)*sy;
			hy=fy+(sy*0.5f);
			ly=fy-(sy*0.5f);
		}
		else
		{
			fy=(float)y*sy;
			hy=fy+0.5f;
			ly=fy-0.5f;
		}

		for(uint32_t x=0;x<dst->width;x++)
		{
			if(src->width>dst->width)
			{
				fx=((float)x+0.5f)*sx;
				hx=fx+(sx*0.5f);
				lx=fx-(sx*0.5f);
			}
			else
			{
				fx=(float)x*sx;
				hx=fx+0.5f;
				lx=fx-0.5f;
			}

			total[0]=total[1]=total[2]=total[3]=Sum=0.0f;

			fy=ly;
			iy=(int)fy;

			while(fy<hy)
			{
				if(hy<iy+1)
					yPercent=hy-fy;
				else
					yPercent=(iy+1)-fy;

				fx=lx;
				ix=(int)fx;

				while(fx<hx)
				{
					if(hx<ix+1)
						xPercent=hx-fx;
					else
						xPercent=(ix+1)-fx;

					float Percent=xPercent*yPercent;
					Sum+=Percent;

					uint32_t Index=min(src->height-1, iy)*src->width+min(src->width-1, ix);

					switch(src->depth)
					{
						case 128:
							total[0]+=((float *)src->data)[4*Index+0]*Percent;
							total[1]+=((float *)src->data)[4*Index+1]*Percent;
							total[2]+=((float *)src->data)[4*Index+2]*Percent;
							total[3]+=((float *)src->data)[4*Index+3]*Percent;
							break;

						case 64:
							total[0]+=((uint16_t *)src->data)[4*Index+0]*Percent;
							total[1]+=((uint16_t *)src->data)[4*Index+1]*Percent;
							total[2]+=((uint16_t *)src->data)[4*Index+2]*Percent;
							total[3]+=((uint16_t *)src->data)[4*Index+3]*Percent;
							break;

						case 32:
							total[0]+=(float)src->data[4*Index+0]*Percent;
							total[1]+=(float)src->data[4*Index+1]*Percent;
							total[2]+=(float)src->data[4*Index+2]*Percent;
							total[3]+=(float)src->data[4*Index+3]*Percent;
							break;

						case 16:
							total[0]+=((((uint16_t *)src->data)[Index]>>0x0)&0x1F)*Percent;
							total[1]+=((((uint16_t *)src->data)[Index]>>0x5)&0x1F)*Percent;
							total[2]+=((((uint16_t *)src->data)[Index]>>0xA)&0x1F)*Percent;
							break;

						case 8:
							total[0]+=src->data[Index]*Percent;
					}

					ix++;
					fx=(float)ix;
				}

				iy++;
				fy=(float)iy;
			}

			uint32_t Index=y*dst->width+x;
			Sum=1.0f/Sum;

			switch(dst->depth)
			{
				case 128:
					((float *)dst->data)[4*Index+0]=(float)(total[0]*Sum);
					((float *)dst->data)[4*Index+1]=(float)(total[1]*Sum);
					((float *)dst->data)[4*Index+2]=(float)(total[2]*Sum);
					((float *)dst->data)[4*Index+3]=(float)(total[3]*Sum);
					break;

				case 64:
					((uint16_t *)dst->data)[4*Index+0]=(uint16_t)(total[0]*Sum);
					((uint16_t *)dst->data)[4*Index+1]=(uint16_t)(total[1]*Sum);
					((uint16_t *)dst->data)[4*Index+2]=(uint16_t)(total[2]*Sum);
					((uint16_t *)dst->data)[4*Index+3]=(uint16_t)(total[3]*Sum);
					break;

				case 32:
					((uint8_t *)dst->data)[4*Index+0]=(uint8_t)(total[0]*Sum);
					((uint8_t *)dst->data)[4*Index+1]=(uint8_t)(total[1]*Sum);
					((uint8_t *)dst->data)[4*Index+2]=(uint8_t)(total[2]*Sum);
					((uint8_t *)dst->data)[4*Index+3]=(uint8_t)(total[3]*Sum);
					break;

				case 16:
					((uint16_t *)dst->data)[Index]=((uint16_t)((total[0]*Sum))&0x1F)<<0x0|((uint16_t)(total[1]*Sum)&0x1F)<<0x5|((uint16_t)(total[2]*Sum)&0x1F)<<0xA;
					break;

				case 8:
					((uint8_t *)dst->data)[Index]=(uint8_t)(total[0]*Sum);
					break;
			}
		}
	}

	return true;
}

// _HalfImage, internal function not currently used.
// Simple 4->1 pixel average downscale to only half the image size.
static bool _HalfImage(VkuImage_t *src, VkuImage_t *dst)
{
	dst->width=src->width>>1;
	dst->height=src->height>>1;
	dst->depth=src->depth;
	dst->data=(uint8_t *)Zone_Malloc(zone, (size_t)dst->width*dst->height*(dst->depth>>3));

	if(dst->data==NULL)
		return false;

	for(uint32_t y=0;y<dst->height;y++)
	{
		uint32_t sy=y<<1;

		for(uint32_t x=0;x<dst->width;x++)
		{
			uint32_t sx=x<<1;

			uint32_t indexDst=y*dst->width+x;
			uint32_t indexSrc00=(sy+0)*src->width+(sx+0);
			uint32_t indexSrc10=(sy+0)*src->width+(sx+1);
			uint32_t indexSrc01=(sy+1)*src->width+(sx+0);
			uint32_t indexSrc11=(sy+1)*src->width+(sx+1);

			switch(src->depth)
			{
				case 128:
					((float *)dst->data)[4*indexDst+0]=(((float *)src->data)[4*indexSrc00+0]+
														((float *)src->data)[4*indexSrc10+0]+
														((float *)src->data)[4*indexSrc01+0]+
														((float *)src->data)[4*indexSrc11+0])*0.25f;
					((float *)dst->data)[4*indexDst+1]=(((float *)src->data)[4*indexSrc00+1]+
														((float *)src->data)[4*indexSrc10+1]+
														((float *)src->data)[4*indexSrc01+1]+
														((float *)src->data)[4*indexSrc11+1])*0.25f;
					((float *)dst->data)[4*indexDst+2]=(((float *)src->data)[4*indexSrc00+2]+
														((float *)src->data)[4*indexSrc10+2]+
														((float *)src->data)[4*indexSrc01+2]+
														((float *)src->data)[4*indexSrc11+2])*0.25f;
					((float *)dst->data)[4*indexDst+3]=(((float *)src->data)[4*indexSrc00+3]+
														((float *)src->data)[4*indexSrc10+3]+
														((float *)src->data)[4*indexSrc01+3]+
														((float *)src->data)[4*indexSrc11+3])*0.25f;
					break;

				case 64:
					((uint16_t *)dst->data)[4*indexDst+0]=(((uint16_t *)src->data)[4*indexSrc00+0]+
														   ((uint16_t *)src->data)[4*indexSrc10+0]+
														   ((uint16_t *)src->data)[4*indexSrc01+0]+
														   ((uint16_t *)src->data)[4*indexSrc11+0])>>2;
					((uint16_t *)dst->data)[4*indexDst+1]=(((uint16_t *)src->data)[4*indexSrc00+1]+
														   ((uint16_t *)src->data)[4*indexSrc10+1]+
														   ((uint16_t *)src->data)[4*indexSrc01+1]+
														   ((uint16_t *)src->data)[4*indexSrc11+1])>>2;
					((uint16_t *)dst->data)[4*indexDst+2]=(((uint16_t *)src->data)[4*indexSrc00+2]+
														   ((uint16_t *)src->data)[4*indexSrc10+2]+
														   ((uint16_t *)src->data)[4*indexSrc01+2]+
														   ((uint16_t *)src->data)[4*indexSrc11+2])>>2;
					((uint16_t *)dst->data)[4*indexDst+3]=(((uint16_t *)src->data)[4*indexSrc00+3]+
														   ((uint16_t *)src->data)[4*indexSrc10+3]+
														   ((uint16_t *)src->data)[4*indexSrc01+3]+
														   ((uint16_t *)src->data)[4*indexSrc11+3])>>2;
					break;

				case 32:
					dst->data[4*indexDst+0]=(src->data[4*indexSrc00+0]+
											 src->data[4*indexSrc10+0]+
											 src->data[4*indexSrc01+0]+
											 src->data[4*indexSrc11+0])>>2;
					dst->data[4*indexDst+1]=(src->data[4*indexSrc00+1]+
											 src->data[4*indexSrc10+1]+
											 src->data[4*indexSrc01+1]+
											 src->data[4*indexSrc11+1])>>2;
					dst->data[4*indexDst+2]=(src->data[4*indexSrc00+2]+
											 src->data[4*indexSrc10+2]+
											 src->data[4*indexSrc01+2]+
											 src->data[4*indexSrc11+2])>>2;
					dst->data[4*indexDst+3]=(src->data[4*indexSrc00+3]+
											 src->data[4*indexSrc10+3]+
											 src->data[4*indexSrc01+3]+
											 src->data[4*indexSrc11+3])>>2;
					break;

				case 16:
				{
					uint16_t p0=((uint16_t *)src->data)[indexSrc00];
					uint16_t p1=((uint16_t *)src->data)[indexSrc10];
					uint16_t p2=((uint16_t *)src->data)[indexSrc01];
					uint16_t p3=((uint16_t *)src->data)[indexSrc11];

					((uint16_t *)dst->data)[indexDst] =(uint16_t)((((p0>>0x0)&0x1F)+((p1>>0x0)&0x1F)+((p2>>0x0)&0x1F)+((p3>>0x0)&0x1F))>>2)<<0x0;
					((uint16_t *)dst->data)[indexDst]|=(uint16_t)((((p0>>0x5)&0x1F)+((p1>>0x5)&0x1F)+((p2>>0x5)&0x1F)+((p3>>0x5)&0x1F))>>2)<<0x5;
					((uint16_t *)dst->data)[indexDst]|=(uint16_t)((((p0>>0xA)&0x1F)+((p1>>0xA)&0x1F)+((p2>>0xA)&0x1F)+((p3>>0xA)&0x1F))>>2)<<0xA;
					break;
				}

				case 8:
					dst->data[indexDst]=(src->data[indexSrc00]+
										 src->data[indexSrc10]+
										 src->data[indexSrc01]+
										 src->data[indexSrc11])>>2;
					break;
			}
		}
	}

	return true;
}

// _BuildMipmaps, internal function used by Image_Upload.
// Builds a Vulkan mipmap chain by using a CPU scaler to resize the images.
// 
// TODO: This doesn't obviously work, the data must persist until the command buffer is complete and submitted
// 
//static void _BuildMipmaps(VkDevice device, VkCommandBuffer CommandBuffer, VkuBuffer_t *StagingBuffer, VkuImage_t *image, uint32_t mipLevels, uint32_t layerCount, uint32_t baseLayer)
//{
//	VkuImage_t Stage, Temp;
//
//	// Create a temporary image
//	Temp.width=image->width;
//	Temp.height=image->height;
//	Temp.depth=image->depth;
//
//	// Create and map memory for the already existing staging buffer (this contains the original image)
//	Stage.width=image->width;
//	Stage.height=image->height;
//	Stage.depth=image->depth;
//	vkMapMemory(device, StagingBuffer->deviceMemory, 0, VK_WHOLE_SIZE, 0, &Stage.data);
//
//	for(uint32_t i=1;i<mipLevels;i++)
//	{
//		// Half the image from the staging buffer image into the temporary image
//		_HalfImage(&Stage, &Temp);
//
//		// Copy the halved image from the temporary image into the staging buffer
//		memcpy(Stage.data, Temp.data, Temp.width*Temp.height*(Temp.depth>>3));
//
//		// Copy from staging buffer to the texture buffer
//		vkCmdCopyBufferToImage(CommandBuffer, StagingBuffer->buffer, image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, (VkBufferImageCopy[1])
//		{
//			{ 0, 0, 0, { VK_IMAGE_ASPECT_COLOR_BIT, i, baseLayer, layerCount }, { 0, 0, 0 }, { Temp.width, Temp.height, 1 } }
//		});
//
//		// The staging buffer now contains the halved image, so update the width/height for the next round
//		Stage.width=Temp.width;
//		Stage.height=Temp.height;
//
//		// Free the temp data allocated by _HalfImage
//		Zone_Free(zone, Temp.data);
//	}
//
//	// Unmap the staging buffer
//	vkUnmapMemory(device, StagingBuffer->deviceMemory);
//
//	// Final image layout transition to shader read-only
//	vkuTransitionLayout(CommandBuffer, image->image, 1, mipLevels-1, 1, baseLayer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
//}

// _GetPixelBilinear, internal function used by functions that need to sample pixels out of image data.
// Essentially does what the GPU does when sampling textures, xy coords are unnormalized (0 to image size).
//
// Output must be sized correctly for sampled image bit depth and format.
// TODO: Source/destination color depths *could* be different,
//         maybe make this do a format conversion as well?
static void _GetPixelBilinear(VkuImage_t *image, float x, float y, void *out)
{
	uint32_t ix=(int)x, iy=(int)y;
	uint32_t ox=ix+1, oy=iy+1;
	float fx=x-ix, fy=y-iy;

	if(ox>=image->width)
		ox=image->width-1;

	if(oy>=image->height)
		oy=image->height-1;

	if(fx<0.0f)
		ix=ox=0;

	if(fy<0.0f)
		iy=oy=0;

	float w11=fx*fy;
	float w00=1.0f-fx-fy+w11;
	float w10=fx-w11;
	float w01=fy-w11;

	uint32_t i00=iy*image->width+ix;
	uint32_t i10=iy*image->width+ox;
	uint32_t i01=oy*image->width+ix;
	uint32_t i11=oy*image->width+ox;

	switch(image->depth)
	{
		case 128:
			((float *)out)[0]=((float *)image->data)[4*i00+0]*w00+((float *)image->data)[4*i10+0]*w10+((float *)image->data)[4*i01+0]*w01+((float *)image->data)[4*i11+0]*w11;
			((float *)out)[1]=((float *)image->data)[4*i00+1]*w00+((float *)image->data)[4*i10+1]*w10+((float *)image->data)[4*i01+1]*w01+((float *)image->data)[4*i11+1]*w11;
			((float *)out)[2]=((float *)image->data)[4*i00+2]*w00+((float *)image->data)[4*i10+2]*w10+((float *)image->data)[4*i01+2]*w01+((float *)image->data)[4*i11+2]*w11;
			((float *)out)[3]=((float *)image->data)[4*i00+3]*w00+((float *)image->data)[4*i10+3]*w10+((float *)image->data)[4*i01+3]*w01+((float *)image->data)[4*i11+3]*w11;
			break;

		case 64:
			((uint16_t *)out)[0]=(uint16_t)(((uint16_t *)image->data)[4*i00+0]*w00+((uint16_t *)image->data)[4*i10+0]*w10+((uint16_t *)image->data)[4*i01+0]*w01+((uint16_t *)image->data)[4*i11+0]*w11);
			((uint16_t *)out)[1]=(uint16_t)(((uint16_t *)image->data)[4*i00+1]*w00+((uint16_t *)image->data)[4*i10+1]*w10+((uint16_t *)image->data)[4*i01+1]*w01+((uint16_t *)image->data)[4*i11+1]*w11);
			((uint16_t *)out)[2]=(uint16_t)(((uint16_t *)image->data)[4*i00+2]*w00+((uint16_t *)image->data)[4*i10+2]*w10+((uint16_t *)image->data)[4*i01+2]*w01+((uint16_t *)image->data)[4*i11+2]*w11);
			((uint16_t *)out)[3]=(uint16_t)(((uint16_t *)image->data)[4*i00+3]*w00+((uint16_t *)image->data)[4*i10+3]*w10+((uint16_t *)image->data)[4*i01+3]*w01+((uint16_t *)image->data)[4*i11+3]*w11);
			break;

		case 32:
			((uint8_t *)out)[0]=(uint8_t)(image->data[4*i00+0]*w00+image->data[4*i10+0]*w10+image->data[4*i01+0]*w01+image->data[4*i11+0]*w11);
			((uint8_t *)out)[1]=(uint8_t)(image->data[4*i00+1]*w00+image->data[4*i10+1]*w10+image->data[4*i01+1]*w01+image->data[4*i11+1]*w11);
			((uint8_t *)out)[2]=(uint8_t)(image->data[4*i00+2]*w00+image->data[4*i10+2]*w10+image->data[4*i01+2]*w01+image->data[4*i11+2]*w11);
			((uint8_t *)out)[3]=(uint8_t)(image->data[4*i00+3]*w00+image->data[4*i10+3]*w10+image->data[4*i01+3]*w01+image->data[4*i11+3]*w11);
			break;

		case 16:
		{
			uint16_t p0=((uint16_t *)image->data)[i00];
			uint16_t p1=((uint16_t *)image->data)[i10];
			uint16_t p2=((uint16_t *)image->data)[i01];
			uint16_t p3=((uint16_t *)image->data)[i11];

			*((uint16_t *)out) =(uint16_t)(((p0>>0x0)&0x1F)*w00+((p1>>0x0)&0x1F)*w10+((p2>>0x0)&0x1F)*w01+((p3>>0x0)&0x1F)*w11)<<0x0;
			*((uint16_t *)out)|=(uint16_t)(((p0>>0x5)&0x1F)*w00+((p1>>0x5)&0x1F)*w10+((p2>>0x5)&0x1F)*w01+((p3>>0x5)&0x1F)*w11)<<0x5;
			*((uint16_t *)out)|=(uint16_t)(((p0>>0xA)&0x1F)*w00+((p1>>0xA)&0x1F)*w10+((p2>>0xA)&0x1F)*w01+((p3>>0xA)&0x1F)*w11)<<0xA;
			break;
		}

		case 8:
			*((uint8_t *)out)=(uint8_t)(image->data[i00]*w00+image->data[i10]*w10+image->data[i01]*w01+image->data[i11]*w11);
			break;
	}
}

// _GetUVAngularMap, internal function used by _AngularMapFace.
// Gets angular map lightprobe UV coordinate from a 3D coordinate.
static vec2 _GetUVAngularMap(vec3 xyz)
{
	float phi=-(float)acos(xyz.z), theta=(float)atan2(xyz.y, xyz.x);

	return Vec2(
		0.5f*((phi/PI)*(float)cos(theta))+0.5f,
		0.5f*((phi/PI)*(float)sin(theta))+0.5f);
}

// _GetXYZFace, internal function used by _AngularMapFace.
// Gets 3D coordinate from a 2D UV coordinate and face selection.
static vec3 _GetXYZFace(vec2 uv, int face)
{
	vec3 xyz;

	switch(face)
	{
		case 0: xyz=Vec3(1.0f, (uv.y-0.5f)*2.0f, (0.5f-uv.x)*2.0f);		break; // +X
		case 1: xyz=Vec3(-1.0f, (uv.y-0.5f)*2.0f, (uv.x-0.5f)*2.0f);	break; // -X
		case 2: xyz=Vec3((uv.x-0.5f)*2.0f, -1.0f, (uv.y-0.5f)*2.0f);	break; // +Y
		case 3: xyz=Vec3((uv.x-0.5f)*2.0f, 1.0f, (0.5f-uv.y)*2.0f);		break; // -Y
		case 4: xyz=Vec3((uv.x-0.5f)*2.0f, (uv.y-0.5f)*2.0f, 1.0f);		break; // +Z
		case 5: xyz=Vec3((0.5f-uv.x)*2.0f, (uv.y-0.5f)*2.0f, -1.0f);	break; // -Z
	}

	Vec3_Normalize(&xyz);

	return xyz;
}

// _AngularMapFace, internal function used by Image_Upload.
// Gets a selected 2D cubemap face from an angular lightmap probe image
static bool _AngularMapFace(VkuImage_t *in, int face, VkuImage_t *out)
{
	// Allocate memory for output
	out->data=(uint8_t *)Zone_Malloc(zone, (size_t)out->width*out->height*(out->depth>>3));

	if(out->data==NULL)
		return false;

	for(uint32_t y=0;y<out->height;y++)
	{
		float fy=(float)y/(out->height-1);

		for(uint32_t x=0;x<out->width;x++)
		{
			float fx=(float)x/(out->width-1);
			vec3 xyz;

			// UV coordinate into the image we're producing
			vec2 uv={ fx, fy };

			// Get a 3D cubemap coordinate for the given UV and selected cube face
			xyz=_GetXYZFace(uv, face);

			// Use that 3D coordinate and this function to transform it into a UV coord into an angular map lightprobe
			// TODO: This function can also be substituted for other image formats (lat/lon,  vertical cross, mirror ball),
			//		   maybe add a flag for different layouts?
			uv=_GetUVAngularMap(xyz);

			// Bilinearly sample the pixel from the source image and set it into the output data (destination cubemap face)
			_GetPixelBilinear(in, uv.x*in->width, uv.y*in->height, (void *)&out->data[(out->depth>>3)*(y*out->width+x)]);
		}
	}

	return true;
}

// _RGBtoRGBA, internal function used by Image_Upload.
// Converts any RGB format into an RGBA image.
//
// This is needed for Vulkan, most GPUs do not have RGB image format support.
// Also allows any helper functions here to be simplified by not needing to support as many bit depths.
static void _RGBtoRGBA(VkuImage_t *image)
{
	if(image->depth==96)
	{
		float *dst=(float *)Zone_Malloc(zone, sizeof(float)*image->width*image->height*4);

		if(dst==NULL)
			return;

		for(uint32_t i=0;i<image->width*image->height;i++)
		{
			uint32_t srcIdx=3*i;
			uint32_t dstIdx=4*i;

			dst[dstIdx+0]=((float *)image->data)[srcIdx+0];
			dst[dstIdx+1]=((float *)image->data)[srcIdx+1];
			dst[dstIdx+2]=((float *)image->data)[srcIdx+2];
			dst[dstIdx+3]=1.0f;
		}

		Zone_Free(zone, image->data);
		image->data=(uint8_t *)dst;
		image->depth=128;
	}
	else if(image->depth==48)
	{
		uint16_t *dst=(uint16_t *)Zone_Malloc(zone, sizeof(uint16_t)*image->width*image->height*4);

		if(dst==NULL)
			return;

		for(uint32_t i=0;i<image->width*image->height;i++)
		{
			uint32_t srcIdx=3*i;
			uint32_t dstIdx=4*i;

			dst[dstIdx+0]=((uint16_t *)image->data)[srcIdx+0];
			dst[dstIdx+1]=((uint16_t *)image->data)[srcIdx+1];
			dst[dstIdx+2]=((uint16_t *)image->data)[srcIdx+2];
			dst[dstIdx+3]=UINT16_MAX;
		}

		Zone_Free(zone, image->data);
		image->data=(uint8_t *)dst;
		image->depth=64;
	}
	else if(image->depth==24)
	{
		uint8_t *dst=(uint8_t *)Zone_Malloc(zone, sizeof(uint8_t)*image->width*image->height*4);

		if(dst==NULL)
			return;

		for(uint32_t i=0;i<image->width*image->height;i++)
		{
			uint32_t srcIdx=3*i;
			uint32_t dstIdx=4*i;

			dst[dstIdx+0]=image->data[srcIdx+0];
			dst[dstIdx+1]=image->data[srcIdx+1];
			dst[dstIdx+2]=image->data[srcIdx+2];
			dst[dstIdx+3]=UINT8_MAX;
		}

		Zone_Free(zone, image->data);
		image->data=dst;
		image->depth=32;
	}
}

// _GenerateMipmaps, internal function used by Image_Upload.
// Builds a Vulkan mipmap chain by using the GPU image blitter to resize the images.
static void _GenerateMipmaps(VkCommandBuffer commandBuffer, VkuImage_t *image, uint32_t mipLevels, uint32_t layerCount, uint32_t baseLayer)
{
	for(uint32_t i=1;i<mipLevels;i++)
	{
		vkuTransitionLayout(commandBuffer, image->image, 1, i-1, 1, baseLayer, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

		vkCmdBlitImage(commandBuffer, image->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &(VkImageBlit)
		{
			.srcOffsets[0]={ 0, 0, 0 },
			.srcOffsets[1]={ image->width>>(i-1), image->height>>(i-1), 1 },
			.srcSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
			.srcSubresource.mipLevel=i-1,
			.srcSubresource.baseArrayLayer=baseLayer,
			.srcSubresource.layerCount=layerCount,
			.dstOffsets[0]={ 0, 0, 0 },
			.dstOffsets[1]={ image->width>>i, image->height>>i, 1 },
			.dstSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
			.dstSubresource.mipLevel=i,
			.dstSubresource.baseArrayLayer=baseLayer,
			.dstSubresource.layerCount=layerCount,
		}, VK_FILTER_LINEAR);

		vkuTransitionLayout(commandBuffer, image->image, 1, i-1, layerCount, baseLayer, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}

	vkuTransitionLayout(commandBuffer, image->image, 1, mipLevels-1, layerCount, baseLayer, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

// Image_Upload, external function.
// Loads an image file and uploads to the GPU, while performing conversions
//   and other processes based on the set flags.
//
// Flags also set Vulkan texture sampler properties.
VkBool32 Image_Upload(VkuContext_t *context, VkuImage_t *image, const char *filename, uint32_t flags)
{
	const char *extension=strrchr(filename, '.');
	VkFilter minFilter=VK_FILTER_LINEAR;
	VkFilter magFilter=VK_FILTER_LINEAR;
	VkSamplerMipmapMode mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
	VkSamplerAddressMode wrapModeU=VK_SAMPLER_ADDRESS_MODE_REPEAT;
	VkSamplerAddressMode wrapModeV=VK_SAMPLER_ADDRESS_MODE_REPEAT;
	VkSamplerAddressMode wrapModeW=VK_SAMPLER_ADDRESS_MODE_REPEAT;
	VkFormat format=VK_FORMAT_UNDEFINED;
	VkCommandBuffer commandBuffer;
	VkuBuffer_t stagingBuffer;
	uint32_t mipLevels=1;
	void *data=NULL;

	if(extension!=NULL)
	{
		if(!strcmp(extension, ".tga"))
		{
			if(!TGA_Load(filename, image))
				return VK_FALSE;
		}
		else if(!strcmp(extension, ".qoi"))
		{
			if(!QOI_Load(filename, image))
				return VK_FALSE;
		}
		else
			return VK_FALSE;
	}

	if(flags&IMAGE_NEAREST)
	{
		magFilter=VK_FILTER_NEAREST;
		mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
		minFilter=VK_FILTER_NEAREST;
	}

	if(flags&IMAGE_BILINEAR)
	{
		magFilter=VK_FILTER_LINEAR;
		mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
		minFilter=VK_FILTER_LINEAR;
	}

	if(flags&IMAGE_TRILINEAR)
	{
		magFilter=VK_FILTER_LINEAR;
		mipmapMode=VK_SAMPLER_MIPMAP_MODE_LINEAR;
		minFilter=VK_FILTER_LINEAR;
	}

	if(flags&IMAGE_CLAMP_U)
		wrapModeU=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

	if(flags&IMAGE_CLAMP_V)
		wrapModeV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

	if(flags&IMAGE_CLAMP_W)
		wrapModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

	if(flags&IMAGE_REPEAT_U)
		wrapModeU=VK_SAMPLER_ADDRESS_MODE_REPEAT;

	if(flags&IMAGE_REPEAT_V)
		wrapModeV=VK_SAMPLER_ADDRESS_MODE_REPEAT;

	if(flags&IMAGE_REPEAT_W)
		wrapModeW=VK_SAMPLER_ADDRESS_MODE_REPEAT;

	// Convert from RGB to RGBA if needed.
	_RGBtoRGBA(image);

	if(flags&IMAGE_RGBE)
		_RGBE2Float(image);

	if(flags&IMAGE_NORMALMAP)
		_MakeNormalMap(image);

	if(flags&IMAGE_NORMALIZE)
		_Normalize(image);

	switch(image->depth)
	{
		case 128:
			format=VK_FORMAT_R32G32B32A32_SFLOAT;
			break;

		case 64:
			format=VK_FORMAT_R16G16B16A16_UNORM;
			break;

		case 32:
			format=VK_FORMAT_B8G8R8A8_SRGB;
			break;

		case 16:
			format=VK_FORMAT_R5G6B5_UNORM_PACK16;
			break;

		case 8:
			format=VK_FORMAT_R8_UNORM;
			break;

		default:
			Zone_Free(zone, image->data);
			return VK_FALSE;
	}

	// Upload as cubemap if that flag was set
	if(flags&IMAGE_CUBEMAP_ANGULAR)
	{
		VkuImage_t Out;
		void *data=NULL;

		// Calculate the cubemap face size from the angular map size (half the size of the original seems to be good)
		Out.width=NextPower2(image->width>>1);
		Out.height=NextPower2(image->height>>1);
		Out.depth=image->depth;

		// Precalculate each cube face iamge size in bytes
		uint32_t size=Out.width*Out.height*(image->depth>>3);

		// Create staging buffer
		vkuCreateHostBuffer(context, &stagingBuffer, size*6, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

		// Map image memory and copy data for each cube face
		data=stagingBuffer.memory->mappedPointer;
		for(uint32_t i=0;i<6;i++)
		{
			// Extract the cubemap face from the angular map lightprobe
			if(!_AngularMapFace(image, i, &Out))
				return VK_FALSE;

			// Copy the image into the staging buffer
			memcpy((uint8_t *)data+((size_t)size*i), Out.data, size);

			// Free the output data for the next face
			Zone_Free(zone, Out.data);
		}

		// All faces are copied, free the original image data
		Zone_Free(zone, image->data);

		// Going from an angular map to 6 faces, the faces are a different size than the original image,
		//	so change the width/height of the working image we're loading.
		image->width=Out.width;
		image->height=Out.height;

		// If mipmaps are requested, calculate how many levels there are
		if(flags&IMAGE_MIPMAP)
			mipLevels=ComputeLog(max(image->width, image->height))+1;

		// Create the GPU side image memory buffer
		vkuCreateImageBuffer(context, image,
							 VK_IMAGE_TYPE_2D, format, mipLevels, 6, Out.width, Out.height, 1, VK_SAMPLE_COUNT_1_BIT,
							 format==VK_FORMAT_R32G32B32A32_SFLOAT?VK_IMAGE_TILING_LINEAR:VK_IMAGE_TILING_OPTIMAL,
							 VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,
							 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
							 VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);

		// Start a one shot command buffer
		commandBuffer=vkuOneShotCommandBufferBegin(context);

		// Change image layout from undefined to destination optimal, so we can copy from the staging buffer to the texture.
		vkuTransitionLayout(commandBuffer, image->image, mipLevels, 0, 6, 0, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		// Copy all faces from staging buffer to the texture image buffer.
		vkCmdCopyBufferToImage(commandBuffer, stagingBuffer.buffer, image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 6, (VkBufferImageCopy[6])
		{
			{ (VkDeviceSize)size*0, 0, 0, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 }, { 0, 0, 0 }, { Out.width, Out.height, 1 } },
			{ (VkDeviceSize)size*1, 0, 0, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 1 }, { 0, 0, 0 }, { Out.width, Out.height, 1 } },
			{ (VkDeviceSize)size*2, 0, 0, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 2, 1 }, { 0, 0, 0 }, { Out.width, Out.height, 1 } },
			{ (VkDeviceSize)size*3, 0, 0, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 3, 1 }, { 0, 0, 0 }, { Out.width, Out.height, 1 } },
			{ (VkDeviceSize)size*4, 0, 0, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 4, 1 }, { 0, 0, 0 }, { Out.width, Out.height, 1 } },
			{ (VkDeviceSize)size*5, 0, 0, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 5, 1 }, { 0, 0, 0 }, { Out.width, Out.height, 1 } }
		});

		// Final change to image layout from destination optimal to be optimal reading only by shader.
		// This is also done by generating mipmaps, if requested.
		if(flags&IMAGE_MIPMAP)
		{
			_GenerateMipmaps(commandBuffer, image, mipLevels, 6, 0);
			_GenerateMipmaps(commandBuffer, image, mipLevels, 6, 1);
			_GenerateMipmaps(commandBuffer, image, mipLevels, 6, 2);
			_GenerateMipmaps(commandBuffer, image, mipLevels, 6, 3);
			_GenerateMipmaps(commandBuffer, image, mipLevels, 6, 4);
			_GenerateMipmaps(commandBuffer, image, mipLevels, 6, 5);
		}
		else
			vkuTransitionLayout(commandBuffer, image->image, mipLevels, 0, 6, 0, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		// End one shot command buffer and submit
		vkuOneShotCommandBufferEnd(context, commandBuffer);

		// Delete staging buffers
		vkuDestroyBuffer(context, &stagingBuffer);
	}
	else // Otherwise it's a 2D texture
	{
		// Byte size of image data
		uint32_t size=image->width*image->height*(image->depth>>3);

		// Create staging buffer
		vkuCreateHostBuffer(context, &stagingBuffer, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

		// Map image memory and copy data
		data=stagingBuffer.memory->mappedPointer;
		memcpy(data, image->data, size);

		// Original image data is now in a Vulkan memory object, so no longer need the original data.
		Zone_Free(zone, image->data);

		if(flags&IMAGE_MIPMAP)
			mipLevels=ComputeLog(max(image->width, image->height))+1;

		if(!vkuCreateImageBuffer(context, image,
		   VK_IMAGE_TYPE_2D, format, mipLevels, 1, image->width, image->height, 1,
		   VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
		   VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0))
			return VK_FALSE;

		// Start a one shot command buffer
		commandBuffer=vkuOneShotCommandBufferBegin(context);

		// Change image layout from undefined to destination optimal, so we can copy from the staging buffer to the texture.
		vkuTransitionLayout(commandBuffer, image->image, mipLevels, 0, 1, 0, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		// Copy from staging buffer to the texture buffer.
		vkCmdCopyBufferToImage(commandBuffer, stagingBuffer.buffer, image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, (VkBufferImageCopy[1])
		{
			{ 0, 0, 0, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 }, { 0, 0, 0 }, { image->width, image->height, 1 } }
		});

		// Final change to image layout from destination optimal to be optimal reading only by shader.
		// This is also done by generating mipmaps, if requested.
		if(flags&IMAGE_MIPMAP)
			_GenerateMipmaps(commandBuffer, image, mipLevels, 1, 0);
		else
			vkuTransitionLayout(commandBuffer, image->image, mipLevels, 0, 1, 0, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		// End one shot command buffer and submit
		vkuOneShotCommandBufferEnd(context, commandBuffer);

		// Delete staging buffers
		vkuDestroyBuffer(context, &stagingBuffer);
	}

	// Create texture sampler object
	vkCreateSampler(context->device, &(VkSamplerCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter=magFilter,
		.minFilter=minFilter,
		.mipmapMode=mipmapMode,
		.addressModeU=wrapModeU,
		.addressModeV=wrapModeV,
		.addressModeW=wrapModeW,
		.mipLodBias=0.0f,
		.compareOp=VK_COMPARE_OP_NEVER,
		.minLod=0.0f,
		.maxLod=VK_LOD_CLAMP_NONE,
		.maxAnisotropy=1.0f,
		.anisotropyEnable=VK_FALSE,
		.borderColor=VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
	}, VK_NULL_HANDLE, &image->sampler);

	// Create texture image view object
	vkCreateImageView(context->device, &(VkImageViewCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image=image->image,
		.viewType=flags&IMAGE_CUBEMAP_ANGULAR?VK_IMAGE_VIEW_TYPE_CUBE:VK_IMAGE_VIEW_TYPE_2D,
		.format=format,
		.components={ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
		.subresourceRange={ VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, flags&IMAGE_CUBEMAP_ANGULAR?6:1 },
	}, VK_NULL_HANDLE, &image->imageView);

	return VK_TRUE;
}
