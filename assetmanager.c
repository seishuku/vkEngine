#include "system/system.h"
#include "vulkan/vulkan.h"
#include "audio/audio.h"
#include "model/bmodel.h"
#include "utils/loadingscreen.h"
#include "assetmanager.h"

extern VkuContext_t vkContext;
extern LoadingScreen_t loadingScreen;

// Layout is:
// { asset index, asset type, asset filename, asset loading flags }
// Index mapping is done through assetIndices to access asset by enum name for fast lookup.
// Example: To get SOUND_PEW1, use asset[assetIndices[SOUND_PEW1].sound...
// This will probably change later, just not sure of a less bad way of doing it right now.
AssetManager_t assets[]=
{
	{ TEXTURE_ASTEROID1,        ASSET_TEXTURE, "assets/textures/asteroid1.qoi",		IMAGE_MIPMAP|IMAGE_BILINEAR },
	{ TEXTURE_ASTEROID1_NORMAL, ASSET_TEXTURE, "assets/textures/asteroid1_n.qoi",	IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE },
	{ TEXTURE_ASTEROID2,        ASSET_TEXTURE, "assets/textures/asteroid2.qoi",		IMAGE_MIPMAP|IMAGE_BILINEAR },
	{ TEXTURE_ASTEROID2_NORMAL, ASSET_TEXTURE, "assets/textures/asteroid2_n.qoi",	IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE },
	{ TEXTURE_ASTEROID3,        ASSET_TEXTURE, "assets/textures/asteroid3.qoi",		IMAGE_MIPMAP|IMAGE_BILINEAR },
	{ TEXTURE_ASTEROID3_NORMAL, ASSET_TEXTURE, "assets/textures/asteroid3_n.qoi",	IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE },
	{ TEXTURE_ASTEROID4,        ASSET_TEXTURE, "assets/textures/asteroid4.qoi",		IMAGE_MIPMAP|IMAGE_BILINEAR },
	{ TEXTURE_ASTEROID4_NORMAL, ASSET_TEXTURE, "assets/textures/asteroid4_n.qoi",	IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE },
	{ TEXTURE_FIGHTER1,         ASSET_TEXTURE, "assets/textures/crono782.qoi",		IMAGE_MIPMAP|IMAGE_BILINEAR },
	{ TEXTURE_FIGHTER1_NORMAL,  ASSET_TEXTURE, "assets/textures/null_normal.qoi",	IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE },
	{ TEXTURE_FIGHTER2,         ASSET_TEXTURE, "assets/textures/cubik.qoi",			IMAGE_MIPMAP|IMAGE_BILINEAR },
	{ TEXTURE_FIGHTER2_NORMAL,  ASSET_TEXTURE, "assets/textures/null_normal.qoi",	IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE },
	{ TEXTURE_FIGHTER3,         ASSET_TEXTURE, "assets/textures/freelancer.qoi",	IMAGE_MIPMAP|IMAGE_BILINEAR },
	{ TEXTURE_FIGHTER3_NORMAL,  ASSET_TEXTURE, "assets/textures/null_normal.qoi",	IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE },
	{ TEXTURE_FIGHTER4,         ASSET_TEXTURE, "assets/textures/idolknight.qoi",	IMAGE_MIPMAP|IMAGE_BILINEAR },
	{ TEXTURE_FIGHTER4_NORMAL,  ASSET_TEXTURE, "assets/textures/null_normal.qoi",	IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE },
	{ TEXTURE_FIGHTER5,         ASSET_TEXTURE, "assets/textures/krulspeld1.qoi",	IMAGE_MIPMAP|IMAGE_BILINEAR },
	{ TEXTURE_FIGHTER5_NORMAL,  ASSET_TEXTURE, "assets/textures/null_normal.qoi",	IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE },
	{ TEXTURE_FIGHTER6,         ASSET_TEXTURE, "assets/textures/psionic.qoi",		IMAGE_MIPMAP|IMAGE_BILINEAR },
	{ TEXTURE_FIGHTER6_NORMAL,  ASSET_TEXTURE, "assets/textures/null_normal.qoi",	IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE },
	{ TEXTURE_FIGHTER7,         ASSET_TEXTURE, "assets/textures/thor.qoi",			IMAGE_MIPMAP|IMAGE_BILINEAR },
	{ TEXTURE_FIGHTER7_NORMAL,  ASSET_TEXTURE, "assets/textures/null_normal.qoi",	IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE },
	{ TEXTURE_FIGHTER8,         ASSET_TEXTURE, "assets/textures/wilko.qoi",			IMAGE_MIPMAP|IMAGE_BILINEAR },
	{ TEXTURE_FIGHTER8_NORMAL,  ASSET_TEXTURE, "assets/textures/null_normal.qoi",	IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE },
	{ TEXTURE_CUBE,             ASSET_TEXTURE, "assets/textures/null.qoi",			IMAGE_MIPMAP|IMAGE_BILINEAR },
	{ TEXTURE_CUBE_NORMAL,      ASSET_TEXTURE, "assets/textures/null_normal.qoi",	IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE },
	{ TEXTURE_FOX,				ASSET_TEXTURE, "assets/fox.tga",					IMAGE_NEAREST },
	{ TEXTURE_CROSSHAIR,        ASSET_TEXTURE, "assets/crosshair.qoi",				IMAGE_NONE },
	// TODO: If I load this before the fox, the fox has corruption, if I load the fox before... Everything is fine? Why?
	{ TEXTURE_CONSOLEBG,		ASSET_TEXTURE, "assets/splash.qoi",					IMAGE_NONE },
	
	{ MODEL_ASTEROID1,          ASSET_MODEL,   "assets/models/asteroid1.bmodel" },
	{ MODEL_ASTEROID2,          ASSET_MODEL,   "assets/models/asteroid2.bmodel" },
	{ MODEL_ASTEROID3,          ASSET_MODEL,   "assets/models/asteroid3.bmodel" },
	{ MODEL_ASTEROID4,          ASSET_MODEL,   "assets/models/asteroid4.bmodel" },
	{ MODEL_FIGHTER,            ASSET_MODEL,   "assets/models/fighter1.bmodel" },
	{ MODEL_CUBE,               ASSET_MODEL,   "assets/models/cube.bmodel" },
	{ MODEL_TEST,				ASSET_MODEL,   "assets/models/test.bmodel" },

	{ ANIMATION_TEST_CROSS,		ASSET_ANIMATION, "assets/animations/test_cross.banim" },
	{ ANIMATION_TEST_DANCE,		ASSET_ANIMATION, "assets/animations/test_dance.banim" },
	{ ANIMATION_TEST_FALL,		ASSET_ANIMATION, "assets/animations/test_fall.banim" },
	{ ANIMATION_TEST_IDLE,		ASSET_ANIMATION, "assets/animations/test_idle.banim" },
	{ ANIMATION_TEST_JAB,		ASSET_ANIMATION, "assets/animations/test_jab.banim" },
	{ ANIMATION_TEST_JUMP,		ASSET_ANIMATION, "assets/animations/test_jump.banim" },
	{ ANIMATION_TEST_LAND,		ASSET_ANIMATION, "assets/animations/test_land.banim" },
	{ ANIMATION_TEST_RUN,		ASSET_ANIMATION, "assets/animations/test_run.banim" },
	{ ANIMATION_TEST_WALK, 		ASSET_ANIMATION, "assets/animations/test_walk.banim" },

	{ SOUND_PEW1,               ASSET_SOUND,   "assets/sounds/pew1.wav" },
	{ SOUND_PEW2,               ASSET_SOUND,   "assets/sounds/pew2.wav" },
	{ SOUND_PEW3,               ASSET_SOUND,   "assets/sounds/pew3.wav" },
	{ SOUND_STONE1,             ASSET_SOUND,   "assets/sounds/stone1.wav" },
	{ SOUND_STONE2,             ASSET_SOUND,   "assets/sounds/stone2.wav" },
	{ SOUND_STONE3,             ASSET_SOUND,   "assets/sounds/stone3.wav" },
	{ SOUND_CRASH,              ASSET_SOUND,   "assets/sounds/crash.wav" },
	{ SOUND_EXPLODE1,           ASSET_SOUND,   "assets/sounds/explode1.qoa" },
	{ SOUND_EXPLODE2,           ASSET_SOUND,   "assets/sounds/explode2.qoa" },
	{ SOUND_EXPLODE3,           ASSET_SOUND,   "assets/sounds/explode3.qoa" },
};

uint32_t assetIndices[NUM_ASSETS]={ 0 };

AssetManager_t *AssetManager_GetAsset(AssetManager_t *assets, uint32_t ID)
{
	return &assets[assetIndices[ID]];
}

bool AssetManagerLoad(AssetManager_t *assets, uint32_t numAssets)
{
	for(uint32_t i=0;i<numAssets;i++)
	{
		if(assets[i].filename!=NULL)
		{
			DBGPRINTF(DEBUG_INFO, "Loading %s...\n", assets[i].filename);
			bool result=true;

			switch(assets[i].type)
			{
				case ASSET_TEXTURE:
				{
					result=Image_Upload(&vkContext, &assets[i].image, assets[i].filename, assets[i].flags);
					break;
				}

				case ASSET_MODEL:
				{
					result=LoadBModel(&assets[i].model, assets[i].filename);

					if(result)
						BuildMemoryBuffersBModel(&vkContext, &assets[i].model);
					break;
				}

				case ASSET_ANIMATION:
				{
					result=LoadBAnim(&assets[i].animation, assets[i].filename);
					break;
				}

				case ASSET_SOUND:
				{
					result=Audio_LoadStatic(assets[i].filename, &assets[i].sound);
					break;
				}

				default:
					DBGPRINTF(DEBUG_ERROR, "Unknown asset type.\n");
			}

			if(!result)
			{
				DBGPRINTF(DEBUG_ERROR, "Init: Failed to load %s\n", assets[i].filename);
				return false;
			}

			LoadingScreenAdvance(&loadingScreen);
		}

		assetIndices[assets[i].index]=i;
	}

	return true;
}

void AssetManagerDestroy(AssetManager_t *assets, uint32_t numAssets)
{
	for(uint32_t i=0;i<numAssets;i++)
	{
		switch(assets[i].type)
		{
			case ASSET_TEXTURE:
			{
				vkuDestroyImageBuffer(&vkContext, &assets[i].image);
				break;
			}

			case ASSET_MODEL:
			{
				vkuDestroyBuffer(&vkContext, &assets[i].model.boneBuffer);
				vkuDestroyBuffer(&vkContext, &assets[i].model.vertexBuffer);

				for(uint32_t j=0;j<assets[i].model.numMesh;j++)
					vkuDestroyBuffer(&vkContext, &assets[i].model.mesh[j].indexBuffer);

				FreeBModel(&assets[i].model);
				break;
			}

			case ASSET_ANIMATION:
			{
				FreeBAnim(&assets[i].animation);
				break;
			}

			case ASSET_SOUND:
			{
				Zone_Free(zone, assets[i].sound.data);
				break;
			}

			default:
				DBGPRINTF(DEBUG_ERROR, "Unknown asset type.\n");
		}
	}
}
