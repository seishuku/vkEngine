TARGET=engine

# model loading/drawing
OBJS=model/3ds.o

# image loading
OBJS+=image/qoi.o
OBJS+=image/tga.o
OBJS+=image/image.o

# math
OBJS+=math/math.o

# camera
OBJS+=camera/camera.o

# Vulkan
OBJS+=vulkan/vulkan_buffer.o
OBJS+=vulkan/vulkan_context.o
OBJS+=vulkan/vulkan_descriptorset.o
OBJS+=vulkan/vulkan_instance.o
OBJS+=vulkan/vulkan_mem.o
OBJS+=vulkan/vulkan_pipeline.o

# core stuff
OBJS+=system/linux_x11.o
OBJS+=font/font.o
OBJS+=lights/lights.o
OBJS+=utils/genid.o
OBJS+=utils/list.o
OBJS+=utils/memzone.o
OBJS+=engine.o

SHADERS=shaders/distance.frag.spv
SHADERS+=shaders/distance.vert.spv
SHADERS+=shaders/font.frag.spv
SHADERS+=shaders/font.vert.spv
SHADERS+=shaders/lighting.frag.spv
SHADERS+=shaders/lighting.vert.spv

CC=gcc
CFLAGS=-Wall -O3 -std=c17 -I/usr/X11/include
LDFLAGS=-L/usr/X11/lib -lvulkan -lX11 -lm

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
