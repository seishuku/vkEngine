#include <stdint.h>
#include <string.h>

#define PADDING_LITERALS 5

#define WINDOW_BITS 16
#define WINDOW_SIZE (1<<WINDOW_BITS)
#define WINDOW_MASK (WINDOW_SIZE-1)

#define MIN_MATCH 4

#define MIN(a, b) (((a)<(b))?(a):(b))
#define MAX(a, b) (((a)>(b))?(a):(b))

#define HASH_BITS 18
#define HASH_SIZE (1<<HASH_BITS)
#define HASH_32(p) ((*(uint32_t *)(p)*0x9E3779B9)>>(32-HASH_BITS))

size_t lz4_compress(const uint8_t *in, size_t inLength, uint8_t *out)
{
    static int32_t head[HASH_SIZE];
    static int32_t tail[WINDOW_SIZE];

    for(uint32_t i=0;i<HASH_SIZE;i++)
        head[i]=-1;

    for(uint32_t i=0;i<WINDOW_SIZE;i++)
        tail[i]=0;

    size_t op=0, pp=0;
    int32_t p=0;

    while(p<(int32_t)inLength)
    {
        int32_t bestLength=0;
        uint16_t distance=0;

        const int32_t maxMatch=(int32_t)((inLength-PADDING_LITERALS)-p);

        if(maxMatch>=MAX(12-PADDING_LITERALS, MIN_MATCH))
        {
            const int32_t limit=MAX(p-WINDOW_SIZE, -1);
            uint8_t chainLength=15;

            int32_t s=head[HASH_32(&in[p])];

            while(s>limit)
            {
                if(in[s+bestLength]==in[p+bestLength]&&*(uint32_t *)&in[s]==*(uint32_t *)&in[p])
                {
                    int32_t length=MIN_MATCH;

                    while(length<maxMatch&&in[s+length]==in[p+length])
                        length++;

                    if(length>bestLength)
                    {
                        bestLength=length;
                        distance=(uint16_t)(p-s);

                        if(length==maxMatch)
                            break;
                    }
                }

                if(--chainLength<=0)
                    break;

                s=tail[s&WINDOW_MASK];
            }
        }

        if(bestLength>=MIN_MATCH)
        {
            uint32_t length=bestLength-MIN_MATCH;
            const uint32_t nibble=MIN(length, 15);

            if(pp!=p)
            {
                const uint32_t run=(uint32_t)(p-pp);

                if(run>=15)
                {
                    out[op++]=(15<<4)+nibble;

                    uint32_t j=run-15;

                    for(;j>=255;j-=255)
                        out[op++]=255;

                    out[op++]=j;
                }
                else
                    out[op++]=(run<<4)+nibble;

                for(uint32_t i=0;i<run;i+=4)
                    *(uint32_t *)&out[op+i]=*(uint32_t *)&in[pp+i];

                op+=run;
            }
            else
                out[op++]=nibble;

            *(uint16_t *)&out[op]=distance, op+=2;

            if(length>=15)
            {
                length-=15;

                for(;length>=255;length-=255)
                    out[op++]=255;

                out[op++]=length;
            }

            pp=p+bestLength;

            while((size_t)p<pp)
            {
                const uint32_t h=HASH_32(&in[p]);
                tail[p&WINDOW_MASK]=head[h];
                head[h]=p++;
            }
        }
        else
        {
            const uint32_t h=HASH_32(&in[p]);
            tail[p&WINDOW_MASK]=head[h];
            head[h]=p++;
        }
    }

    if(pp!=p)
    {
        const uint32_t run=(uint32_t)(p-pp);

        if(run>=15)
        {
            out[op++]=15<<4;

            uint32_t j=run-15;

            for(;j>=255;j-=255)
                out[op++]=255;

            out[op++]=j;
        }
        else
            out[op++]=run<<4;

        for(uint32_t i=0;i<run;i+=4)
            *(uint32_t *)&out[op+i]=*(uint32_t *)&in[pp+i];

        op+=run;
    }

    return op;
}

size_t lz4_decompress(const uint8_t *in, size_t inLength, uint8_t *out, size_t outLength)
{
    size_t p=0, ip=0;

    // if output buffer is null, just count run lengths and return decompressed size
    if(out==NULL||outLength==0)
    {
        for(;;)
        {
            const uint8_t token=in[ip++];

            if(token>=16)
            {
                uint32_t run=token>>4;

                if(run==15)
                {
                    for(;;)
                    {
                        const uint8_t c=in[ip++];
                        run+=c;

                        if(c!=255)
                            break;
                    }
                }

                p+=run;
                ip+=run;

                if(ip>=inLength)
                    break;
            }

            ip+=2;

            uint32_t len=(token&15)+MIN_MATCH;

            if(len==(15+MIN_MATCH))
            {
                for(;;)
                {
                    const uint8_t c=in[ip++];
                    len+=c;

                    if(c!=255)
                        break;
                }
            }

            p+=len;
        }
    }
    else // do actual decompressing
    {
        for(;;)
        {
            const uint8_t token=in[ip++];

            if(token>=16)
            {
                uint32_t run=token>>4;

                if(run==15)
                {
                    for(;;)
                    {
                        const uint8_t c=in[ip++];
                        run+=c;

                        if(c!=255)
                            break;
                    }
                }

                if((p+run)>outLength)
                    return 0;

                for(uint32_t i=0;i<run;i+=4)
                    *(uint32_t *)&out[p+i]=*(uint32_t *)&in[ip+i];

                p+=run;
                ip+=run;

                if(ip>=inLength)
                    break;
            }

            size_t s=p-(*(uint16_t *)&in[ip]);
            ip+=2;

            if(s<0)
                return 0;

            uint32_t len=(token&15)+MIN_MATCH;

            if(len==(15+MIN_MATCH))
            {
                for(;;)
                {
                    const uint8_t c=in[ip++];
                    len+=c;

                    if(c!=255)
                        break;
                }
            }

            if((p+len)>outLength)
                return 0;

            if((p-s)>=4)
            {
                for(uint32_t i=0;i<len;i+=4)
                    *(uint32_t *)&out[p+i]=*(uint32_t *)&out[s+i];

                p+=len;
            }
            else
            {
                while(len--)
                    out[p++]=out[s++];
            }
        }
    }

    return p;
}
