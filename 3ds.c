// A very crude and naive rewrite of the old 3DS loader, which had issues with x64.

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <memory.h>
#include "math.h"
#include "3ds.h"

#ifdef _linux_
#include <inttypes.h>
#else
typedef unsigned int uint32_t;
#endif

#ifndef FREE
#define FREE(p) { if(p) { free(p); p=NULL; } }
#endif

void CalculateTangent(Mesh3DS_t *Mesh)
{
	int i;
	float v0[3], v1[3], uv0[2], uv1[2];
	float s[3], t[3], n[3], r;

	Mesh->Tangent=(float *)malloc(sizeof(float)*3*Mesh->NumVertex);

	if(Mesh->Tangent==NULL)
		return;

	memset(Mesh->Tangent, 0, sizeof(float)*3*Mesh->NumVertex);

	Mesh->Binormal=(float *)malloc(sizeof(float)*3*Mesh->NumVertex);

	if(Mesh->Binormal==NULL)
		return;

	memset(Mesh->Binormal, 0, sizeof(float)*3*Mesh->NumVertex);

	Mesh->Normal=(float *)malloc(sizeof(float)*3*Mesh->NumVertex);

	if(Mesh->Normal==NULL)
		return;

	memset(Mesh->Normal, 0, sizeof(float)*3*Mesh->NumVertex);

	for(i=0;i<Mesh->NumFace;i++)
	{
		unsigned short i1=Mesh->Face[3*i+0];
		unsigned short i2=Mesh->Face[3*i+1];
		unsigned short i3=Mesh->Face[3*i+2];

		v0[0]=Mesh->Vertex[3*i2+0]-Mesh->Vertex[3*i1+0];
		v0[1]=Mesh->Vertex[3*i2+1]-Mesh->Vertex[3*i1+1];
		v0[2]=Mesh->Vertex[3*i2+2]-Mesh->Vertex[3*i1+2];

		uv0[0]=Mesh->UV[2*i2+0]-Mesh->UV[2*i1+0];
		uv0[1]=Mesh->UV[2*i2+1]-Mesh->UV[2*i1+1];

		v1[0]=Mesh->Vertex[3*i3+0]-Mesh->Vertex[3*i1+0];
		v1[1]=Mesh->Vertex[3*i3+1]-Mesh->Vertex[3*i1+1];
		v1[2]=Mesh->Vertex[3*i3+2]-Mesh->Vertex[3*i1+2];

		uv1[0]=Mesh->UV[2*i3+0]-Mesh->UV[2*i1+0];
		uv1[1]=Mesh->UV[2*i3+1]-Mesh->UV[2*i1+1];

		r=1.0f/(uv0[0]*uv1[1]-uv1[0]*uv0[1]);

		s[0]=(uv1[1]*v0[0]-uv0[1]*v1[0])*r;
		s[1]=(uv1[1]*v0[1]-uv0[1]*v1[1])*r;
		s[2]=(uv1[1]*v0[2]-uv0[1]*v1[2])*r;
		Normalize(s);

		Mesh->Tangent[3*i1+0]+=s[0];	Mesh->Tangent[3*i1+1]+=s[1];	Mesh->Tangent[3*i1+2]+=s[2];
		Mesh->Tangent[3*i2+0]+=s[0];	Mesh->Tangent[3*i2+1]+=s[1];	Mesh->Tangent[3*i2+2]+=s[2];
		Mesh->Tangent[3*i3+0]+=s[0];	Mesh->Tangent[3*i3+1]+=s[1];	Mesh->Tangent[3*i3+2]+=s[2];

		t[0]=(uv0[0]*v1[0]-uv1[0]*v0[0])*r;
		t[1]=(uv0[0]*v1[1]-uv1[0]*v0[1])*r;
		t[2]=(uv0[0]*v1[2]-uv1[0]*v0[2])*r;
		Normalize(t);

		Mesh->Binormal[3*i1+0]-=t[0];	Mesh->Binormal[3*i1+1]-=t[1];	Mesh->Binormal[3*i1+2]-=t[2];
		Mesh->Binormal[3*i2+0]-=t[0];	Mesh->Binormal[3*i2+1]-=t[1];	Mesh->Binormal[3*i2+2]-=t[2];
		Mesh->Binormal[3*i3+0]-=t[0];	Mesh->Binormal[3*i3+1]-=t[1];	Mesh->Binormal[3*i3+2]-=t[2];

		Cross(v0, v1, n);
		Normalize(n);

		Mesh->Normal[3*i1+0]+=n[0];		Mesh->Normal[3*i1+1]+=n[1];		Mesh->Normal[3*i1+2]+=n[2];
		Mesh->Normal[3*i2+0]+=n[0];		Mesh->Normal[3*i2+1]+=n[1];		Mesh->Normal[3*i2+2]+=n[2];
		Mesh->Normal[3*i3+0]+=n[0];		Mesh->Normal[3*i3+1]+=n[1];		Mesh->Normal[3*i3+2]+=n[2];
	}
}

int Load3DS(Model3DS_t *Model, char *Filename)
{
	FILE *Stream=NULL;
	long i, Length;
	unsigned short ChunkID;
	uint32_t ChunkLength;
	uint32_t Temp;
	unsigned short Temp16;
	char *Ptr=NULL;
	unsigned char RGB[3];
	float *ColorPtr=NULL;

	if((Stream=fopen(Filename, "rb"))==NULL)
		return 0;

	fseek(Stream, 0, SEEK_END);
	Length=ftell(Stream);
	fseek(Stream, 0, SEEK_SET);

	while(ftell(Stream)<Length)
	{
		fread(&ChunkID, sizeof(unsigned short), 1, Stream);
		fread(&ChunkLength, sizeof(uint32_t), 1, Stream);

		// Any chunk to be read *must* be in the switch block.
		// Any chunk not in this switch will be skipped over by the default case.
		switch(ChunkID)
		{
			// Main chunk
			case 0x4D4D:
				break;

			// 3D editor chunk
			case 0x3D3D:
				break;

			// Object block chunk
			case 0x4000:
				Model->NumMesh++;

				Model->Mesh=(Mesh3DS_t *)realloc(Model->Mesh, sizeof(Mesh3DS_t)*Model->NumMesh);

				if(Model->Mesh==NULL)
				{
					fclose(Stream);
					return 0;
				}

				memset(&Model->Mesh[Model->NumMesh-1], 0, sizeof(Mesh3DS_t));

				Ptr=Model->Mesh[Model->NumMesh-1].Name;

				for(;;Ptr++)
				{
					fread(Ptr, sizeof(char), 1, Stream);

					if(*Ptr=='\0')
						break;
				}
				break;

			// Triangle mesh subchunk
			case 0x4100:
				break;

			// Vertex list subchunk
			case 0x4110:
				fread(&Model->Mesh[Model->NumMesh-1].NumVertex, sizeof(unsigned short), 1, Stream);

				if(!Model->Mesh[Model->NumMesh-1].NumVertex)
					break;

				Model->Mesh[Model->NumMesh-1].Vertex=(float *)malloc(3*sizeof(float)*Model->Mesh[Model->NumMesh-1].NumVertex);

				if(Model->Mesh[Model->NumMesh-1].Vertex==NULL)
				{
					Free3DS(Model);
					fclose(Stream);

					return 0;
				}

				fread(Model->Mesh[Model->NumMesh-1].Vertex, sizeof(float), 3*Model->Mesh[Model->NumMesh-1].NumVertex, Stream);

				for(i=0;i<Model->Mesh[Model->NumMesh-1].NumVertex;i++)
				{
					float Temp=Model->Mesh[Model->NumMesh-1].Vertex[3*i+1];
					Model->Mesh[Model->NumMesh-1].Vertex[3*i+1]=Model->Mesh[Model->NumMesh-1].Vertex[3*i+2];
					Model->Mesh[Model->NumMesh-1].Vertex[3*i+2]=-Temp;
				}
				break;

			// Face description (contains vertex indices) subchunk
			case 0x4120:
				fread(&Model->Mesh[Model->NumMesh-1].NumFace, sizeof(unsigned short), 1, Stream);

				if(!Model->Mesh[Model->NumMesh-1].NumFace)
					break;

				Model->Mesh[Model->NumMesh-1].Face=(unsigned short *)malloc(3*sizeof(unsigned short)*Model->Mesh[Model->NumMesh-1].NumFace);

				if(Model->Mesh[Model->NumMesh-1].Face==NULL)
				{
					Free3DS(Model);
					fclose(Stream);

					return 0;
				}

				for(i=0;i<Model->Mesh[Model->NumMesh-1].NumFace;i++)
				{
					fread(&Model->Mesh[Model->NumMesh-1].Face[3*i], sizeof(unsigned short), 3, Stream);
					fread(&Temp, sizeof(unsigned short), 1, Stream);
				}
				break;

			// Face material name (matches to material list) subchunk
			case 0x4130:
				Ptr=Model->Mesh[Model->NumMesh-1].MaterialName;

				for(;;Ptr++)
				{
					fread(Ptr, sizeof(char), 1, Stream);

					if(*Ptr=='\0')
						break;
				}

				// Skip face groups, probably should read these though.
				fread(&Temp16, sizeof(unsigned short), 1, Stream);
				fseek(Stream, sizeof(unsigned short)*Temp16, SEEK_CUR);
				break;

			// Texture coordinates subchunk
			case 0x4140:
				fread(&Temp16, sizeof(unsigned short), 1, Stream);

				if(!Temp16||Temp16!=Model->Mesh[Model->NumMesh-1].NumVertex)
					break;

				Model->Mesh[Model->NumMesh-1].UV=(float *)malloc(2*sizeof(float)*Model->Mesh[Model->NumMesh-1].NumVertex);

				if(Model->Mesh[Model->NumMesh-1].UV==NULL)
				{
					Free3DS(Model);
					fclose(Stream);

					return 0;
				}

				fread(Model->Mesh[Model->NumMesh-1].UV, sizeof(float), 2*Model->Mesh[Model->NumMesh-1].NumVertex, Stream);
				break;

			// Material block chunk
			case 0xAFFF:
				Model->NumMaterial++;

				Model->Material=(Material3DS_t *)realloc(Model->Material, sizeof(Material3DS_t)*Model->NumMaterial);

				if(Model->Material==NULL)
				{
					fclose(Stream);
					return 0;
				}

				memset(&Model->Material[Model->NumMaterial-1], 0, sizeof(Material3DS_t));
				break;

			// Material name subchunk
			case 0xA000:
				Ptr=Model->Material[Model->NumMaterial-1].Name;

				for(;;Ptr++)
				{
					fread(Ptr, sizeof(char), 1, Stream);

					if(*Ptr=='\0')
						break;
				}
				break;

			// Material texture maps subchunk
			case 0xA200:
				break;

			// Material diffuse texture map subsubchunk
			case 0xA300:
				Ptr=Model->Material[Model->NumMaterial-1].Texture;

				for(;;Ptr++)
				{
					fread(Ptr, sizeof(char), 1, Stream);

					if(*Ptr=='\0')
						break;
				}
				break;

			// Material ambient color property
			case 0xA010:
				// Set pointer for subchunk read in next pass
				ColorPtr=Model->Material[Model->NumMaterial-1].Ambient;
				break;

			// Material diffuse color property
			case 0xA020:
				// Set pointer for subchunk read in next pass
				ColorPtr=Model->Material[Model->NumMaterial-1].Diffuse;
				break;

			// Material specular color property
			case 0xA030:
				// Set pointer for subchunk read in next pass
				ColorPtr=Model->Material[Model->NumMaterial-1].Specular;
				break;

			// Material shininess property
			case 0xA040:
				// Set pointer for subchunk read in next pass
				ColorPtr=&Model->Material[Model->NumMaterial-1].Shininess;
				break;

			case 0xA080:
				// Set pointer for subchunk read in next pass
				ColorPtr=Model->Material[Model->NumMaterial-1].Emission;
				break;

			// Subchunk reads, need to check for pointer reference, 3DS files apparently may contain color chunks with out a subchunk property reference.

			// RGB float color subchunk read
			case 0x0010:
				// Read in floats from file stream
				if(ColorPtr!=NULL)
					fread(ColorPtr, sizeof(float), 3, Stream);
				else
					fseek(Stream, sizeof(float)*3, SEEK_CUR);

				// Dereference pointer
				ColorPtr=NULL;
				break;

			// RGB byte color subchunk read
			case 0x0011:
				// Read in bytes from file stream
				fread(RGB, sizeof(unsigned char), 3, Stream);

				if(ColorPtr!=NULL)
				{
					*ColorPtr++=(float)RGB[0]/255.0f;
					*ColorPtr++=(float)RGB[1]/255.0f;
					*ColorPtr++=(float)RGB[2]/255.0f;
				}

				// Dereference pointer
				ColorPtr=NULL;
				break;

			// Percent byte subchunk read
			case 0x0030:
				// Read in byte from file stream
				fread(RGB, sizeof(short), 1, Stream);

				// Rescale
				if(ColorPtr!=NULL)
					*ColorPtr=(float)((RGB[1]<<8)|RGB[0])*140.0f; // Is this correct?

				// Dereference pointer
				ColorPtr=NULL;
				break;

			// Percent float subchunk read
			case 0x0031:
				// Read in float from file stream
				if(ColorPtr!=NULL)
					fread(ColorPtr, sizeof(float), 1, Stream);
				else
					fseek(Stream, sizeof(float), SEEK_CUR);

				// Rescale
				if(ColorPtr!=NULL)
					*ColorPtr*=140.0f; // Is this correct?

				// Dereference pointer
				ColorPtr=NULL;
				break;

			// Skip chunk
			default:
				fseek(Stream, ChunkLength-6, SEEK_CUR);
		}
	}

	fclose(Stream);

	for(i=0;i<Model->NumMesh;i++)
		CalculateTangent(&Model->Mesh[i]);

	return 1;
}

void Free3DS(Model3DS_t *Model)
{
	int i;

	if(Model->Mesh)
	{
		for(i=0;i<Model->NumMesh;i++)
		{
			FREE(Model->Mesh[i].Vertex);
			FREE(Model->Mesh[i].UV);
			FREE(Model->Mesh[i].Face);
			FREE(Model->Mesh[i].Normal);
			FREE(Model->Mesh[i].Tangent);
			FREE(Model->Mesh[i].Binormal);
		}

		FREE(Model->Mesh);
	}

	FREE(Model->Material);
}
