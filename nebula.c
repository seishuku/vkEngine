#include "vulkan/vulkan.h"
#include "math/math.h"

extern VkuContext_t Context;
extern VkSampleCountFlags MSAA;
extern VkFormat ColorFormat;
extern VkFormat DepthFormat;

// Volume rendering vulkan stuff
VkuDescriptorSet_t VolumeDescriptorSet;
VkPipelineLayout VolumePipelineLayout;
VkuPipeline_t VolumePipeline;
//////

// Nebula volume texture generation
static int p[512]=
{
	151, 160, 137, 91,  90,  15,  131, 13,  201, 95,  96,  53,  194, 233, 7,   225,
	140, 36,  103, 30,  69,  142, 8,   99,  37,  240, 21,  10,  23,  190, 6,   148,
	247, 120, 234, 75,  0,   26,  197, 62,  94,  252, 219, 203, 117, 35,  11,  32,
	57,  177, 33,  88,  237, 149, 56,  87,  174, 20,  125, 136, 171, 168, 68,  175,
	74,  165, 71,  134, 139, 48,  27,  166, 77,  146, 158, 231, 83,  111, 229, 122,
	60,  211, 133, 230, 220, 105, 92,  41,  55,  46,  245, 40,  244, 102, 143, 54,
	65,  25,  63,  161, 1,   216, 80,  73,  209, 76,  132, 187, 208, 89,  18,  169,
	200, 196, 135, 130, 116, 188, 159, 86,  164, 100, 109, 198, 173, 186, 3,   64,
	52,  217, 226, 250, 124, 123, 5,   202, 38,  147, 118, 126, 255, 82,  85,  212,
	207, 206, 59,  227, 47,  16,  58,  17,  182, 189, 28,  42,  223, 183, 170, 213,
	119, 248, 152, 2,   44,  154, 163, 70,  221, 153, 101, 155, 167, 43,  172, 9,
	129, 22,  39,  253, 19,  98,  108, 110, 79,  113, 224, 232, 178, 185, 112, 104,
	218, 246, 97,  228, 251, 34,  242, 193, 238, 210, 144, 12,  191, 179, 162, 241,
	81,  51,  145, 235, 249, 14,  239, 107, 49,  192, 214, 31,  181, 199, 106, 157,
	184, 84,  204, 176, 115, 121, 50,  45,  127, 4,   150, 254, 138, 236, 205, 93,
	222, 114, 67,  29,  24,  72,  243, 141, 128, 195, 78,  66,  215, 61,  156, 180
};

static float fade(float t)
{
	return t*t*t*(t*(t*6-15)+10);
}

static float grad(int hash, float x, float y, float z)
{
	int h=hash&15;
	float u=h<8?x:y, v=h<4?y:h==12||h==14?x:z;

	return ((h&1)==0?u:-u)+((h&2)==0?v:-v);
}

static float noise(float x, float y, float z)
{
	static bool init=true;

	if(init)
	{
		for(int i=0; i<256; i++)
			p[256+i]=p[i];

		init=false;
	}

	int X=(int)floor(x)&255, Y=(int)floor(y)&255, Z=(int)floor(z)&255;

	x-=floorf(x);
	y-=floorf(y);
	z-=floorf(z);

	float u=fade(x), v=fade(y), w=fade(z);

	int A=p[X]+Y, AA=p[A]+Z, AB=p[A+1]+Z, B=p[X+1]+Y, BA=p[B]+Z, BB=p[B+1]+Z;

	return Lerp(
		Lerp(
			Lerp(
				grad(p[AA], x, y, z),
				grad(p[BA], x-1, y, z),
				u
			),
			Lerp(
				grad(p[AB], x, y-1, z),
				grad(p[BB], x-1, y-1, z),
				u
			),
			v
		),
		Lerp(
			Lerp(
				grad(p[AA+1], x, y, z-1),
				grad(p[BA+1], x-1, y, z-1),
				u
			),
			Lerp(
				grad(p[AB+1], x, y-1, z-1),
				grad(p[BB+1], x-1, y-1, z-1),
				u
			),
			v
		),
		w
	);
}

static float nebula(vec3 p)
{
	const int iterations=6;
	float turb=0.0f, scale=1.0f;

	for(int i=0;i<iterations;i++)
	{
		scale*=0.5f;
		turb+=scale*noise(p.x/scale, p.y/scale, p.z/scale);
	}

	return min(1.0f, max(0.0f, turb));
}

VkBool32 GenNebulaVolume(VkuContext_t *Context, VkuImage_t *Image)
{
	VkCommandBuffer CommandBuffer;
	VkuBuffer_t StagingBuffer;
	void *Data=NULL;

	Image->Width=64;
	Image->Height=64;
	Image->Depth=64; // Slight abuse of image struct, depth is supposed to be color depth, not image depth.

	// Byte size of image data
	uint32_t Size=Image->Width*Image->Height*Image->Depth;

	// Create staging buffer
	vkuCreateHostBuffer(Context, &StagingBuffer, Size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

	// Map image memory and copy data
	vkMapMemory(Context->Device, StagingBuffer.DeviceMemory, 0, VK_WHOLE_SIZE, 0, &Data);

	const float Scale=2.0f;

	for(uint32_t i=0;i<Size;i++)
	{
		uint32_t x=i%Image->Width;
		uint32_t y=(i%(Image->Width*Image->Height))/Image->Width;
		uint32_t z=i/(Image->Width*Image->Height);

		vec3 v=
		{
			((float)x-(Image->Width>>1))/Image->Width,
			((float)y-(Image->Height>>1))/Image->Height,
			((float)z-(Image->Depth>>1))/Image->Depth,
		};

		float p=nebula(Vec3_Muls(v, Scale));

		((uint8_t *)Data)[i]=(uint8_t)(p*255.0f);
	}

	vkUnmapMemory(Context->Device, StagingBuffer.DeviceMemory);

	if(!vkuCreateImageBuffer(Context, Image,
		VK_IMAGE_TYPE_3D, VK_FORMAT_R8_UNORM, 1, 1, Image->Width, Image->Height, Image->Depth,
		VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0))
		return VK_FALSE;

	// Start a one shot command buffer
	CommandBuffer=vkuOneShotCommandBufferBegin(Context);

	// Change image layout from undefined to destination optimal, so we can copy from the staging buffer to the texture.
	vkuTransitionLayout(CommandBuffer, Image->Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	// Copy from staging buffer to the texture buffer.
	vkCmdCopyBufferToImage(CommandBuffer, StagingBuffer.Buffer, Image->Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, (VkBufferImageCopy[1])
	{
		{
			0, 0, 0, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 }, { 0, 0, 0 }, { Image->Width, Image->Height, Image->Depth }
		}
	});

	// Final change to image layout from destination optimal to be optimal reading only by shader.
	// This is also done by generating mipmaps, if requested.
	vkuTransitionLayout(CommandBuffer, Image->Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	// End one shot command buffer and submit
	vkuOneShotCommandBufferEnd(Context, CommandBuffer);

	// Delete staging buffers
	vkuDestroyBuffer(Context, &StagingBuffer);

	// Create texture sampler object
	vkCreateSampler(Context->Device, &(VkSamplerCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter=VK_FILTER_LINEAR,
		.minFilter=VK_FILTER_LINEAR,
		.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST,
		.addressModeU=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.mipLodBias=0.0f,
		.compareOp=VK_COMPARE_OP_NEVER,
		.minLod=0.0f,
		.maxLod=VK_LOD_CLAMP_NONE,
		.maxAnisotropy=1.0f,
		.anisotropyEnable=VK_FALSE,
		.borderColor=VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
	}, VK_NULL_HANDLE, &Image->Sampler);

	// Create texture image view object
	vkCreateImageView(Context->Device, &(VkImageViewCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image=Image->Image,
		.viewType=VK_IMAGE_VIEW_TYPE_3D,
		.format=VK_FORMAT_R8_UNORM,
		.components={ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
		.subresourceRange={ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
	}, VK_NULL_HANDLE, &Image->View);

	return VK_TRUE;
}
//////

// Create functions for volume rendering
bool CreateVolumePipeline(void)
{
	vkuInitDescriptorSet(&VolumeDescriptorSet, &Context);
	vkuDescriptorSet_AddBinding(&VolumeDescriptorSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	vkuDescriptorSet_AddBinding(&VolumeDescriptorSet, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT);
	vkuAssembleDescriptorSetLayout(&VolumeDescriptorSet);

	vkCreatePipelineLayout(Context.Device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.setLayoutCount=1,
			.pSetLayouts=&VolumeDescriptorSet.DescriptorSetLayout,
			.pushConstantRangeCount=1,
			.pPushConstantRanges=&(VkPushConstantRange)
		{
			.offset=0,
				.size=sizeof(uint32_t),
				.stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT,
		},
	}, 0, &VolumePipelineLayout);

	vkuInitPipeline(&VolumePipeline, &Context);

	vkuPipeline_SetPipelineLayout(&VolumePipeline, VolumePipelineLayout);

	VolumePipeline.DepthTest=VK_TRUE;
	VolumePipeline.CullMode=VK_CULL_MODE_BACK_BIT;
	VolumePipeline.DepthCompareOp=VK_COMPARE_OP_GREATER_OR_EQUAL;
	VolumePipeline.RasterizationSamples=MSAA;

	VolumePipeline.Blend=VK_TRUE;
	VolumePipeline.SrcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;
	VolumePipeline.DstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	VolumePipeline.ColorBlendOp=VK_BLEND_OP_ADD;

	if(!vkuPipeline_AddStage(&VolumePipeline, "./shaders/volume.vert.spv", VK_SHADER_STAGE_VERTEX_BIT))
		return false;

	if(!vkuPipeline_AddStage(&VolumePipeline, "./shaders/volume.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT))
		return false;

	VkPipelineRenderingCreateInfo PipelineRenderingCreateInfo=
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.colorAttachmentCount=1,
		.pColorAttachmentFormats=&ColorFormat,
		.depthAttachmentFormat=DepthFormat,
	};

	if(!vkuAssemblePipeline(&VolumePipeline, &PipelineRenderingCreateInfo))
		return false;

	return true;
}
//////