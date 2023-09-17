#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "../math/math.h"
#include "../system/system.h"
#include "audio.h"

HRIR_Sphere_t Sphere;

bool HRIR_Init(void)
{
	FILE *Stream=NULL;

	Stream=fopen("./assets/hrir_full.bin", "rb");

	if(!Stream)
		return false;

	fread(&Sphere, sizeof(uint32_t), 5, Stream);

	if(Sphere.Magic!=HRIR_MAGIC)
		return false;

	if(Sphere.SampleLength>NUM_SAMPLES)
		return false;

	Sphere.Indices=(uint32_t *)Zone_Malloc(Zone, sizeof(uint32_t)*Sphere.NumIndex);

	if(Sphere.Indices==NULL)
		return false;

	fread(Sphere.Indices, sizeof(uint32_t), Sphere.NumIndex, Stream);

	Sphere.Vertices=(HRIR_Vertex_t *)Zone_Malloc(Zone, sizeof(HRIR_Vertex_t)*Sphere.NumVertex);

	if(Sphere.Vertices==NULL)
		return false;

	memset(Sphere.Vertices, 0, sizeof(HRIR_Vertex_t)*Sphere.NumVertex);

	for(uint32_t i=0;i<Sphere.NumVertex;i++)
	{
		fread(&Sphere.Vertices[i].Vertex, sizeof(vec3), 1, Stream);

		fread(Sphere.Vertices[i].Left, sizeof(float), Sphere.SampleLength, Stream);
		fread(Sphere.Vertices[i].Right, sizeof(float), Sphere.SampleLength, Stream);
	}

	fclose(Stream);

	return true;
}