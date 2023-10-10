TARGET=vkEngine

# model loading/drawing
OBJS=model/bmodel.o

# image loading
OBJS+=image/qoi.o
OBJS+=image/tga.o
OBJS+=image/image.o

# math
OBJS+=math/math.o
OBJS+=math/matrix.o
OBJS+=math/quat.o
OBJS+=math/vec2.o
OBJS+=math/vec3.o
OBJS+=math/vec4.o

# physics
OBJS+=particle/particle.o
OBJS+=physics/physics.o

# camera
OBJS+=camera/camera.o

# network
OBJS+=network/network.o

# Vulkan
OBJS+=vulkan/vulkan_buffer.o
OBJS+=vulkan/vulkan_context.o
OBJS+=vulkan/vulkan_descriptorset.o
OBJS+=vulkan/vulkan_framebuffer.o
OBJS+=vulkan/vulkan_instance.o
OBJS+=vulkan/vulkan_mem.o
OBJS+=vulkan/vulkan_pipeline.o
OBJS+=vulkan/vulkan_renderpass.o
OBJS+=vulkan/vulkan_swapchain.o

# Audio
OBJS+=audio/audio.o
OBJS+=audio/hrir.o
OBJS+=audio/wave.o

# UI
OBJS+=ui/bargraph.o
OBJS+=ui/button.o
OBJS+=ui/checkbox.o
OBJS+=ui/ui.o

# core stuff
OBJS+=vr/vr.o

ifeq ($(OS),Windows_NT)
	OBJS+=system/win32.o
else
	OBJS+=system/linux_x11.o
endif

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
OBJS+=nebula.o
OBJS+=composite.o
OBJS+=engine.o

SHADERS=shaders/bezier.frag.spv
SHADERS+=shaders/bezier.geom.spv
SHADERS+=shaders/bezier.vert.spv
SHADERS+=shaders/composite.frag.spv
SHADERS+=shaders/compositeVR.frag.spv
SHADERS+=shaders/fullscreen.vert.spv
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
SHADERS+=shaders/gaussian.frag.spv
SHADERS+=shaders/threshold.frag.spv

CFLAGS=-Wall -Wno-missing-braces -Wextra -O3 -std=gnu17 -march=skylake
LDFLAGS=-Wold-style-definition -lm -lpthread -lopenvr_api -lportaudio

ifeq ($(OS),Windows_NT)
	CC=x86_64-w64-mingw32-gcc
	CFLAGS+=-DWIN32
	LDFLAGS+=-lvulkan-1 -lws2_32 -lgdi32
else
	CC=gcc
	CFLAGS+=-I/usr/X11/include
	LDFLAGS+=-lvulkan -lX11 -L/usr/X11/lib
endif

all: $(TARGET) $(SHADERS)

debug: CFLAGS=-Wall -Wno-missing-braces -Wextra -msse3 -DDEBUG -D_DEBUG -g -ggdb -O1 -std=gnu17

ifneq ($(OS),Windows_NT)
debug: CFLAGS+=-I/usr/X11/include
endif

debug: $(TARGET) $(SHADERS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

%.frag.spv: %.frag
	glslc --target-env=vulkan1.2 -O $< -o $@

%.vert.spv: %.vert
	glslc --target-env=vulkan1.2 -O $< -o $@

%.geom.spv: %.geom
	glslc --target-env=vulkan1.2 -O $< -o $@

%.comp.spv: %.comp
	glslc --target-env=vulkan1.2 -O $< -o $@

clean:
	$(RM) $(TARGET) $(OBJS) $(SHADERS)
