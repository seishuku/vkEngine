#ifndef __QOA_H__
#define __QOA_H__

#include <stdint.h>

#define QOA_MAX_CHANNELS 8
#define QOA_LMS_LEN 4

typedef struct
{
	int32_t history[QOA_LMS_LEN];
	int32_t weights[QOA_LMS_LEN];
} QOA_LMS_t;

typedef struct
{
	uint8_t channels;
	uint32_t sampleRate;
	uint16_t frameNumSamples;
	uint16_t frameSize;
	uint32_t numSamples;
	QOA_LMS_t LMS[QOA_MAX_CHANNELS];
} QOA_Desc_t;

typedef struct
{
	FILE *file;
	QOA_Desc_t qoa;
	int16_t *samples;
	uint32_t totalSamples;
	uint32_t currentSampleIndex;
} QOA_File_t;

uint32_t QOA_EncodeFrame(const int16_t *samples, QOA_Desc_t *qoa, uint32_t frameLength, void *bytes);
void *QOA_Encode(const int16_t *samples, QOA_Desc_t *qoa, uint32_t *outLength);

uint32_t QOA_DecodeFrame(const void *bytes, uint32_t size, QOA_Desc_t *qoa, int16_t *samples, uint32_t *frameLength);
void *QOA_Decode(const uint8_t *bytes, uint32_t size, QOA_Desc_t *qoa);

bool QOA_OpenFile(QOA_File_t *qoaFile, const char *filename);
void QOA_CloseFile(QOA_File_t *qoaFile);
size_t QOA_Read(QOA_File_t *qoaFile, int16_t *output, size_t sampleCount);

#endif
