## linux
CC=gcc
EXECUTABLE=zimedit
CFLAGS = -g -Wall -lm -DLINUX_COMPILE -DLINUX_INSTALL
LIBS = -lncurses -lX11 -lpng -ljpeg -lz
LDLIBS = -lm -static-libgcc $(LIBS)

#CC = i686-w64-mingw32-gcc
#EXECUTABLE=zim.exe
# CFLAGS = -g -Wall -lm -DWINDOWS -DWINDOWS_INSTALL  -DWINDOWS_COMPILE -I/usr/i686-w64-mingw32/usr/include
#LIBS = -lmingw32 -lkernel32 -luser32 -lgdi32  -mwindows -lncurses
#EXECUTABLE=zim.exe
#CFLAGS = -g  -lm -DWINDOWS -DWINDOWS_INSTALL  -DWINDOWS_COMPILE 
#LIBS = -lmingw32 -lkernel32 -luser32 -lgdi32  -mwindows
#LDLIBS = -lm -static-libgcc $(LIBS)


SOURCES=main.c text_editor.c log.c file_browser.c config.c \
json.c x11.c

OBJECTS=$(SOURCES:.c=.o)

#windows: $(SOURCES_WINDOWS) $(EXECUTABLE_WINDOWS)

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(OBJECTS) $(LDLIBS) -o $@

#$(EXECUTABLE_WINDOWS): $(OBJECTS_MINGW) 
#	$(CC_MINGW) $(OBJECTS_MINGW) $(LDLIBS_WINDOWS) -o $@

# wihndows unused right now
# all: createResourcesO $(SOURCES) $(EXECUTABLE)

# $(EXECUTABLE): $(OBJECTS) icon.o
# 	$(CC) $(OBJECTS) icon.o $(LDLIBS) -o $@

# createResourcesO: icon.rc
# 	windres icon.rc -o icon.o

.c.o:
	$(CC) -c $(CFLAGS) $< -o $@

install:
	mkdir -p $(DESTDIR)/usr/bin/
	cp $(EXECUTABLE) $(DESTDIR)/usr/bin/	

clean:
	rm -rf *.o
