# use pkg-config for getting CFLAGS and LDLIBS
FFMPEG_LIBS=    libavdevice                        \
                libavformat                        \
                libavfilter                        \
                libavcodec                         \
                libswresample                      \
                libswscale                         \
                libavutil                          \

CFLAGS += -Wall -O2
CFLAGS := $(shell pkg-config --cflags $(FFMPEG_LIBS)) $(CFLAGS)
LDLIBS := $(shell pkg-config --libs $(FFMPEG_LIBS)) $(LDLIBS)

TARGET=player

SRC=main.c playback.c control.c sndcard.c alsa.c

OBJS=main.o playback.o control.o sndcard.o alsa.o

$(TARGET): $(OBJS)
	gcc -o $(TARGET) $(OBJS) $(LDLIBS)

$(OBJS): $(SRC)
	gcc -c $(CFLAGS) $(SRC)

clean:
	rm *.o $(TARGET)
