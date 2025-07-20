#include "system/system.h"
#include "vulkan/vulkan.h"
#include "audio/audio.h"
#include "model/bmodel.h"
#include "loadingscreen.h"
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
	{ TEXTURE_ASTEROID1,        ASSET_TEXTURE, "assets/asteroid1.qoi",   IMAGE_MIPMAP|IMAGE_BILINEAR },
	{ TEXTURE_ASTEROID1_NORMAL, ASSET_TEXTURE, "assets/asteroid1_n.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE },
	{ TEXTURE_ASTEROID2,        ASSET_TEXTURE, "assets/asteroid2.qoi",   IMAGE_MIPMAP|IMAGE_BILINEAR },
	{ TEXTURE_ASTEROID2_NORMAL, ASSET_TEXTURE, "assets/asteroid2_n.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE },
	{ TEXTURE_ASTEROID3,        ASSET_TEXTURE, "assets/asteroid3.qoi",   IMAGE_MIPMAP|IMAGE_BILINEAR },
	{ TEXTURE_ASTEROID3_NORMAL, ASSET_TEXTURE, "assets/asteroid3_n.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE },
	{ TEXTURE_ASTEROID4,        ASSET_TEXTURE, "assets/asteroid4.qoi",   IMAGE_MIPMAP|IMAGE_BILINEAR },
	{ TEXTURE_ASTEROID4_NORMAL, ASSET_TEXTURE, "assets/asteroid4_n.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE },
	{ TEXTURE_VOLUME,           ASSET_TEXTURE, NULL }, // Generated volume cloud texture, not handled by asset manager
	{ TEXTURE_CROSSHAIR,        ASSET_TEXTURE, "assets/crosshair.qoi",   IMAGE_NONE },
	{ TEXTURE_FIGHTER1,         ASSET_TEXTURE, "assets/crono782.qoi",    IMAGE_MIPMAP|IMAGE_BILINEAR },
	{ TEXTURE_FIGHTER1_NORMAL,  ASSET_TEXTURE, "assets/null_normal.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE },
	{ TEXTURE_FIGHTER2,         ASSET_TEXTURE, "assets/cubik.qoi",       IMAGE_MIPMAP|IMAGE_BILINEAR },
	{ TEXTURE_FIGHTER2_NORMAL,  ASSET_TEXTURE, "assets/null_normal.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE },
	{ TEXTURE_FIGHTER3,         ASSET_TEXTURE, "assets/freelancer.qoi",  IMAGE_MIPMAP|IMAGE_BILINEAR },
	{ TEXTURE_FIGHTER3_NORMAL,  ASSET_TEXTURE, "assets/null_normal.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE },
	{ TEXTURE_FIGHTER4,         ASSET_TEXTURE, "assets/idolknight.qoi",  IMAGE_MIPMAP|IMAGE_BILINEAR },
	{ TEXTURE_FIGHTER4_NORMAL,  ASSET_TEXTURE, "assets/null_normal.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE },
	{ TEXTURE_FIGHTER5,         ASSET_TEXTURE, "assets/krulspeld1.qoi",  IMAGE_MIPMAP|IMAGE_BILINEAR },
	{ TEXTURE_FIGHTER5_NORMAL,  ASSET_TEXTURE, "assets/null_normal.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE },
	{ TEXTURE_FIGHTER6,         ASSET_TEXTURE, "assets/psionic.qoi",     IMAGE_MIPMAP|IMAGE_BILINEAR },
	{ TEXTURE_FIGHTER6_NORMAL,  ASSET_TEXTURE, "assets/null_normal.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE },
	{ TEXTURE_FIGHTER7,         ASSET_TEXTURE, "assets/thor.qoi",        IMAGE_MIPMAP|IMAGE_BILINEAR },
	{ TEXTURE_FIGHTER7_NORMAL,  ASSET_TEXTURE, "assets/null_normal.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE },
	{ TEXTURE_FIGHTER8,         ASSET_TEXTURE, "assets/wilko.qoi",       IMAGE_MIPMAP|IMAGE_BILINEAR },
	{ TEXTURE_FIGHTER8_NORMAL,  ASSET_TEXTURE, "assets/null_normal.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE },
	{ TEXTURE_CUBE,             ASSET_TEXTURE, "assets/null_normal.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR },
	{ TEXTURE_CUBE_NORMAL,      ASSET_TEXTURE, "assets/null_normal.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE },

	{ MODEL_ASTEROID1,          ASSET_MODEL,   "assets/asteroid1.bmodel" },
	{ MODEL_ASTEROID2,          ASSET_MODEL,   "assets/asteroid2.bmodel" },
	{ MODEL_ASTEROID3,          ASSET_MODEL,   "assets/asteroid3.bmodel" },
	{ MODEL_ASTEROID4,          ASSET_MODEL,   "assets/asteroid4.bmodel" },
	{ MODEL_FIGHTER,            ASSET_MODEL,   "assets/fighter1.bmodel" },
	{ MODEL_CUBE,               ASSET_MODEL,   "assets/cube.bmodel" },

	{ SOUND_PEW1,               ASSET_SOUND,   "assets/pew1.wav" },
	{ SOUND_PEW2,               ASSET_SOUND,   "assets/pew1.wav" },
	{ SOUND_PEW3,               ASSET_SOUND,   "assets/pew1.wav" },
	{ SOUND_STONE1,             ASSET_SOUND,   "assets/stone1.wav" },
	{ SOUND_STONE2,             ASSET_SOUND,   "assets/stone2.wav" },
	{ SOUND_STONE3,             ASSET_SOUND,   "assets/stone3.wav" },
	{ SOUND_CRASH,              ASSET_SOUND,   "assets/crash.wav" },
	{ SOUND_EXPLODE1,           ASSET_SOUND,   "assets/explode1.qoa" },
	{ SOUND_EXPLODE2,           ASSET_SOUND,   "assets/explode2.qoa" },
	{ SOUND_EXPLODE3,           ASSET_SOUND,   "assets/explode3.qoa" },
};

uint32_t assetIndices[NUM_ASSETS]={ 0 };

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
				vkuDestroyBuffer(&vkContext, &assets[i].model.vertexBuffer);

				for(uint32_t j=0;j<assets[i].model.numMesh;j++)
					vkuDestroyBuffer(&vkContext, &assets[i].model.mesh[j].indexBuffer);

				FreeBModel(&assets[i].model);
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
