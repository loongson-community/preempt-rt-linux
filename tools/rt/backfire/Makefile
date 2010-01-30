THREADLIB = -lpthread
EXTRA_CFLAGS = -Wall -O2

all:	sendme
	@echo Done

sendme.o: sendme.c
	$(CC) $(EXTRA_CFLAGS) $^ -o $@ -c

sendme: sendme.o
	$(CC) $^ -o $@ $(THREADLIB)

clean:
	@rm -f *.o

tar:	clean
	@rm -f sendme
	$(shell bn=`basename $$PWD`; cd ..; tar -zcf $$bn.tgz $$bn)
