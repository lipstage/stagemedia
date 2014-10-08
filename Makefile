
CC=gcc
CFLAGS=-Wall -g3 # -O3 -fomit-frame-pointer
LDFLAGS=-lpthread -lmp3lame -lcurl -ljansson -lrt -lm -g3
SOURCES=config.c encoder_mp3.c master.c mem.c sock.c top.c http.c bytes.c control.c lib.c distro.c timing.c inject.c task.c api.c
OBJECTS=$(SOURCES:.c=.o)
EXEC=stagemediad

all: $(SOURCES) $(EXEC)

$(EXEC): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -c -o $@

clean:
	rm -rf $(OBJECTS) $(EXEC)
