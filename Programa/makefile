CC=gcc
DIR_INICIO=$(shell cd .. && pwd)
LIB1=$(DIR_INICIO)/Biblioteca-commons
LIB2=$(DIR_INICIO)/Biblioteca-parser
LIB3=$(DIR_INICIO)/Biblioteca-compartida

instalar: 
	$(CC) programa.c -I$(LIB1) -I$(LIB2) -I$(LIB3) -L$(LIB1) -L$(LIB2) -L$(LIB3) -lcommons -lparser -lcompartidas -lpthread -o programa
	rm -rf /usr/bin/ansisop
	ln -s $(DIR_INICIO)/Programa/programa /usr/bin/ansisop
	chmod a+x *.ansisop

desinstalar:
	rm -rf programa
	rm -rf /usr/bin/ansisop

