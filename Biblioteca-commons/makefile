CC=gcc
C_ARCHS1=$(shell find . -iname "*.c" | tr '\n' ' ') 
OBJS1=$(C_ARCHS1:./%.c=%.o)
%.o:%.c
	$(CC) -c -fmessage-length=0 -fPIC -o "$@" "$<"

instalar: $(OBJS1)
	$(CC) -o "libcommons.so" $(OBJS1) -shared
desinstalar:
	rm -rf libcommons.so
	rm -rf commons/*.o
	rm -rf commons/collections/*.o

