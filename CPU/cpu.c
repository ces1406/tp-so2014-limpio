/*
 * cpu.c
 *
 *  Created on: 28/05/2014
 *      Author: utnso
 */

#include "cpu_primitivas.h"

typedef struct{
	uint32_t entero;
}__attribute__ ((__packed__)) t_entero_pack;

//VARIABLES GLOBALES
t_dictionary  *g_diccionario_var;
char          *g_infoEtiquetas;
int            g_socketUMV;
int            g_socketKernel;
char           g_ipKernel[16];
char           g_ipUmv[16];
int            g_puertoKernel,g_puertoUmv;
int            g_quantum;
int            g_retardo;
t_msg          g_mensaje;
t_pcb          g_pcb;
char          *g_infoDiccioVar;
int            g_quantumGastado;
bool           g_expulsar;
bool           g_desconexion=false;
sem_t          sem_hotPlug;
bool           g_procesoAceptado=false;
t_log         *g_logger;
pthread_t      idHiloHotPlug;
//bool           fallaDeMemoria=false;

AnSISOP_funciones funciones={
		.AnSISOP_asignar=                  primitiva_asignar,
		.AnSISOP_definirVariable=          primitiva_definirVariable,
		.AnSISOP_dereferenciar=            primitiva_dereferenciar,
		.AnSISOP_finalizar=                primitiva_finalizar,
		.AnSISOP_imprimir=                 primitiva_imprimir,
		.AnSISOP_imprimirTexto=            primitiva_imprimirTexto,
		.AnSISOP_irAlLabel=                primitiva_irAlLabel,
		.AnSISOP_llamarConRetorno=         primitiva_llamarConRetorno,
		.AnSISOP_llamarSinRetorno=         primitiva_llamarSinRetorno,
		.AnSISOP_obtenerPosicionVariable=  primitiva_obtenerPosicionVariable,
		.AnSISOP_retornar=                 primitiva_retornar,
		.AnSISOP_obtenerValorCompartida=   primitiva_obtenerValorCompartida,
		.AnSISOP_asignarValorCompartida=   primitiva_asignarValorCompartida,
		.AnSISOP_entradaSalida         =   primitiva_entradaSalida,
};

AnSISOP_kernel funciones_kernel={
		.AnSISOP_wait                  =primitiva_wait,
		.AnSISOP_signal                =primitiva_signal
};

int main(int argc,char * argv[]){

	g_mensaje.flujoDatos=NULL;
	g_quantumGastado=0;

	//INICIALIZANDO EL LOG
	g_logger=crearLog(argv[0]);

	//CAPTAR LA SEÑAL SIUSR1
	signal(SIGUSR1,desconectarse);
	sem_init(&sem_hotPlug,0,0);

	//LANZANDO HILO HOT PLUG
	pthread_create(&idHiloHotPlug,NULL,&hiloHotPlug,NULL);

	//LEVANTANDO IPs Y PUERTOS DE KERNEL Y VMU
	leerArchivoConf(argv[1]);

    //CREANDO SOCKETS
    g_socketKernel=crearSocket();
    g_socketUMV=crearSocket();
    log_debug(g_logger,"sockets umv y kernel creados");

    //CONECTANDOSE A KERNEL Y HACIENDO EL HANDSHAKE
    conectarseCon(g_ipKernel,g_puertoKernel,g_socketKernel);
    handshakeConKernel();
    log_debug(g_logger,"conexion y handshake con kernel hecho");

    //CONECTANDOSE A UMV Y HACIENDO EL HANDSHAKE
   	conectarseCon(g_ipUmv,g_puertoUmv,g_socketUMV);
	handshakeConUmv();
	log_debug(g_logger,"conexion y handshake con umv hecho");

    //EJECUTANDO LOS PROCESOS QUE LLEGAN
    while(1){
    	//ESPERANDO HASTA QUE LLEGUE UN PROCESO CON SU PCB+QUANTUM+RETARDO
    	aceptarPcb();
    	//AVISANDO A UMV EL ID DEL PROCESO QUE SE EJECUTARA
    	avisoUMVprocesoActivo();
    	//LEVANTANDO Y RECREANDO EL DICCIONARIO DE VARIABLES DEL PROCESO
    	g_diccionario_var=dictionary_create();
    	recrearDiccioVars();
       	//LEVANTANDO EN UN t_dictionary EL MAPA DE ETIQUETAS
    	levantarSegmentoEtiquetas();
    	//EJECUTANDO EL PROGRAMA DURANTE SU QUANTUM
    	ejecutarProceso();
    }
	return 0;
}
void leerArchivoConf(char* path){
	char         *ip;
	t_config     *configCPU;

	configCPU=config_create(path);
	ip              =config_get_string_value(configCPU,"IPKERNEL");
	memcpy(g_ipKernel,ip,strlen(ip)+1);
	ip              =config_get_string_value(configCPU,"IPUMV");
	memcpy(g_ipUmv,ip,strlen(ip)+1);
	g_puertoKernel  =config_get_int_value(configCPU,"PUERTOKERNEL");
	g_puertoUmv     =config_get_int_value(configCPU,"PUERTOUMV");
	log_debug(g_logger,"Archivo de configuracion lenvantado... ipkernel:%s  ipUmv:%s  puertoKernel:%i  puertoUmv:%i",g_ipKernel,g_ipUmv,g_puertoKernel,g_puertoUmv);
	config_destroy(configCPU);
}
void handshakeConKernel(){
	g_mensaje.encabezado.codMsg=K_HANDSHAKE;
	g_mensaje.encabezado.longitud=0;
	enviarMsg(g_socketKernel,g_mensaje);
	recibirMsg(g_socketKernel,&g_mensaje);
	printf("***********handshakeConKernel*********");
	if(g_mensaje.encabezado.codMsg!=CONEXION_OK){
		printf("error de conexion\n");
		log_debug(g_logger,"error en la conexion con kernel");
		exit(EXIT_FAILURE);
	}
	liberarMsg();
}
void aceptarPcb(){
	log_debug(g_logger,"en funcion acpetarPcb esperando recibir de kernel un proceso y su pcb");
	recibirMsg(g_socketKernel,&g_mensaje);
	g_procesoAceptado=true;

	//deserializando el mensaje.flujoDatos en el orden pcb-quantum-retardo --bibliotequizar y optimizar---
	memcpy(&g_pcb.cursor_de_pila,            g_mensaje.flujoDatos,                                                       sizeof(u_int32_t));
	memcpy(&g_pcb.id_proceso,                g_mensaje.flujoDatos+sizeof(u_int32_t),                                     sizeof(uint16_t));
	memcpy(&g_pcb.indice_codigo,             g_mensaje.flujoDatos+sizeof(u_int32_t)+sizeof(uint16_t),                    sizeof(u_int32_t));
	memcpy(&g_pcb.indice_etiquetas,          g_mensaje.flujoDatos+sizeof(u_int32_t)*2+sizeof(uint16_t),                  sizeof(u_int32_t));
	memcpy(&g_pcb.program_counter,           g_mensaje.flujoDatos+sizeof(u_int32_t)*3+sizeof(uint16_t),                  sizeof(uint16_t));
	memcpy(&g_pcb.segmento_codigo,           g_mensaje.flujoDatos+sizeof(u_int32_t)*3+sizeof(uint16_t)*2,                sizeof(u_int32_t));
	memcpy(&g_pcb.segmento_pila,             g_mensaje.flujoDatos+sizeof(u_int32_t)*4+sizeof(uint16_t)*2,                sizeof(u_int32_t));
	memcpy(&g_pcb.tamanio_contexto_actual,   g_mensaje.flujoDatos+sizeof(u_int32_t)*5+sizeof(uint16_t)*2,                sizeof(u_int32_t));
	memcpy(&g_pcb.tamanio_indice_etiquetas,  g_mensaje.flujoDatos+sizeof(u_int32_t)*6+sizeof(uint16_t)*2,                sizeof(u_int32_t));
	memcpy(&g_quantum,                       g_mensaje.flujoDatos+sizeof(u_int32_t)*7+sizeof(uint16_t)*2,                sizeof(int));
	memcpy(&g_retardo,                       g_mensaje.flujoDatos+sizeof(int)+sizeof(u_int32_t)*7+sizeof(uint16_t)*2,    sizeof(int));
	liberarMsg();

	printf("\n*********acEptar_pcb**************\n");
	printf("QUANTUM------------->%i\n",g_quantum);
	printf("RETARDO------------->%i\n",g_retardo);
	printf("PROGRAMcOUNTER------>%i\n",g_pcb.program_counter);
	printf("INDICEcODIGO-------->%i\n",g_pcb.indice_codigo);
	printf("idPROCESO----------->%i\n",g_pcb.id_proceso);
	printf("SEGMENTOcODIGO------>%i\n",g_pcb.segmento_codigo);
	printf("SEGMENTOpILA-------->%i\n",g_pcb.segmento_pila);
	printf("CURSORdEpILA-------->%i\n",g_pcb.cursor_de_pila);
	printf("INDICEdEeTIQUETAS--->%i\n",g_pcb.indice_etiquetas);
	log_debug(g_logger,"Se recibio el pcb de un proceso con:\n");
	log_debug(g_logger,"quantum:           %i",g_quantum);
	log_debug(g_logger,"retardo:           %i",g_retardo);
	log_debug(g_logger,"programCounter:    %i",g_pcb.program_counter);
	log_debug(g_logger,"indice_codigo:     %i",g_pcb.indice_codigo);
	log_debug(g_logger,"id_proceso:        %i",g_pcb.id_proceso);
	log_debug(g_logger,"segmento_codigo:   %i",g_pcb.segmento_codigo);
	log_debug(g_logger,"segmento_pila:     %i",g_pcb.segmento_pila);
	log_debug(g_logger,"cursor_pila:       %i",g_pcb.cursor_de_pila);
	log_debug(g_logger,"indice_etiquetas:  %i",g_pcb.indice_etiquetas);
	//printf("tamanio_indice_etiquetas:%i\n",g_pcb.tamanio_indice_etiquetas);
	//printf("tamanio_contexto_actual:%i\n",g_pcb.tamanio_contexto_actual);
}
void handshakeConUmv(){
	g_mensaje.encabezado.codMsg=CPU_HANDSHAKE;
	g_mensaje.encabezado.longitud=0;//serializacion seria mensaje.flujo=cpu pero ya por el mensaje.encabezado.codMsg=U_HANDSHAKE  umv se da cuenta
	g_mensaje.flujoDatos=0;
	printf("*********handshakeConUmv**********\n");
	enviarMsg(g_socketUMV,g_mensaje);
	recibirMsg(g_socketUMV,&g_mensaje);
	liberarMsg();
	if(g_mensaje.encabezado.codMsg!=CONEXION_OK){
		//error en la conexion con umv
		printf("error en el handshake con umv\n");
		log_debug(g_logger,"Error en la conexion con UMV");
		exit(EXIT_FAILURE);
	}
}
void avisoUMVprocesoActivo(){
	g_mensaje.encabezado.codMsg=U_PROCESO_ACTIVO;
	g_mensaje.encabezado.longitud=sizeof(uint16_t);
	g_mensaje.flujoDatos=realloc(g_mensaje.flujoDatos,sizeof(uint16_t));
	memcpy(g_mensaje.flujoDatos,&g_pcb.id_proceso,sizeof(uint16_t));
	printf("*******avisoUMVprocesoActivo******\n");
	log_debug(g_logger,"enviando mensaje de proceso activo a umv");
	enviarMsg(g_socketUMV,g_mensaje);
	liberarMsg();
}
void recrearDiccioVars(){
	g_infoDiccioVar=NULL;
	t_size offset=0,tam;

	//TRAYENDO TODA LA PILA PARA CARGARLA AL DICCIONARIO DE VARIABLES
	tam=g_pcb.tamanio_contexto_actual*(sizeof(t_byte)+sizeof(t_palabra));
	offset=g_pcb.cursor_de_pila-g_pcb.segmento_pila;
	log_debug(g_logger,"Se pedira a umv todo el segmento pila para recrear el diccionario de variables");
	if(lecturaUMV(g_pcb.segmento_pila,offset,tam)!=0){//pedido a umv todo el segmento pila
		//error
		printf("error en la lectura del segmento pila del programa\n");
		log_debug(g_logger,"Hubo un error en la lectura del segmento pila en umv");
	}else{
		//reseteando el contenido de infoDiccioVar para ser el soporte de los datos del diccionario
		g_infoDiccioVar=realloc(g_infoDiccioVar,g_mensaje.encabezado.longitud);
		memcpy(g_infoDiccioVar,g_mensaje.flujoDatos,g_mensaje.encabezado.longitud);
		liberarMsg();
		llenarDiccionario();
	}
	//free(g_infoDiccioVar);--->se hizo en llenarDiccionario()
	//g_infoDiccioVar=NULL;---->se hizo en llenarDiccionario()
}
void ejecutarProceso(){
	g_expulsar=false;
	t_size offset,tamanio;
	char *lineaCodigo=NULL;
	g_quantumGastado=0;

	//TRAER DE UMV LAS quantum LINEAS DE CODIGO
	//ver si se puede en vez de ir a buscar a umv una por una las quantum lineas del indice_codigo y del segmento_codigo, traer todo el cacho, que pasa con
	//las instrucciones de salto?
	for(g_quantumGastado=0;g_quantumGastado<g_quantum;g_quantumGastado++){
		//pcb.program_counter ME INDICA EL INDICE EN EL ARREGLO QUE pcb.indice_codigo APUNTA Y QUE CONTIENE LA DIRECCION Y OFFSET DE LA LINEA DE CODIGO

		//LEER LA LINEA DEL SEGMENTO INDICE DE CODIGO QUE CORRESPONDA SEGUN EL PROGRAMA COUNTER
		offset=g_pcb.program_counter*(sizeof(t_size)+sizeof(t_puntero_instruccion));  //offset dentro del pcb.indice_codigo-el array de instrucciones--
		g_pcb.program_counter++;
		if(lecturaUMV(g_pcb.indice_codigo,offset,sizeof(t_size)+sizeof(t_puntero_instruccion))==-1){//lee y la data queda disponible en mensaje.flujoDatos
			//ERROR DE LECTURA
			printf("error en la lectura devuelta por la umv");
			log_debug(g_logger,"Error en la lectura del indice de codigo en umv...");
			exit(EXIT_FAILURE);
		}
		memcpy(&offset,    g_mensaje.flujoDatos,                                sizeof(t_puntero_instruccion));      //offset dentro del pcb.segmento_codigo
		memcpy(&tamanio,   g_mensaje.flujoDatos+sizeof(t_puntero_instruccion),  sizeof(t_size));                     //tamanio de esa linea de codigo
		liberarMsg();

	    //BUSCAR LA LINEA DE CODIGO DEL SEGMENTO DE CODIGO
		if(lecturaUMV(g_pcb.segmento_codigo,offset,tamanio)==-1){
			//ERROR DE LECTURA
			log_debug(g_logger,"Error en la lectura de la linea del segmento de codigo en umv...");
			exit(EXIT_FAILURE);
		}
		g_mensaje.flujoDatos[g_mensaje.encabezado.longitud-1]='\0';//hace quilombo por que esa pos es \n ???
		printf("\n***************Linea traida de UMV para ejecutar==>%s<==******************\n",g_mensaje.flujoDatos);
		log_debug(g_logger,"*******ejecutando la linea traida de umv:%s********",g_mensaje.flujoDatos);
		listarDiccio();
		lineaCodigo=malloc(g_mensaje.encabezado.longitud);
		memcpy(lineaCodigo,g_mensaje.flujoDatos,g_mensaje.encabezado.longitud);
		liberarMsg();
		//analizadorLinea(strdup(lineaCodigo),&funciones,&funciones_kernel);
		analizadorLinea(lineaCodigo,&funciones,&funciones_kernel);
		//printf("vine del analizador\n\n");
		free(lineaCodigo);
		lineaCodigo=NULL;
		usleep(g_retardo*1000);
	}
	if(!g_expulsar&&g_desconexion){
		//se activo la senial para desconectarse y no es el fin del programa
		expulsarProceso();
	}else{
		if(!g_expulsar){
		//g_expulsar es falso=>se termino el quantum (no es fin de programa)
		g_mensaje.encabezado.codMsg=K_EXPULSADO_FIN_QUANTUM;
		cargarPcb();
		enviarMsg(g_socketKernel,g_mensaje);
		liberarEstructuras();
		}
	}
}
int lecturaUMV(t_puntero base,t_size offset,t_size tamanio){
	/*char a;
	int i;
	t_byte c;
	t_entero_pack  b;
	t_byte num[4];*/

	//printf("en lectura con base:%i offset:%i y tamanio:%i\n",base,offset,tamanio);
	log_debug(g_logger,"Pedido de lectura a umv:\nbase:%i offset:%i tamanio:%i",base,offset,tamanio);
	printf("==>PEDIDo  dE  LEcTuRA uMv===> ");
	printf("base:%i offset:%i tamanio:%i\n",base,offset,tamanio);
	//PEDIDO A UMV DE LECTURA
	g_mensaje.encabezado.codMsg=U_PEDIDO_BYTES;
	g_mensaje.encabezado.longitud=sizeof(t_puntero)+2*sizeof(t_size);
	g_mensaje.flujoDatos=realloc(g_mensaje.flujoDatos,g_mensaje.encabezado.longitud);
	memcpy(g_mensaje.flujoDatos,                                  &base,      sizeof(t_puntero));
	memcpy(g_mensaje.flujoDatos+sizeof(t_puntero),                &offset,    sizeof(t_size));
	memcpy(g_mensaje.flujoDatos+sizeof(t_puntero)+sizeof(t_size), &tamanio,   sizeof(t_size));
	//printf("*********lecturaUMV*********\n");
	//printf("mensaje a UMV base: %i offset:%i tamanio:%i\n",(int)*g_mensaje.flujoDatos,(int)*(g_mensaje.flujoDatos+4),(int)*(g_mensaje.flujoDatos+8));
	//printf("mensaje a UMV base: %i offset:%i tamanio:%i\n",base,offset,tamanio);
	enviarMsg(g_socketUMV,g_mensaje);
	free(g_mensaje.flujoDatos);g_mensaje.flujoDatos=NULL;

	//RESPUESTA DE UMV AL PEDIDO (depositado en mensaje.flujoDatos)
	recibirMsg(g_socketUMV,&g_mensaje);
	if(g_mensaje.encabezado.codMsg!=C_PEDIDO_OK){
		//error
		log_debug(g_logger,"error de lectura en umv\n");
		printf("eRROR dE LECTURA\n");
		return -1;
	}
/*
	printf("llego el dato:");
	if(g_mensaje.encabezado.longitud==1){
		memcpy(&c,g_mensaje.flujoDatos,sizeof(t_byte));
		memcpy(&a,g_mensaje.flujoDatos,sizeof(char));
		printf("%c\n o:%c",a,c);
	}else{
		if(g_mensaje.encabezado.longitud==sizeof(u_int32_t)){
			memcpy(&b.entero,g_mensaje.flujoDatos,sizeof(t_entero_pack));
			memcpy(num,g_mensaje.flujoDatos,4);
			//printf("int:%i atoi:%i atoi2:%i\n",(int)b.entero,atoi(g_mensaje.flujoDatos),atoi(num));
		}else{
			for(i=0;i<g_mensaje.encabezado.longitud;i++){
				if(isalpha(g_mensaje.flujoDatos[i])){
					printf("%c",g_mensaje.flujoDatos[i]);
				}else{
					printf("%x",g_mensaje.flujoDatos[i]);
				}
			}
			printf("\n");
		}
	}*/

	return 0;
}
int escrituraUMV(t_puntero base,t_size offset,t_size tamanio,const t_byte* contenido){
	/*t_byte d;
	int i;
	t_entero_pack b;*/

	//PEDIDO A UMV DE ESCRITURA
	g_mensaje.encabezado.codMsg=U_ALMACENAR_BYTES;
	g_mensaje.encabezado.longitud=sizeof(t_puntero)+2*sizeof(t_size)+tamanio;
	g_mensaje.flujoDatos=(char*)realloc(g_mensaje.flujoDatos,g_mensaje.encabezado.longitud);

	log_debug(g_logger,"Pedido de escritura a umv:\nbase:%i offset:%i tamanio:%i contenido(primer caracter):%c",base,offset,tamanio,contenido[0]);

	printf("==>PEDIDo dE EscRITuRA uMv===> ");
	printf("base:%i offset:%i tamanio:%i\n",base,offset,tamanio);

	memcpy(g_mensaje.flujoDatos,                                      &base,            sizeof(t_puntero));
	memcpy(g_mensaje.flujoDatos+sizeof(t_puntero),                    &offset,          sizeof(t_size));
	memcpy(g_mensaje.flujoDatos+sizeof(t_puntero)+sizeof(t_size),     &tamanio,         sizeof(t_size));
	memcpy(g_mensaje.flujoDatos+sizeof(t_puntero)+2*sizeof(t_size),   contenido,        tamanio);
/*
	if(tamanio==1){
		memcpy(&d,contenido,sizeof(t_byte));
		printf("%c\n",d);
	}else{
		if(tamanio==sizeof(u_int32_t)){
			memcpy(&b.entero,contenido,sizeof(int));
			printf("int:%i atoi:%i\n",(int)b.entero,atoi(g_mensaje.flujoDatos));
		}else{
			for(i=0;i<g_mensaje.encabezado.longitud;i++){
				if(isalpha(g_mensaje.flujoDatos[i])){
					printf("%c",g_mensaje.flujoDatos[i]);
				}else{
					printf("%x",g_mensaje.flujoDatos[i]);
				}
			}
			printf("\n");
		}
	}
*/
	enviarMsg(g_socketUMV,g_mensaje);
	//RESPUESTA DE UMV AL PEDIDO DE ESCRITURA
	recibirMsg(g_socketUMV,&g_mensaje);
	if(g_mensaje.encabezado.codMsg!=C_PEDIDO_OK){
		//error
		log_debug(g_logger,"hubo un error en la escritura en umv\n");
		printf("eRROR dE ESCRITURA\n");
		return -1;
	}
	free(g_mensaje.flujoDatos);g_mensaje.flujoDatos=NULL;
	return 0;
}
void levantarSegmentoEtiquetas(){
	g_infoEtiquetas=NULL;

	log_debug(g_logger,"Se levantara la info del segmento de etiquetas trayendo directamente todo el cacho de segmento...");
	if(g_pcb.tamanio_indice_etiquetas!=0){ //si es =0 => el codigo-script no usa ninguna etiqueta
		//TRAYENDO EL SEGMENTO DE ETIQUETAS PARA CARGARLO AL DICCIONARIO DE ETIQUETAS
		if(lecturaUMV(g_pcb.indice_etiquetas,0,g_pcb.tamanio_indice_etiquetas)!=0){
			//error de lectura del segmento de etiquetas
			log_debug(g_logger,"error al traer todo el segmento de etiquetas de la umv.");
			printf("ERROR eN lECTRA dEL sEGMENTo dE eTIQUETAs\n");
			exit(EXIT_FAILURE);
		}
		//colocando el segmento de etiquetas en infoEtiquetas
		g_infoEtiquetas=realloc(g_infoEtiquetas,g_pcb.tamanio_indice_etiquetas);
		memcpy(g_infoEtiquetas,g_mensaje.flujoDatos,g_pcb.tamanio_indice_etiquetas);
		liberarMsg();
	}
}
void listarDiccio(){
	printf("Diccionario de variables (direcciones logicas):\n");
	void imprime(char* clave, t_var *vari){printf("  clave:%s nombre:%s direccion:%i\n",clave,vari->nombre_var,vari->direccion_var);}
	dictionary_iterator(g_diccionario_var,(void *)imprime);
}
void expulsarProceso(){
	g_mensaje.encabezado.codMsg=K_EXPULSADO_DESCONEXION;
	cargarPcb();
	enviarMsg(g_socketKernel,g_mensaje);

	g_mensaje.encabezado.codMsg=U_EXPULSADO_DESCONEXION;
	g_mensaje.encabezado.longitud=0;
	enviarMsg(g_socketUMV,g_mensaje);

	log_debug(g_logger,"Se enviaron mensajes a umv y kernel avisando la desconexion del proceso cpu");
	//printf("enviados mensajes a umv y kernel\n");
	liberarEstructuras();
	close(g_socketKernel);
	close(g_socketUMV);
	log_destroy(g_logger);
	exit(EXIT_SUCCESS);
}
void llenarDiccionario(){
	int  i,offset;
	char nombre;

	log_debug(g_logger,"Se llenara el diccionario de variables con:");
	//levantar del cacho de segmento de satck traido de umv las variables y sus direcciones
	for (i=0;i<g_pcb.tamanio_contexto_actual;i++){
		offset=i*(sizeof(t_byte)+sizeof(t_palabra))+g_pcb.cursor_de_pila-g_pcb.segmento_pila;
		memcpy(&nombre,g_infoDiccioVar+offset-g_pcb.cursor_de_pila+g_pcb.segmento_pila,sizeof(char));
		log_debug(g_logger,"nombre_variable:%c  direccion:%i",nombre,offset);
		t_var *varDiccio=crearVarDiccio((char)nombre,offset+1);
		dictionary_put(g_diccionario_var,varDiccio->nombre_var,varDiccio);
	}
	free(g_infoDiccioVar);
	g_infoDiccioVar=NULL;
}
void liberarEstructuras (){
	log_debug(g_logger,"Se liberaran todas las estructuras usadas en el proceso cpu");
	if(g_infoEtiquetas!=NULL){
		free(g_infoEtiquetas);
		g_infoEtiquetas=NULL;
	}
	if(g_infoDiccioVar!=NULL){
		free(g_infoDiccioVar);
		g_infoDiccioVar=NULL;
	}
	if(g_mensaje.flujoDatos!=NULL){
		free(g_mensaje.flujoDatos);
		g_mensaje.flujoDatos=NULL;
	}
	dictionary_clean_and_destroy_elements(g_diccionario_var,(void*)liberarElementoDiccio);
	dictionary_destroy(g_diccionario_var);
	g_procesoAceptado=false;//aca???
}
void desconectarse(){
	int cpuInactiva=0;

	log_debug(g_logger,"Se recibio la senial SIGUSR1 y se desconectara el proceso CPU");
	sem_post(&sem_hotPlug);
	pthread_join(idHiloHotPlug,(void*)&cpuInactiva);
	if(cpuInactiva==1){
		log_destroy(g_logger);
		exit(EXIT_SUCCESS);
	}
}
void cargarPcb(){
	g_mensaje.encabezado.longitud=sizeof(t_pcb);
	g_mensaje.flujoDatos=malloc(g_mensaje.encabezado.longitud);
	memcpy(g_mensaje.flujoDatos,                                                      &g_pcb.cursor_de_pila,                   sizeof(u_int32_t));
	memcpy(g_mensaje.flujoDatos+sizeof(u_int32_t),                                    &g_pcb.id_proceso,                       sizeof(uint16_t));
	memcpy(g_mensaje.flujoDatos+sizeof(uint16_t)+sizeof(u_int32_t),                   &g_pcb.indice_codigo,                    sizeof(u_int32_t));
	memcpy(g_mensaje.flujoDatos+sizeof(uint16_t)+2*sizeof(u_int32_t),                 &g_pcb.indice_etiquetas,                 sizeof(u_int32_t));
	memcpy(g_mensaje.flujoDatos+sizeof(uint16_t)+3*sizeof(u_int32_t),                 &g_pcb.program_counter,                  sizeof(uint16_t));
	memcpy(g_mensaje.flujoDatos+2*sizeof(uint16_t)+3*sizeof(u_int32_t),               &g_pcb.segmento_codigo,                  sizeof(u_int32_t));
	memcpy(g_mensaje.flujoDatos+2*sizeof(uint16_t)+4*sizeof(u_int32_t),               &g_pcb.segmento_pila,                    sizeof(u_int32_t));
	memcpy(g_mensaje.flujoDatos+2*sizeof(uint16_t)+5*sizeof(u_int32_t),               &g_pcb.tamanio_contexto_actual,          sizeof(u_int32_t));
	memcpy(g_mensaje.flujoDatos+2*sizeof(uint16_t)+6*sizeof(u_int32_t),               &g_pcb.tamanio_indice_etiquetas,         sizeof(u_int32_t));
}
void *hiloHotPlug(void *sinUso){
	int            cpuInactiva;

	log_debug(g_logger,"hilo hiloHotPlug lanzado");
	sem_wait(&sem_hotPlug);
	printf("===------>CpU DeSCoNECTANdoSE<-------===\n");
	if(g_procesoAceptado){
		log_debug(g_logger,"se cerrara el proceso CPU con un programa ejecutandose al momento del cierre");
		g_desconexion=true;
		cpuInactiva=0;
	}else{
		//printf("desconectandose\n");
		log_debug(g_logger,"se cerrara el proceso CPU sin ningun programa ejecutandose al momento del cierre");

		g_mensaje.encabezado.codMsg=K_EXPULSADO_DESCONEXION;
		g_mensaje.encabezado.longitud=0;
		enviarMsg(g_socketKernel,g_mensaje);

		g_mensaje.encabezado.codMsg=U_EXPULSADO_DESCONEXION;
		g_mensaje.encabezado.longitud=0;
		enviarMsg(g_socketUMV,g_mensaje);

		//printf("enviados mensajes a umv y kernel\n");
		close(g_socketKernel);
		close(g_socketUMV);
		cpuInactiva=1;
	}
	return (void*) cpuInactiva;
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

void liberarMsg(){
	free(g_mensaje.flujoDatos);
	g_mensaje.flujoDatos=NULL;
}
