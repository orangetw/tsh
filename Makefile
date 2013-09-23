
CLIENT_OBJ=pel.c aes.c sha1.c  tsh.c
SERVER_OBJ=pel.c aes.c sha1.c tshd.c

all:
	@echo
	@echo "Please specify one of these targets:"
	@echo
	@echo "	make linux"
	@echo "	make freebsd"
	@echo "	make openbsd"
	@echo "	make netbsd"
	@echo "	make cygwin"
	@echo "	make sunos"
	@echo "	make irix"
	@echo "	make hpux"
	@echo "	make osf"
	@echo

clean:
	rm -f *.o tsh tshd

linux:
	gcc -O -W -Wall -o tsh  $(CLIENT_OBJ)
	gcc -O -W -Wall -o tshd $(SERVER_OBJ) -lutil -DLINUX
	strip tsh tshd

freebsd:
	gcc -O -W -Wall -o tsh  $(CLIENT_OBJ)
	gcc -O -W -Wall -o tshd $(SERVER_OBJ) -lutil -DFREEBSD
	strip tsh tshd

openbsd:
	gcc -O -W -Wall -o tsh  $(CLIENT_OBJ)
	gcc -O -W -Wall -o tshd $(SERVER_OBJ) -lutil -DOPENBSD
	strip tsh tshd

netbsd: openbsd

cygwin:
	gcc -O -W -Wall -o tsh  $(CLIENT_OBJ)
	gcc -O -W -Wall -o tshd $(SERVER_OBJ) -DCYGWIN
	strip tsh tshd

sunos:
	gcc -O -W -Wall -o tsh  $(CLIENT_OBJ) -lsocket -lnsl
	gcc -O -W -Wall -o tshd $(SERVER_OBJ) -lsocket -lnsl -DSUNOS
	strip tsh tshd

irix:
	cc -O -o tsh  $(CLIENT_OBJ)
	cc -O -o tshd $(SERVER_OBJ) -DIRIX
	strip tsh tshd

hpux:
	cc -O -o tsh  $(CLIENT_OBJ)
	cc -O -o tshd $(SERVER_OBJ) -DHPUX
	strip tsh tshd

osf:
	cc -O -o tsh  $(CLIENT_OBJ)
	cc -O -o tshd $(SERVER_OBJ) -DOSF
	strip tsh tshd

