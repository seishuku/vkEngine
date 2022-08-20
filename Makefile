TARGET=engine

# model loading/drawing
OBJS=model/3ds.o

# image loading
OBJS+=image/qoi.o
OBJS+=image/tga.o
OBJS+=image/image.o

# math
OBJS+=math/math.o

# core stuff
OBJS+=system/linux_x11.o
OBJS+=vulkan/vulkan.o
OBJS+=font/font.o
OBJS+=lights/lights.o
OBJS+=utils/genid.o
OBJS+=utils/list.o
OBJS+=engine.o

CC=gcc
CFLAGS=-Wall -O3 -std=c17 -I/usr/X11/include
LDFLAGS=-L/usr/X11/lib -lvulkan -lX11 -lm

all: $(TARGET)

debug: CFLAGS+= -DDEBUG -D_DEBUG -g -ggdb -O1
debug: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

.c: .o
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	-rm -r *.o
	-rm $(TARGET)
