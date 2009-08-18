# Makefile for fnkey
# uncomment the -DDEBUG=1 to enable debugging

CC=gcc
CFLAGS=-O2 #-DDEBUG=1

all: fnkey
fnkey: fnkey.c xosd.o
	$(CC) $(CFLAGS) -o fnkey fnkey.c xosd.o -lX11 -lXext -lpthread -lXft `pkg-config glib-2.0 --cflags --libs`
xosd.o: xosd.c
	$(CC) $(CFLAGS) -c -o xosd.o xosd.c -I/usr/include/freetype2

install: fnkey
	killall fnkey 2>&1 >/dev/null &
	sleep 1
	cp fnkey /usr/bin/
	rm -rf /etc/fnkey/
	mkdir /etc/fnkey/
	cp default.sh fnkey.conf /etc/fnkey/
	nohup fnkey > /dev/null 2>&1 >/dev/null &

uninstall:
	killall fnkey 2>&1 >/dev/null &
	sleep 1
	rm -rf /etc/fnkey
	rm /usr/bin/fnkey

clean:
	rm -f fnkey xosd.o
