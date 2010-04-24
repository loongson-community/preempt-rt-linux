CC=gcc
TEST1=clock_gettime
TEST2=gettimeofday

all:
	@$(CC) -o $(TEST1) $(TEST1).c -lrt
	@$(CC) -o $(TEST2) $(TEST2).c

clean:
	@rm -f $(TEST1) $(TEST2)
