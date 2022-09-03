// A very crude and naive rewrite of the old 3DS loader, which had issues with x64.

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <malloc.h>
#include <memory.h>
#include "../system/system.h"
#include "../vulkan/vulkan.h"
#include "../math/math.h"
#include "3ds.h"

void CalculateTangent3DS(Mesh3DS_t *Mesh)
{
	int32_t i;
	vec3 v0, v1, s, t, n;
	vec2 uv0, uv1;
	float r;

	Mesh->Tangent=(float *)Zone_Malloc(Zone, sizeof(float)*3*Mesh->NumVertex);

	if(Mesh->Tangent==NULL)
		return;

	memset(Mesh->Tangent, 0, sizeof(float)*3*Mesh->NumVertex);

	Mesh->Binormal=(float *)Zone_Malloc(Zone, sizeof(float)*3*Mesh->NumVertex);

	if(Mesh->Binormal==NULL)
		return;

	memset(Mesh->Binormal, 0, sizeof(float)*3*Mesh->NumVertex);

	Mesh->Normal=(float *)Zone_Malloc(Zone, sizeof(float)*3*Mesh->NumVertex);

	if(Mesh->Normal==NULL)
		return;

	memset(Mesh->Normal, 0, sizeof(float)*3*Mesh->NumVertex);

	for(i=0;i<Mesh->NumFace;i++)
	{
		uint16_t i1=Mesh->Face[3*i+0];
		uint16_t i2=Mesh->Face[3*i+1];
		uint16_t i3=Mesh->Face[3*i+2];

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
		Vec3_Normalize(s);

		Mesh->Tangent[3*i1+0]+=s[0];	Mesh->Tangent[3*i1+1]+=s[1];	Mesh->Tangent[3*i1+2]+=s[2];
		Mesh->Tangent[3*i2+0]+=s[0];	Mesh->Tangent[3*i2+1]+=s[1];	Mesh->Tangent[3*i2+2]+=s[2];
		Mesh->Tangent[3*i3+0]+=s[0];	Mesh->Tangent[3*i3+1]+=s[1];	Mesh->Tangent[3*i3+2]+=s[2];

		t[0]=(uv0[0]*v1[0]-uv1[0]*v0[0])*r;
		t[1]=(uv0[0]*v1[1]-uv1[0]*v0[1])*r;
		t[2]=(uv0[0]*v1[2]-uv1[0]*v0[2])*r;
		Vec3_Normalize(t);

		Mesh->Binormal[3*i1+0]-=t[0];	Mesh->Binormal[3*i1+1]-=t[1];	Mesh->Binormal[3*i1+2]-=t[2];
		Mesh->Binormal[3*i2+0]-=t[0];	Mesh->Binormal[3*i2+1]-=t[1];	Mesh->Binormal[3*i2+2]-=t[2];
		Mesh->Binormal[3*i3+0]-=t[0];	Mesh->Binormal[3*i3+1]-=t[1];	Mesh->Binormal[3*i3+2]-=t[2];

		Cross(v0, v1, n);
		Vec3_Normalize(n);

		Mesh->Normal[3*i1+0]+=n[0];		Mesh->Normal[3*i1+1]+=n[1];		Mesh->Normal[3*i1+2]+=n[2];
		Mesh->Normal[3*i2+0]+=n[0];		Mesh->Normal[3*i2+1]+=n[1];		Mesh->Normal[3*i2+2]+=n[2];
		Mesh->Normal[3*i3+0]+=n[0];		Mesh->Normal[3*i3+1]+=n[1];		Mesh->Normal[3*i3+2]+=n[2];
	}

	for(uint32_t i=0;i<Mesh->NumVertex;i++)
	{
		float *t=&Mesh->Tangent[3*i];
		float *b=&Mesh->Binormal[3*i];
		float *n=&Mesh->Normal[3*i];

		float d=Vec3_Dot(n, t);
		t[0]-=n[0]*d;
		t[1]-=n[1]*d;
		t[2]-=n[2]*d;
		Vec3_Normalize(t);
		Vec3_Normalize(b);
		Vec3_Normalize(n);

		vec3 NxT;
		Cross(n, t, NxT);

		if(Vec3_Dot(NxT, b)<0.0f)
			Vec3_Muls(t, -1.0f);

		Vec3_Setv(b, NxT);
	}
}

bool Load3DS(Model3DS_t *Model, char *Filename)
{
	FILE *Stream=NULL;
	long Length;
	uint16_t ChunkID;
	uint32_t ChunkLength;
	uint32_t Temp;
	uint16_t Temp16;
	char *Ptr=NULL;
	uint8_t RGB[3];
	float *ColorPtr=NULL;

	if((Stream=fopen(Filename, "rb"))==NULL)
		return false;

	fseek(Stream, 0, SEEK_END);
	Length=ftell(Stream);
	fseek(Stream, 0, SEEK_SET);

	while(ftell(Stream)<Length)
	{
		fread(&ChunkID, sizeof(uint16_t), 1, Stream);
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

				Model->Mesh=(Mesh3DS_t *)Zone_Realloc(Zone, Model->Mesh, sizeof(Mesh3DS_t)*Model->NumMesh);

				if(Model->Mesh==NULL)
				{
					fclose(Stream);
					return false;
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
				fread(&Model->Mesh[Model->NumMesh-1].NumVertex, sizeof(uint16_t), 1, Stream);

				if(!Model->Mesh[Model->NumMesh-1].NumVertex)
					break;

				Model->Mesh[Model->NumMesh-1].Vertex=(float *)Zone_Malloc(Zone, 3*sizeof(float)*Model->Mesh[Model->NumMesh-1].NumVertex);

				if(Model->Mesh[Model->NumMesh-1].Vertex==NULL)
				{
					Free3DS(Model);
					fclose(Stream);

					return false;
				}

				fread(Model->Mesh[Model->NumMesh-1].Vertex, sizeof(float), 3*Model->Mesh[Model->NumMesh-1].NumVertex, Stream);

				for(int32_t i=0;i<Model->Mesh[Model->NumMesh-1].NumVertex;i++)
				{
					float Temp=Model->Mesh[Model->NumMesh-1].Vertex[3*i+1];
					Model->Mesh[Model->NumMesh-1].Vertex[3*i+1]=Model->Mesh[Model->NumMesh-1].Vertex[3*i+2];
					Model->Mesh[Model->NumMesh-1].Vertex[3*i+2]=-Temp;
				}
				break;

			// Face description (contains vertex indices) subchunk
			case 0x4120:
				fread(&Model->Mesh[Model->NumMesh-1].NumFace, sizeof(uint16_t), 1, Stream);

				if(!Model->Mesh[Model->NumMesh-1].NumFace)
					break;

				Model->Mesh[Model->NumMesh-1].Face=(uint16_t *)Zone_Malloc(Zone, 3*sizeof(uint16_t)*Model->Mesh[Model->NumMesh-1].NumFace);

				if(Model->Mesh[Model->NumMesh-1].Face==NULL)
				{
					Free3DS(Model);
					fclose(Stream);

					return false;
				}

				for(int32_t i=0;i<Model->Mesh[Model->NumMesh-1].NumFace;i++)
				{
					fread(&Model->Mesh[Model->NumMesh-1].Face[3*i], sizeof(uint16_t), 3, Stream);
					fread(&Temp, sizeof(uint16_t), 1, Stream);
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
				fread(&Temp16, sizeof(uint16_t), 1, Stream);
				fseek(Stream, sizeof(uint16_t)*Temp16, SEEK_CUR);
				break;

			// Texture coordinates subchunk
			case 0x4140:
				fread(&Temp16, sizeof(uint16_t), 1, Stream);

				if(!Temp16||Temp16!=Model->Mesh[Model->NumMesh-1].NumVertex)
					break;

				Model->Mesh[Model->NumMesh-1].UV=(float *)Zone_Malloc(Zone, 2*sizeof(float)*Model->Mesh[Model->NumMesh-1].NumVertex);

				if(Model->Mesh[Model->NumMesh-1].UV==NULL)
				{
					Free3DS(Model);
					fclose(Stream);

					return false;
				}

				fread(Model->Mesh[Model->NumMesh-1].UV, sizeof(float), 2*Model->Mesh[Model->NumMesh-1].NumVertex, Stream);
				break;

			// Material block chunk
			case 0xAFFF:
				Model->NumMaterial++;

				Model->Material=(Material3DS_t *)Zone_Realloc(Zone, Model->Material, sizeof(Material3DS_t)*Model->NumMaterial);

				if(Model->Material==NULL)
				{
					fclose(Stream);
					return false;
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
				fread(RGB, sizeof(uint8_t), 3, Stream);

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

	// If there are materials, match them to their meshes with an index number
	if(Model->Material)
	{
		for(int32_t i=0;i<Model->NumMesh;i++)
		{
			for(int32_t j=0;j<Model->NumMaterial;j++)
			{
				if(strcmp(Model->Mesh[i].MaterialName, Model->Material[j].Name)==0)
					Model->Mesh[i].MaterialNumber=j;
			}
		}
	}

	for(int32_t i=0;i<Model->NumMesh;i++)
		CalculateTangent3DS(&Model->Mesh[i]);

	return true;
}

void Free3DS(Model3DS_t *Model)
{
	if(Model->Mesh)
	{
		for(int32_t i=0;i<Model->NumMesh;i++)
		{
			Zone_Free(Zone, Model->Mesh[i].Vertex);
			Zone_Free(Zone, Model->Mesh[i].UV);
			Zone_Free(Zone, Model->Mesh[i].Face);
			Zone_Free(Zone, Model->Mesh[i].Normal);
			Zone_Free(Zone, Model->Mesh[i].Tangent);
			Zone_Free(Zone, Model->Mesh[i].Binormal);
		}

		Zone_Free(Zone, Model->Mesh);
	}

	Zone_Free(Zone, Model->Material);
}
