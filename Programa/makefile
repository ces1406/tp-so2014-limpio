CC=gcc
DIR_INICIO=$(shell cd .. && pwd)
LIB1=$(DIR_INICIO)/biblioteca-commons
LIB2=$(DIR_INICIO)/biblioteca-parser
LIB3=$(DIR_INICIO)/biblioteca-compartida

instalar: 
	$(CC) unprog.c -I$(LIB1) -I$(LIB2) -I$(LIB3) -L$(LIB1) -L$(LIB2) -L$(LIB3) -lcommons -lparser -lcompartidas -lpthread -o programa
	rm -rf /usr/bin/ansisop
	ln -s $(DIR_INICIO)/programa/programa /usr/bin/ansisop

desinstalar:
	rm -rf programa.out
	rm -rf /usr/bin/ansisop

