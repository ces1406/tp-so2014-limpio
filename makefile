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

instalar-kernel:
	cd Kernel && make instalar
desinstalar-kernel:
	cd Kernel && make desinstalar

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
