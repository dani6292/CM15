LINKFILE=
CC = gcc


CFLAGS = -DDEVEL -DDEBUG -DLINUX -DX10DEMO -c
LFLAGS = -DDEVEL -DDEBUG -DLINUX -DX10DEMO -o $@

PROGS = cm15ademo
EXTLIBS = -lusb -lm -lpthread


all: $(PROGS)


cm15ademo: cm15ademo.c libusbahp.c config.h onetext.h libusbahp.c libusbahp.h cm15ax10.c
	$(CC) $(LFLAGS) cm15ax10.c cm15ademo.c libusbahp.c $(EXTLIBS)

%.o: %.c
	$(CC) $(CFLAGS) $<

clean:
	rm -f *.o $(PROGS)


