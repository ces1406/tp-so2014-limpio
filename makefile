instalar-commons: 
	cd Biblioteca-commons && make instalar
desinstalar-commons:
	cd Biblioteca-commons && make desinstalar

instalar-parser:
	cd Biblioteca-parser && make instalar
desinstalar-parser:
	cd Biblioteca-parser && make desinstalar

instalar-compartida:
	cd Biblioteca-compartida && make instalar
desinstalar-compartida:
	cd Biblioteca-compartida && make desinstalar

instalar-kernelPoll:
	cd KernelPoll && make instalar
desinstalar-kernelPoll:
	cd KernelPoll && make desinstalar
	
instalar-kernelSelect:
	cd KernelSelect && make instalar
desinstalar-kernelSelect:	
	cd KernelSelect && make instalar

instalar-umv:
	cd UMV && make instalar
desinstalar-umv:
	cd UMV && make desinstalar

instalar-cpu:
	cd CPU && make instalar
desinstalar-cpu:
	cd CPU && make desinstalar

instalar-programa:
	cd Programa && make instalar
desinstalar-programa:
	cd Programa && make desinstalar
