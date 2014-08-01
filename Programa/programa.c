/*
 * programa.c
 *  Created on: 25/05/2014
 *      Author: utnso
 */
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <parser/parser.h>
#include <parser/metadata_program.h>
#include <commons/log.h>
#include "uso-sockets.h"
#include <commons/config.h>
#include "estructuras.h"

#define ARCHIVO_CONFIG "programaConfig.cfg"
#define ARCHIVO_LOG    "programaLog.log"

int socketKernel;
int puertoKernel;
char ipKernel[16];

//t_log *crearLog(char *archivo);

int main (int argc,char *argv[]){
	extern int   socketKernel;
	extern int   puertoKernel;
	extern char  ipKernel[16];
	t_msg mensaje;
	mensaje.flujoDatos=NULL;

	//int       descriptorArch;
	FILE      *script;
	//struct     stat infoArchivo;
	int        tamanio=0;
	char      *data=NULL;
	//char       data2[610];
	char      *ip_prog;
	t_config  *configPrograma;
	char       c;
	//t_log    *logger;

	//INICIALIZANDO EL LOG
	//logger=crearLog(argv[0]);

	//--------------------------------------->METODO LARGO<------------------------------------------------
 	//ABRIENDO EL ARCHIVO DEL SCRIPT A EJECUTAR
	if((script=fopen(argv[1],"r"))==NULL){
		printf("No se encontro el archivo");//----->nunca deberia llegar aca
	}else{
		printf("archivo encontrado\n");
		while((c=fgetc(script))!=EOF){
			tamanio++;
			data=realloc(data,tamanio);
			data[tamanio-1]=c;
		}
	}
	fclose(script);
	//-------------------------------------->FIN METODO LARGO<----------------------------------------------


	//---------------------------------------->METODO CORTO<-------------------------------------------------
	/*descriptorArch=open(argv[1],O_RDONLY);
	//EXTRAYENDO INFO DEL ARCHIVO EN infoArchivo
	fstat(descriptorArch,&infoArchivo);
	tamanio=infoArchivo.st_size;
	data=malloc(tamanio);
	//LEYENDO EL CONTENIDO DEL ARCHIVO ENTERO Y ALOJANDOLO EN data
	read(descriptorArch,data,tamanio);
	close(descriptorArch);
	//log_debug(logger,"Se abrio el script a ejecutar de tamanio:%i con el contenido:\n%s",tamanio,data);
	  */
	//-------------------------------------->FIN METODO CORTO<----------------------------------------------

	//LEVANTANDO ARCHIVO DE CONFIGURACION
	configPrograma=config_create(ARCHIVO_CONFIG);
	puertoKernel  =config_get_int_value(configPrograma,"Puerto-Kernel");
	ip_prog       =config_get_string_value(configPrograma,"IP-Kernel");
    strcpy(ipKernel,ip_prog);
    printf("se conectara a ip-kernel:%s puerto-kernel:%i...\n",ip_prog,puertoKernel);
 	config_destroy(configPrograma);
 	//log_debug(logger,"Se levanto el archivo de configuracion con puerto-kernel:%i ip-kernel:%s",puertoKernel,ipKernel);
 	//CONECTANDOSE AL KERNEL
	socketKernel=crearSocket();
	conectarseCon(ipKernel,puertoKernel,socketKernel);
	//log_debug(logger,"Se conectara con kernel...");
	printf("se hizo la conexion\n");

	//HACIENDO HANDSHAKE CON KERNEL
	mensaje.encabezado.codMsg=K_HANDSHAKE;
	mensaje.encabezado.longitud=0;
	enviarMsg(socketKernel,mensaje);
	printf("handshake mandado\n");
	free(mensaje.flujoDatos);mensaje.flujoDatos=NULL;
	//log_debug(logger,"Se mando mensaje a kernel con codigo:K_HANDSHAKE...");

	while(1){
		printf("entrando al while esperando mensaje...\n");
		recibirMsg(socketKernel,&mensaje);
		printf("llego mensaje\n");

		if(mensaje.encabezado.codMsg==P_ENVIAR_SCRIPT){
			printf("se recibio enviar-script\n");
			//log_debug(logger,"Se recibio un mensaje de kernel con el codigo: P_ENVIAR_SCRIPT y se enviara el script...");
			mensaje.encabezado.codMsg=K_ENVIO_SCRIPT;
			mensaje.encabezado.longitud=tamanio;
			printf("tamanio del script%i\n",tamanio);
			mensaje.flujoDatos=malloc(tamanio);
			memcpy(mensaje.flujoDatos,data,tamanio);
			enviarMsg(socketKernel,mensaje);
		}
		if(mensaje.encabezado.codMsg==P_IMPRIMIR_VAR){
			//log_debug(logger,"Se recicibio un mensaje de kernel con el valor a imprimir de una variable");
			printf("********************************************\n");
			printf("-------------RESULTADO: %i\n",(int)*(mensaje.flujoDatos));
			printf("********************************************\n");
			free(mensaje.flujoDatos);mensaje.flujoDatos=NULL;
		}
		if(mensaje.encabezado.codMsg==P_IMPRIMIR_TXT){
			//log_debug(logger,"Se recibio un mensaje de kernel con un texto a imprimir");
			printf("********************************************\n");
			printf("-------------TEXTO: %s\n",mensaje.flujoDatos);
			printf("********************************************\n");
			free(mensaje.flujoDatos);mensaje.flujoDatos=NULL;
		}
		if(mensaje.encabezado.codMsg==P_DESCONEXION){
			//log_debug(logger,"Se recibio un mensaje de kernel para desconectarse, el programa finalizo, chauuu...");
			printf("FINALIZO EL PROGRAMA..\n");
			//sleep(2);//-------------------------->sleep porque sino apenas cierra el socket manda basura -el kernel recibe basura
			//seguir=false;
			free(mensaje.flujoDatos);mensaje.flujoDatos=NULL;
			break;
		}
		if(mensaje.encabezado.codMsg==P_SERVICIO_DENEGADO){
			//log_debug(logger,"Se recibio un mensaje de kernel para desconectarse, el programa tiene errores de manejo de memoria, chauuu...");
			printf("SERVICIO DENEGADO (memory overload)...\n");
			//seguir=false;
			free(mensaje.flujoDatos);mensaje.flujoDatos=NULL;
			break;
		}
	}
	/*recibirMsg(socketKernel,&mensaje);
	if(mensaje.encabezado.codMsg==P_DESCONEXION){
		printf("desconexion definitiva\n");
	}/*/
	//sleep(4);
	//close(socketKernel);

	//free(mensaje.flujoDatos);
	return EXIT_SUCCESS;
}
/*
t_log *crearLog(char *archivo){
	char path[11]={0};
	//char aux[17]={0};

	strcpy(path,ARCHIVO_LOG);
	//strcat(aux,"touch ");
	//strcat(aux,path);
	//system(aux);
	t_log *logAux=log_create(path,archivo,false,LOG_LEVEL_DEBUG);
	log_info(logAux,"ARCHIVO DE LOG CREADO");
	return logAux;
}--------------------------------->Sacado porque trae problemas con los file descriptor de los archvos*/

