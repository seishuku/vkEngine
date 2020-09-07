#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include "vulkan.h"
#include "math.h"
#include "image.h"

#ifndef FREE
#define FREE(p) { if(p) { free(p); p=NULL; } }
#endif

#ifndef min
#define min(a, b) ((a)<(b)?(a):(b))
#endif

#ifndef max
#define max(a, b) ((a)>(b)?(a):(b))
#endif

extern VkDevice device;

extern VkPhysicalDeviceMemoryProperties deviceMemProperties;

extern VkCommandPool commandPool;

extern uint32_t queueFamilyIndex;
extern VkQueue queue;

void _MakeNormalMap(Image_t *Image)
{
	int x, y, xx, yy;
	int Channels=Image->Depth>>3;
	unsigned short *Buffer=NULL;
	const float OneOver255=1.0f/255.0f;
	float KernelX[9]={ 1.0f, 0.0f, -1.0f, 2.0f, 0.0f, -2.0f, 1.0f, 0.0f, -1.0f };
	float KernelY[9]={ -1.0f, -2.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 2.0f, 1.0f };

	if(!((Image->Depth==32)||(Image->Depth==24)||(Image->Depth==8)))
		return;

	Buffer=(unsigned short *)malloc(sizeof(unsigned short)*Image->Width*Image->Height*4);

	if(Buffer==NULL)
		return;

	for(y=0;y<Image->Height;y++)
	{
		for(x=0;x<Image->Width;x++)
		{
			float n[3]={ 0.0f, 0.0f, 1.0f }, mag;

			for(yy=0;yy<3;yy++)
			{
				int oy=min(Image->Height-1, y+yy);

				for(xx=0;xx<3;xx++)
				{
					int ox=min(Image->Width-1, x+xx);

					n[0]+=KernelX[yy*3+xx]*(float)(Image->Data[Channels*(oy*Image->Width+ox)]*OneOver255);
					n[1]+=KernelY[yy*3+xx]*(float)(Image->Data[Channels*(oy*Image->Width+ox)]*OneOver255);
				}
			}

			mag=1.0f/sqrtf(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
			n[0]*=mag;
			n[1]*=mag;
			n[2]*=mag;

			Buffer[4*(y*Image->Width+x)+0]=(unsigned short)(65535.0f*(0.5f*n[0]+0.5f));
			Buffer[4*(y*Image->Width+x)+1]=(unsigned short)(65535.0f*(0.5f*n[1]+0.5f));
			Buffer[4*(y*Image->Width+x)+2]=(unsigned short)(65535.0f*(0.5f*n[2]+0.5f));

			if(Channels==4)
				Buffer[4*(y*Image->Width+x)+3]=(unsigned short)(Image->Data[4*(y*Image->Width+x)+3]<<8);
			else
				Buffer[4*(y*Image->Width+x)+3]=65535;
		}
	}

	Image->Depth=64;

	FREE(Image->Data);
	Image->Data=(unsigned char *)Buffer;
}

void _Normalize(Image_t *Image)
{
	int i, Channels=Image->Depth>>3;
	unsigned short *Buffer=NULL;
	const float OneOver255=1.0f/255.0f;

	if(!((Image->Depth==32)||(Image->Depth==24)))
		return;

	Buffer=(unsigned short *)malloc(sizeof(unsigned short)*Image->Width*Image->Height*4);

	if(Buffer==NULL)
		return;

	for(i=0;i<Image->Width*Image->Height;i++)
	{
		float n[3], mag;

		n[0]=2.0f*((float)Image->Data[Channels*i+2]*OneOver255)-1.0f;
		n[1]=2.0f*((float)Image->Data[Channels*i+1]*OneOver255)-1.0f;
		n[2]=2.0f*((float)Image->Data[Channels*i+0]*OneOver255)-1.0f;

		mag=1.0f/sqrtf(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);
		n[0]*=mag;
		n[1]*=mag;
		n[2]*=mag;

		Buffer[4*i+0]=(unsigned short)(65535.0f*(0.5f*n[0]+0.5f));
		Buffer[4*i+1]=(unsigned short)(65535.0f*(0.5f*n[1]+0.5f));
		Buffer[4*i+2]=(unsigned short)(65535.0f*(0.5f*n[2]+0.5f));

		if(Channels==4)
			Buffer[4*i+3]=(unsigned short)(Image->Data[4*i+3]<<8);
		else
			Buffer[4*i+3]=65535;
	}

	Image->Depth=64;

	FREE(Image->Data);
	Image->Data=(unsigned char *)Buffer;
}

void _RGBE2Float(Image_t *Image)
{
	int i;
	float *Buffer=NULL;

	Buffer=(float *)malloc(sizeof(float)*Image->Width*Image->Height*3);

	if(Buffer==NULL)
		return;

	for(i=0;i<Image->Width*Image->Height;i++)
	{
		unsigned char *rgbe=&Image->Data[4*i];
		float *rgb=&Buffer[3*i];

		if(rgbe[3])
		{
			float f=1.0f;
			int i, e=rgbe[3]-(128+8);

			if(e>0)
			{
				for(i=0;i<e;i++)
					f*=2.0f;
			}
			else
			{
				for(i=0;i<-e;i++)
					f/=2.0f;
			}

			rgb[0]=((float)rgbe[0]+0.5f)*f;
			rgb[1]=((float)rgbe[1]+0.5f)*f;
			rgb[2]=((float)rgbe[2]+0.5f)*f;
		}
		else
			rgb[0]=rgb[1]=rgb[2]=0.0f;
	}

	Image->Depth=96;

	FREE(Image->Data);
	Image->Data=(unsigned char *)Buffer;
}

void _Resample(Image_t *Src, Image_t *Dst)
{
	float fx, fy, hx, hy, lx, ly, sx, sy;
	float xPercent, yPercent, Percent;
	float Total[4], Sum;
	int x, y, iy, ix, Index;

	if(Dst->Data==NULL)
		return;

	sx=(float)Src->Width/Dst->Width;
	sy=(float)Src->Height/Dst->Height;

	for(y=0;y<Dst->Height;y++)
	{
		if(Src->Height>Dst->Height)
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

		for(x=0;x<Dst->Width;x++)
		{
			if(Src->Width>Dst->Width)
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

			Total[0]=Total[1]=Total[2]=Total[3]=Sum=0.0f;

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

					Percent=xPercent*xPercent;
					Sum+=Percent;

					Index=min(Src->Height-1, iy)*Src->Width+min(Src->Width-1, ix);

					switch(Src->Depth)
					{
						case 128:
							Total[0]+=((float *)Src->Data)[4*Index+0]*Percent;
							Total[1]+=((float *)Src->Data)[4*Index+1]*Percent;
							Total[2]+=((float *)Src->Data)[4*Index+2]*Percent;
							Total[3]+=((float *)Src->Data)[4*Index+3]*Percent;
							break;

						case 96:
							Total[0]+=((float *)Src->Data)[3*Index+0]*Percent;
							Total[1]+=((float *)Src->Data)[3*Index+1]*Percent;
							Total[2]+=((float *)Src->Data)[3*Index+2]*Percent;
							break;

						case 64:
							Total[0]+=((unsigned short *)Src->Data)[4*Index+0]*Percent;
							Total[1]+=((unsigned short *)Src->Data)[4*Index+1]*Percent;
							Total[2]+=((unsigned short *)Src->Data)[4*Index+2]*Percent;
							Total[3]+=((unsigned short *)Src->Data)[4*Index+3]*Percent;
							break;

						case 48:
							Total[0]+=((unsigned short *)Src->Data)[3*Index+0]*Percent;
							Total[1]+=((unsigned short *)Src->Data)[3*Index+1]*Percent;
							Total[2]+=((unsigned short *)Src->Data)[3*Index+2]*Percent;
							break;

						case 32:
							Total[0]+=(float)Src->Data[4*Index+0]*Percent;
							Total[1]+=(float)Src->Data[4*Index+1]*Percent;
							Total[2]+=(float)Src->Data[4*Index+2]*Percent;
							Total[3]+=(float)Src->Data[4*Index+3]*Percent;
							break;

						case 24:
							Total[0]+=Src->Data[3*Index+0]*Percent;
							Total[1]+=Src->Data[3*Index+1]*Percent;
							Total[2]+=Src->Data[3*Index+2]*Percent;
							break;

						case 16:
							Total[0]+=((((unsigned short *)Src->Data)[Index]>>0x0)&0x1F)*Percent;
							Total[1]+=((((unsigned short *)Src->Data)[Index]>>0x5)&0x1F)*Percent;
							Total[2]+=((((unsigned short *)Src->Data)[Index]>>0xA)&0x1F)*Percent;
							break;

						case 8:
							Total[0]+=Src->Data[Index]*Percent;
					}

					ix++;
					fx=(float)ix;
				}

				iy++;
				fy=(float)iy;
			}

			Index=y*Dst->Width+x;
			Sum=1.0f/Sum;

			switch(Dst->Depth)
			{
				case 128:
					((float *)Dst->Data)[4*Index+0]=(float)(Total[0]*Sum);
					((float *)Dst->Data)[4*Index+1]=(float)(Total[1]*Sum);
					((float *)Dst->Data)[4*Index+2]=(float)(Total[2]*Sum);
					((float *)Dst->Data)[4*Index+3]=(float)(Total[3]*Sum);
					break;

				case 96:
					((float *)Dst->Data)[3*Index+0]=(float)(Total[0]*Sum);
					((float *)Dst->Data)[3*Index+1]=(float)(Total[1]*Sum);
					((float *)Dst->Data)[3*Index+2]=(float)(Total[2]*Sum);
					break;

				case 64:
					((unsigned short *)Dst->Data)[4*Index+0]=(unsigned short)(Total[0]*Sum);
					((unsigned short *)Dst->Data)[4*Index+1]=(unsigned short)(Total[1]*Sum);
					((unsigned short *)Dst->Data)[4*Index+2]=(unsigned short)(Total[2]*Sum);
					((unsigned short *)Dst->Data)[4*Index+3]=(unsigned short)(Total[3]*Sum);
					break;

				case 48:
					((unsigned short *)Dst->Data)[3*Index+0]=(unsigned short)(Total[0]*Sum);
					((unsigned short *)Dst->Data)[3*Index+1]=(unsigned short)(Total[1]*Sum);
					((unsigned short *)Dst->Data)[3*Index+2]=(unsigned short)(Total[2]*Sum);
					break;

				case 32:
					((unsigned char *)Dst->Data)[4*Index+0]=(unsigned char)(Total[0]*Sum);
					((unsigned char *)Dst->Data)[4*Index+1]=(unsigned char)(Total[1]*Sum);
					((unsigned char *)Dst->Data)[4*Index+2]=(unsigned char)(Total[2]*Sum);
					((unsigned char *)Dst->Data)[4*Index+3]=(unsigned char)(Total[3]*Sum);
					break;

				case 24:
					((unsigned char *)Dst->Data)[3*Index+0]=(unsigned char)(Total[0]*Sum);
					((unsigned char *)Dst->Data)[3*Index+1]=(unsigned char)(Total[1]*Sum);
					((unsigned char *)Dst->Data)[3*Index+2]=(unsigned char)(Total[2]*Sum);
					break;

				case 16:
					((unsigned short *)Dst->Data)[Index]=((unsigned short)((Total[0]*Sum))&0x1F)<<0x0|((unsigned short)(Total[1]*Sum)&0x1F)<<0x5|((unsigned short)(Total[2]*Sum)&0x1F)<<0xA;
					break;

				case 8:
					((unsigned char *)Dst->Data)[Index]=(unsigned char)(Total[0]*Sum);
					break;
			}
		}
	}
}

void _BuildMipmaps(Image_t *Image, unsigned int Target)
{
	int i=0, levels;
	unsigned int MaxSize=UINT_MAX;
	Image_t Dst;

//	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &MaxSize);

	Dst.Depth=Image->Depth;
	Dst.Width=min(MaxSize, NextPower2(Image->Width));
	Dst.Height=min(MaxSize, NextPower2(Image->Height));

	if(Dst.Height>Dst.Width)
		levels=ComputeLog(Dst.Height);
	else
		levels=ComputeLog(Dst.Width);

	while(i<=levels)
	{
		Dst.Data=(unsigned char *)malloc(Dst.Width*Dst.Height*(Dst.Depth>>3));

		_Resample(Image, &Dst);

		switch(Dst.Depth)
		{
			case 128:
//				glTexImage2D(Target, i, GL_RGBA16, Dst.Width, Dst.Height, 0, GL_RGBA, GL_FLOAT, Dst.Data);
				break;

			case 96:
//				glTexImage2D(Target, i, GL_RGB16, Dst.Width, Dst.Height, 0, GL_RGB, GL_FLOAT, Dst.Data);
				break;

			case 64:
//				glTexImage2D(Target, i, GL_RGBA16, Dst.Width, Dst.Height, 0, GL_RGBA, GL_UNSIGNED_SHORT, Dst.Data);
				break;

			case 48:
//				glTexImage2D(Target, i, GL_RGB16, Dst.Width, Dst.Height, 0, GL_RGB, GL_UNSIGNED_SHORT, Dst.Data);
				break;

			case 32:
//				glTexImage2D(Target, i, GL_RGBA8, Dst.Width, Dst.Height, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, Dst.Data);
				break;

			case 24:
//				glTexImage2D(Target, i, GL_RGB8, Dst.Width, Dst.Height, 0, GL_BGR, GL_UNSIGNED_BYTE, Dst.Data);
				break;

			case 16:
//				glTexImage2D(Target, i, GL_RGB5, Dst.Width, Dst.Height, 0, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, Dst.Data);
				break;

			case 8:
//				glTexImage2D(Target, i, GL_INTENSITY8, Dst.Width, Dst.Height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, Dst.Data);
				break;
		}

		FREE(Dst.Data);

		Dst.Width=(Dst.Width>1)?Dst.Width>>1:Dst.Width;
		Dst.Height=(Dst.Height>1)?Dst.Height>>1:Dst.Height;
		i++;
	}
}

void _GetPixelBilinear(Image_t *Image, float x, float y, unsigned char *Out)
{
	int ix=(int)x, iy=(int)y;
	int ox=ix+1, oy=iy+1;
	float fx=x-ix, fy=y-iy;
	float w00, w01, w10, w11;

	if(ox>=Image->Width)
		ox=Image->Width-1;

	if(oy>=Image->Height)
		oy=Image->Height-1;

	if(fx<0.0f)
		ix=ox=0;

	if(fy<0.0f)
		iy=oy=0;

	w11=fx*fy;
	w00=1.0f-fx-fy+w11;
	w10=fx-w11;
	w01=fy-w11;

	switch(Image->Depth)
	{
		case 128:
			((float *)Out)[0]=((float *)Image->Data)[4*(iy*Image->Width+ix)+0]*w00+((float *)Image->Data)[4*(iy*Image->Width+ox)+0]*w10+((float *)Image->Data)[4*(oy*Image->Width+ix)+0]*w01+((float *)Image->Data)[4*(oy*Image->Width+ox)+0]*w11;
			((float *)Out)[1]=((float *)Image->Data)[4*(iy*Image->Width+ix)+1]*w00+((float *)Image->Data)[4*(iy*Image->Width+ox)+1]*w10+((float *)Image->Data)[4*(oy*Image->Width+ix)+1]*w01+((float *)Image->Data)[4*(oy*Image->Width+ox)+1]*w11;
			((float *)Out)[2]=((float *)Image->Data)[4*(iy*Image->Width+ix)+2]*w00+((float *)Image->Data)[4*(iy*Image->Width+ox)+2]*w10+((float *)Image->Data)[4*(oy*Image->Width+ix)+2]*w01+((float *)Image->Data)[4*(oy*Image->Width+ox)+2]*w11;
			((float *)Out)[3]=((float *)Image->Data)[4*(iy*Image->Width+ix)+3]*w00+((float *)Image->Data)[4*(iy*Image->Width+ox)+3]*w10+((float *)Image->Data)[4*(oy*Image->Width+ix)+3]*w01+((float *)Image->Data)[4*(oy*Image->Width+ox)+3]*w11;
			break;

		case 96:
			((float *)Out)[0]=((float *)Image->Data)[3*(iy*Image->Width+ix)+0]*w00+((float *)Image->Data)[3*(iy*Image->Width+ox)+0]*w10+((float *)Image->Data)[3*(oy*Image->Width+ix)+0]*w01+((float *)Image->Data)[3*(oy*Image->Width+ox)+0]*w11;
			((float *)Out)[1]=((float *)Image->Data)[3*(iy*Image->Width+ix)+1]*w00+((float *)Image->Data)[3*(iy*Image->Width+ox)+1]*w10+((float *)Image->Data)[3*(oy*Image->Width+ix)+1]*w01+((float *)Image->Data)[3*(oy*Image->Width+ox)+1]*w11;
			((float *)Out)[2]=((float *)Image->Data)[3*(iy*Image->Width+ix)+2]*w00+((float *)Image->Data)[3*(iy*Image->Width+ox)+2]*w10+((float *)Image->Data)[3*(oy*Image->Width+ix)+2]*w01+((float *)Image->Data)[3*(oy*Image->Width+ox)+2]*w11;
			break;

		case 64:
			((unsigned short *)Out)[0]=(unsigned short)(((unsigned short *)Image->Data)[4*(iy*Image->Width+ix)+0]*w00+((unsigned short *)Image->Data)[4*(iy*Image->Width+ox)+0]*w10+((unsigned short *)Image->Data)[4*(oy*Image->Width+ix)+0]*w01+((unsigned short *)Image->Data)[4*(oy*Image->Width+ox)+0]*w11);
			((unsigned short *)Out)[1]=(unsigned short)(((unsigned short *)Image->Data)[4*(iy*Image->Width+ix)+1]*w00+((unsigned short *)Image->Data)[4*(iy*Image->Width+ox)+1]*w10+((unsigned short *)Image->Data)[4*(oy*Image->Width+ix)+1]*w01+((unsigned short *)Image->Data)[4*(oy*Image->Width+ox)+1]*w11);
			((unsigned short *)Out)[2]=(unsigned short)(((unsigned short *)Image->Data)[4*(iy*Image->Width+ix)+2]*w00+((unsigned short *)Image->Data)[4*(iy*Image->Width+ox)+2]*w10+((unsigned short *)Image->Data)[4*(oy*Image->Width+ix)+2]*w01+((unsigned short *)Image->Data)[4*(oy*Image->Width+ox)+2]*w11);
			((unsigned short *)Out)[3]=(unsigned short)(((unsigned short *)Image->Data)[4*(iy*Image->Width+ix)+3]*w00+((unsigned short *)Image->Data)[4*(iy*Image->Width+ox)+3]*w10+((unsigned short *)Image->Data)[4*(oy*Image->Width+ix)+3]*w01+((unsigned short *)Image->Data)[4*(oy*Image->Width+ox)+3]*w11);
			break;

		case 48:
			((unsigned short *)Out)[0]=(unsigned short)(((unsigned short *)Image->Data)[3*(iy*Image->Width+ix)+0]*w00+((unsigned short *)Image->Data)[3*(iy*Image->Width+ox)+0]*w10+((unsigned short *)Image->Data)[3*(oy*Image->Width+ix)+0]*w01+((unsigned short *)Image->Data)[3*(oy*Image->Width+ox)+0]*w11);
			((unsigned short *)Out)[1]=(unsigned short)(((unsigned short *)Image->Data)[3*(iy*Image->Width+ix)+1]*w00+((unsigned short *)Image->Data)[3*(iy*Image->Width+ox)+1]*w10+((unsigned short *)Image->Data)[3*(oy*Image->Width+ix)+1]*w01+((unsigned short *)Image->Data)[3*(oy*Image->Width+ox)+1]*w11);
			((unsigned short *)Out)[2]=(unsigned short)(((unsigned short *)Image->Data)[3*(iy*Image->Width+ix)+2]*w00+((unsigned short *)Image->Data)[3*(iy*Image->Width+ox)+2]*w10+((unsigned short *)Image->Data)[3*(oy*Image->Width+ix)+2]*w01+((unsigned short *)Image->Data)[3*(oy*Image->Width+ox)+2]*w11);
			break;

		case 32:
			Out[0]=(unsigned char)(Image->Data[4*(iy*Image->Width+ix)+0]*w00+Image->Data[4*(iy*Image->Width+ox)+0]*w10+Image->Data[4*(oy*Image->Width+ix)+0]*w01+Image->Data[4*(oy*Image->Width+ox)+0]*w11);
			Out[1]=(unsigned char)(Image->Data[4*(iy*Image->Width+ix)+1]*w00+Image->Data[4*(iy*Image->Width+ox)+1]*w10+Image->Data[4*(oy*Image->Width+ix)+1]*w01+Image->Data[4*(oy*Image->Width+ox)+1]*w11);
			Out[2]=(unsigned char)(Image->Data[4*(iy*Image->Width+ix)+2]*w00+Image->Data[4*(iy*Image->Width+ox)+2]*w10+Image->Data[4*(oy*Image->Width+ix)+2]*w01+Image->Data[4*(oy*Image->Width+ox)+2]*w11);
			Out[3]=(unsigned char)(Image->Data[4*(iy*Image->Width+ix)+3]*w00+Image->Data[4*(iy*Image->Width+ox)+3]*w10+Image->Data[4*(oy*Image->Width+ix)+3]*w01+Image->Data[4*(oy*Image->Width+ox)+3]*w11);
			break;

		case 24:
			Out[0]=(unsigned char)(Image->Data[3*(iy*Image->Width+ix)+0]*w00+Image->Data[3*(iy*Image->Width+ox)+0]*w10+Image->Data[3*(oy*Image->Width+ix)+0]*w01+Image->Data[3*(oy*Image->Width+ox)+0]*w11);
			Out[1]=(unsigned char)(Image->Data[3*(iy*Image->Width+ix)+1]*w00+Image->Data[3*(iy*Image->Width+ox)+1]*w10+Image->Data[3*(oy*Image->Width+ix)+1]*w01+Image->Data[3*(oy*Image->Width+ox)+1]*w11);
			Out[2]=(unsigned char)(Image->Data[3*(iy*Image->Width+ix)+2]*w00+Image->Data[3*(iy*Image->Width+ox)+2]*w10+Image->Data[3*(oy*Image->Width+ix)+2]*w01+Image->Data[3*(oy*Image->Width+ox)+2]*w11);
			break;

		case 16:
		{
			unsigned short p0=((unsigned short *)Image->Data)[iy*Image->Width+ix];
			unsigned short p1=((unsigned short *)Image->Data)[iy*Image->Width+ox];
			unsigned short p2=((unsigned short *)Image->Data)[oy*Image->Width+ix];
			unsigned short p3=((unsigned short *)Image->Data)[oy*Image->Width+ox];

			*((unsigned short *)Out) =(unsigned short)(((p0>>0x0)&0x1F)*w00+((p1>>0x0)&0x1F)*w10+((p2>>0x0)&0x1F)*w01+((p3>>0x0)&0x1F)*w11)<<0x0;
			*((unsigned short *)Out)|=(unsigned short)(((p0>>0x5)&0x1F)*w00+((p1>>0x5)&0x1F)*w10+((p2>>0x5)&0x1F)*w01+((p3>>0x5)&0x1F)*w11)<<0x5;
			*((unsigned short *)Out)|=(unsigned short)(((p0>>0xA)&0x1F)*w00+((p1>>0xA)&0x1F)*w10+((p2>>0xA)&0x1F)*w01+((p3>>0xA)&0x1F)*w11)<<0xA;
			break;
		}

		case 8:
			*Out=(unsigned char)(Image->Data[iy*Image->Width+ix]*w00+Image->Data[iy*Image->Width+ox]*w10+Image->Data[oy*Image->Width+ix]*w01+Image->Data[oy*Image->Width+ox]*w11);
			break;
	}
}

void _GetUVAngularMap(float xyz[3], float *uv)
{
	float phi=-(float)acos(xyz[2]), theta=(float)atan2(xyz[1], xyz[0]);

	uv[0]=0.5f*((phi/3.1415926f)*(float)cos(theta))+0.5f;
	uv[1]=0.5f*((phi/3.1415926f)*(float)sin(theta))+0.5f;
}

void _GetXYZFace(float uv[2], float *xyz, int face)
{
	float mag;

	switch(face)
	{
		// +X
		case 0:
			xyz[0]=1.0f;
			xyz[1]=(uv[1]-0.5f)*2.0f;
			xyz[2]=(0.5f-uv[0])*2.0f;
			break;

		// -X
		case 1:
			xyz[0]=-1.0f;
			xyz[1]=(uv[1]-0.5f)*2.0f;
			xyz[2]=(uv[0]-0.5f)*2.0f;
			break;

		// +Y
		case 2:
			xyz[0]=(uv[0]-0.5f)*2.0f;
			xyz[1]=-1.0f;
			xyz[2]=(uv[1]-0.5f)*2.0f;
			break;

		// -Y
		case 3:
			xyz[0]=(uv[0]-0.5f)*2.0f;
			xyz[1]=1.0f;
			xyz[2]=(0.5f-uv[1])*2.0f;
			break;

		// +Z
		case 4:
			xyz[0]=(uv[0]-0.5f)*2.0f;
			xyz[1]=(uv[1]-0.5f)*2.0f;
			xyz[2]=1.0f;
			break;

		// -Z
		case 5:
			xyz[0]=(0.5f-uv[0])*2.0f;
			xyz[1]=(uv[1]-0.5f)*2.0f;
			xyz[2]=-1.0f;
			break;
	}

	mag=sqrtf(xyz[0]*xyz[0]+xyz[1]*xyz[1]+xyz[2]*xyz[2]);

	if(mag)
	{
		mag=1.0f/mag;
		xyz[0]*=mag;
		xyz[1]*=mag;
		xyz[2]*=mag;
	}
}

void _AngularMapFace(Image_t *In, int Face, int Mipmap)
{
	Image_t Out;
	int x, y;
	VkFormat Format;

	switch(In->Depth)
	{
		case 128:
			Format=VK_FORMAT_R32G32B32A32_SFLOAT;
			break;

		case 96:
			Format=VK_FORMAT_R32G32B32_SFLOAT;
			break;

		case 64:
			Format=VK_FORMAT_R16G16B16A16_SFLOAT;
			break;

		case 48:
			Format=VK_FORMAT_R16G16B16_SFLOAT;
			break;

		case 32:
			Format=VK_FORMAT_R8G8B8A8_UNORM;
			break;

		case 24:
			Format=VK_FORMAT_R8G8B8_UNORM;
			break;

 		case 16:
			Format=VK_FORMAT_A1R5G5B5_UNORM_PACK16;
			break;

		case 8:
			Format=VK_FORMAT_R8_UNORM;
			break;

		default:
			return;
	}

	Out.Depth=In->Depth;
	Out.Width=NextPower2(In->Width>>1);
	Out.Height=NextPower2(In->Height>>1);
	Out.Data=(unsigned char *)malloc(Out.Width*Out.Height*(Out.Depth>>3));

	for(y=0;y<Out.Height;y++)
	{
		float fy=(float)y/(Out.Height-1);

		for(x=0;x<Out.Width;x++)
		{
			float fx=(float)x/(Out.Width-1);
			float uv[2]={ fx, fy }, xyz[3];

			_GetXYZFace(uv, xyz, Face);
			_GetUVAngularMap(xyz, uv);

			_GetPixelBilinear(In, uv[0]*In->Width, uv[1]*In->Height, &Out.Data[(Out.Depth>>3)*(y*Out.Width+x)]);
		}
	}

//	if(Mipmap)
//		_BuildMipmaps(&Out, GL_TEXTURE_CUBE_MAP_POSITIVE_X+Face);
//	else
//		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+Face, 0, Internal, Out.Width, Out.Height, 0, External, Type, Out.Data);

	FREE(Out.Data);
}

unsigned int Image_Upload(Image_t *Image, char *Filename, unsigned long Flags)
{
	char *Extension=strrchr(Filename, '.');
	VkFilter minFilter=VK_FILTER_LINEAR;
	VkFilter magFilter=VK_FILTER_LINEAR;
	VkSamplerMipmapMode mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
	VkSamplerAddressMode wrapModeU=VK_SAMPLER_ADDRESS_MODE_REPEAT;
	VkSamplerAddressMode wrapModeV=VK_SAMPLER_ADDRESS_MODE_REPEAT;
	VkCommandBuffer copyCmd=VK_NULL_HANDLE;
	VkFence fence=VK_NULL_HANDLE;
	VkMemoryRequirements memoryRequirements;
	VkFormat Format=VK_FORMAT_UNDEFINED;
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	void *data=NULL;
	uint32_t size=0;

	if(Extension!=NULL)
	{
		if(!strcmp(Extension, ".dds"))
		{
			if(!DDS_Load(Filename, Image))
				return 0;
		}
		else
		if(!strcmp(Extension, ".tga"))
		{
			if(!TGA_Load(Filename, Image))
				return 0;
		}
		else
			return 0;
	}

	if(Flags&IMAGE_NEAREST)
	{
		magFilter=VK_FILTER_NEAREST;
		mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
		minFilter=VK_FILTER_NEAREST;
	}

	if(Flags&IMAGE_BILINEAR)
	{
		magFilter=VK_FILTER_LINEAR;

		if(Flags&IMAGE_MIPMAP)
		{
			mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;
			minFilter=VK_FILTER_LINEAR;
		}
		else
			minFilter=VK_FILTER_LINEAR;
	}

	if(Flags&IMAGE_TRILINEAR)
	{
		magFilter=VK_FILTER_LINEAR;

		if(Flags&IMAGE_MIPMAP)
		{
			mipmapMode=VK_SAMPLER_MIPMAP_MODE_LINEAR;
			minFilter=VK_FILTER_LINEAR;
		}
		else
			minFilter=VK_FILTER_LINEAR;
	}

	if(Flags&IMAGE_CLAMP_U)
		wrapModeU=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

	if(Flags&IMAGE_CLAMP_V)
		wrapModeV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

	if(Flags&IMAGE_REPEAT_U)
		wrapModeU=VK_SAMPLER_ADDRESS_MODE_REPEAT;

	if(Flags&IMAGE_REPEAT_V)
		wrapModeV=VK_SAMPLER_ADDRESS_MODE_REPEAT;

	if(Flags&IMAGE_RGBE)
		_RGBE2Float(Image);

	if(Flags&IMAGE_NORMALMAP)
		_MakeNormalMap(Image);

	if(Flags&IMAGE_NORMALIZE)
		_Normalize(Image);

	if(Flags&IMAGE_CUBEMAP_ANGULAR)
	{
		_AngularMapFace(Image, 0, Flags&IMAGE_MIPMAP);
		_AngularMapFace(Image, 1, Flags&IMAGE_MIPMAP);
		_AngularMapFace(Image, 2, Flags&IMAGE_MIPMAP);
		_AngularMapFace(Image, 3, Flags&IMAGE_MIPMAP);
		_AngularMapFace(Image, 4, Flags&IMAGE_MIPMAP);
		_AngularMapFace(Image, 5, Flags&IMAGE_MIPMAP);

		FREE(Image->Data);

		return 1;
	}

	switch(Image->Depth)
	{
		case IMAGE_DXT5:
			break;

		case IMAGE_DXT3:
			break;

		case IMAGE_DXT1:
			break;

		case 128:
			Format=VK_FORMAT_R32G32B32A32_UINT;
			break;

		case 96:
			Format=VK_FORMAT_R32G32B32_UINT;
			break;

		case 64:
			Format=VK_FORMAT_R16G16B16A16_UNORM;
			break;

		case 48:
			Format=VK_FORMAT_R16G16B16_UNORM;
			break;

		case 32:
			Format=VK_FORMAT_B8G8R8A8_UNORM;
			break;

		case 24:
			Format=VK_FORMAT_B8G8R8_UNORM;
			break;

		case 16:
			Format=VK_FORMAT_A1R5G5B5_UNORM_PACK16;
			break;

		case 8:
			Format=VK_FORMAT_R8_UNORM;
			break;

		default:
			FREE(Image->Data);
			return 0;
	}

	// Byte size of image data
	size=Image->Width*Image->Height*(Image->Depth>>3);

	// Create staging buffer
	vkuCreateBuffer(device, &queueFamilyIndex, deviceMemProperties,
		&stagingBuffer, &stagingBufferMemory,
		size,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	// Map image memory and copy data
	vkMapMemory(device, stagingBufferMemory, 0, VK_WHOLE_SIZE, 0, &data);
	memcpy(data, Image->Data, Image->Width*Image->Height*(Image->Depth>>3));
	vkUnmapMemory(device, stagingBufferMemory);

	// Create the actual texture buffers and memory on device
	vkCreateImage(device, &(VkImageCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType=VK_IMAGE_TYPE_2D,
		.format=Format,
		.mipLevels=1,
		.arrayLayers=1,
		.samples=VK_SAMPLE_COUNT_1_BIT,
		.tiling=VK_IMAGE_TILING_OPTIMAL,
		.usage=VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		.sharingMode=VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED,
		.extent.width=Image->Width,
		.extent.height=Image->Height,
		.extent.depth=1,
	}, NULL, &Image->image);

	// Get memory requirements for this image like size and alignment
    vkGetImageMemoryRequirements(device, Image->image, &memoryRequirements);

    vkAllocateMemory(device, &(VkMemoryAllocateInfo)
    {
        .sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		// Set memory allocation size to required memory size
        .allocationSize=memoryRequirements.size,
		// Get memory type that can be mapped to host memory
		.memoryTypeIndex=vkuMemoryTypeFromProperties(deviceMemProperties, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    }, NULL, &Image->deviceMemory);

	vkBindImageMemory(device, Image->image, Image->deviceMemory, 0);

	// Linear tiled images don't need to be staged and can be directly used as textures
	Image->imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// Setup a command buffer to transfer image to device and change shader read layout
	vkAllocateCommandBuffers(device, &(VkCommandBufferAllocateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool=commandPool,
		.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount=1,
	}, &copyCmd);

	// Start recording commands
	vkBeginCommandBuffer(copyCmd, &(VkCommandBufferBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	});

	vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &(VkImageMemoryBarrier)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.image=Image->image,
		.subresourceRange=(VkImageSubresourceRange)
		{
			.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel=0,
			.levelCount=1,
			.layerCount=1,
		},
		.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT,
		.dstAccessMask=VK_ACCESS_SHADER_READ_BIT,
		.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	});

	// Copy from staging buffer to the texture buffer
	vkCmdCopyBufferToImage(copyCmd, stagingBuffer, Image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &(VkBufferImageCopy)
	{
		.imageSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
		.imageSubresource.mipLevel=0,
		.imageSubresource.baseArrayLayer=0,
		.imageSubresource.layerCount=1,
		.imageExtent.width=Image->Width,
		.imageExtent.height=Image->Height,
		.imageExtent.depth=1,
		.bufferOffset=0,
	});

	vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &(VkImageMemoryBarrier)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.image=Image->image,
		.subresourceRange=(VkImageSubresourceRange)
		{
			.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel=0,
			.levelCount=1,
			.layerCount=1,
		},
		.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT,
		.dstAccessMask=VK_ACCESS_SHADER_READ_BIT,
		.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	});

	vkEndCommandBuffer(copyCmd);
		
	// Submit to the queue
	vkQueueSubmit(queue, 1, &(VkSubmitInfo)
	{
		.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount=1,
		.pCommandBuffers=&copyCmd,
	}, VK_NULL_HANDLE);

	// Wait for the queue to idle (finished commands)
	vkQueueWaitIdle(queue);

	// Free the command buffer
	vkFreeCommandBuffers(device, commandPool, 1, &copyCmd);

	// Delete staging buffers
	vkFreeMemory(device, stagingBufferMemory, VK_NULL_HANDLE);
	vkDestroyBuffer(device, stagingBuffer, VK_NULL_HANDLE);

	FREE(Image->Data);

	vkCreateSampler(device, &(VkSamplerCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.maxAnisotropy=1.0f,
		.magFilter=magFilter,
		.minFilter=minFilter,
		.mipmapMode=mipmapMode,
		.addressModeU=wrapModeU,
		.addressModeV=wrapModeV,
		.addressModeW=VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.mipLodBias=0.0f,
		.compareOp=VK_COMPARE_OP_NEVER,
		.minLod=0.0f,
		.maxLod=0.0f,
		.maxAnisotropy=1.0,
		.anisotropyEnable=VK_FALSE,
		.borderColor=VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
	}, VK_NULL_HANDLE, &Image->sampler);

	vkCreateImageView(device, &(VkImageViewCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.viewType=VK_IMAGE_VIEW_TYPE_2D,
		.format=Format,
		.components=
		{
			VK_COMPONENT_SWIZZLE_R,
			VK_COMPONENT_SWIZZLE_G,
			VK_COMPONENT_SWIZZLE_B,
			VK_COMPONENT_SWIZZLE_A
		},
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
		.subresourceRange.baseMipLevel=0,
		.subresourceRange.baseArrayLayer=0,
		.subresourceRange.layerCount=1,
		.subresourceRange.levelCount=1,
		.image=Image->image,
	}, VK_NULL_HANDLE, &Image->view);

	return 1;
}
