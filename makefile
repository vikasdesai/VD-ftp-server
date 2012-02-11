myftp : server client
	@echo "myftp compiled"

server : server.o comm.o
	gcc -o server server.o comm.o

server.o : server.c server.h
	gcc -c server.c

comm.o : comm.c comm.h
	gcc -c comm.c

client : comm.o pi.o ui.o
	cc -o client comm.o pi.o ui.o

pi.o : pi.c pi.h
	cc -c pi.c

ui.o : ui.c ui.h
	cc -c ui.c

clean :
	rm -f *.o core client server
