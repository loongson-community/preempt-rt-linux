CC=gcc
TEST1=pi
TEST2=prio

all:
	@$(CC) -o $(TEST1) $(TEST1).c -lrt -lpthread
	@$(CC) -o $(TEST2) $(TEST2).c

clean:
	@rm -f $(TEST1) $(TEST2)
