TARGET=vkEngine

# model loading/drawing
OBJS=model/bmodel.o

# image loading
OBJS+=image/qoi.o
OBJS+=image/tga.o
OBJS+=image/image.o

# math
OBJS+=math/math.o

# particle
OBJS+=particle/particle.o

# camera
OBJS+=camera/camera.o

# Vulkan
OBJS+=vulkan/vulkan_buffer.o
OBJS+=vulkan/vulkan_context.o
OBJS+=vulkan/vulkan_descriptorset.o
OBJS+=vulkan/vulkan_instance.o
OBJS+=vulkan/vulkan_mem.o
OBJS+=vulkan/vulkan_pipeline.o
OBJS+=vulkan/vulkan_swapchain.o

# core stuff
OBJS+=vr/vr.o
OBJS+=system/linux_x11.o
OBJS+=font/font.o
OBJS+=threads/threads.o
OBJS+=utils/input.o
OBJS+=utils/event.o
OBJS+=utils/genid.o
OBJS+=utils/list.o
OBJS+=utils/memzone.o
OBJS+=perframe.o
OBJS+=shadow.o
OBJS+=skybox.o
OBJS+=engine.o

SHADERS=shaders/bezier.frag.spv
SHADERS+=shaders/bezier.geom.spv
SHADERS+=shaders/bezier.vert.spv
SHADERS+=shaders/composite.frag.spv
SHADERS+=shaders/compositeVR.frag.spv
SHADERS+=shaders/composite.vert.spv
SHADERS+=shaders/font.frag.spv
SHADERS+=shaders/font.vert.spv
SHADERS+=shaders/lighting.frag.spv
SHADERS+=shaders/lighting.vert.spv
SHADERS+=shaders/particle.frag.spv
SHADERS+=shaders/particle.geom.spv
SHADERS+=shaders/particle.vert.spv
SHADERS+=shaders/shadow.vert.spv
SHADERS+=shaders/skybox.frag.spv
SHADERS+=shaders/skybox.vert.spv

CC=gcc
CFLAGS=-Wall -O3 -std=gnu17 -I/usr/X11/include
LDFLAGS=-Wold-style-definition -L/usr/X11/lib -lvulkan -lX11 -lm -lpthread -lopenvr_api

all: $(TARGET) $(SHADERS)

debug: CFLAGS+= -DDEBUG -D_DEBUG -g -ggdb -O1
debug: $(TARGET) $(SHADERS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

%.frag.spv: %.frag
	glslangValidator -V $< -o $@

%.vert.spv: %.vert
	glslangValidator -V $< -o $@

%.geom.spv: %.geom
	glslangValidator -V $< -o $@

%.comp.spv: %.comp
	glslangValidator -V $< -o $@

clean:
	$(RM) $(TARGET) $(OBJS) $(SHADERS)
