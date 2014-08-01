/* umv.c  Created on: 21/05/2014    Author: utnso SIMULANDO LA UMV PARA INTERACTUAR CON MI CPU*/
#include "umv.h"

#define STDIN            0
#define TAM_BUFF_COMANDO 128
#define NOM_ARCH_DUMP    "reporteUMV.log"
#define WORST_FIT         0
#define FIRST_FIT         1

//VARIABLES GLOBALES
int     socketHilo,socketKernel;
int     puertoUMV;
int     tamanioBloque;
int     retardo;
int     algoritmo=FIRST_FIT;// 1=first-fit  0=worst-fit
int     ultimaDirLogica;
char    ipUMV[16];
t_byte *mp;
t_log  *loguer;
t_list *listaSegmentos;
fd_set  descriptoresLectura;
pthread_mutex_t mutex_listaSeg;
pthread_mutex_t mutex_mp;

void levantarArchivoConf(char*);
void *hiloAtencionKernel(void *sinUso);
void *hiloAtencionCPU(void *socketParametro);
void atenderConsola();
void atenderConexion(int);
t_puntero crearSegmento(uint16_t,t_size);
void destruirSegmentos(uint16_t);
int evaluarCompactacion(t_size);
void compactar();
void llenarSegmento(t_segmento*,u_int32_t,u_int32_t,u_int32_t,u_int32_t);
void listar(FILE*);
void mapearMemoria(FILE*);
void mapearArgu(t_byte * cadena);
void grabarMP(t_byte* data,int direccion,int tamanio);
t_log *crearLog(char *archivo);
void pantallaComandos();
void listarScreen();
void limpiarMsg(t_msg *mensaje);
void mapearMemoria2();
void crearEstructuras();
void leerMP(t_byte* data,t_byte* direccion,int tamanio);

int main(int argc, char** argv){
	int socketActivo,socketEscucha;

	//INICIALIZANDO EL LOG
	loguer=crearLog(argv[0]);

	//LEVANTANDO INFO DEL ARCHIVO DE CONFIGURACION
	levantarArchivoConf(argv[1]);

	//CREANDO ESTRUCTURAS
	crearEstructuras();

	//CREANDO SOCKETS Y ESPERANDO LA CONEXION DEL KERNEL
	socketEscucha=crearSocket();
	bindearSocket(socketEscucha,ipUMV,puertoUMV);
	escucharSocket(socketEscucha);

	//PANTALLA DE COMANDOS
	pantallaComandos();

	while(1){
		//ESTRUCTURAS PARA SELECT()
		FD_ZERO(&descriptoresLectura);
		FD_SET(socketEscucha,&descriptoresLectura);
		FD_SET(STDIN,&descriptoresLectura);//----->consola

		socketActivo=select(socketEscucha+1,&descriptoresLectura,NULL,NULL,NULL);

		if(socketActivo<0){
			//error
			printf("error en la funcion select **");
			return -1;
		}
		if(FD_ISSET(0,&descriptoresLectura)){
			//SE INGRESO UN COMANDO EN LA CONSOLA
			atenderConsola();
		}
		if(FD_ISSET(socketEscucha,&descriptoresLectura)){
			//UNA NUEVA CPU INTENTA CONECTARSE
			atenderConexion(socketEscucha);//=>aceptar la conexion
		}
	}
	return 0;
}
void levantarArchivoConf(char* path){
	/* el archivo configuracion sera del tipo:
	 * TAMANIO-BLOQUE=1000;
	 * IP=127.0.0.1
	 * PUERTO=5000
	 * RETARDO=2000
	 */
	char          *ip;
	t_config      *configUMV;
	extern t_log  *loguer;

	configUMV=config_create(path);
	ip            =config_get_string_value(configUMV,"IP");
	memcpy(ipUMV,ip,strlen(ip)+1);
	puertoUMV     =config_get_int_value(configUMV,"PUERTO");
	tamanioBloque =config_get_int_value(configUMV,"TAMANIO-BLOQUE");
	retardo       =config_get_int_value(configUMV,"RETARDO");
	config_destroy(configUMV);
	//printf("archivo de configuracion levantado: ip:%s puerto:%i tamanio de bloque:%i\n",ipUMV,puertoUMV,tamanioBloque);
	log_debug(loguer,"levantarArchivoConf()==>Se levanto el archivo con ip:%s puerto:%i tamanio de bloque:%i retardo:%i...",ipUMV,puertoUMV,tamanioBloque,retardo);
}
void *hiloAtencionKernel(void *sinUso){
	uint16_t       idProceso;
	t_byte        *data=NULL;
	t_size         tamanio,offset;
	t_puntero      dirLogica,base;
	t_msg          mensajeKernel;
	int            i;
	t_segmento    *segmento=NULL;

	mensajeKernel.flujoDatos=NULL;
	mensajeKernel.encabezado.codMsg=CONEXION_OK;
	mensajeKernel.encabezado.longitud=0; //no pongo nada en mensajeKernel.flujoDatos
	enviarMsg(socketKernel,mensajeKernel);
	limpiarMsg(&mensajeKernel);
	log_debug(loguer,"hiloAtencionKernel()==>Se lanzo un hiloAtencionKernel, se respondio con CONEXION_OK a kernel...");

	while(1){
		recibirMsg(socketKernel,&mensajeKernel);
		switch(mensajeKernel.encabezado.codMsg){
		case U_CREAR_SEGMENTO:
			//CREAR EL SEGMENTO PARA UN PROGRAMA--mensaje serializado: idPrograma+tamanio
			memcpy(&idProceso,mensajeKernel.flujoDatos,sizeof(uint16_t));
			memcpy(&tamanio,mensajeKernel.flujoDatos+sizeof(uint16_t),sizeof(t_size));
			limpiarMsg(&mensajeKernel);

			dirLogica=crearSegmento(idProceso,tamanio);
			if(dirLogica==-1){
				mensajeKernel.encabezado.codMsg=DESBORDE_MEMORIA;
				mensajeKernel.encabezado.longitud=0;
				enviarMsg(socketKernel,mensajeKernel);
				break;
			}
			//contestar si se pudo crear el segmento y devolver dirLogica del segmento
			mensajeKernel.encabezado.codMsg=K_CREACION_SEGMENTO_OK;
			mensajeKernel.encabezado.longitud=sizeof(t_puntero);
			mensajeKernel.flujoDatos=realloc(mensajeKernel.flujoDatos,sizeof(t_puntero));
			memcpy(mensajeKernel.flujoDatos,&dirLogica,sizeof(t_puntero));
			log_debug(loguer,"hiloAtencionKernel()==>Llego un mensaje con U_CREAR_SEGMENTO de %i bytes, se responde con la dir-logica:%i...",tamanio,dirLogica);
			enviarMsg(socketKernel,mensajeKernel);
			break;
		case U_DESTRUIR_SEGMENTO:
			//DESTRUIR EL SEGMENTO DE UN PROGRAMA
			log_debug(loguer,"hiloAtencionKernel()==>Llego un mensaje con U_DESTRUIR_SEGMENTO del proceso id:%i se destruiran todos sus segmentos...",*((int*)mensajeKernel.flujoDatos));
			memcpy(&idProceso,mensajeKernel.flujoDatos,sizeof(int));
			limpiarMsg(&mensajeKernel);
			destruirSegmentos(idProceso);
			break;
		case U_PROCESO_ACTIVO:
			//EL CPU INFORMA EL INGRESO DE UN PROCESO
			memcpy(&idProceso,mensajeKernel.flujoDatos,sizeof(uint16_t));
			log_debug(loguer,"hiloAtencionKernel()==>Llego un mensaje con U_PROCESO_ACTIVO id del proceso:%i...",idProceso);
			limpiarMsg(&mensajeKernel);
			break;
		case U_ALMACENAR_BYTES:
			//EL KERNEL PIDE QUE ALMACENEMOS DATOS EN MP
			memcpy(&base,mensajeKernel.flujoDatos,sizeof(t_puntero));
			memcpy(&offset,mensajeKernel.flujoDatos+sizeof(t_puntero),sizeof(t_size));
			memcpy(&tamanio,mensajeKernel.flujoDatos+2*sizeof(u_int32_t),sizeof(t_size));
			data=realloc(data,tamanio);
			memcpy(data,mensajeKernel.flujoDatos+3*sizeof(u_int32_t),tamanio);
			limpiarMsg(&mensajeKernel);

			for(i=0;i<list_size(listaSegmentos);i++){
				segmento=list_get(listaSegmentos,i);
				if(segmento->idProceso==idProceso && segmento->dirLogica==base) break;
			}
			if(segmento==NULL||segmento->tamanio<tamanio){
				log_debug(loguer,"hiloAtencionKernel==>pedido de grabado de bytes rechazado por desborde de segmento...");
				mensajeKernel.encabezado.codMsg=DESBORDE_MEMORIA;
				mensajeKernel.encabezado.longitud=0;
			}else{
				mensajeKernel.encabezado.codMsg=DATOS_ALMACENADOS_OK;
				mensajeKernel.encabezado.longitud=0;
				grabarMP(data,segmento->inicio, tamanio);
				log_debug(loguer,"hiloAtencionKernel()==>pedido de grabado de bytes ok...");
			}
			segmento=NULL;
			enviarMsg(socketKernel,mensajeKernel);
			limpiarMsg(&mensajeKernel);
			break;
		default:
			break;
		}
	}
	return NULL;
}
void *hiloAtencionCPU(void *socketParametro){
	int            procesoActivo,base,offset,socketCPU,tamanio;
	t_byte        *dirAbsoluta;
	t_byte        *data=NULL;
	t_segmento    *segmento=NULL;
	t_msg          mensajeCPU;
	mensajeCPU.flujoDatos=NULL;

	memcpy(&socketCPU,socketParametro,sizeof(int));
	mensajeCPU.encabezado.codMsg=CONEXION_OK;
	mensajeCPU.encabezado.longitud=0;
	enviarMsg(socketCPU,mensajeCPU);
	limpiarMsg(&mensajeCPU);
	log_debug(loguer,"hiloAtencionCPU()==>hilo lanzado...");

	while(1){
		recibirMsg(socketCPU,&mensajeCPU);

		switch(mensajeCPU.encabezado.codMsg){
		case U_PEDIDO_BYTES:
			//EL CPU PIDE QUE LE ENTREGUE EL CONTENIDO DE UNA REGION DE MP
			memcpy(&base,mensajeCPU.flujoDatos,sizeof(u_int32_t));
			memcpy(&offset,mensajeCPU.flujoDatos+sizeof(u_int32_t),sizeof(u_int32_t));
			memcpy(&tamanio,mensajeCPU.flujoDatos+2*sizeof(u_int32_t),sizeof(u_int32_t));
			limpiarMsg(&mensajeCPU);

			log_debug(loguer,"hiloAtencionCPU()==>mensaje U_PEDIDO_BYTES base:%i offset:%i tamanio:%i",base,offset,tamanio);
			//buscar el segmento del proceso activo al cual pertenece la base
			int mismoIDyBase1(t_segmento *segmento){return ((segmento->idProceso==procesoActivo)&&(segmento->dirLogica==base));	}
			segmento=list_find(listaSegmentos,(void*)mismoIDyBase1);
			//printf("hiloAtencionCpu()==>case U_PEDIDO_BYTES: segmento.tamanio:%i tamanio:%i\n",segmento->tamanio,tamanio);

			if(segmento==NULL||segmento->tamanio<offset){	//error no hay ningun segemnto del proceso activo que le pertenezca
				log_debug(loguer,"hiloAtencionCPU()==>Error, no hay segmentos que pertenezcan e el proceso o desborde de segmento...");
				mensajeCPU.encabezado.codMsg=DESBORDE_MEMORIA;
				mensajeCPU.encabezado.longitud=0;
			}else{
				dirAbsoluta=(t_byte*)(segmento->inicio+offset);
				data=realloc(data,tamanio);
				leerMP(data,dirAbsoluta,tamanio);
				log_debug(loguer,"hiloAtencionCPU()==>se enviara el pedido de lectura a cpu...");
				mensajeCPU.encabezado.codMsg=C_PEDIDO_OK;
				mensajeCPU.encabezado.longitud=tamanio;
				mensajeCPU.flujoDatos=realloc(mensajeCPU.flujoDatos,mensajeCPU.encabezado.longitud);
				memcpy(mensajeCPU.flujoDatos,data,mensajeCPU.encabezado.longitud);
				free(data);
				data=NULL;
			}
			enviarMsg(socketCPU,mensajeCPU);
			limpiarMsg(&mensajeCPU);
			break;
		case U_ALMACENAR_BYTES:
			//EL CPU PIDE QUE ALMACENEMOS DATOS EN MP
			memcpy(&base,mensajeCPU.flujoDatos,sizeof(u_int32_t));
			memcpy(&offset,mensajeCPU.flujoDatos+sizeof(u_int32_t),sizeof(u_int32_t));
			memcpy(&tamanio,mensajeCPU.flujoDatos+2*sizeof(u_int32_t),sizeof(u_int32_t));
			data=realloc(data,tamanio);
			memcpy(data,mensajeCPU.flujoDatos+3*sizeof(u_int32_t),tamanio);

			limpiarMsg(&mensajeCPU);
			log_debug(loguer,"hiloAtencionCPU()==>mensaje U_ALMACENAR_BYTES en base:%i offset:%i tamanio:%i...",base,offset,tamanio);
			//buscar los segmentos del proceso activo y chequear a cual pertenece la base
			int mismoIDyBase2(t_segmento *segmento){return ((segmento->idProceso==procesoActivo)&&(segmento->dirLogica==base));}
			segmento=list_find(listaSegmentos,(void*)mismoIDyBase2);
			//printf("hiloAtencionCpu()==>case U_ALMACENAR_BYTES: segmento.tamanio:%i tamanio:%i\n",segmento->tamanio,tamanio);
			if(segmento==NULL||segmento->tamanio<offset){
				log_debug(loguer,"hiloAtencionCPU()==>el pedido de escritura genera error...");
				mensajeCPU.encabezado.codMsg=PEDIDO_NOK;
			}else{
				grabarMP(data,segmento->inicio+offset,tamanio);
				mensajeCPU.encabezado.codMsg=C_PEDIDO_OK;
				log_debug(loguer,"hiloAtencionCPU()==>el pedido de escritura fue exitoso...");
			}
			mensajeCPU.encabezado.longitud=0;
			enviarMsg(socketCPU,mensajeCPU);
			limpiarMsg(&mensajeCPU);
			free(data);
			data=NULL;
			break;
		case U_PROCESO_ACTIVO:
			//EL CPU INFORMA EL INGRESO DE UN PROCESO
			procesoActivo=(int)*(mensajeCPU.flujoDatos);
			limpiarMsg(&mensajeCPU);
			log_debug(loguer,"hiloAtencionCPU()==>mensaje U_PROCESO_ACTIVO informa el proceso activo id:%i...",procesoActivo);
			break;
		case U_EXPULSADO_DESCONEXION:
			log_debug(loguer,"hiloAtencionCPU()==>mensaje U_EXPULSADO_DESCONEXION se bajo el cpu con socket:%i el hilo finaliza...",socketCPU);
			close(socketCPU);
			FD_CLR(socketCPU,&descriptoresLectura);
			pthread_exit(NULL);
		break;
		default:
			break;
		}
	}
	return NULL;
}
void atenderConsola(){
	t_segmento    *segmento;
    char           comando[TAM_BUFF_COMANDO];
	char          *primera;
	FILE          *archDump;
	char          *pid;
	char          *base;
	char          *offset;
	char          *tamanio;
	t_byte        *data=NULL;
	int            i;

	gets(comando);
	primera=strtok(comando," ");

	int mismoIDyBase1(t_segmento *segmento){return ((segmento->idProceso==atoi(pid))&&(segmento->dirLogica==atoi(base)));	}

	if(string_equals_ignore_case(primera,"algoritmo")){
		primera=strtok(NULL," ");
		if(string_equals_ignore_case(primera,"FIRST-FIT")){
			algoritmo=FIRST_FIT;
		}else if(string_equals_ignore_case(primera,"WORST-FIT")){
			algoritmo=WORST_FIT;
		}else{
			printf("nombre del algoritmo incorrecto\n");
		}
	}else if(string_equals_ignore_case(primera,"compactacion")){
		compactar();
		listarScreen();
	}else if(string_equals_ignore_case(primera,"retardo")){
		retardo=atoi(strtok(NULL," "));
		printf("nuevo retardo:%i\n",retardo);
	}else if(string_equals_ignore_case(primera,"dump")){
		archDump=fopen(NOM_ARCH_DUMP,"a");
		fprintf(archDump,"=====>SEGMENTOS POR PROCESOS:\n");
		listar(archDump);
		fprintf(archDump,"=====>MAPA DE MEMORIA:\n");
		mapearMemoria(archDump);
		fclose(archDump);
		mapearMemoria2();
		listarScreen();
	}else if(string_equals_ignore_case(primera,"escribir")){
		pid=strtok(NULL," ");
		base=strtok(NULL," ");
		offset=strtok(NULL," ");
		tamanio=strtok(NULL," ");
		segmento=list_find(listaSegmentos,(void*)mismoIDyBase1);
		if(segmento==NULL){
			printf("no existe un proceso con ese id o no hay un segmento del proceso que comience en esa base\n");
		}else{
			if(segmento->tamanio<atoi(tamanio)){
				printf("el tamanio de la data es mayor al tamnio del segmento\n");
			}else{
				grabarMP((t_byte*)comando+strlen(primera)+strlen(pid)+strlen(base)+strlen(offset)+strlen(tamanio)+5,segmento->inicio+atoi(offset),atoi(tamanio));//ver bien
			}
		}
	}else if(string_equals_ignore_case(primera,"leer")){
		pid=strtok(NULL," ");
		base=strtok(NULL," ");
		offset=strtok(NULL," ");
		tamanio=strtok(NULL," ");
		segmento=list_find(listaSegmentos,(void*)mismoIDyBase1);
		if(segmento==NULL){
			printf("no existe un proceso con ese id o no hay proceso que comience con esa base\n");
		}else{
			data=realloc(data,atoi(tamanio));
			leerMP(data,(t_byte*)(segmento->inicio+atoi(offset)),atoi(tamanio));
			for(i=0;i<atoi(tamanio);i++){
				if(isalpha(data[i]))printf(" %c",data[i]);
				else printf(" %x",data[i]);
			}
			printf("\n");
			free(data);
			data=NULL;
		}
	}else if(string_equals_ignore_case(primera,"crear-Segmento")){
		pid=strtok(NULL," ");
		tamanio=strtok(NULL," ");
		crearSegmento((uint16_t)atoi(pid),atoi(tamanio));
		listarScreen();
	}else if(string_equals_ignore_case(primera,"destruir-Segmento")){
		pid=strtok(NULL," ");
		destruirSegmentos(atoi(pid));
		listarScreen();
	}else{
		printf("no se ingreso un comando correctamente\n");
	}
	pantallaComandos();
}
t_puntero crearSegmento(uint16_t p_idPrograma,t_size p_tamanio){
	int                     adelante=-1,posDeSegVecino=0;
	u_int32_t               dirFisicaInicio,dirFisicaFin,i;
	t_segmento             *segmentoNuevo;
	t_segmento             *segmentoAux=NULL;

	dirFisicaInicio=(int)(mp);
	dirFisicaFin=(int)(mp)+tamanioBloque;
	log_debug(loguer,"crearSegmento()==>se creara segmento para el proceso-id:%i...",p_idPrograma);
	bool porDirFisica(t_segmento *seg1,t_segmento *seg2){return seg1->inicio<seg2->inicio;}
//********ALGORITMO SETADO=FIRST FIT*********
	if(algoritmo==FIRST_FIT){
		log_debug(loguer,"crearSegmento()==>algoritmo activo: FIRST-FIT...");
		for(i=0;i<list_size(listaSegmentos);i++){
			segmentoAux=list_get(listaSegmentos,i);
			dirFisicaFin=segmentoAux->inicio;
			if(dirFisicaFin-dirFisicaInicio>p_tamanio){
				break;//ya encontre el espacio donde entra el segmento a reservar
			}else{
				dirFisicaInicio=segmentoAux->tamanio+segmentoAux->inicio;
				dirFisicaFin=(int)(mp)+tamanioBloque;
				//avanzo al siguiente espacio entre segmentos
			}
		}
		if(dirFisicaFin-dirFisicaInicio>=p_tamanio){
			//ASIGNAR
			segmentoNuevo=malloc(sizeof(t_segmento));
			llenarSegmento(segmentoNuevo,p_tamanio,p_idPrograma,i,dirFisicaInicio);
			pthread_mutex_lock(&mutex_listaSeg);
			list_add_in_index(listaSegmentos,i,segmentoNuevo);
			pthread_mutex_unlock(&mutex_listaSeg);
			log_debug(loguer,"crearSegmento()==>se creo el segmento usando FIRST-FIT.");
		}else{
			//evaluar si hay que hacer compactacion
			log_debug(loguer,"crearSegmento()==>no alcanza el espacio de ningun segmento entoces se evaluara la compactacion...");
			if(evaluarCompactacion(p_tamanio)){
				log_debug(loguer,"crearSegmento()==>alcanza el espacio se comenzara a compactar...");
				//alcanza el espacio si es compactada la memoria
				compactar();
				return crearSegmento(p_idPrograma,p_tamanio);//llamo recursivamente, ahora si va a poder asignar el segmento
			}else{
				//no alcanza el espacio aunque se compacte la memoria=>denegar
				log_debug(loguer,"crearSegmento()==>no alcanza el espacio a pesar de hacer una compactacion...");
				return -1;
			}
		}
	}else{
//*******ALGORITMO SETEADO=WORST FIT*******
		int tamMaximo=0;
		for(i=0;i<list_size(listaSegmentos);i++){
			segmentoAux=list_get(listaSegmentos,i);
			dirFisicaFin=segmentoAux->inicio;
			if(tamMaximo<=dirFisicaFin-dirFisicaInicio){
				tamMaximo=dirFisicaFin-dirFisicaInicio;
				posDeSegVecino=i;//indice en la lista del segmento vecino al espacio donde se alojara el segmento
			}
			dirFisicaInicio=segmentoAux->inicio+segmentoAux->tamanio;
			dirFisicaFin=(int)(mp)+tamanioBloque;
		}

		if(dirFisicaFin-dirFisicaInicio>tamMaximo){
			tamMaximo=dirFisicaFin-dirFisicaInicio;
			posDeSegVecino=list_size(listaSegmentos)-1;
			adelante=1;//el espacio que contendra al segmento esta por delante del segmento vecino
		}else{
			adelante=0;//el espacio que contendra al segmento esta por detras del segmento vecino
		}

		if(tamMaximo<p_tamanio){
			if(evaluarCompactacion(p_tamanio)){
				//el espacio fragmentado alcanza para almacenar al segmento
				log_debug(loguer,"crearSegmento()==>alcanza el espacio se comenzara a compactar...");
				compactar();
				return crearSegmento(p_idPrograma,p_tamanio);//llamada recursiva luego de la compactacion
			}else{
				//el espacio fragmentado no alcanza para almacenar al segmento=>se rechaza
				log_debug(loguer,"crearSegmento()==>no alcanza el espacio a pesar de hacer una compactacion...");
				return -1;
			}
		}else{//Se encontro un espacio para almacenar al bloque
			segmentoNuevo=malloc(sizeof(t_segmento));
			if(list_size(listaSegmentos)!=0){
				if(adelante==1){
					segmentoAux=list_get(listaSegmentos,posDeSegVecino);
					dirFisicaInicio=segmentoAux->inicio+segmentoAux->tamanio;
				}
				if(adelante==0){
					if(posDeSegVecino!=0){
						segmentoAux=list_get(listaSegmentos,posDeSegVecino-1);
						dirFisicaInicio=segmentoAux->inicio+segmentoAux->tamanio;
					}else{
						dirFisicaInicio=(int)mp;
					}
				}
			}

			llenarSegmento(segmentoNuevo,p_tamanio,p_idPrograma,i,dirFisicaInicio);//hago lo de dirFisicaInicio parael casodonde la listaSegmentos este vacia
			pthread_mutex_lock(&mutex_listaSeg);
			list_add_in_index(listaSegmentos,i,segmentoNuevo);
			pthread_mutex_unlock(&mutex_listaSeg);
			log_debug(loguer,"crearSegmento()==>se creo el segmento usando WORST-FIT");
		}
	}
	pthread_mutex_lock(&mutex_listaSeg);
	list_sort(listaSegmentos,(void*)porDirFisica);
	pthread_mutex_unlock(&mutex_listaSeg);
	return (t_puntero)(segmentoNuevo->dirLogica);
}
void destruirSegmentos(uint16_t p_idPrograma){
	short int               i,z;

	//solo sacar ese nodo de la lista de segmentos
	bool mismoID(t_segmento *segmento){	return (segmento->idProceso==p_idPrograma);}
	i=list_count_satisfying(listaSegmentos,(void*)mismoID);
	pthread_mutex_lock(&mutex_listaSeg);
	for(z=0;z<i;z++){
	list_remove_and_destroy_by_condition(listaSegmentos, (void*)mismoID, (void*)free) ;
	}
	pthread_mutex_unlock(&mutex_listaSeg);
}
void compactar(){
	int                     dirInicio=(int)(mp);
	int                     i,j,dirAux;
	t_byte                 *valor1,*valor2;
	t_segmento             *segmento;//=malloc(sizeof(t_segmento));

	log_debug(loguer,"compactar()==>se correran todos los segmentos al inicio de la mp...");
	pthread_mutex_lock(&mutex_listaSeg);
	pthread_mutex_lock(&mutex_mp);
	for(i=0;i<list_size(listaSegmentos);i++){
		segmento=list_get(listaSegmentos,i);
		if(segmento->inicio==dirInicio){
			//printf("====>se evaluo que el segmento empieza donde corresponde si estuviera compactado\n");
			//no hace nada ya que el segmento comienza donde corresponde compactadao
		}else{
			//printf("====>se evaluo que e l segmento no empieza donde corresponde, entoces lo correremos\n");
			dirAux=dirInicio;
			for(j=0;j<segmento->tamanio;j++){
				valor1=(t_byte*)dirAux;
				valor2=(t_byte*)(segmento->inicio+j);
				*valor1=*valor2;
				dirAux++;
			}
			segmento->inicio=dirInicio;
		}
		dirInicio=dirInicio+segmento->tamanio;
	}
	pthread_mutex_unlock(&mutex_listaSeg);
	pthread_mutex_unlock(&mutex_mp);
}
int evaluarCompactacion(t_size tamanioSeg){
	int            acumulado=0,i;
	t_segmento    *segmento=malloc(sizeof(t_segmento));

	for(i=0;i<list_size(listaSegmentos);i++){
		segmento=list_get(listaSegmentos,i);
		acumulado=acumulado+segmento->tamanio;
	}
	log_debug(loguer,"evaluarCompactacion()==>chequeando si el espacio fragmentado es suficiente...");

	if(tamanioSeg<tamanioBloque-acumulado){
		return 1;
	}else{
		return 0;
	}
}
void llenarSegmento(t_segmento* segmento,u_int32_t tamanio,u_int32_t idProg,u_int32_t idSegmento,u_int32_t dirInicio){
	segmento->dirLogica=ultimaDirLogica;
	segmento->idProceso=idProg;
	segmento->idSegmento=idSegmento;
	segmento->inicio=dirInicio;
	segmento->tamanio=tamanio;
	ultimaDirLogica=ultimaDirLogica+tamanio;
	log_debug(loguer,"llenarSegmento()==>se llena un nodoSegmento con dirLogica:%i idProceso:%i idSegmento:%i dirInicio:%i tamanio:%i",ultimaDirLogica,idProg,idSegmento,dirInicio,tamanio);
}
void listar(FILE *archDump){
	int i;
	t_segmento *segmento=malloc(sizeof(t_segmento));

	for(i=0;i<list_size(listaSegmentos);i++){
		segmento=list_get(listaSegmentos,i);
		//printf("idProceso:%i idSegmento:%i tamanio:%i dirLogica:%i dirFisicaInicio:%i\n",segmento->idProceso,segmento->idSegmento,segmento->tamanio,segmento->dirLogica,segmento->inicio);
		fprintf(archDump,"idProceso:%i idSegmento:%i tamanio:%i dirLogica:%i dirFisicaInicio:%i\n",segmento->idProceso,segmento->idSegmento,segmento->tamanio,segmento->dirLogica,segmento->inicio);
	}
	printf("\n");
}
void atenderConexion(int listener){
	int           soquet;
	pthread_t     idHiloKernel;
	pthread_t     threadHilo;
	t_msg         mensaje;
	mensaje.flujoDatos=NULL;

	soquet=aceptarConexion(listener);
	recibirMsg(soquet,&mensaje);

	if(mensaje.encabezado.codMsg==KERNEL_HANDSHAKE){
		socketKernel=soquet;
		pthread_create(&idHiloKernel,NULL,&hiloAtencionKernel,NULL);
	}
	if(mensaje.encabezado.codMsg==CPU_HANDSHAKE){
		socketHilo=soquet;
		log_debug(loguer,"atenderConexion()==>Se detecto una nueva conexion, se lanzara un hilo para que la atienda...");
		pthread_create(&threadHilo,NULL,&hiloAtencionCPU,&socketHilo);
	}
}
void mapearMemoria(FILE *archDump){
	int                    w=0;

	pthread_mutex_lock(&mutex_mp);
	for(w=0;w<tamanioBloque;w++){
		if(isalpha(mp[w])){
			//printf("%c",mp[w]);
			fprintf(archDump,"%c",mp[w]);
		}else{
			//printf("%x",mp[w]);
			fprintf(archDump,"%x",mp[w]);
		}
	}
	pthread_mutex_unlock(&mutex_mp);
	//printf("\n");
	fprintf(archDump,"\n");

}
void mapearArgu(t_byte * cadena){
	int            w;

	for(w=0;w<strlen((char*)cadena)+1;w++){
		if(isalpha(cadena[w])){
			printf("%c",cadena[w]);
		}else{
			printf("%x",cadena[w]);
		}
	}
	printf("\n");

}
void grabarMP(t_byte* data,int direccion,int tamanio){
	int i;

	log_debug(loguer,"grabarMP()==>Se grabara en la direccion:%i el dato:%s..",direccion,data);

	pthread_mutex_lock(&mutex_mp);
	for(i=0;i<tamanio;i++){
		memcpy((char*)(direccion+i),data+i,sizeof(t_byte));
	}
	pthread_mutex_unlock(&mutex_mp);
}
void leerMP(t_byte* data,t_byte* direccion,int tamanio){
	//extern pthread_mutex_t mutex_mp;

	pthread_mutex_lock(&mutex_mp);
	memcpy(data,direccion,tamanio);
	pthread_mutex_unlock(&mutex_mp);
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
void pantallaComandos(){
	printf("COMANDOS PARA INGRESAR:\n"
			"1-COMPACTAR LA MEMORIA: compactacion\n"
			"2-CAMBIAR EL ALGORITMO: algoritmo {WORST-FIT/FIRST-FIT}\n"
			"3-CAMBIAR EL RETARDO:   retardo {cantidad}\n"
			"4-CREAR SEGMENTO:       crear-segmento {id-Proceso} {tamanio}\n"
			"5-DESTRUIR SEGMENTOS:   destruir-segmento {id-Proceso}\n"
			"6-LEER-BYTES:           leer {id-Proceso} {base} {offset} {tamanio}\n"
			"7-ESCRIBIR-BYTES:       escribir {id-Proceso} {base} {offset} {tamanio} {data}\n"
			"8-REPORTE DE ESTADO:    dump\n");
}
void listarScreen(){
	int i;
	t_segmento *segmento=malloc(sizeof(t_segmento));

	for(i=0;i<list_size(listaSegmentos);i++){
		segmento=list_get(listaSegmentos,i);
		printf("idProceso:%i idSegmento:%i tamanio:%i dirLogica:%i dirFisicaInicio:%i\n",segmento->idProceso,segmento->idSegmento,segmento->tamanio,segmento->dirLogica,segmento->inicio);
	}
	printf("\n");
}
void limpiarMsg(t_msg *mensaje){
	free(mensaje->flujoDatos);
	mensaje->flujoDatos=NULL;
}
void mapearMemoria2(){
	int w=0,i=1;

	printf("DIRECCION FISICA DE INICIO: %i\n",(int)mp);
	printf("offset(bytes)           contenido(formato char/si no es char :)\n");
	printf("  %06i      ",w);
	for(w=0;w<tamanioBloque;w++){
		if(w-i*64==0){
				printf("\n  %06i      ",i*64);
				i++;
			}
		if(isalpha(mp[w])){
				printf("%c",mp[w]);
		}else{
				printf(":");
		}
	}
	printf("\n");
}
void crearEstructuras(){
	listaSegmentos=list_create();
	mp=malloc(tamanioBloque);
	ultimaDirLogica=0;
	log_debug(loguer,"crearEstructuras()==>Se creo la memoria principal del tp con la direccion fisica %i...",(int)(mp));
	pthread_mutex_init(&mutex_listaSeg,NULL);
	pthread_mutex_init(&mutex_mp,NULL);
}


