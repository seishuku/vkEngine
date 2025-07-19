#ifndef __MODELS_H__
#define __MODELS_H__

#include "model/bmodel.h"

enum
{
	MODEL_ASTEROID1,
	MODEL_ASTEROID2,
	MODEL_ASTEROID3,
	MODEL_ASTEROID4,
	NUM_MODELS
};

extern BModel_t models[NUM_MODELS];
extern BModel_t fighter;
extern BModel_t cube;

typedef struct
{
	const char *filename;
	BModel_t model;
} Models_t;

#endif
