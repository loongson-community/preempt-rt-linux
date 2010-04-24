CC=gcc
TEST1=sigsuspend
TEST2=sigwaitinfo

all:
	@$(CC) -o $(TEST1) $(TEST1).c -lrt
	@$(CC) -o $(TEST2) $(TEST2).c -lrt

clean:
	@rm -f $(TEST1) $(TEST2)
