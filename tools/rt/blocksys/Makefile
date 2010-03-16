THREADLIB = -lpthread
EXTRA_CFLAGS = -Wall -O2

all:	mklatency
	@echo Done

mklatency.o: mklatency.c
	$(CC) $(EXTRA_CFLAGS) $^ -o $@ -c

mklatency: mklatency.o
	$(CC) $^ -o $@ $(THREADLIB)

clean:
	@rm -f *.o Module.markers modules.order

tar:	clean
	@rm -f mklatency
	$(shell bn=`basename $$PWD`; cd ..; tar -zcf $$bn.tgz $$bn)
