/*
 * kernel.c
 *
 *  Created on: 22/05/2014
 *      Author: utnso
 */
#include "kernel.h"

//valores de configuracion
int    g_puertoCPU,g_puertoProg,g_puertoUMV;
char   gc_ipUMV[16];
char   gc_ipKernel[16];
int    g_quantum;
int    g_retardo;
int    g_multiprogramacion;
int    g_tamanioPila;
char **idHIOs;
char **valorHIO;
int    g_flag=1;
char **semaforos;
char **valorSemafs;
char **varCompartidas;
//contadores sockets-abiertos
int g_socketsAbiertosProg;
int g_socketsCpuAbiertos;
int g_socketsProg[50];
int g_socketsCpu[50];
//listas
t_list  *listaNuevos;
t_queue *colaListos;
t_list  *listaBloqueados;
t_list  *listaTerminados;
t_list  *listaEjecutando;
t_list  *listaCpuLibres;
t_list  *listaSemaforos;
//diccionarios
t_dictionary *diccio_hilos=NULL;
t_dictionary *diccio_varCompartidas=NULL;
//semaforos
sem_t sem_listaCpu;
sem_t sem_listaNuevos;
sem_t sem_listaListos;
sem_t sem_multiprog;
sem_t sem_semaforos;
sem_t sem_listaTerminados;
pthread_mutex_t mutex_listaCpu;
pthread_mutex_t mutex_listaListos;
pthread_mutex_t mutex_semaforos;
pthread_mutex_t mutex_listaTerminados;
pthread_mutex_t mutex_listaEjecutando;
pthread_mutex_t mutex_listaNuevos;
//hilos
pthread_t idHiloPCP;
pthread_t idHiloManejoListas;
pthread_t idHiloDespachador;
pthread_t idHiloSemaforo;
pthread_t idHiloTerminarProcesos;
//estructuras para select()
fd_set g_fds_lecturaProg,g_fds_lecturaCpu;

//log-socket-mensaje
t_log    *g_logger;
int       g_socketUMV;
t_msg     gs_mensajeUMV;
uint16_t  g_ids=0;

//HILOS
void *hiloPCP(void*);
void *hiloManejoListas(void*);
void *hiloDespachador(void *parametros);
void *hiloSemaforos(void *parametros);
void *hiloIO(void *sinUso);
void *hiloTerminarProcesos(void *sinUso);

void levantarArchivoConf(char*);
void atenderNuevaConexion(int,void(*funcionQueAtiende)(int),int*,int,int*);//atiende las nuevas conexiones:socket,cantConexiones,el ufds,funcion que atiende la nueva conexion
void atenderPrograma(int);//---->funcion para ser pasada como argumento
void atenderCPU(int);
uint16_t asignarID(t_pcb*);
int crearSegmentosYpcb(t_medatada_program *,t_pcb *,int,char *);
void encolarEnNuevos(t_pcb,short int,short int);
short int calcularPeso(t_size,t_size,t_size);
t_nodo_proceso* crearNodoProceso(t_pcb,short int);
void crearEstructuras();
void listarCpu();
void listarEjecutando();
void listarNuevos();
void listarListos();
void listarTerminados();
void listarBloqueados();
void actualizarPcb(t_pcb*,t_msg);
void crearHilosIO();
void crearListaSemaforos();
t_semaforo *crearSemaforo(char*,uint32_t);
void crearDiccioCompartidas();
t_varCompartida *crearVarCompartida(char*,uint32_t);
t_log *crearLog(char *archivo);
void liberarMsg(t_msg *mensaje);
void liberarVarGlob(char **varGlob);
void ponerCpuDisponible(int soquet);
void pasarProcesoATerminados(t_nodo_proceso *proceso);
static void destruirNodoSemaforo(t_semaforo *semaforo);

int main(int argc,char** argv){
	int  socketEscuchaPLP,socketMayor;
	int i;

	g_logger=crearLog(argv[0]);
	gs_mensajeUMV.flujoDatos=NULL;

	//LEVANTANDO EL ARCHIVO DE CONFIGURACION
	levantarArchivoConf(argv[1]);

	//CAPTAR LAS SEÑALES
	signal(SIGUSR1,liberarRecursos);
	signal(SIGSEGV,liberarRecursos);

	//CONECTANDOSE CON UMV
	g_socketUMV=crearSocket();
	conectarseCon(gc_ipUMV,g_puertoUMV,g_socketUMV);
	gs_mensajeUMV.encabezado.codMsg=KERNEL_HANDSHAKE;
	gs_mensajeUMV.encabezado.longitud=0;//nada para mandar serializado por ahora
	enviarMsg(g_socketUMV,gs_mensajeUMV);
	recibirMsg(g_socketUMV,&gs_mensajeUMV);//---->UMV ME RESPONDE EL HANDSHAKE POR AHORA CON mensajeUMV.encabezado.codMSg=CONEXION_OK hacer una funcion conexioUMVok()
	liberarMsg(&gs_mensajeUMV);
	log_debug(g_logger,"main()==>Conexion y handshake con umv hecha...");

	//CREAR ESTRUCTURAS
	crearEstructuras();

	//LANZANDO HILO PCP
	pthread_create(&idHiloPCP,NULL,(void*)&hiloPCP,NULL);
	//LANZANDO HILO DE MANEJO DE LISTAS
	pthread_create(&idHiloManejoListas,NULL,(void*)&hiloManejoListas,NULL);
	//LANZANDO HILO DESPACHADOR
	pthread_create(&idHiloDespachador,NULL,(void*)&hiloDespachador,NULL);
	//LANZANDO HILO TERMINAR PROCESOS
	pthread_create(&idHiloTerminarProcesos,NULL,(void*)&hiloTerminarProcesos,NULL);
	//LANZANDO HILOS DE ENTRADA/SALIDA
	crearHilosIO();
	//CREAR DICCIONARIO DE SEMAFOROS
	crearListaSemaforos();
	//LANZANDO HILO DE SEMAFOROS
	pthread_create(&idHiloSemaforo,NULL,(void*)&hiloSemaforos,NULL);
	//CREAR DICCIONARIO DE VARIABLES COMPARTIDAS
	crearDiccioCompartidas();

	//MULTIPLEXANDO CONEXIONES ENTRANTES DE PROGRAMA
	socketEscuchaPLP=crearSocket();
	bindearSocket(socketEscuchaPLP,gc_ipKernel,g_puertoProg);//es identica ip(tanto para plp como para pcp) pero distinto puerto???
	escucharSocket(socketEscuchaPLP);

	//SELECT
	FD_ZERO(&g_fds_lecturaProg);
	FD_SET(socketEscuchaPLP,&g_fds_lecturaProg);
	socketMayor=socketEscuchaPLP;
	g_socketsProg[0]=socketEscuchaPLP;
	g_socketsAbiertosProg=1;

	//printf("main()==>hiloPLP=>socketEscuchaPLP socketEscucha:%i g_socketsAbiertos:%i\n",socketEscuchaPLP,g_socketsAbiertosProg);
	//MAIN HILO PLP
	while(1){
		//printf("en while PLP con socketMayor:%i...\n",socketMayor);
		for (i=1;i<g_socketsAbiertosProg;i++){
			FD_SET(g_socketsProg[i],&g_fds_lecturaProg);
			//printf("puso en el conjunto g_fds_lecturaProg al socket %i\n",g_socketsProg[i]);
		}

		if(select(socketMayor+1,&g_fds_lecturaProg,NULL,NULL,NULL)==-1){
			printf("ERROR:error en select()\n");
			return EXIT_FAILURE;
		}
		if(FD_ISSET(g_socketsProg[0],&g_fds_lecturaProg)){
			printf("Nueva conexion de un programa\n");
			//un nuevo programa quiere conexion
			g_socketsAbiertosProg++;
			atenderNuevaConexion(socketEscuchaPLP,(void*)atenderPrograma,&socketMayor,g_socketsAbiertosProg,g_socketsProg);
		}
		//chequando pedidos de programas ya conectados
		for(i=1;i<g_socketsAbiertosProg;i++){
			if(FD_ISSET(g_socketsProg[i],&g_fds_lecturaProg)){
				//printf("actividad en el g_socketsProg[ %i ]\n",i);
				atenderPrograma(g_socketsProg[i]);
			}
		}
		FD_ZERO(&g_fds_lecturaProg);
		FD_SET(socketEscuchaPLP,&g_fds_lecturaProg);
	}
return EXIT_SUCCESS;
}
void atenderCPU(int p_sockCPU){
	//VARIABLES LOCALES
	int              tiempoIO,longNombreIO,k,i,r;
	char            *nombreIO=NULL;
	char            *nombCompar=NULL;
	char            *nombreSem=NULL;
	t_varCompartida *varBuscada;
	t_semaforo      *semaforo=NULL;
	t_nodo_proceso  *procesoAux=NULL;
	uint32_t         valorCompar;
	t_pcb            l_pcb;
	uint16_t         id;
	t_msg            mensajeCPU;
	mensajeCPU.flujoDatos=NULL;

	recibirMsg(p_sockCPU,&mensajeCPU);
	log_debug(g_logger,"atenderCPU()==>Se atiende a un mensaje de CPU...");
	bool buscaPorId(t_nodo_proceso *prog){return (prog->pcb.id_proceso==id);}
	bool mismoSoquet(void *sock1){
		int s1;
		memcpy(&s1,(int*)sock1,sizeof(int));
		return s1==p_sockCPU;
	}
	switch(mensajeCPU.encabezado.codMsg){
	case K_HANDSHAKE:
		log_debug(g_logger,"atenderCPU()===>mensaje de cpu: K_HANDSHAKE");
		//printf("atenderCPU()===>mensaje de cpu: K_HANDSHAKE\n");
		//se presento una cpu nueva
		mensajeCPU.encabezado.codMsg=CONEXION_OK;
		mensajeCPU.encabezado.longitud=0;
		enviarMsg(p_sockCPU,mensajeCPU);
		liberarMsg(&mensajeCPU);
		//una cpu disponible => agregar a la lista cpuLibres el socket con la cpu
		ponerCpuDisponible(p_sockCPU);
	break;
	case K_EXPULSADO_FIN_PROG:
		log_debug(g_logger,"atenderCPU()==>mensaje de cpu:K_EXPULSADO_FIN_PROG");
		//printf("atenderCPU()==>mensaje de cpu:K_EXPULSADO_FIN_PROG\n");
		//un proceso termino=>a la lista listaTerminados
		actualizarPcb(&l_pcb,mensajeCPU);//------------>hace falta?----->VER SINO OTRA FORMA DE SACAR EL id
		liberarMsg(&mensajeCPU);
		//buscarlo en listaEjecutando y removerlo
		id=l_pcb.id_proceso;
		pthread_mutex_lock(&mutex_listaEjecutando);
		procesoAux=(t_nodo_proceso*)list_remove_by_condition(listaEjecutando,(void *)buscaPorId);
		pthread_mutex_unlock(&mutex_listaEjecutando);
		listarEjecutando();
		printf("***********FIN DE PROGRAMA el proceso id:%i finalizo***********\n",id);
		//pasar el proceso a la listaTerminados
		pasarProcesoATerminados(procesoAux);
		//liberando la cpu
		ponerCpuDisponible(p_sockCPU);
	break;
	case K_EXPULSADO_ES://un proceso en cpu pidio e/s. Serializacion: pcb+dispositivo+tiempo
		log_debug(g_logger,"atenderCPU()==>mensaje de cpu:K_EXPULSADO_ES");
		//printf("atenderCPU()==>mensaje de cpu:K_EXPULSADO_ES\n");
		actualizarPcb(&l_pcb,mensajeCPU);

		//sacar el proceso de listaEjecutando
		id=l_pcb.id_proceso;
		pthread_mutex_lock(&mutex_listaEjecutando);
		procesoAux=(t_nodo_proceso*)list_remove_by_condition(listaEjecutando,(void *)buscaPorId);
		pthread_mutex_unlock(&mutex_listaEjecutando);
		listarEjecutando();

		//actualizar el pcb
		procesoAux->pcb=l_pcb;

		longNombreIO=mensajeCPU.encabezado.longitud-sizeof(u_int32_t)*8-sizeof(uint16_t)*2;
		nombreIO=realloc(nombreIO,longNombreIO+1);
		memcpy(nombreIO, mensajeCPU.flujoDatos+sizeof(u_int32_t)*7+sizeof(uint16_t)*2,longNombreIO);
		nombreIO[longNombreIO]='\0';
		memcpy(&tiempoIO, mensajeCPU.flujoDatos+sizeof(u_int32_t)*7+sizeof(uint16_t)*2+longNombreIO,sizeof(u_int32_t));
		liberarMsg(&mensajeCPU);

		//busco el hiloIO que lo va a tomar
		t_hiloIO *hiloES=dictionary_get(diccio_hilos,nombreIO);
		if(hiloES==NULL){
			printf("***********ERROR: el programa id:%i usa un dispositivo E/S inexistente =>expulsandolo...\n**********",id);
			pasarProcesoATerminados(procesoAux);
			//liberar cpu
			ponerCpuDisponible(p_sockCPU);
			free(nombreIO);nombreIO=NULL;
			break;
		}
		//creo el elemento que se agregara a la cola de ese hiloIO
		t_bloqueadoIO *procesoBlq=malloc(sizeof(t_bloqueadoIO));
		procesoBlq->proceso=procesoAux;
		procesoBlq->espera=tiempoIO;

		//agrego el elemento a la cola del hiloIO
		pthread_mutex_lock(&hiloES->dataHilo.mutex_io);
		queue_push(hiloES->dataHilo.bloqueados,procesoBlq);
		listarBloqueados();
		pthread_mutex_unlock(&hiloES->dataHilo.mutex_io);
		sem_post(&hiloES->dataHilo.sem_io);

		//PONIENDO A CPU "DISPONIBLE" PARA OTROS PROCESOS
		ponerCpuDisponible(p_sockCPU);
		free(nombreIO);nombreIO=NULL;
	break;
	case K_WAIT://serializado: id_proceso+nombre_de_semaforo
		semaforo=NULL;
		log_debug(g_logger,"atenderCPU()==>mensaje de cpu: K_WAIT");
		//printf("atenderCPU()==>mensaje de cpu: K_WAIT\n");

		nombreSem=realloc(nombreSem,mensajeCPU.encabezado.longitud-sizeof(uint16_t)+1);
		memcpy(nombreSem,mensajeCPU.flujoDatos+sizeof(uint16_t),mensajeCPU.encabezado.longitud-sizeof(uint16_t));
		nombreSem[mensajeCPU.encabezado.longitud-sizeof(uint16_t)]='\0';
		//semaforo=(t_semaforo*)dictionary_get(diccio_semaforos,nombreSem);
		printf("llego pedido de wait semaforo:%s\n",nombreSem);
		for(i=0;i<list_size(listaSemaforos);i++){
			printf("iteracion %i\n",i);
			semaforo=list_get(listaSemaforos,i);
			printf("semaforo->nombre:%s nombreSema:%s\nstrcmp(semaforo->nombre,nombreSem)=%i\n",semaforo->nombre,nombreSem,strcmp(semaforo->nombre,nombreSem));
			if(strcmp(semaforo->nombre,nombreSem)==0){
				printf("strincmp dio distinto de cero\n");
				break;
			}
			else semaforo=NULL;
		}

		if (semaforo==NULL){
			printf("***********ERROR: el programa usa un semaforo inexistente =>expulsandolo...\n***********");
			memcpy(&id,mensajeCPU.flujoDatos,sizeof(uint16_t));

			mensajeCPU.encabezado.codMsg=C_ERROR;
			mensajeCPU.encabezado.longitud=0;
			enviarMsg(p_sockCPU,mensajeCPU);
			liberarMsg(&mensajeCPU);

			//buscarlo en listaEjecutando
			pthread_mutex_lock(&mutex_listaEjecutando);
			procesoAux=(t_nodo_proceso*)list_remove_by_condition(listaEjecutando,(void *)buscaPorId);
			pthread_mutex_unlock(&mutex_listaEjecutando);
			listarEjecutando();
			//pasar el proceso a la listaTerminados
			pasarProcesoATerminados(procesoAux);
			ponerCpuDisponible(p_sockCPU);
			free(nombreSem);nombreSem=NULL;
			break;
		}
		//printf("se encontro el semaforo en la lista nombre:%s valor:%i\n",semaforo->nombre,semaforo->valor);
		r=semaforo->valor;
		if(r>0){
			//mandar mensaje a cpu que no se bloqueara el proceso en ese semaforo
			mensajeCPU.encabezado.codMsg=C_WAIT_OK;
			mensajeCPU.encabezado.longitud=0;
			enviarMsg(p_sockCPU,mensajeCPU);
			liberarMsg(&mensajeCPU);
			pthread_mutex_lock(&mutex_semaforos);
			semaforo->valor--;
			pthread_mutex_unlock(&mutex_semaforos);
		}else{
			//se avisa a cpu que el semaforo esta ocupado
			mensajeCPU.encabezado.codMsg=95;
			mensajeCPU.encabezado.longitud=0;
			enviarMsg(p_sockCPU,mensajeCPU);
			liberarMsg(&mensajeCPU);
			recibirMsg(p_sockCPU,&mensajeCPU);

			actualizarPcb(&l_pcb,mensajeCPU);
			//buscarlo en listaEjecutando
			id=l_pcb.id_proceso;

			pthread_mutex_lock(&mutex_listaEjecutando);
			procesoAux=(t_nodo_proceso*)list_remove_by_condition(listaEjecutando,(void *)buscaPorId);
			pthread_mutex_unlock(&mutex_listaEjecutando);
			listarEjecutando();

			//actualizar el pcb
			procesoAux->pcb=l_pcb;

			pthread_mutex_lock(&mutex_semaforos);
			queue_push(semaforo->bloqueados,procesoAux);
			semaforo->valor--;
			pthread_mutex_unlock(&mutex_semaforos);
			listarBloqueados();
			//liberando la cpu
			ponerCpuDisponible(p_sockCPU);
		}
		free(nombreSem);nombreSem=NULL;
	break;
	case K_SIGNAL://serializado: id_proceso+nombre de_semaforo
		semaforo=NULL;
		log_debug(g_logger,"atenderCPU()==>mensaje de cpu: K_SIGNAL");
		//printf("atenderCPU()==>mensaje de cpu: K_SIGNAL\n");

		nombreSem=realloc(nombreSem,mensajeCPU.encabezado.longitud-sizeof(uint16_t)+1);
		memcpy(nombreSem,mensajeCPU.flujoDatos+sizeof(uint16_t),mensajeCPU.encabezado.longitud-sizeof(uint16_t));
		nombreSem[mensajeCPU.encabezado.longitud-sizeof(uint16_t)]='\0';
		//semaforo=(t_semaforo*)dictionary_get(diccio_semaforos,nombreSem);
		for(i=0;i<list_size(listaSemaforos);i++){
			semaforo=list_get(listaSemaforos,i);
			if(strcmp(semaforo->nombre,nombreSem)==0)break;
			else semaforo=NULL;
		}

		//mensajeCPU.flujoDatos[mensajeCPU.encabezado.longitud+sizeof(uint16_t)]='\0';
		//semaforo=(t_semaforo*)dictionary_get(diccio_semaforos,&mensajeCPU.flujoDatos[sizeof(uint16_t)]);
		if (semaforo==NULL){
			printf("***********ERROR: el programa usa un semaforo inexistente =>expulsandolo...\n*********");
			memcpy(&id,mensajeCPU.flujoDatos,sizeof(uint16_t));

			mensajeCPU.encabezado.codMsg=C_ERROR;
			mensajeCPU.encabezado.longitud=0;
			enviarMsg(p_sockCPU,mensajeCPU);
			liberarMsg(&mensajeCPU);

			//buscarlo en listaEjecutando
			pthread_mutex_lock(&mutex_listaEjecutando);
			procesoAux=(t_nodo_proceso*)list_remove_by_condition(listaEjecutando,(void *)buscaPorId);
			pthread_mutex_unlock(&mutex_listaEjecutando);
			listarEjecutando();
			//pasar el proceso a la listaTerminados
			pasarProcesoATerminados(procesoAux);
			//liberar cpu
			ponerCpuDisponible(p_sockCPU);
			free(nombreSem);nombreSem=NULL;
			break;
		}
		//printf("se encontro el semaforo en la lista nombre:%s valor:%i\n",semaforo->nombre,semaforo->valor);

		mensajeCPU.encabezado.codMsg=C_SIGNAL_OK;
		mensajeCPU.encabezado.longitud=0;
		enviarMsg(p_sockCPU,mensajeCPU);
		liberarMsg(&mensajeCPU);
		k=semaforo->valor;//sino en el if hace cagada(????)
		//printf("semaforo:%s valor:%i\n",semaforo->nombre,k);
		if(k<0){//=>el semaforo era negativo =>desencolar algun proceso que estuviera bloqueado
			pthread_mutex_lock(&mutex_semaforos);
			semaforo->valor++;
			semaforo->desbloquear=true;
			pthread_mutex_unlock(&mutex_semaforos);
			sem_post(&sem_semaforos);
		}else{//=>el semaforo era positivo =>no hay que desbloquear nada
			pthread_mutex_lock(&mutex_semaforos);
			semaforo->valor++;
			pthread_mutex_unlock(&mutex_semaforos);
		}
		free(nombreSem);nombreSem=NULL;
	break;
	case K_EXPULSADO_SEG_FAULT://un proceso en cpu salio por operacion invalida en memoria
		log_debug(g_logger,"atenderCPU()==>mensaje de cpu: K_EXPULSADO_SEG_FAULT");
		//printf("atenderCPU()==>mensaje de cpu: K_EXPULSADO_SEG_FAULT\n");
		actualizarPcb(&l_pcb,mensajeCPU);//--------->VER OTRA FORMA DE SACAR EL id
		id=l_pcb.id_proceso;
		liberarMsg(&mensajeCPU);
		pthread_mutex_lock(&mutex_listaEjecutando);
		procesoAux=(t_nodo_proceso*)list_remove_by_condition(listaEjecutando,(void *)buscaPorId);
		pthread_mutex_unlock(&mutex_listaEjecutando);
		listarEjecutando();

		log_debug(g_logger,"atenderCPU()==>el proceso id:%i produjo una falla en acceso a memoria...",procesoAux->pcb.id_proceso);
		printf("***********ERROR: el programa id:%i produjo un acceso a memoria incorrecto =>expulsandolo...**********\n",id);

		//pasar el proceso a la listaTerminados
		pasarProcesoATerminados(procesoAux);
		//liberar cpu
		ponerCpuDisponible(p_sockCPU);
	break;
	case K_EXPULSADO_FIN_QUANTUM://un proceso en cpu salio por fin de quantum
		log_debug(g_logger,"atenderCPU()==>mensaje de cpu: K_EXPULSADO_FIN_QUANTUM");
		//printf("atenderCPU()==>mensaje de cpu: K_EXPULSADO_FIN_QUANTUM\n");

		actualizarPcb(&l_pcb,mensajeCPU);
		liberarMsg(&mensajeCPU);
		id=l_pcb.id_proceso;

		pthread_mutex_lock(&mutex_listaEjecutando);
		procesoAux=(t_nodo_proceso*)list_remove_by_condition(listaEjecutando,(void*)buscaPorId);
		pthread_mutex_unlock(&mutex_listaEjecutando);
		listarEjecutando();

		//actualizar el pcb
		procesoAux->pcb=l_pcb;

		pthread_mutex_lock(&mutex_listaListos);
		//list_add_in_index(listaListos,list_size(listaListos),procesoAux);
		queue_push(colaListos,procesoAux);
		pthread_mutex_unlock(&mutex_listaListos);
		sem_post(&sem_listaListos);
		listarListos();

		//PONIENDO A CPU "DISPONIBLE" PARA OTROS PROCESOS
		ponerCpuDisponible(p_sockCPU);
	break;
	case K_EXPULSADO_DESCONEXION: //cpu avisa que se va a desconectar
		log_debug(g_logger,"atenderCPU()==>	mensaje de cpu: K_EXPULSADO_DESCONEXION");
		//printf("atenderCPU()==>	mensaje de cpu: K_EXPULSADO_DESCONEXION\n");
		printf("\n==>	mensaje de cpu: K_EXPULSADO_DESCONEXION\n");
		if(list_any_satisfy(listaCpuLibres,(void*) mismoSoquet)){
			//la cpu que se desconecta no esta ejecutando ningun proceso
			liberarMsg(&mensajeCPU);
			sem_wait(&sem_listaCpu);
			pthread_mutex_lock(&mutex_listaCpu);
			int* cpu= (int*)list_remove_by_condition(listaCpuLibres,(void*)mismoSoquet);
			free(cpu);
			pthread_mutex_unlock(&mutex_listaCpu);
		}else{
			actualizarPcb(&l_pcb,mensajeCPU);
			liberarMsg(&mensajeCPU);
			id=l_pcb.id_proceso;
			pthread_mutex_lock(&mutex_listaEjecutando);
			procesoAux=(t_nodo_proceso*)list_remove_by_condition(listaEjecutando,(void*)buscaPorId);
			pthread_mutex_unlock(&mutex_listaEjecutando);
			listarEjecutando();
			//actualizar el pcb
			procesoAux->pcb=l_pcb;

			pthread_mutex_lock(&mutex_listaListos);
			queue_push(colaListos,procesoAux);
			pthread_mutex_unlock(&mutex_listaListos);
			sem_post(&sem_listaListos);
			listarListos();
		}
		//CERRAR EL SOCKET DE ESA CONEXION
		//reordenando el vector de sockets de los cpus
		/*printf("vector de sockets de cpu antes de la desconexion:\n");
		for(i=1;i<g_socketsCpuAbiertos;i++){
			printf("elemento:%i socket:%i\n",i,g_socketsCpu[i]);
		}*/
		for(i=1;i<g_socketsCpuAbiertos;i++){
			if(g_socketsCpu[i]==p_sockCPU){break;}
		}
		for(k=0;k<g_socketsCpuAbiertos-i;k++){
			g_socketsCpu[i]=g_socketsCpu[i+1];
		}
		g_socketsCpuAbiertos--;
		/*printf("vector de sockets de cpu despues de la desconexion:\n");
			for(i=1;i<g_socketsCpuAbiertos;i++){
				printf("elemento:%i socket:%i\n",i,g_socketsCpu[i]);
			}*/
		//cerrando la conexion
		close(p_sockCPU);
	break;
	case K_IMPRIMIR_VAR:
		log_debug(g_logger,"atenderCPU()==>mensaje de cpu: K_IMPRIMIR_VAR");
		//printf("K_IMPRIMIR_VAR****\n");
		//mandarle un mensaje a programa con el valor a ser impreso

		uint32_t dato;
		memcpy(&dato,mensajeCPU.flujoDatos,sizeof(uint32_t));
		memcpy(&id,mensajeCPU.flujoDatos+sizeof(uint32_t),sizeof(uint16_t));

		procesoAux=(t_nodo_proceso*)list_find(listaEjecutando,(void*)buscaPorId);//no haria falta semaforos, solo busca una referencia a el

		mensajeCPU.encabezado.codMsg=P_IMPRIMIR_VAR;
		mensajeCPU.encabezado.longitud=sizeof(uint32_t);
		mensajeCPU.flujoDatos=realloc(mensajeCPU.flujoDatos,mensajeCPU.encabezado.longitud);
		memcpy(mensajeCPU.flujoDatos,&dato,sizeof(uint32_t));
		enviarMsg(procesoAux->soquet_prog,mensajeCPU);
		free(mensajeCPU.flujoDatos);mensajeCPU.flujoDatos=NULL;
	break;
	case K_IMPRIMIR_TXT://serializado idProceso(uint16_t)+texto
		log_debug(g_logger,"atenderCPU()==>mensaje de cpu: K_IMPRIMIR_TXT");
		//printf("atenderCPU()==>mensaje de cpu: K_IMPRIMIR_TXT\n");
		//mandarle un mensaje a programa con el texto a ser impreso
		memcpy(&id,mensajeCPU.flujoDatos,sizeof(uint16_t));
		mensajeCPU.encabezado.longitud=mensajeCPU.encabezado.longitud-sizeof(uint16_t);

		memmove(mensajeCPU.flujoDatos,&(mensajeCPU.flujoDatos[sizeof(uint16_t)]),mensajeCPU.encabezado.longitud);
		procesoAux=(t_nodo_proceso*)list_find(listaEjecutando,(void*)buscaPorId);
		mensajeCPU.encabezado.codMsg=P_IMPRIMIR_TXT;

		enviarMsg(procesoAux->soquet_prog,mensajeCPU);
		free(mensajeCPU.flujoDatos);mensajeCPU.flujoDatos=NULL;
	break;
	case K_PEDIDO_VAR_GL://serializacion:id_proceso+nombre_var_compartida
		log_debug(g_logger,"atenderCPU()==>mensaje de cpu: K_PEDIDO_VAR_GL");
		//printf("atenderCPU()==>mensaje de cpu: K_PEDIDO_VAR_GL\n");

		nombCompar=realloc(nombCompar,mensajeCPU.encabezado.longitud-sizeof(uint16_t)+1);
		memcpy(nombCompar,mensajeCPU.flujoDatos+sizeof(uint16_t),mensajeCPU.encabezado.longitud-sizeof(uint16_t));
		nombCompar[mensajeCPU.encabezado.longitud-sizeof(uint16_t)]='\0';
		varBuscada=dictionary_get(diccio_varCompartidas,nombCompar);

		//mensajeCPU.flujoDatos[mensajeCPU.encabezado.longitud+sizeof(uint16_t)]='\0';
		//varBuscada=dictionary_get(diccio_varCompartidas,&mensajeCPU.flujoDatos[sizeof(uint16_t)]);
		//printf("K_VAR_GL()==>llego pedido por la var:%s\n",nombCompar);

		if(varBuscada==NULL){
			printf("***********ERROR:el programa referencia una variable compartida inexistente=>expulsandolo...***********\n");
			memcpy(&id,mensajeCPU.flujoDatos,sizeof(uint16_t));

			mensajeCPU.encabezado.codMsg=C_ERROR;
			mensajeCPU.encabezado.longitud=0;
			enviarMsg(p_sockCPU,mensajeCPU);
			liberarMsg(&mensajeCPU);
			//buscarlo en listaEjecutando
			pthread_mutex_lock(&mutex_listaEjecutando);
			procesoAux=(t_nodo_proceso*)list_remove_by_condition(listaEjecutando,(void *)buscaPorId);
			pthread_mutex_unlock(&mutex_listaEjecutando);
			listarEjecutando();
			//pasar el proceso a la listaTerminados
			pasarProcesoATerminados(procesoAux);
			//liberar cpu
			ponerCpuDisponible(p_sockCPU);
			free(nombCompar);nombCompar=NULL;
			break;
		}
		//printf("el pedido de varGL fue ok\n");
		mensajeCPU.encabezado.codMsg=VALOR_COMPARTIDA_OK;
		mensajeCPU.encabezado.longitud=sizeof(uint32_t);
		mensajeCPU.flujoDatos=realloc(mensajeCPU.flujoDatos,sizeof(uint32_t));
		memcpy(mensajeCPU.flujoDatos,&varBuscada->valor,sizeof(uint32_t));
		enviarMsg(p_sockCPU,mensajeCPU);
		liberarMsg(&mensajeCPU);
		free(nombCompar);nombCompar=NULL;
	break;
	case K_ASIGNAR_VAR_GL://serializado: id_proceso+nombre+valor
		log_debug(g_logger,"atenderCPU()==>mensaje de cpu: K_ASIGNAR_VAR_GL");
		//printf("atenderCPU()==>mensaje de cpu: K_ASIGNAR_VAR_GL\n");
		k=mensajeCPU.encabezado.longitud-sizeof(uint16_t)-sizeof(uint32_t);
		nombCompar=realloc(nombCompar,k+1);
		memcpy(&id,mensajeCPU.flujoDatos,sizeof(uint16_t));
		memcpy(nombCompar,mensajeCPU.flujoDatos+sizeof(uint16_t),k);
		memcpy(&valorCompar,mensajeCPU.flujoDatos+k+sizeof(uint16_t),sizeof(uint32_t));
		nombCompar[k]='\0';
		//printf("K_ASIGNAR_VAR_GL()==>llego el id:%i nombreVar:%s valor:%i\n",id,nombCompar,valorCompar);

		varBuscada=dictionary_get(diccio_varCompartidas,nombCompar);
		if(varBuscada==NULL){
			printf("***********ERROR:el programa referencia una variable compartida inexistente=>expulsandolo...***********\n");
			mensajeCPU.encabezado.codMsg=C_ERROR;
			mensajeCPU.encabezado.longitud=0;
			enviarMsg(p_sockCPU,mensajeCPU);
			printf("se envio mensaje con C_ERROR a cpu\n");
			liberarMsg(&mensajeCPU);
			//buscarlo en listaEjecutando
			pthread_mutex_lock(&mutex_listaEjecutando);
			procesoAux=(t_nodo_proceso*)list_remove_by_condition(listaEjecutando,(void *)buscaPorId);
			pthread_mutex_unlock(&mutex_listaEjecutando);
			//printf("se removio el proceso id:%i  seg_pila:%i de listaListos\n",procesoAux->pcb.id_proceso,procesoAux->pcb.segmento_pila);
			listarEjecutando();
			//pasar el proceso a la listaTerminados
			pasarProcesoATerminados(procesoAux);
			//liberar cpu
			//printf("Se liberara la cpu\n");
			ponerCpuDisponible(p_sockCPU);
			free(nombCompar);nombCompar=NULL;
			break;
		}
		mensajeCPU.encabezado.codMsg=VALOR_COMPARTIDA_OK;
		mensajeCPU.encabezado.longitud=0;
		enviarMsg(p_sockCPU,mensajeCPU);
		//printf("se envio el mensaje de valorcompartida ok por el socket: %i\n",p_sockCPU);
		liberarMsg(&mensajeCPU);

		varBuscada->valor=valorCompar;
		free(nombCompar);nombCompar=NULL;
	break;
	}
}
void levantarArchivoConf(char *path){
	char         *ip=NULL;
	t_config     *configKernel;

	configKernel=config_create(path);

	ip                  =config_get_string_value(configKernel,"IPUMV");
	memcpy(gc_ipUMV,ip,strlen(ip)+1);
	ip                  =config_get_string_value(configKernel,"IPKERNEL");
	memcpy(gc_ipKernel,ip,strlen(ip)+1);
	g_puertoUMV         =config_get_int_value(configKernel,"PUERTO_UMV");
	g_puertoProg        =config_get_int_value(configKernel,"PUERTO_PROG");
	g_puertoCPU         =config_get_int_value(configKernel,"PUERTO_CPU");
	g_quantum           =config_get_int_value(configKernel,"QUANTUM");
	g_multiprogramacion =config_get_int_value(configKernel,"MULTIPROGRAMACION");
	g_retardo           =config_get_int_value(configKernel,"RETARDO");
	g_tamanioPila       =config_get_int_value(configKernel,"TAMANIO_STACK");
	semaforos           =config_get_array_value(configKernel,"SEMAFOROS");
	valorSemafs         =config_get_array_value(configKernel,"VALOR_SEMAFORO");
	valorHIO            =config_get_array_value(configKernel,"HIO");
	varCompartidas      =config_get_array_value(configKernel,"VARIABLES_GLOBALES");
	idHIOs              =config_get_array_value(configKernel,"ID_HIO");
	config_destroy(configKernel);

	log_debug(g_logger,"levantarArchivoConf()==>Se levanto el archivo de configuracion...")	;
	printf("******se levanto el archivo de configuracion********\n");
}
void atenderNuevaConexion(int sockEscucha,void (*funcionQueAtiende)(int),int* mayorSock,int socketsAbiertos,int* g_sockets){
	int socketNuevo;

	socketNuevo=aceptarConexion(sockEscucha);
	//FD_SET(socketNuevo,p_fds_maestro);------>deprecado
	g_sockets[socketsAbiertos-1]=socketNuevo;
	if(socketNuevo>*mayorSock) *mayorSock=socketNuevo;
	//printf("en atenderNuevaConexion con socketEscucha:%i socketNuevo:%i socketMayor(ahora):%i\n",sockEscucha,socketNuevo,*mayorSock);
	funcionQueAtiende(socketNuevo);//derivo a 1 funcion que sigue atendiendo a la conexion -tanto para cpus como para programas- por viene como parametro
}
void atenderPrograma(int p_sockPrograma){
	char               *codigo=NULL;
	int                 longitud=0;
	t_medatada_program *metadata;
	t_pcb               pcb;
	t_msg               mensajeProg;
	mensajeProg.flujoDatos=NULL;

	recibirMsg(p_sockPrograma,&mensajeProg);
	log_debug(g_logger,"atenderPrograma()==>Se recibio un mensaje de un programa:");
	//printf("atenderPrograma()==>Se recibio un mensaje de un programa:\n");

	switch(mensajeProg.encabezado.codMsg){
	case K_HANDSHAKE:
		log_debug(g_logger,"atenderPrograma()==>mensaje de programa:K_HANDSHAKE");
		//printf("atenderPrograma()==>mensaje de programa:K_HANDSHAKE\n");
		//programa manda el handshake=>pedir el codigo del script
		liberarMsg(&mensajeProg);
		mensajeProg.encabezado.codMsg=P_ENVIAR_SCRIPT;
		mensajeProg.encabezado.longitud=0;
		enviarMsg(p_sockPrograma,mensajeProg);
		//printf("enviando P_ENVIAR_SCRIPT\n");
		liberarMsg(&mensajeProg);
		break;
	case K_ENVIO_SCRIPT:
		log_debug(g_logger,"atenderPrograma()==>mensaje de programa:K_ENVIO_SCRIPT");
		//printf("atenderPrograma()==>mensaje de programa:K_ENVIO_SCRIPT\n");
		//programa manda el codigo del script
		codigo=malloc(mensajeProg.encabezado.longitud);
		memcpy(codigo,mensajeProg.flujoDatos,mensajeProg.encabezado.longitud);
		//printf("CODIGO RECIBIDO: %s\n",codigo);
		longitud=mensajeProg.encabezado.longitud;
		codigo[longitud-1]='\0';
		liberarMsg(&mensajeProg);

		//metadata=metadata_desde_literal(mensajeProg.flujoDatos);
		metadata=metadata_desde_literal(codigo);
		if(crearSegmentosYpcb(metadata,&pcb,longitud,codigo)!=0){
			//error en la creacion de algun segmento=>denegar ejecucion a programa
			log_debug(g_logger,"atenderPrograma()==>Error al crear alguno de los segmentos, se expulsa a el proceso...");
			printf("*****ERROR: no hay suficiente espacio en UMV para todos los segmentos del programa==>expulsandolo...*****\n");
			//mandarle mensaje a programa
			mensajeProg.encabezado.codMsg=P_SERVICIO_DENEGADO;
			mensajeProg.encabezado.longitud=0;
			enviarMsg(p_sockPrograma,mensajeProg);
			liberarMsg(&mensajeProg);

			//CERRAR ESA CONEXION
			//reordenando el vector de sockets de los programas
			int i,k;
			printf("vector de sockets de progs antes de la desconexion:\n");
			for(i=1;i<g_socketsAbiertosProg;i++){
				printf("elemento:%i socket:%i\n",i,g_socketsProg[i]);
			}
			for(i=1;i<g_socketsAbiertosProg;i++){
				if(g_socketsProg[i]==p_sockPrograma)break;
			}
			for(k=0;k<g_socketsAbiertosProg-i;k++){
				g_socketsProg[i]=g_socketsProg[i+1];
			}
			g_socketsAbiertosProg--;
			/*printf("vector de sockets de cpu despues de la desconexion:\n");
				for(i=1;i<g_socketsAbiertosProg;i++){
					printf("elemento:%i socket:%i\n",i,g_socketsProg[i]);
				}*/
			//sacando el socket del conjunto de lectura
			//FD_CLR(p_sockPrograma,&g_fds_maestroProg);--------->deprecado
			//cerrando la conexion
			close(p_sockPrograma);

			//PEDIR A UMV LIBERE TODOS LOS SEGMENTOS DEL PROCESO
			mensajeProg.encabezado.codMsg=U_DESTRUIR_SEGMENTO;
			mensajeProg.encabezado.longitud=sizeof(int16_t);
			mensajeProg.flujoDatos=malloc(sizeof(int16_t));
			memcpy(mensajeProg.flujoDatos,&pcb.id_proceso,sizeof(uint16_t));
			enviarMsg(g_socketUMV,mensajeProg);
			liberarMsg(&mensajeProg);
		}else{
			//INGRESAR EL PCB A LA COLA DE NEWS
			log_debug(g_logger,"atenderCPU()==>Los segmentos y el pcb se crearon con exito...");
			//free(mensajeProg.flujoDatos);
			short int peso=calcularPeso(metadata->cantidad_de_etiquetas,metadata->cantidad_de_funciones,metadata->instrucciones_size);
			log_debug(g_logger,"atenderCPU()==>proceso id:%i peso:%i",pcb.id_proceso,peso);
			//free(metadata);
			metadata_destruir(metadata);
			encolarEnNuevos(pcb,peso,p_sockPrograma);
		}
		free(codigo);
		break;
	default:
		break;
	}
}
int crearSegmentosYpcb(t_medatada_program *p_metadata,t_pcb *p_pcb,int longitud,char*codigo){
	uint16_t      id;
	t_size        offset=0;

	id=asignarID(p_pcb);
	log_debug(g_logger,"crearSegmentosYpcb()==>Se crearan los segmentos y el pcb del programa...");
	//printf("crearSegmentosYpcb()==>Se crearan los segmentos y el pcb del programa...\n");

	//PEDIR ESPACIO PARA EL SEGMENTO CODIGO
	gs_mensajeUMV.encabezado.codMsg=U_CREAR_SEGMENTO;
	gs_mensajeUMV.encabezado.longitud=sizeof(uint16_t)+sizeof(t_size);//el serializado es: idPrograma+tamanio
	gs_mensajeUMV.flujoDatos=realloc(gs_mensajeUMV.flujoDatos,sizeof(uint16_t)+sizeof(t_size));
	memcpy(gs_mensajeUMV.flujoDatos,&id,sizeof(uint16_t));
	t_size tam=(t_size)(longitud);
	memcpy(gs_mensajeUMV.flujoDatos+sizeof(uint16_t),&tam,sizeof(t_size));
	//printf("crearSegmentosYpcb()==>TAMANIO SEGEMNTO-CODIGO:%i\n",tam);
	//ENVIANDO PEDIDO A UMV
	enviarMsg(g_socketUMV,gs_mensajeUMV);
	//RECIBIENDO CONTESTACION DE UMV
	recibirMsg(g_socketUMV,&gs_mensajeUMV);
	if(gs_mensajeUMV.encabezado.codMsg!=K_CREACION_SEGMENTO_OK) return -1;
	memcpy(&(p_pcb->segmento_codigo),gs_mensajeUMV.flujoDatos,sizeof(t_puntero));//llenando pcb.segmento_codigo
	liberarMsg(&gs_mensajeUMV);
	log_debug(g_logger,"crearSegmentosYpcb()==>Se reservo el espacio para el segmento de codigo en umv...");

	//PEDIR ESPACIO PARA EL SEGMENTO PILA
	gs_mensajeUMV.encabezado.codMsg=U_CREAR_SEGMENTO;
	gs_mensajeUMV.encabezado.longitud=sizeof(uint16_t)+sizeof(t_size);
	gs_mensajeUMV.flujoDatos=realloc(gs_mensajeUMV.flujoDatos,sizeof(uint16_t)+sizeof(t_size));
	memcpy(gs_mensajeUMV.flujoDatos,&id,sizeof(uint16_t));
	memcpy(gs_mensajeUMV.flujoDatos+sizeof(uint16_t),&g_tamanioPila,sizeof(t_size));
	//printf("crearSegmentosYpcb()==>TAMANIO SEGMENTO-PILA:%i\n",g_tamanioPila);
	//ENVIANDO PEDIDO A UMV
	enviarMsg(g_socketUMV,gs_mensajeUMV);
	//RECIBIENDO CONTESTACION DE UMV
	recibirMsg(g_socketUMV,&gs_mensajeUMV);
	if(gs_mensajeUMV.encabezado.codMsg!=K_CREACION_SEGMENTO_OK) return -1;
	memcpy(&(p_pcb->segmento_pila),gs_mensajeUMV.flujoDatos,sizeof(t_puntero));
	liberarMsg(&gs_mensajeUMV);
	log_debug(g_logger,"crearSegmentosYpcb()==>Se reservo el espacio para el segmento para la pila en umv...");

	//printf("crearSegmentosYpcb()==>TAMANIO SEGMENTO-ETIQUETAS:%i\n",p_metadata->instrucciones_size);
	//PEDIR ESPACIO PARA EL SEGMENTO ETIQUETAS
	if(p_metadata->etiquetas_size!=0){
		gs_mensajeUMV.encabezado.codMsg=U_CREAR_SEGMENTO;
		gs_mensajeUMV.encabezado.longitud=sizeof(uint16_t)+sizeof(t_size);
		gs_mensajeUMV.flujoDatos=realloc(gs_mensajeUMV.flujoDatos,sizeof(uint16_t)+sizeof(t_size));
		memcpy(gs_mensajeUMV.flujoDatos,&id,sizeof(uint16_t));
		memcpy(gs_mensajeUMV.flujoDatos+sizeof(uint16_t),&(p_metadata->etiquetas_size),sizeof(t_size));
		//ENVIANDO PEDIDO A UMV
		enviarMsg(g_socketUMV,gs_mensajeUMV);
		//RECIBIENDO CONTESTACION DE UMV
		recibirMsg(g_socketUMV,&gs_mensajeUMV);
		if(gs_mensajeUMV.encabezado.codMsg!=K_CREACION_SEGMENTO_OK) return -1;
		memcpy(&(p_pcb->indice_etiquetas),gs_mensajeUMV.flujoDatos,sizeof(t_puntero));
		liberarMsg(&gs_mensajeUMV);
	}else{p_pcb->indice_etiquetas=0;}
	log_debug(g_logger,"crearSegmentosYpcb()==>Se reservo el espacio para el segmento para etiquetas en umv...");

	//PEDIR ESPACIO PARA EL SEGMENTO INDICE_CODIGO
	gs_mensajeUMV.encabezado.codMsg=U_CREAR_SEGMENTO;
	gs_mensajeUMV.encabezado.longitud=sizeof(uint16_t)+sizeof(t_size);
	gs_mensajeUMV.flujoDatos=realloc(gs_mensajeUMV.flujoDatos,sizeof(uint16_t)+sizeof(t_size));
	memcpy(gs_mensajeUMV.flujoDatos,&id,sizeof(int));
	t_size tamanioIndice=p_metadata->instrucciones_size*sizeof(t_intructions);
	memcpy(gs_mensajeUMV.flujoDatos+sizeof(uint16_t),&tamanioIndice,sizeof(t_size));
	//printf("crearSegmetnosYpcb()==>TAMANIO SEGMENTO-INDICE-CODIGO:%i\n",tamanioIndice);
	//ENVIANDO PEDIDO A UMV
	enviarMsg(g_socketUMV,gs_mensajeUMV);
	//RECIBIENDO CONTESTACION DE UMV
	recibirMsg(g_socketUMV,&gs_mensajeUMV);
	if(gs_mensajeUMV.encabezado.codMsg!=K_CREACION_SEGMENTO_OK) return -1;
	memcpy(&(p_pcb->indice_codigo),gs_mensajeUMV.flujoDatos,sizeof(t_puntero));
	liberarMsg(&gs_mensajeUMV);
	log_debug(g_logger,"crearSegmentosYpcb()==>Se reservo el espacio para el segmento de indice codigo en umv...");

	//LE MANDO A QUE PROCESO CORRESPONDERAN LOS SUBSIGUIENTES PEDIDOS DE ESCRITURA
	gs_mensajeUMV.encabezado.codMsg=U_PROCESO_ACTIVO;
	gs_mensajeUMV.encabezado.longitud=sizeof(uint16_t);
	gs_mensajeUMV.flujoDatos=realloc(gs_mensajeUMV.flujoDatos,sizeof(uint16_t));
	memcpy(gs_mensajeUMV.flujoDatos,&p_pcb->id_proceso,sizeof(uint16_t));
	//EVIANDO ID A UMV
	enviarMsg(g_socketUMV,gs_mensajeUMV);
	liberarMsg(&gs_mensajeUMV);
	log_debug(g_logger,"crearSegmentosYpcb()==>Se envio a umv el id del proceso al que se le crearon los segmentos...");

	//GRABAR EN SEGMENTO_CODIGO EL CODIGO
	gs_mensajeUMV.encabezado.codMsg=U_ALMACENAR_BYTES;
	gs_mensajeUMV.encabezado.longitud=(t_size)(longitud)+sizeof(u_int32_t)*3;//base+offset+tamanio+data
	gs_mensajeUMV.flujoDatos=realloc(gs_mensajeUMV.flujoDatos,gs_mensajeUMV.encabezado.longitud);
	memcpy(gs_mensajeUMV.flujoDatos,&(p_pcb->segmento_codigo),sizeof(t_puntero));
	memcpy(gs_mensajeUMV.flujoDatos+sizeof(t_puntero),&offset,sizeof(t_size));
	memcpy(gs_mensajeUMV.flujoDatos+2*sizeof(t_size),&tam,sizeof(t_size));
	memcpy(gs_mensajeUMV.flujoDatos+3*sizeof(t_size),codigo,tam);
	//ENVIANDO A UMV EL CODIGO A GRABAR
	enviarMsg(g_socketUMV,gs_mensajeUMV);
	liberarMsg(&gs_mensajeUMV);
	//RECIBIENDO CONTESTACION DE UMV
	recibirMsg(g_socketUMV,&gs_mensajeUMV);
	if(gs_mensajeUMV.encabezado.codMsg!=DATOS_ALMACENADOS_OK) return -1;
	liberarMsg(&gs_mensajeUMV);
	log_debug(g_logger,"crearSegmentosYpcb()==>Se grabo en umv el segmento de codigo...");

	//GRABANDO EN EL SEGMENTO INDICE_CODIGO
	gs_mensajeUMV.encabezado.codMsg=U_ALMACENAR_BYTES;
	gs_mensajeUMV.encabezado.longitud= tamanioIndice+sizeof(u_int32_t)*3;//base+offset+tamanio+data
	gs_mensajeUMV.flujoDatos=realloc(gs_mensajeUMV.flujoDatos,gs_mensajeUMV.encabezado.longitud);
	memcpy(gs_mensajeUMV.flujoDatos,&(p_pcb->indice_codigo),sizeof(t_puntero));
	memcpy(gs_mensajeUMV.flujoDatos+sizeof(t_puntero),&offset,sizeof(t_size));
	memcpy(gs_mensajeUMV.flujoDatos+2*sizeof(t_size),&tamanioIndice,sizeof(t_size));
	memcpy(gs_mensajeUMV.flujoDatos+3*sizeof(t_size),p_metadata->instrucciones_serializado,tamanioIndice);
	//ENVIANDO A UMV EL INDICE DE CODIGO
	enviarMsg(g_socketUMV,gs_mensajeUMV);
	liberarMsg(&gs_mensajeUMV);
	//RECIBIENDO CONTESTACION DE UMV
	recibirMsg(g_socketUMV,&gs_mensajeUMV);
	if(gs_mensajeUMV.encabezado.codMsg!=DATOS_ALMACENADOS_OK) return -1;
	liberarMsg(&gs_mensajeUMV);
	log_debug(g_logger,"crearSegmentosYpcb()==>Se grabo en el segmento de indice de codigo en umv el indice de codigo...");

	//GRABANDO EN EL SEGMENTO INDICE_ETIQUETAS
	if(p_metadata->etiquetas_size!=0){
		//printf("hay etiquetas, entonces semandaran a grabar en umv\n");
		gs_mensajeUMV.encabezado.codMsg=U_ALMACENAR_BYTES;
		gs_mensajeUMV.encabezado.longitud= p_metadata->etiquetas_size+sizeof(t_size)*3;//base+offset+tamanio+data
		gs_mensajeUMV.flujoDatos=realloc(gs_mensajeUMV.flujoDatos,gs_mensajeUMV.encabezado.longitud);
		memcpy(gs_mensajeUMV.flujoDatos,                       &(p_pcb->indice_etiquetas),         sizeof(t_puntero));
		memcpy(gs_mensajeUMV.flujoDatos+sizeof(t_puntero),     &offset,                            sizeof(t_size));
		memcpy(gs_mensajeUMV.flujoDatos+2*sizeof(t_size),      &p_metadata->etiquetas_size,        sizeof(t_size));
		memcpy(gs_mensajeUMV.flujoDatos+3*sizeof(t_size),      p_metadata->etiquetas,              p_metadata->etiquetas_size);
		enviarMsg(g_socketUMV,gs_mensajeUMV);
		liberarMsg(&gs_mensajeUMV);
		recibirMsg(g_socketUMV,&gs_mensajeUMV);

		if(gs_mensajeUMV.encabezado.codMsg!=DATOS_ALMACENADOS_OK) return -1;
		liberarMsg(&gs_mensajeUMV);
	}
	log_debug(g_logger,"crearSegmentosYpcb()==>Se grabo en el segmento de indice de etiquetas en umv las etiquetas...");
	//printf("LISTO SE TERMINO EL INTERCAMBIO CON UMV SE CREARON Y LLENARON TODOS LOS SEGMENTOS");

	//TERMINANDO DE COMPLETAR EL PCB
	p_pcb->cursor_de_pila=p_pcb->segmento_pila;
	p_pcb->program_counter=p_metadata->instruccion_inicio;//0;
	p_pcb->tamanio_contexto_actual=0;
	p_pcb->tamanio_indice_etiquetas=p_metadata->etiquetas_size;

	log_debug(g_logger,"crearSegmentosYpcb()==>Se crea el pcb con:");
	log_debug(g_logger,"crearSegmentosYpcb()==>pcb.id_proceso:                                   %i",p_pcb->id_proceso);
	log_debug(g_logger,"crearSegmentosYpcb()==>pcb.segemnto_codigo:                              %i",p_pcb->segmento_codigo);
	log_debug(g_logger,"crearSegmentosYpcb()==>pcb.segmento_pila:                                %i",p_pcb->segmento_pila);
	log_debug(g_logger,"crearSegmentosYpcb()==>pcb.indice_etiquetas(segmento de etiquetas):      %i",p_pcb->indice_etiquetas);
	log_debug(g_logger,"crearSegmentosYpcb()==>pcb.indice_codigo (segmento de indice de codigo): %i",p_pcb->indice_codigo);
	log_debug(g_logger,"crearSegmentosYpcb()==>pcb.cursor_de_pila:                               %i",p_pcb->cursor_de_pila);
	log_debug(g_logger,"crearSegmentosYpcb()==>pcb.program_counter:                              %i",p_pcb->program_counter);
	log_debug(g_logger,"crearSegmentosYpcb()==>pcb.tamanio_contexto_actual:                      %i",p_pcb->tamanio_contexto_actual);
	log_debug(g_logger,"crearSegmentosYpcb()==>pcb.tamanio_indice_etiquetas:                     %i",p_pcb->tamanio_indice_etiquetas);
	return 0;
}
uint16_t asignarID(t_pcb* p_pcb){
	g_ids++;
	p_pcb->id_proceso=g_ids;
	return g_ids;
}
void encolarEnNuevos(t_pcb p_pcb, short int peso,short int soquet){
	t_nodo_proceso         *nuevoProceso;
	int                     i;

	//printf("***********FUNCION: encolarNuevos*********Peso: %i\n",peso);
	nuevoProceso=crearNodoProceso(p_pcb,peso);
	nuevoProceso->soquet_prog=soquet;
	//recorro la lista y agrego por peso
	for(i=0;i<list_size(listaNuevos);i++){
		t_nodo_proceso *programa=list_get(listaNuevos,i);
		if(programa->peso>peso) break;
	}
	pthread_mutex_lock(&mutex_listaNuevos);
	list_add_in_index(listaNuevos,i,nuevoProceso);
	pthread_mutex_unlock(&mutex_listaNuevos);
	sem_post(&sem_listaNuevos);//avisamos al hiloManejoListas
	listarNuevos();
}
short int calcularPeso(t_size cantEtiquetas,t_size cantFunciones,t_size cantLineas){
	return 5*cantEtiquetas+3*cantFunciones+cantLineas;
}
t_nodo_proceso *crearNodoProceso(t_pcb p_pcb, short int peso){
	//printf("***********FUNCION: crearNodoProceso*********\n");
	//printf("sizeof(t_nodoProceso):%i\n",sizeof(t_nodo_proceso));
	t_nodo_proceso *nuevoProceso=malloc(sizeof(t_nodo_proceso));

	nuevoProceso->pcb.cursor_de_pila=           p_pcb.cursor_de_pila;
	nuevoProceso->pcb.id_proceso=               p_pcb.id_proceso;
	nuevoProceso->pcb.indice_codigo=            p_pcb.indice_codigo;
	nuevoProceso->pcb.indice_etiquetas=         p_pcb.indice_etiquetas;
	nuevoProceso->pcb.program_counter=          p_pcb.program_counter;
	nuevoProceso->pcb.segmento_codigo=          p_pcb.segmento_codigo;
	nuevoProceso->pcb.segmento_pila=            p_pcb.segmento_pila;
	nuevoProceso->pcb.tamanio_contexto_actual=  p_pcb.tamanio_contexto_actual;
	nuevoProceso->pcb.tamanio_indice_etiquetas= p_pcb.tamanio_indice_etiquetas;
	nuevoProceso->peso=                         peso;

/*	printf("listando el proceso llenado:\n");
	printf("nuevoProceso->pcb.cursor_de_pila:%i\n",nuevoProceso->pcb.cursor_de_pila);
	printf("nuevoProceso->pcb.id_proceso:%i\n",nuevoProceso->pcb.id_proceso);
	printf("nuevoProceso->pcb.indice_codigo:%i\n",nuevoProceso->pcb.indice_codigo);
	printf("nuevoProceso->pcb.indice_etiquetas:%i\n",nuevoProceso->pcb.program_counter);
	printf("nuevoProceso->pcb.segmento_codigo:%i\n",nuevoProceso->pcb.segmento_codigo);
	printf("nuevoProceso->pcb.segmento_pila:%i\n",nuevoProceso->pcb.segmento_pila);
	printf("nuevoProceso->pcb.tamanio_contexto_actual:%i\n",nuevoProceso->pcb.tamanio_contexto_actual);
	printf("nuevoProceso->pcb.tamanio_indice_etiquetas:%i\n",nuevoProceso->pcb.tamanio_indice_etiquetas);
	printf("peso del proceso:%i\n",nuevoProceso->peso);*/
	return nuevoProceso;
}
void crearEstructuras(){
	colaListos              =queue_create();
	listaNuevos             =list_create();
	listaSemaforos          =list_create();
	listaBloqueados         =list_create();
	listaTerminados         =list_create();
	listaEjecutando         =list_create();
	listaCpuLibres          =list_create();
	diccio_hilos            =dictionary_create();
	diccio_varCompartidas   =dictionary_create();

	sem_init(&sem_listaTerminados,0,0);
	sem_init(&sem_listaCpu,0,0);
	sem_init(&sem_listaNuevos,0,0);
	sem_init(&sem_listaListos,0,0);
	sem_init(&sem_multiprog,0,g_multiprogramacion);//------>grado multiprogramacion
	sem_init(&sem_semaforos,0,0);

	pthread_mutex_init(&mutex_listaTerminados,NULL);
	pthread_mutex_init(&mutex_listaCpu,NULL);
	pthread_mutex_init(&mutex_listaEjecutando,NULL);
	pthread_mutex_init(&mutex_semaforos,NULL);
	pthread_mutex_init(&mutex_listaListos,NULL);
	pthread_mutex_init(&mutex_listaNuevos,NULL);
}
void liberarRecursos(){
	log_debug(g_logger,"liberarRecursos()==>************se libera y limpian todas las estructuras usadas*************");
	//ELIMINANDO LAS LISTAS
	printf("destruyendo listaS\n");
	queue_destroy_and_destroy_elements(colaListos,     (void*)destruirNodoProceso);
	list_destroy_and_destroy_elements(listaNuevos,     (void*)destruirNodoProceso);
	list_destroy_and_destroy_elements(listaBloqueados, (void*)destruirNodoProceso);
	list_destroy_and_destroy_elements(listaTerminados, (void*)destruirNodoProceso);
	list_destroy_and_destroy_elements(listaEjecutando, (void*)destruirNodoProceso);
	list_destroy_and_destroy_elements(listaSemaforos,  (void*)destruirNodoSemaforo);
	list_destroy_and_destroy_elements(listaCpuLibres,  (void*)free);
	printf("listas eliminadas\n");

	//ELIMINANDO LOS DICCIONARIOS
	void destruirElementoColaBloq(t_bloqueadoIO *procesoBloq){
		free(procesoBloq->proceso);
		free(procesoBloq);
	}
	void eliminarColaBloq(char *clave,t_hiloIO *hilo){
		//queue_clean_and_destroy_elements(hilo->dataHilo.bloqueados, (void*)destruirElementoColaBloq);
		//queue_destroy(hilo->dataHilo.bloqueados);
		queue_destroy_and_destroy_elements(hilo->dataHilo.bloqueados,(void*)destruirElementoColaBloq);
	}
	void cancelarHilos(char *clave,t_hiloIO *hilo){
		pthread_detach(hilo->hiloID);
		pthread_cancel(hilo->hiloID);
	}
	dictionary_iterator(diccio_hilos,(void*)eliminarColaBloq);
	dictionary_iterator(diccio_hilos,(void*)cancelarHilos);
	//dictionary_clean_and_destroy_elements(diccio_hilos,(void*)liberarElementoDiccioIO);
	//dictionary_destroy(diccio_hilos);
	dictionary_destroy_and_destroy_elements(diccio_hilos,free);
	//dictionary_clean_and_destroy_elements(diccio_varCompartidas,(void*)destruirVarCompartida);
	//dictionary_destroy(diccio_varCompartidas);
	dictionary_destroy_and_destroy_elements(diccio_varCompartidas,(void*)destruirVarCompartida);
	printf("diccionarios hilos, semaforos y varCompartidas eliminados\n");

	//ELIMINANDO LOS HILOS
	pthread_detach(idHiloPCP);
	pthread_detach(idHiloManejoListas);
	pthread_detach(idHiloDespachador);
	pthread_detach(idHiloSemaforo);
	pthread_detach(idHiloTerminarProcesos);

	pthread_cancel(idHiloPCP);
	pthread_cancel(idHiloManejoListas);
	pthread_cancel(idHiloDespachador);
	pthread_cancel(idHiloSemaforo);
	pthread_cancel(idHiloTerminarProcesos);
	printf("hilos eliminados...\n");
	//LIMPIANDO VARIABLES GLOBALES
	liberarVarGlob(idHIOs);
	liberarVarGlob(valorHIO);
	liberarVarGlob(semaforos);
	liberarVarGlob(valorSemafs);
	liberarVarGlob(varCompartidas);

	log_destroy(g_logger);
	exit(EXIT_SUCCESS);
}
static void destruirVarCompartida(t_varCompartida *varComp){
	free(varComp->nombre);
	free(varComp);
}
static void destruirNodoProceso(t_nodo_proceso *proceso){
	free(proceso);
}
static void destruirNodoSemaforo(t_semaforo *semaforo){
	free(semaforo->nombre);
	queue_clean_and_destroy_elements(semaforo->bloqueados,(void*)destruirNodoProceso);
	queue_destroy(semaforo->bloqueados);
	free(semaforo);
}

void *hiloPCP(void *sinUso){
	int i,socketEscuchaPCP,socketMayor;

	printf("******************hiloPCP lanzado*******************\n");
	log_debug(g_logger,"hiloPCP()==>Hilo hiloPCP lanzado...");

	//MULTIPLEXANDO CONEXIONES ENTRANTES DE CPU
	socketEscuchaPCP=crearSocket();
	bindearSocket(socketEscuchaPCP,gc_ipKernel,g_puertoCPU);//es identica ip(tanto para plp como para pcp) pero distinto puerto???
	escucharSocket(socketEscuchaPCP);

	//SELECT
	FD_ZERO(&g_fds_lecturaCpu);
	FD_SET(socketEscuchaPCP,&g_fds_lecturaCpu);
	socketMayor=socketEscuchaPCP;
	g_socketsCpu[0]=socketEscuchaPCP;
	g_socketsCpuAbiertos=1;

	//MAIN HILO PCP
	while(1){
		//printf("en while pcp con socketMayor: %i...\n",socketMayor);
		for(i=1;i<g_socketsCpuAbiertos;i++){
			FD_SET(g_socketsCpu[i],&g_fds_lecturaCpu);
			//printf("puso en el conjunto g_fds_lecturaCpu al socket %i\n",g_socketsCpu[i]);
		}
		if(select(socketMayor+1,&g_fds_lecturaCpu,NULL,NULL,NULL)==-1){
			printf("ERROR:error en select()\n");
		}
		if(FD_ISSET(g_socketsCpu[0],&g_fds_lecturaCpu)){
			//un nuevo programa quiere conexion
			//printf("actividad en el g_socketsCpu[0]\n");
			g_socketsCpuAbiertos++;
			atenderNuevaConexion(socketEscuchaPCP,(void*)atenderCPU,&socketMayor,g_socketsCpuAbiertos,g_socketsCpu);
		}
		//chequando pedidos de programas ya conectados
		for(i=1;i<g_socketsCpuAbiertos;i++){
			if(FD_ISSET(g_socketsCpu[i],&g_fds_lecturaCpu)){
				atenderCPU(g_socketsCpu[i]);
			}
		}
		FD_ZERO(&g_fds_lecturaCpu);
		FD_SET(socketEscuchaPCP,&g_fds_lecturaCpu);
	}
return NULL;
}
void *hiloManejoListas(void* sinUso){
	printf("**************hiloManejaListas lanzado**************\n");
	log_debug(g_logger,"hiloManejoListas()==>Se lanzo el hilo hiloManejoListas...");

	while(1){
		//espera a que haya nuevos ----fijarse se puede usar una funcion para que no se bloquee
		sem_wait(&sem_listaNuevos);//listo-no se usa mas el semaforo-
		pthread_mutex_lock(&mutex_listaNuevos);
		t_nodo_proceso *proceso=list_remove(listaNuevos,0);
		pthread_mutex_unlock(&mutex_listaNuevos);
		log_debug(g_logger,"hiloManejoListas()==>Se saco un proceso de la listaNuevos...");
		listarNuevos();

		//esperar a que el grado de multiprogramacion lo permita
		sem_wait(&sem_multiprog);
		pthread_mutex_lock(&mutex_listaListos);
		//se saca de la listauevos y se agrega el nodo a la cola de listos
		queue_push(colaListos,proceso);
		pthread_mutex_unlock(&mutex_listaListos);
		sem_post(&sem_listaListos);
		listarListos();
		log_debug(g_logger,"hiloManejoListas()==>Se grego un proceso a la listaListos...");
	}
	return NULL;
}
void *hiloDespachador(void *sinUso){
	int            *soquet;
	int             soquet2;
	t_nodo_proceso *procesoAux;
	t_msg           mensajeCPU;
	mensajeCPU.flujoDatos=NULL;

	printf("**************hiloDespachador lanzado***************\n");
	log_debug(g_logger,"hiloDespachador()==>Se lanzo el hilo hiloDespachador...");

	while(1){
		//printf("esperando elemento que llegue a (cola)listalistos...\n");
		sem_wait(&sem_listaListos);
		//printf("llego a (cola)listalistos\n");
		pthread_mutex_lock(&mutex_listaListos);
		procesoAux=queue_pop(colaListos);
		pthread_mutex_unlock(&mutex_listaListos);

		log_debug(g_logger,"hiloDespachador()==>Esperando una cpu libre...");
		//printf("esperando por una cpu libre...\n");
		listarListos();

		sem_wait(&sem_listaCpu);
		//printf("llego una cpu libre a listaCpu\n");
		pthread_mutex_lock(&mutex_listaCpu);
		soquet=(int*)list_remove(listaCpuLibres,0);
		pthread_mutex_unlock(&mutex_listaCpu);

		memcpy(&soquet2,soquet,sizeof(int));
		free(soquet);
		//printf("se enviara el mensaje con pcb+quantum+retardo a cpu\n");

		mensajeCPU.encabezado.codMsg=C_ENVIO_PCB;
		mensajeCPU.encabezado.longitud=sizeof(t_pcb)+sizeof(uint32_t)*2;
		mensajeCPU.flujoDatos=realloc(mensajeCPU.flujoDatos,mensajeCPU.encabezado.longitud);
		memcpy(mensajeCPU.flujoDatos,                                                                     &(procesoAux->pcb.cursor_de_pila),            sizeof(t_puntero));
		memcpy(mensajeCPU.flujoDatos+sizeof(t_puntero),                                                   &(procesoAux->pcb.id_proceso),                sizeof(uint16_t));
		memcpy(mensajeCPU.flujoDatos+sizeof(t_puntero)+sizeof(uint16_t),                                  &(procesoAux->pcb.indice_codigo),             sizeof(t_puntero));
		memcpy(mensajeCPU.flujoDatos+sizeof(t_puntero)*2+sizeof(uint16_t),                                &(procesoAux->pcb.indice_etiquetas),          sizeof(t_puntero));
		memcpy(mensajeCPU.flujoDatos+sizeof(t_puntero)*3+sizeof(uint16_t),                                &(procesoAux->pcb.program_counter),           sizeof(uint16_t));
		memcpy(mensajeCPU.flujoDatos+sizeof(t_puntero)*3+sizeof(uint16_t)*2,                              &(procesoAux->pcb.segmento_codigo),           sizeof(t_puntero));
		memcpy(mensajeCPU.flujoDatos+sizeof(t_puntero)*4+sizeof(uint16_t)*2,                              &(procesoAux->pcb.segmento_pila),             sizeof(t_puntero));
		memcpy(mensajeCPU.flujoDatos+sizeof(t_puntero)*5+sizeof(uint16_t)*2,                              &(procesoAux->pcb.tamanio_contexto_actual),   sizeof(t_size));
		memcpy(mensajeCPU.flujoDatos+sizeof(t_puntero)*5+sizeof(uint16_t)*2+sizeof(t_size),               &(procesoAux->pcb.tamanio_indice_etiquetas),  sizeof(t_size));
		memcpy(mensajeCPU.flujoDatos+sizeof(t_puntero)*5+sizeof(uint16_t)*2+sizeof(t_size)*2,             &g_quantum,                             sizeof(int));
		memcpy(mensajeCPU.flujoDatos+sizeof(t_puntero)*5+sizeof(uint16_t)*2+sizeof(t_size)*2+sizeof(int), &g_retardo,                             sizeof(int));
		/*
		printf("se envia a CPU el programa:\n");
		printf("cursor_de_pila: %i\n",procesoAux->pcb.cursor_de_pila);
		printf("id_proceso:     %i\n",procesoAux->pcb.id_proceso);
		printf("segmento_codigo:%i\n",procesoAux->pcb.segmento_codigo);
		printf("segmento_pila:  %i\n",procesoAux->pcb.segmento_pila);
		printf("indice_codigo:  %i\n",procesoAux->pcb.indice_codigo);
		printf("program_counter:%i\n",procesoAux->pcb.program_counter);*/

		enviarMsg(soquet2,mensajeCPU);
		liberarMsg(&mensajeCPU);
		log_debug(g_logger,"hiloDespachador()==>Se envio el pcb+quantum+retardo al proceso cpu...");

		//se mueve el proceso a listaEjecutando---->por ahora sin semaforos...(??)
		pthread_mutex_lock(&mutex_listaEjecutando);
		list_add(listaEjecutando,procesoAux);
		pthread_mutex_unlock(&mutex_listaEjecutando);
		listarEjecutando();
	}
	return NULL;
}
void *hiloTerminarProcesos(void *sinUso){
	t_nodo_proceso *        proceso;
	t_msg                   mensaje;

	printf("************hiloTerminarProcesos lanzado************\n");
	log_debug(g_logger,"hiloTerminarProcesos()==>Se lanzo el hilo hiloTerminarPorcesos...");

	while(1){
		sem_wait(&sem_listaTerminados);
		//REMOVER EL PROCESO DE LA LISTA LISTATERMINADOS
		pthread_mutex_lock(&mutex_listaTerminados);
		proceso=list_remove(listaTerminados,0);
		pthread_mutex_unlock(&mutex_listaTerminados);
		//se saco al proceos de listaTerminados
		log_debug(g_logger,"hiloTerminarProcesos()==>Termino de ejecutarse y se expulsa el proceso id:%i",proceso->pcb.id_proceso);
		//printf("hiloTerminarProcesos()==>Termino de ejecutarse y se expulsa el proceso id:%i  soquet:%i\n",proceso->pcb.id_proceso,proceso->soquet_prog);
		//PEDIR A UMV LIBERE TODOS LOS SEGMENTOS DEL PROCESO
		mensaje.encabezado.codMsg=U_DESTRUIR_SEGMENTO;
		mensaje.encabezado.longitud=sizeof(int16_t);
		mensaje.flujoDatos=malloc(sizeof(int16_t));
		memcpy(mensaje.flujoDatos,&proceso->pcb.id_proceso,sizeof(uint16_t));
		enviarMsg(g_socketUMV,mensaje);
		liberarMsg(&mensaje);

		shutdown(proceso->soquet_prog,0);

		mensaje.encabezado.codMsg=P_DESCONEXION;
		mensaje.encabezado.longitud=0;
		enviarMsg(proceso->soquet_prog,mensaje);

		//CERRAR ESA CONEXION
		//reordenando el vector de sockets de los programas
		int i,k;
		/*printf("vector de sockets de progs antes de la desconexion:\n");
		for(i=1;i<g_socketsAbiertosProg;i++){
			printf("elemento:%i socket:%i\n",i,g_socketsProg[i]);
		}*/
		for(i=1;i<g_socketsAbiertosProg;i++){
			if(g_socketsProg[i]==proceso->soquet_prog)break;
		}
		for(k=0;k<g_socketsAbiertosProg-i;k++){
			g_socketsProg[i]=g_socketsProg[i+1];
		}
		g_socketsAbiertosProg--;
		/*printf("vector de sockets de cpu despues de la desconexion:\n");
			for(i=1;i<g_socketsAbiertosProg;i++){
				printf("elemento:%i socket:%i\n",i,g_socketsProg[i]);
			}*/
		//sacando el socket del conjunto de lectura
		//FD_CLR(proceso->soquet_prog,&g_fds_maestroProg);------>deprecado
		//cerrando la conexion
		close(proceso->soquet_prog);
	}
	return NULL;
}
void *hiloIO(void *parametros){
	t_bloqueadoIO *proceso;
	int            suenio;
	t_dataHilo    *parametro=(t_dataHilo*)parametros;

	printf("*******************hiloIO lanzado*******************\n");
	log_debug(g_logger,"hiloIO()==> hiloIO lanzado...");

	while(1){
		sem_wait(&parametro->sem_io);
		pthread_mutex_lock(&parametro->mutex_io);
		proceso=queue_pop(parametro->bloqueados);
		pthread_mutex_unlock(&parametro->mutex_io);
		listarBloqueados();
		log_debug(g_logger,"hiloIO()==>Se saca un proceso de la cola de bloqueados para que haga su E/S...");
		suenio=parametro->retardo*proceso->espera*1000;
		usleep(suenio);

		pthread_mutex_lock(&mutex_listaListos);
		queue_push(colaListos,proceso->proceso);
		pthread_mutex_unlock(&mutex_listaListos);
		sem_post(&sem_listaListos);
		listarListos();
	}
	return NULL;
}
void *hiloSemaforos(void *sinUso){
	printf("***********hiloSemaforos lanzado*********** \n");
	int i;

	log_debug(g_logger,"hiloSemaforos()==>hilo hiloSemaforos lanzado...");

	while(1){
		//printf(".......en while de hiloSemaforos esperando en el semaforo......\n");
		sem_wait(&sem_semaforos);
		//printf("......se desbloqueo el semaforo y lo vamos a buscar al diccionario..................\n");

		for(i=0;i<list_size(listaSemaforos);i++){
			t_semaforo *semaforo=list_get(listaSemaforos,i);
			if(semaforo->desbloquear&&!(queue_is_empty(semaforo->bloqueados))){

				log_debug(g_logger,"hiloSemaforos()==>Se desbloqueara un proceso bloqueado en el semaforo %s",semaforo->nombre);
				//printf("se encontro el desbloquear true y cola de bloqueados no vacia\n");

				pthread_mutex_lock(&mutex_semaforos);
				t_nodo_proceso *proceso=queue_pop(semaforo->bloqueados);
				semaforo->desbloquear=false;
				pthread_mutex_unlock(&mutex_semaforos);

				pthread_mutex_lock(&mutex_listaListos);
				queue_push(colaListos,proceso);
				pthread_mutex_unlock(&mutex_listaListos);
				sem_post(&sem_listaListos);
				listarListos();
			}
		}
	}
	return NULL;
}

void listarNuevos(){
	int     i;
	printf("\nESTADO-->NUEVOS(%i):",list_size(listaNuevos));
	for(i=0;i<list_size(listaNuevos);i++){
		t_nodo_proceso *prog= list_get(listaNuevos,i);
		printf(" -->idProceso:%i (program counter:%i)",prog->pcb.id_proceso,prog->pcb.program_counter);
	}
	//if(list_size(listaNuevos)!=0)printf("\n");
}
void listarEjecutando(){
	int i;
	printf("\nESTADO-->EJECUTANDO(%i):",list_size(listaEjecutando));
	for(i=0;i<list_size(listaEjecutando);i++){
		t_nodo_proceso *prog= list_get(listaEjecutando,i);
		printf(" -->idProceso:%i (program counter:%i)",prog->pcb.id_proceso,prog->pcb.program_counter);
	}
	//if(list_size(listaEjecutando)!=0)printf("\n");
}
void listarListos(){
	int i;
	printf("\nESTADO-->LISTOS(%i):",queue_size(colaListos));
	for(i=0;i<queue_size(colaListos);i++){
		t_nodo_proceso *prog= list_get(colaListos->elements,i);//queue_peek(colaListos);
		printf(" -->idProceso:%i (program counter:%i)",prog->pcb.id_proceso,prog->pcb.program_counter);
	}
	//if(queue_size(colaListos)!=0)printf("\n");
}
void listarTerminados(){
	int i;
	printf("\nESTADO-->TERMINADOS(%i):",list_size(listaTerminados));
	for(i=0;i<list_size(listaTerminados);i++){
		t_nodo_proceso *prog=(t_nodo_proceso*) list_get(listaTerminados,i);
		printf(" -->idProceso:%i (program counter:%i)",prog->pcb.id_proceso,prog->pcb.program_counter);
	}
	//if(list_size(listaTerminados)!=0)printf("\n");
}
void listarBloqueados(){
	int i,j;

	printf("\nESTADO-->BLOQUEADOS:");
	void iterarSemafs2(char *clave,t_hiloIO *hilo){
		int i;
		printf("\n                   Dispositivo %s (%i):",clave,queue_size(hilo->dataHilo.bloqueados));
		for(i=0;i<queue_size(hilo->dataHilo.bloqueados);i++){
			t_bloqueadoIO *bloqueado=list_get(hilo->dataHilo.bloqueados->elements,i);//queue_peek(hilo->dataHilo.bloqueados);
			printf(" -->idProceso:%i (program counter:%i)",bloqueado->proceso->pcb.id_proceso,bloqueado->proceso->pcb.program_counter);
		}
		//if(queue_size(hilo->dataHilo.bloqueados)!=0)printf("\n");
	}
	dictionary_iterator(diccio_hilos,(void*)iterarSemafs2);

	//LISTANDO LOS BLOQUEADOS EN LOS SEMAFOROS
	for(i=0;i<list_size(listaSemaforos);i++){
		t_semaforo *semaforo=list_get(listaSemaforos,i);
		printf("\n                   Semaforo %s(%i):",semaforo->nombre,queue_size(semaforo->bloqueados));
		for(j=0;j<queue_size(semaforo->bloqueados);j++){
			t_nodo_proceso *prog=list_get(semaforo->bloqueados->elements,j);
			printf(" -->idProceso:%i (program counter:%i)",prog->pcb.id_proceso,prog->pcb.program_counter);
		}
		//if(queue_size(semaforo->bloqueados)!=0)printf("\n");
	}
}
void listarCpu(){
	int    i,*soquet;
	printf("LISTACPULIBRES (tamanio%i):\n",list_size(listaCpuLibres));
	for(i=0;i<list_size(listaCpuLibres);i++){
		soquet=list_get(listaCpuLibres,i);
		printf("socket:%i\n",*soquet);
	}
}

void actualizarPcb(t_pcb* pcb,t_msg mensaje){
	memcpy(&(pcb->cursor_de_pila),            mensaje.flujoDatos,                                                       sizeof(u_int32_t));
	memcpy(&(pcb->id_proceso),                mensaje.flujoDatos+sizeof(u_int32_t),                                     sizeof(uint16_t));
	memcpy(&(pcb->indice_codigo),             mensaje.flujoDatos+sizeof(u_int32_t)+sizeof(uint16_t),                    sizeof(u_int32_t));
	memcpy(&(pcb->indice_etiquetas),          mensaje.flujoDatos+sizeof(u_int32_t)*2+sizeof(uint16_t),                  sizeof(u_int32_t));
	memcpy(&(pcb->program_counter),           mensaje.flujoDatos+sizeof(u_int32_t)*3+sizeof(uint16_t),                  sizeof(uint16_t));
	memcpy(&(pcb->segmento_codigo),           mensaje.flujoDatos+sizeof(u_int32_t)*3+sizeof(uint16_t)*2,                sizeof(u_int32_t));
	memcpy(&(pcb->segmento_pila),             mensaje.flujoDatos+sizeof(u_int32_t)*4+sizeof(uint16_t)*2,                sizeof(u_int32_t));
	memcpy(&(pcb->tamanio_contexto_actual),   mensaje.flujoDatos+sizeof(u_int32_t)*5+sizeof(uint16_t)*2,                sizeof(u_int32_t));
	memcpy(&(pcb->tamanio_indice_etiquetas),  mensaje.flujoDatos+sizeof(u_int32_t)*6+sizeof(uint16_t)*2,                sizeof(u_int32_t));

	log_debug(g_logger,"Se actualiza el pcb con:");
	log_debug(g_logger,"pcb.id_proceso:%i",pcb->id_proceso);
	log_debug(g_logger,"pcb.segemnto_codigo:%i",pcb->segmento_codigo);
	log_debug(g_logger,"pcb.segmento_pila:%i",pcb->segmento_pila);
	log_debug(g_logger,"pcb.indice_etiquetas(segmento de etiquetas):%i",pcb->indice_etiquetas);
	log_debug(g_logger,"pcb.indice_codigo (segmento de indice de codigo):%i",pcb->indice_codigo);
	log_debug(g_logger,"pcb.cursor_de_pila:%i",pcb->cursor_de_pila);
	log_debug(g_logger,"pcb.program_counter:%i",pcb->program_counter);
	log_debug(g_logger,"pcb.tamanio_contexto_actual:%i",pcb->tamanio_contexto_actual);
	log_debug(g_logger,"pcb.tamanio_indice_etiquetas:%i",pcb->tamanio_indice_etiquetas);
}
void crearHilosIO(){
	int cantIO=0,i=0;

	while(1){//contando cuantos dispositivos io hay levantados
		if(idHIOs[i]=='\0')break;
		cantIO++;
		i++;
	}
	for(i=0;i<cantIO;i++){//inicializando y lanzando cada uno de los hilos que administra cada dispositivo

		t_hiloIO *hilo=malloc(sizeof(t_hiloIO));
		hilo->dataHilo.retardo=atoi(valorHIO[i]);
		hilo->dataHilo.bloqueados=queue_create();
		sem_init(&hilo->dataHilo.sem_io,0,0);
		pthread_mutex_init(&hilo->dataHilo.mutex_io,NULL);
		//pthread_create(&hilo->hiloID,NULL,&hiloIO,(void*)&(hilo->dataHilo));
		pthread_create(&hilo->hiloID,NULL,(void *)&hiloIO,(void *)&(hilo->dataHilo));
		dictionary_put(diccio_hilos,idHIOs[i],hilo);
	}
	log_debug(g_logger,"crearHilosIO()==>Se crearon y lanzaron cada uno de los hilo que administran a cada uno de los dispositivos");
}
void crearListaSemaforos(){
	int i=0,cantSemaforos=0;

	printf("************FUNCION: crearListaSemaforos************\n");
	while(1){//calculando cuantos semaforos hay
		if(semaforos[i]=='\0')break;
		cantSemaforos++;
		i++;
	}
	log_debug(g_logger,"crearDiccioSemaforos()==>Se creara el diccionario de semaforos...");
	for(i=0;i<cantSemaforos;i++){
		t_semaforo *semaforo=crearSemaforo(semaforos[i],atoi(valorSemafs[i]));
		list_add(listaSemaforos,semaforo);
	}
}
t_semaforo *crearSemaforo(char*nombre,uint32_t valor){
	t_queue    *cola=queue_create();
	t_semaforo *semaforo=malloc(sizeof(t_semaforo));

	semaforo->nombre=strdup(nombre);
	semaforo->valor=valor;
	semaforo->bloqueados=cola;
	semaforo->desbloquear=false;
	printf("se creo el elemento semaforo con nombre:%s y valor:%i\n",semaforo->nombre,semaforo->valor);
	return semaforo;
}
void crearDiccioCompartidas(){
	int i=0;
	printf("***********FUNCION CREARdICCIOcOMPARTIDAS***********\n");
	while(varCompartidas[i]!='\0'){
		//printf("tomando la var compartida:%s\n",varCompartidas[i]);
		t_varCompartida * nuevaVar=crearVarCompartida((char*)varCompartidas[i],0);
		dictionary_put(diccio_varCompartidas,nuevaVar->nombre,nuevaVar);
		i++;
	}
}
t_varCompartida *crearVarCompartida(char*nombre,uint32_t valor){
	t_varCompartida *nuevaVar=malloc(sizeof(t_varCompartida));
	nuevaVar->nombre=strdup(nombre);
	nuevaVar->valor=valor;
	printf("se creo la var compartida:%s valor:%i\n",nuevaVar->nombre,nuevaVar->valor);
	return nuevaVar;
}

t_log *crearLog(char *archivo){
	char path[11]={0};
	char aux[17]={0};

	strcpy(path,"logueo.log");
	strcat(aux,"touch ");
	strcat(aux,path);
	system(aux);
	t_log *logAux=log_create(path,archivo,false,LOG_LEVEL_DEBUG);
	log_info(logAux,"ARCHIVO DE LOG CREADO");
	return logAux;
}
void liberarMsg(t_msg *mensaje){
	free(mensaje->flujoDatos);
	mensaje->flujoDatos=NULL;
}
void liberarVarGlob(char **varGlob){
	string_iterate_lines(varGlob,(void*)free);
	free(varGlob);
}
void ponerCpuDisponible(int p_soquet){
	int *soque=malloc(sizeof(int));//NO HACER FREE() LO CARGO EN LA LISTACPULIBRES LUEGO EN EL DESPACHADOR LO LIBERO
	//printf("poniendo cpu a libre\n");
	*soque=p_soquet;
	pthread_mutex_lock(&mutex_listaCpu);
	list_add(listaCpuLibres,soque);
	pthread_mutex_unlock(&mutex_listaCpu);
	sem_post(&sem_listaCpu);
}
void pasarProcesoATerminados(t_nodo_proceso *proceso){
	//printf("pasarProcesoATerminados()==>llegando con el proceso id:%i seg_pila:%i\n",proceso->pcb.id_proceso,proceso->pcb.segmento_pila);
	pthread_mutex_lock(&mutex_listaTerminados);
	list_add(listaTerminados,proceso);
	pthread_mutex_unlock(&mutex_listaTerminados);
	sem_post(&sem_listaTerminados);
	sem_post(&sem_multiprog);
	listarTerminados();
}
