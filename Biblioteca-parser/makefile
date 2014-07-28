CC=gcc
DIR_INICIO=$(shell cd .. && pwd)
C_ARCHS1=$(shell find . -iname "*.c" | tr '\n' ' ') 
OBJS1=$(C_ARCHS1:./%.c=%.o)
%.o:%.c
	$(CC) -c -fmessage-length=0 -fPIC -o "$@" "$<" -I$(DIR_INICIO)/biblioteca-commons

instalar: $(OBJS1)
	$(CC) -o "libparser.so" $(OBJS1) -shared
desinstalar:
	rm -rf libparser.so
	rm -rf parser/*.o	
