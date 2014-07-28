CC=gcc
DIR_INICIO=$(shell cd .. && pwd)
LIB1=$(DIR_INICIO)/biblioteca-commons
LIB2=$(DIR_INICIO)/biblioteca-parser
LIB3=$(DIR_INICIO)/biblioteca-compartida

instalar: cpu.o cpu_primitivas.o
	$(CC)  -o cpu cpu.o cpu_primitivas.o -I$(LIB1) -I$(LIB2) -I$(LIB3) -L$(LIB1) -L$(LIB2) -L$(LIB3) -lcommons -lparser -lcompartidas -lpthread 

%.o:%.c
	$(CC) -c "$<"  -I$(LIB1) -I$(LIB2) -I$(LIB3) -o "$@" 

desinstalar:
	rm -rf cpu.out

