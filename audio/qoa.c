#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../system/system.h"
#include "qoa.h"

static const uint32_t QOA_MAGIC='q'<<24|'o'<<16|'a'<<8|'f';

#define QOA_SLICE_LEN 20
#define QOA_SLICES_PER_FRAME 256
#define QOA_FRAME_LEN (QOA_SLICES_PER_FRAME*QOA_SLICE_LEN)

#define QOA_HEADER_MAGIC_MASK				0xFFFFFFFF00000000
#define QOA_HEADER_MAGIC_SHIFT				32
#define QOA_HEADER_NUMSAMPLES_MASK			0x00000000FFFFFFFF
#define QOA_HEADER_NUMSAMPLES_SHIFT			0

#define QOA_FRAMEHEADER_CHANNELS_MASK		0xFF00000000000000
#define QOA_FRAMEHEADER_CHANNELS_SHIFT		56
#define QOA_FRAMEHEADER_SAMPLERATE_MASK		0x00FFFFFF00000000
#define QOA_FRAMEHEADER_SAMPLERATE_SHIFT	32
#define QOA_FRAMEHEADER_FRAMELENGTH_MASK	0x00000000FFFF0000
#define QOA_FRAMEHEADER_FRAMELENGTH_SHIFT	16
#define QOA_FRAMEHEADER_FRAMESIZE_MASK		0x000000000000FFFF
#define QOA_FRAMEHEADER_FRAMESIZE_SHIFT		0

static const int32_t QuantTab[17]={ 7, 7, 7, 5, 5, 3, 3, 1, 0, 0, 2, 2, 4, 4, 6, 6, 6 };
static const int32_t DequantTab[16][8]=
{
	{    1,   -1,    3,   -3,    5,   -5,     7,    -7 },
	{    5,   -5,   18,  -18,   32,  -32,    49,   -49 },
	{   16,  -16,   53,  -53,   95,  -95,   147,  -147 },
	{   34,  -34,  113, -113,  203, -203,   315,  -315 },
	{   63,  -63,  210, -210,  378, -378,   588,  -588 },
	{  104, -104,  345, -345,  621, -621,   966,  -966 },
	{  158, -158,  528, -528,  950, -950,  1477, -1477 },
	{  228, -228,  760, -760, 1368,-1368,  2128, -2128 },
	{  316, -316, 1053,-1053, 1895,-1895,  2947, -2947 },
	{  422, -422, 1405,-1405, 2529,-2529,  3934, -3934 },
	{  548, -548, 1828,-1828, 3290,-3290,  5117, -5117 },
	{  696, -696, 2320,-2320, 4176,-4176,  6496, -6496 },
	{  868, -868, 2893,-2893, 5207,-5207,  8099, -8099 },
	{ 1064,-1064, 3548,-3548, 6386,-6386,  9933, -9933 },
	{ 1286,-1286, 4288,-4288, 7718,-7718, 12005,-12005 },
	{ 1536,-1536, 5120,-5120, 9216,-9216, 14336,-14336 },
};

inline static int32_t QOA_LMSPredict(QOA_LMS_t *LMS)
{
	int32_t prediction=0;

	for(int32_t i=0;i<QOA_LMS_LEN;i++)
		prediction+=LMS->weights[i]*LMS->history[i];

	return prediction>>13;
}

inline static void QOA_LMSUpdate(QOA_LMS_t *LMS, int32_t sample, int32_t residual)
{
	const int32_t delta=residual>>4;

	for(int32_t i=0;i<QOA_LMS_LEN;i++)
		LMS->weights[i]+=LMS->history[i]<0?-delta:delta;

	for(int32_t i=0;i<QOA_LMS_LEN-1;i++)
		LMS->history[i]=LMS->history[i+1];

	LMS->history[QOA_LMS_LEN-1]=sample;
}


inline static int32_t QOA_Divide(int32_t v, int32_t scale)
{
	static const int32_t ReciprocalTab[16]={ 65536, 9363, 3121, 1457, 781, 475, 311, 216, 156, 117, 90, 71, 57, 47, 39, 32 };
	const int32_t n=(v*ReciprocalTab[scale]+(1<<15))>>16;

	return n+((v>0)-(v<0))-((n>0)-(n<0));
}

inline static int32_t QOA_Clamp(int32_t v, int32_t min, int32_t max)
{
	if(v<min)
		return min;

	if(v>max)
		return max;

	return v;
}

inline static int32_t QOA_ClampS16(int32_t v)
{
	if((uint32_t)(v+32768)>65535)
	{
		if(v<-32768)
			return -32768;

		if(v>32767)
			return 32767;
	}

	return v;
}

static uint64_t QOA_SwapU64(uint64_t val)
{
	val=((val<< 8)&0xFF00FF00FF00FF00)|((val>> 8)&0x00FF00FF00FF00FF);
	val=((val<<16)&0xFFFF0000FFFF0000)|((val>>16)&0x0000FFFF0000FFFF);

	return (val<<32)|(val>>32);
}

/// <summary>
/// ENCODER
/// </summary>
/// <param name="qoa"></param>
/// <param name="bytes"></param>
/// <returns></returns>
uint32_t QOA_EncodeFrame(const int16_t *samples, QOA_Desc_t *qoa, uint32_t frameLength, void *bytes)
{
	uint64_t *p=(uint64_t *)bytes;
	uint32_t channels=qoa->channels;
	uint32_t slices=(frameLength+QOA_SLICE_LEN-1)/QOA_SLICE_LEN;
	uint32_t frameSize=(8+QOA_LMS_LEN*4*channels+8*slices*channels);
	int prevScaleFactor[QOA_MAX_CHANNELS]={ 0 };

	uint64_t frameHeader=0;
	frameHeader|=((uint64_t)qoa->channels  <<QOA_FRAMEHEADER_CHANNELS_SHIFT   )&QOA_FRAMEHEADER_CHANNELS_MASK;
	frameHeader|=((uint64_t)qoa->sampleRate<<QOA_FRAMEHEADER_SAMPLERATE_SHIFT )&QOA_FRAMEHEADER_SAMPLERATE_MASK;
	frameHeader|=((uint64_t)frameLength    <<QOA_FRAMEHEADER_FRAMELENGTH_SHIFT)&QOA_FRAMEHEADER_FRAMELENGTH_MASK;
	frameHeader|=((uint64_t)frameSize      <<QOA_FRAMEHEADER_FRAMESIZE_SHIFT  )&QOA_FRAMEHEADER_FRAMESIZE_MASK;

	*p++=QOA_SwapU64(frameHeader);

	for(uint32_t c=0;c<channels;c++)
	{
		uint64_t weights=0, history=0;

		for(int32_t i=0;i<QOA_LMS_LEN;i++)
		{
			history=(history<<16)|(qoa->LMS[c].history[i]&0xFFFF);
			weights=(weights<<16)|(qoa->LMS[c].weights[i]&0xFFFF);
		}

		*p++=QOA_SwapU64(history);
		*p++=QOA_SwapU64(weights);
	}

	for(uint32_t sampleIndex=0;sampleIndex<frameLength;sampleIndex+=QOA_SLICE_LEN)
	{
		for(uint32_t channelIndex=0;channelIndex<channels;channelIndex++)
		{
			int32_t sliceLength=QOA_Clamp(QOA_SLICE_LEN, 0, frameLength-sampleIndex);
			int32_t sliceStart=sampleIndex*channels+channelIndex;
			int32_t sliceEnd=(sampleIndex+sliceLength)*channels+channelIndex;
			uint64_t bestRank=-1;
			uint64_t bestSlice=0;
			QOA_LMS_t bestLMS;
			int32_t bestScaleFactor=0;

			for(int32_t scaleFactorIndex=0;scaleFactorIndex<16;scaleFactorIndex++)
			{
				int32_t scaleFactor=(scaleFactorIndex+prevScaleFactor[channelIndex])%16;
				QOA_LMS_t LMS=qoa->LMS[channelIndex];
				uint64_t slice=scaleFactor;
				uint64_t currentRank=0;

				for(int32_t sliceIndex=sliceStart;sliceIndex<sliceEnd;sliceIndex+=channels)
				{
					int32_t sample=samples[sliceIndex];
					int32_t predicted=QOA_LMSPredict(&LMS);
					int32_t residual=sample-predicted;
					int32_t scaled=QOA_Divide(residual, scaleFactor);
					int32_t clamped=QOA_Clamp(scaled, -8, 8);
					int32_t quantized=QuantTab[clamped+8];
					int32_t dequantized=DequantTab[scaleFactor][quantized];
					int32_t reconstructed=QOA_ClampS16(predicted+dequantized);
					int32_t weightsPenalty=((LMS.weights[0]*LMS.weights[0]+
											 LMS.weights[1]*LMS.weights[1]+
											 LMS.weights[2]*LMS.weights[2]+
											 LMS.weights[3]*LMS.weights[3])>>18)-0x8FF;

					if(weightsPenalty<0)
						weightsPenalty=0;

					int64_t error=(sample-reconstructed);
					uint64_t errorSq=error*error;

					currentRank+=errorSq+weightsPenalty*weightsPenalty;

					if(currentRank>bestRank)
						break;

					QOA_LMSUpdate(&LMS, reconstructed, dequantized);

					slice=(slice<<3)|quantized;
				}

				if(currentRank<bestRank)
				{
					bestRank=currentRank;
					bestSlice=slice;
					bestLMS=LMS;
					bestScaleFactor=scaleFactor;
				}
			}

			prevScaleFactor[channelIndex]=bestScaleFactor;

			qoa->LMS[channelIndex]=bestLMS;

			bestSlice<<=(QOA_SLICE_LEN-sliceLength)*3;

			*p++=QOA_SwapU64(bestSlice);
		}
	}

	return (uint32_t)((uint8_t *)p-(uint8_t *)bytes);
}

void *QOA_Encode(const int16_t *samples, QOA_Desc_t *qoa, uint32_t *outLength)
{
	if(qoa->numSamples==0||qoa->sampleRate==0||qoa->sampleRate>0xFFFFFF||qoa->channels==0||qoa->channels>QOA_MAX_CHANNELS)
		return NULL;

	uint32_t numFrames=(qoa->numSamples+QOA_FRAME_LEN-1)/QOA_FRAME_LEN;
	uint32_t numSlices=(qoa->numSamples+QOA_SLICE_LEN-1)/QOA_SLICE_LEN;
	uint32_t encodedSize=8+numFrames*8+numFrames*QOA_LMS_LEN*4*qoa->channels+numSlices*8*qoa->channels;
	uint8_t *bytes=(uint8_t *)Zone_Malloc(zone, encodedSize);

	for(uint32_t channelIndex=0;channelIndex<qoa->channels;channelIndex++)
	{
		qoa->LMS[channelIndex].weights[0]= 0;
		qoa->LMS[channelIndex].weights[1]= 0;
		qoa->LMS[channelIndex].weights[2]=-0x2000;
		qoa->LMS[channelIndex].weights[3]= 0x4000;

		for(int32_t i=0;i<QOA_LMS_LEN;i++)
			qoa->LMS[channelIndex].history[i]=0;
	}

	uint64_t *p=(uint64_t *)bytes;

	uint64_t header=(((uint64_t)QOA_MAGIC<<QOA_HEADER_MAGIC_SHIFT)&QOA_HEADER_MAGIC_MASK)|(((uint64_t)qoa->numSamples<<QOA_HEADER_NUMSAMPLES_SHIFT)&QOA_HEADER_NUMSAMPLES_MASK);

	*p++=QOA_SwapU64(header);

	int32_t frameLength=QOA_FRAME_LEN;

	for(uint32_t sampleIndex=0;sampleIndex<qoa->numSamples;sampleIndex+=frameLength)
	{
		frameLength=QOA_Clamp(QOA_FRAME_LEN, 0, qoa->numSamples-sampleIndex);

		uint32_t frameSize=QOA_EncodeFrame(samples+sampleIndex*qoa->channels, qoa, frameLength, p);
		p+=frameSize>>3;
	}

	*outLength=(uint32_t)((uint8_t *)p-bytes);

	return bytes;
}

// QOA Decoder
uint32_t QOA_DecodeFrame(const void *bytes, uint32_t size, QOA_Desc_t *qoa, int16_t *samples, uint32_t *frameSamples)
{
	uint64_t *p=(uint64_t *)bytes;
	*frameSamples=0;

	if(size<8+QOA_LMS_LEN*4*qoa->channels)
		return 0;

	uint64_t frameHeader=QOA_SwapU64(*p++);

	uint8_t  channels=  (frameHeader&QOA_FRAMEHEADER_CHANNELS_MASK   )>>QOA_FRAMEHEADER_CHANNELS_SHIFT;
	uint32_t sampleRate=(frameHeader&QOA_FRAMEHEADER_SAMPLERATE_MASK )>>QOA_FRAMEHEADER_SAMPLERATE_SHIFT;
	uint16_t numSamples=(frameHeader&QOA_FRAMEHEADER_FRAMELENGTH_MASK)>>QOA_FRAMEHEADER_FRAMELENGTH_SHIFT;
	uint16_t frameSize= (frameHeader&QOA_FRAMEHEADER_FRAMESIZE_MASK  )>>QOA_FRAMEHEADER_FRAMESIZE_SHIFT;

	uint32_t dataSize=frameSize-8-QOA_LMS_LEN*4*channels;
	uint32_t maxTotalSamples=(dataSize>>3)*QOA_SLICE_LEN;

	if(channels!=qoa->channels||sampleRate!=qoa->sampleRate||frameSize>size||numSamples*channels>maxTotalSamples)
		return 0;

	for(uint32_t channelIndex=0;channelIndex<channels;channelIndex++)
	{
		uint64_t history=QOA_SwapU64(*p++);
		uint64_t weights=QOA_SwapU64(*p++);

		for(int32_t i=0;i<QOA_LMS_LEN;i++)
		{
			qoa->LMS[channelIndex].history[i]=((int16_t)(history>>48));
			history<<=16;

			qoa->LMS[channelIndex].weights[i]=((int16_t)(weights>>48));
			weights<<=16;
		}
	}

	for(uint32_t sampleIndex=0;sampleIndex<numSamples;sampleIndex+=QOA_SLICE_LEN)
	{
		for(uint32_t channelIndex=0;channelIndex<channels;channelIndex++)
		{
			uint64_t slice=QOA_SwapU64(*p++);
			int32_t scaleFactor=(slice>>60)&0xF;
			int32_t sliceStart=sampleIndex*channels+channelIndex;
			int32_t sliceEnd=QOA_Clamp(sampleIndex+QOA_SLICE_LEN, 0, numSamples)*channels+channelIndex;

			for(int32_t sliceIndex=sliceStart;sliceIndex<sliceEnd;sliceIndex+=channels)
			{
				int32_t predicted=QOA_LMSPredict(&qoa->LMS[channelIndex]);
				int32_t quantized=(slice>>57)&0x7;
				int32_t dequantized=DequantTab[scaleFactor][quantized];
				int32_t reconstructed=QOA_ClampS16(predicted+dequantized);

				samples[sliceIndex]=reconstructed;

				slice<<=3;

				QOA_LMSUpdate(&qoa->LMS[channelIndex], reconstructed, dequantized);
			}
		}
	}

	*frameSamples=numSamples;

	return (uint8_t *)p-(uint8_t *)bytes;
}

bool QOA_DecodeHeader(uint64_t **ptr, const uint32_t size, QOA_Desc_t *qoa)
{
	if(ptr==NULL||size<16||qoa==NULL)
		return false;

	const uint64_t header=QOA_SwapU64(*(*ptr)++);

	const uint32_t magic=(header&QOA_HEADER_MAGIC_MASK)>>QOA_HEADER_MAGIC_SHIFT;

	if(magic!=QOA_MAGIC)
		return false;

	qoa->numSamples=(header&QOA_HEADER_NUMSAMPLES_MASK)>>QOA_HEADER_NUMSAMPLES_SHIFT;

	if(!qoa->numSamples)
		return false;

	const uint64_t frameHeader=QOA_SwapU64(*(*ptr));

	qoa->channels=(frameHeader&QOA_FRAMEHEADER_CHANNELS_MASK)>>QOA_FRAMEHEADER_CHANNELS_SHIFT;
	qoa->sampleRate=(frameHeader&QOA_FRAMEHEADER_SAMPLERATE_MASK)>>QOA_FRAMEHEADER_SAMPLERATE_SHIFT;
	qoa->frameNumSamples=(frameHeader&QOA_FRAMEHEADER_FRAMELENGTH_MASK)>>QOA_FRAMEHEADER_FRAMELENGTH_SHIFT;
	qoa->frameSize=(frameHeader&QOA_FRAMEHEADER_FRAMESIZE_MASK)>>QOA_FRAMEHEADER_FRAMESIZE_SHIFT;

	if(qoa->channels==0||qoa->numSamples==0||qoa->sampleRate==0)
		return false;

	return true;
}

void *QOA_Decode(const uint8_t *bytes, uint32_t size, QOA_Desc_t *qoa)
{
	uint64_t *p=(uint64_t *)bytes;

	if(!QOA_DecodeHeader(&p, size, qoa))
		return NULL;

	int16_t *samples=(int16_t *)Zone_Malloc(zone, qoa->numSamples*qoa->channels*sizeof(int16_t));

	if(samples==NULL)
		return NULL;

	for(uint32_t sampleIndex=0;sampleIndex<qoa->numSamples;)
	{
		uint32_t frameSamples=0;
		uint32_t frameSize=QOA_DecodeFrame(p, QOA_FRAME_LEN, qoa, samples+sampleIndex*qoa->channels, &frameSamples);

		if(!frameSize)
		{
			qoa->numSamples=sampleIndex;
			break;
		}

		p+=frameSize>>3; // divide by 8, because p is u64, not bytes
		sampleIndex+=frameSamples;
	}

	return samples;
}

bool QOA_OpenFile(QOA_File_t *qoaFile, const char *filename)
{
	qoaFile->file=fopen(filename, "rb");

	if(qoaFile->file==NULL)
		return false;

	// Read the file header and first frame header
	uint64_t header[2];
	if(fread(header, sizeof(uint64_t), 2, qoaFile->file)!=2)
	{
		fclose(qoaFile->file);
		return false;
	}

	// Decode header and initialize qoa descriptor.
	uint64_t *p=header;
	if(!QOA_DecodeHeader(&p, 16, &qoaFile->qoa))
	{
		fclose(qoaFile->file);
		return false;
	}

	// Seek back to frame header
	fseek(qoaFile->file, -(long)sizeof(uint64_t), SEEK_CUR);

	// Allocate memory for decoded samples.
	qoaFile->samples=(int16_t *)Zone_Malloc(zone, qoaFile->qoa.frameNumSamples*qoaFile->qoa.channels*sizeof(int16_t));

	if(qoaFile->samples==NULL)
	{
		fclose(qoaFile->file);
		return false;
	}

	qoaFile->totalSamples=0;
	qoaFile->currentSampleIndex=0;

	return true;
}

void QOA_CloseFile(QOA_File_t *qoaFile)
{
	if(qoaFile->file!=NULL)
		fclose(qoaFile->file);

	if(qoaFile->samples!=NULL)
		Zone_Free(zone, qoaFile->samples);
}

size_t QOA_Read(QOA_File_t *qoaFile, int16_t *output, size_t sampleCount)
{
	size_t samplesRead=0;

	while(samplesRead<sampleCount)
	{
		// Check if we've run out of decoded samples.
		if(qoaFile->currentSampleIndex>=qoaFile->totalSamples)
		{
			// Read the next frame.
			uint8_t frame[QOA_FRAME_LEN*QOA_MAX_CHANNELS];
			
			if(fread(frame, 1, qoaFile->qoa.frameSize, qoaFile->file)!=qoaFile->qoa.frameSize)
				break;

			uint32_t frameSamples=0;
			uint32_t frameSize=QOA_DecodeFrame(frame, qoaFile->qoa.frameSize, &qoaFile->qoa, qoaFile->samples, &frameSamples);

			int32_t sizeDiff=frameSize-qoaFile->qoa.frameSize;

			fseek(qoaFile->file, sizeDiff, SEEK_CUR);

			// If decoding fails, either end of stream or error.
			if(frameSize==0)
				break;

			qoaFile->totalSamples=frameSamples*qoaFile->qoa.channels;
			qoaFile->currentSampleIndex=0;
		}

		// Copy decoded samples to output buffer.
		size_t samplesToCopy=sampleCount-samplesRead;
		size_t availableSamples=qoaFile->totalSamples-qoaFile->currentSampleIndex;

		if(samplesToCopy>availableSamples)
			samplesToCopy=availableSamples;

		memcpy(output+samplesRead, qoaFile->samples+qoaFile->currentSampleIndex, samplesToCopy*sizeof(int16_t));

		qoaFile->currentSampleIndex+=samplesToCopy;
		samplesRead+=samplesToCopy;
	}

	return samplesRead;
}
