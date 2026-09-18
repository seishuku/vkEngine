#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../system/system.h"
#include "../math/math.h"
#include "banim.h"

const uint32_t BANIM_MAGIC='B'|('A'<<8)|('N'<<16)|('M'<<24);

static bool ReadData(FILE *file, void *data, size_t size)
{
    return fread(data, 1, size, file)==size?true:false;
}

static bool ReadString(FILE *file, char *string, size_t size)
{
    size_t i=0;
    int c;

    if(size==0)
        return false;

    while(i+1<size)
    {
        c=fgetc(file);

        if(c==EOF)
            return false;

        if(c=='\0')
        {
            string[i]='\0';
            return true;
        }

        string[i++]=(char)c;
    }

    while((c=fgetc(file))!=EOF&&c!='\0');
    string[size-1]='\0';

	return true;
}

bool LoadBAnim(BAnim_t *animation, const char *filename)
{
    if(!animation)
        return false;

	FILE *file=fopen(filename, "rb");

    if(!file)
        return false;

    uint32_t magic;
    if(!ReadData(file, &magic, sizeof(magic))||magic!=BANIM_MAGIC)
    {
        fclose(file);
        return false;
    }

    if(!ReadString(file, animation->name, sizeof(animation->name))||
       !ReadData(file, &animation->duration, sizeof(animation->duration))||
       !ReadData(file, &animation->frameRate, sizeof(animation->frameRate))||
       !ReadData(file, &animation->numFrame, sizeof(animation->numFrame))||
       !ReadData(file, &animation->numBone, sizeof(animation->numBone)))
    {
        fclose(file);
        return false;
    }

    size_t count=(size_t)animation->numFrame*(size_t)animation->numBone;

    if(animation->numFrame!=0&&count/animation->numFrame!=animation->numBone)
    {
        fclose(file);
        return false;
    }

    if(count!=0)
    {
        if(count>SIZE_MAX/sizeof(*animation->frame))
        {
            fclose(file);
            return false;
        }

        animation->frame=Zone_Malloc(zone, count*sizeof(*animation->frame));

        if(!animation->frame||!ReadData(file, animation->frame, count*sizeof(*animation->frame)))
        {
            fclose(file);
            return false;
        }
    }

    fclose(file);

	return true;
}

void FreeBAnim(BAnim_t *animation)
{
    if(!animation)
        return;

    Zone_Free(zone, animation->frame);
    Zone_Free(zone, animation);
}
