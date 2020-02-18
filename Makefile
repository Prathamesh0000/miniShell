CC=gcc
FIleName = sh550
all:
	gcc $(FIleName).c  -o $(FIleName)
check: 
	valgrind --tool=memcheck --leak-check=yes --show-reachable=yes --num-callers=20 --track-fds=yes ./$(FIleName)
clean: 
	rm -rf $(FIleName)