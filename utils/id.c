#include <stdint.h>
#include <stdbool.h>
#include "id.h"

void ID_Init(ID_t pool)
{
    for(uint32_t i=0;i<ID_WORDS;i++)
        pool[i]=0;
}

uint32_t ID_Generate(ID_t pool)
{
    for(uint32_t word=0;word<ID_WORDS;word++)
    {
        if(pool[word]!=UINT32_MAX)
        {
            for(uint32_t bit=0;bit<ID_BITS;bit++)
            {
                uint32_t newID=word*ID_BITS+bit;

                if(newID>=ID_MAX)
                    break;

                if(!(pool[word]&(1U<<bit)))
                {
                    pool[word]|=(1U<<bit);
                    return newID;
                }
            }
        }
    }

    return UINT32_MAX;
}

bool ID_Claim(ID_t pool, uint32_t id)
{
	if(id>=ID_MAX)
		return false;

	uint32_t word=id/ID_BITS;
	uint32_t bit=id%ID_BITS;

	if(pool[word]&(1U<<bit))
		return false;

	pool[word]|=(1U<<bit);

	return true;
}

void ID_Remove(ID_t pool, int id)
{
    if(id<0||id>=ID_MAX)
        return;

    uint32_t word=id/ID_BITS;
    uint32_t bit=id%ID_BITS;
    pool[word]&=~(1U<<bit);
}
