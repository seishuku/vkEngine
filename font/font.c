#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../vulkan/vulkan.h"
#include "../math/math.h"
#include "../utils/list.h"
#include "../vr/vr.h"
#include "font.h"

extern VkuContext_t Context;
extern VkRenderPass CompositeRenderPass;
extern VkRenderPass RenderPass;
extern VkSampleCountFlags MSAA;

extern uint32_t Width, Height;

VkPipelineLayout FontPipelineLayout;
VkuPipeline_t FontPipeline;

VkuBuffer_t FontBuffer;

// List for storing current string vectors
List_t FontVectors;

// Initialization flag
bool FontInit=true;

struct
{
	uint32_t Advance;
	uint32_t numPath;
	vec2 *Path;
} Gylphs[256];

uint32_t GylphSize=0;

bool LoadFontGylphs(const char *Filename)
{
	FILE *Stream=NULL;
	const uint32_t GYLP=('G'|('Y'<<8)|('L'<<16)|('P'<<24));

	Stream=fopen(Filename, "rb");

	if(!Stream)
		return false;

	uint32_t Magic=0;

	fread(&Magic, sizeof(uint32_t), 1, Stream);

	if(Magic!=GYLP)
		return false;

	// Vertical size
	fread(&GylphSize, sizeof(uint32_t), 1, Stream);
	GylphSize-=2;

	// Read in all chars
	for(uint32_t i=0;i<255;i++)
	{
		// Advancement spacing for char
		fread(&Gylphs[i].Advance, sizeof(uint32_t), 1, Stream);
		// Number of bezier curves in this char
		fread(&Gylphs[i].numPath, sizeof(uint32_t), 1, Stream);

		// Allocate memory and read in the paths
		Gylphs[i].Path=malloc(sizeof(vec2)*Gylphs[i].numPath);

		if(!Gylphs[i].Path)
			return false;

		fread(Gylphs[i].Path, sizeof(vec2), Gylphs[i].numPath, Stream);
	}

	fclose(Stream);

	return true;
}

void Font_Print(VkCommandBuffer CommandBuffer, uint32_t Eye, float x, float y, char *string, ...)
{
	// pointer and buffer for formatted text
	char *ptr, text[4096];
	// variable arguments list
	va_list	ap;
	// some misc variables
	int32_t sx=(int32_t)x;
	// current text color
	float r=1.0f, g=1.0f, b=1.0f;
	// scale factor, smaller value = larger font
	const float scale=2.0f;

	// Check if the string is even valid first
	if(string==NULL)
		return;

	// Format string, including variable arguments
	va_start(ap, string);
	vsprintf(text, string, ap);
	va_end(ap);

	// One time init, loads font paths, set up buffers and list
	if(FontInit)
	{
		if(!LoadFontGylphs("./assets/font.gylph"))
			return;

		vkCreatePipelineLayout(Context.Device, &(VkPipelineLayoutCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.pushConstantRangeCount=1,
			.pPushConstantRanges=&(VkPushConstantRange)
			{
				.offset=0,
				.size=sizeof(matrix),
				.stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_GEOMETRY_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,
			},
		}, 0, &FontPipelineLayout);

		vkuInitPipeline(&FontPipeline, &Context);

		vkuPipeline_SetPipelineLayout(&FontPipeline, FontPipelineLayout);
		vkuPipeline_SetRenderPass(&FontPipeline, CompositeRenderPass);

		FontPipeline.Topology=VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY;
//		FontPipeline.RasterizationSamples=MSAA;

		if(!vkuPipeline_AddStage(&FontPipeline, "./shaders/bezier.vert.spv", VK_SHADER_STAGE_VERTEX_BIT))
			return;

		if(!vkuPipeline_AddStage(&FontPipeline, "./shaders/bezier.geom.spv", VK_SHADER_STAGE_GEOMETRY_BIT))
			return;

		if(!vkuPipeline_AddStage(&FontPipeline, "./shaders/bezier.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT))
			return;

		vkuPipeline_AddVertexBinding(&FontPipeline, 0, sizeof(vec4)*2, VK_VERTEX_INPUT_RATE_VERTEX);
		vkuPipeline_AddVertexAttribute(&FontPipeline, 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*0);
		vkuPipeline_AddVertexAttribute(&FontPipeline, 1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*1);

		if(!vkuAssemblePipeline(&FontPipeline))
			return;

		// Initalize a list for building a list of control points to feed to the Bezier shader
		List_Init(&FontVectors, sizeof(vec4)*2, 4, NULL);

		vkuCreateHostBuffer(&Context, &FontBuffer, 4096*sizeof(vec4)*2*100, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

		// Done with init
		FontInit=false;
	}

	// Loop through the text string until EOL
	for(ptr=text;*ptr!='\0';ptr++)
	{
		// Decrement 'y' for any CR's
		if(*ptr=='\n')
		{
			x=(float)sx;
			y-=GylphSize;
			continue;
		}

		// Just advance spaces instead of rendering empty character
		if(*ptr==' ')
		{
			x+=Gylphs[(uint32_t)*ptr].Advance;
			continue;
		}

		// ANSI color escape codes
		// I'm sure there's a better way to do this!
		// But it works, so whatever.
		if(*ptr=='\x1B')
		{
			ptr++;
			     if(*(ptr+0)=='['&&*(ptr+1)=='3'&&*(ptr+2)=='0'&&*(ptr+3)=='m')	{	r=0.0f;	g=0.0f;	b=0.0f;	}	// BLACK
			else if(*(ptr+0)=='['&&*(ptr+1)=='3'&&*(ptr+2)=='1'&&*(ptr+3)=='m')	{	r=0.5f;	g=0.0f;	b=0.0f;	}	// DARK RED
			else if(*(ptr+0)=='['&&*(ptr+1)=='3'&&*(ptr+2)=='2'&&*(ptr+3)=='m')	{	r=0.0f;	g=0.5f;	b=0.0f;	}	// DARK GREEN
			else if(*(ptr+0)=='['&&*(ptr+1)=='3'&&*(ptr+2)=='3'&&*(ptr+3)=='m')	{	r=0.5f;	g=0.5f;	b=0.0f;	}	// DARK YELLOW
			else if(*(ptr+0)=='['&&*(ptr+1)=='3'&&*(ptr+2)=='4'&&*(ptr+3)=='m')	{	r=0.0f;	g=0.0f;	b=0.5f;	}	// DARK BLUE
			else if(*(ptr+0)=='['&&*(ptr+1)=='3'&&*(ptr+2)=='5'&&*(ptr+3)=='m')	{	r=0.5f;	g=0.0f;	b=0.5f;	}	// DARK MAGENTA
			else if(*(ptr+0)=='['&&*(ptr+1)=='3'&&*(ptr+2)=='6'&&*(ptr+3)=='m')	{	r=0.0f;	g=0.5f;	b=0.5f;	}	// DARK CYAN
			else if(*(ptr+0)=='['&&*(ptr+1)=='3'&&*(ptr+2)=='7'&&*(ptr+3)=='m')	{	r=0.5f;	g=0.5f;	b=0.5f;	}	// GREY
			else if(*(ptr+0)=='['&&*(ptr+1)=='9'&&*(ptr+2)=='0'&&*(ptr+3)=='m')	{	r=0.5f;	g=0.5f;	b=0.5f;	}	// GREY
			else if(*(ptr+0)=='['&&*(ptr+1)=='9'&&*(ptr+2)=='1'&&*(ptr+3)=='m')	{	r=1.0f;	g=0.0f;	b=0.0f;	}	// RED
			else if(*(ptr+0)=='['&&*(ptr+1)=='9'&&*(ptr+2)=='2'&&*(ptr+3)=='m')	{	r=0.0f;	g=1.0f;	b=0.0f;	}	// GREEN
			else if(*(ptr+0)=='['&&*(ptr+1)=='9'&&*(ptr+2)=='3'&&*(ptr+3)=='m')	{	r=1.0f;	g=1.0f;	b=0.0f;	}	// YELLOW
			else if(*(ptr+0)=='['&&*(ptr+1)=='9'&&*(ptr+2)=='4'&&*(ptr+3)=='m')	{	r=0.0f;	g=0.0f;	b=1.0f;	}	// BLUE
			else if(*(ptr+0)=='['&&*(ptr+1)=='9'&&*(ptr+2)=='5'&&*(ptr+3)=='m')	{	r=1.0f;	g=0.0f;	b=1.0f;	}	// MAGENTA
			else if(*(ptr+0)=='['&&*(ptr+1)=='9'&&*(ptr+2)=='6'&&*(ptr+3)=='m')	{	r=0.0f;	g=1.0f;	b=1.0f;	}	// CYAN
			else if(*(ptr+0)=='['&&*(ptr+1)=='9'&&*(ptr+2)=='7'&&*(ptr+3)=='m')	{	r=1.0f;	g=1.0f;	b=1.0f;	}	// WHITE
			ptr+=4;
		}

		// Push the current character gylph path with offset and color on to the list
		for(uint32_t i=0;i<Gylphs[(uint32_t)*ptr].numPath;i++)
		{
			float vert[]={ Gylphs[(uint32_t)*ptr].Path[i][0]+x, Gylphs[(uint32_t)*ptr].Path[i][1]+y, -1.0f, 1.0f, r, g, b, 1.0f};
			List_Add(&FontVectors, vert);
		}

		// Advance one character
		x+=Gylphs[(uint32_t)*ptr].Advance;
	}

	void *Data=NULL;

	vkMapMemory(Context.Device, FontBuffer.DeviceMemory, 0, VK_WHOLE_SIZE, 0, (void *)&Data);

	if(Data)
	{
		memcpy(Data, List_GetPointer(&FontVectors, 0), sizeof(float)*8*List_GetCount(&FontVectors));
		vkUnmapMemory(Context.Device, FontBuffer.DeviceMemory);
	}

	matrix Projection;
	MatrixIdentity(Projection);
	MatrixOrtho(0.0f, ((float)Width/Height)*scale, 0.0f, 1.0f*scale, 0.001f, 100.0f, Projection);

	// Draw characters!
	vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, FontPipeline.Pipeline);
	vkCmdPushConstants(CommandBuffer, FontPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_GEOMETRY_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(matrix), &Projection);
	vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &FontBuffer.Buffer, &(VkDeviceSize) { 0 });
	vkCmdDraw(CommandBuffer, (uint32_t)List_GetCount(&FontVectors), 1, 0, 0);

	// Clear the list for next time
	List_Clear(&FontVectors);
}

void Font_Destroy(void)
{
	// Loop through gylphs to free memory
	for(uint32_t i=0;i<255;i++)
		free(Gylphs[i].Path);

	// Destroy the control point list
	List_Destroy(&FontVectors);

	vkuDestroyBuffer(&Context, &FontBuffer);
	vkDestroyPipeline(Context.Device, FontPipeline.Pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(Context.Device, FontPipelineLayout, VK_NULL_HANDLE);
}
