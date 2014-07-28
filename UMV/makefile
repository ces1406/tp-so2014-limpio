CC=gcc
DIR_INICIO=$(shell cd .. && pwd)
LIB1=$(DIR_INICIO)/biblioteca-commons
LIB2=$(DIR_INICIO)/biblioteca-parser
LIB3=$(DIR_INICIO)/biblioteca-compartida

instalar:
	$(CC) umv.c -I$(LIB1) -I$(LIB2) -I$(LIB3) -L$(LIB1) -L$(LIB2) -L$(LIB3) -lcommons -lparser -lcompartidas -lpthread -o umv
desinstalar:
	rm -rf *.out
