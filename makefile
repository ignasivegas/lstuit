all: 
	gcc lsbird.c  -o lsbird -Wall -Wextra -lpthread
	gcc lsserver.c  -o lsserver -Wall -Wextra -lpthread
clean:
	rm lsserver
	rm lsbird