CFLAGS = -Wall -Os -pipe -g

all: hog periodic

hog: hog.c
	gcc $(CFLAGS) edf_syscall.c hog.c -lrt -lpthread -o hog
	
periodic: periodic.c
	gcc $(CFLAGS) edf_syscall.c periodic.c -lrt -lpthread -Wall -o periodic

clean:
	rm -f *.o hog periodic
