/*
 * uso-sockets.c
 *
 *  Created on: 28/04/2014
 *      Author: utnso
 */

#include "uso-sockets.h"

int crearSocket(){
	int unSocket, optval=1;
	if((unSocket=socket(AF_INET,SOCK_STREAM,0))<0){
		//ERROR
		printf("error al ejecutar la funcion socket()\n");
	}
	setsockopt(unSocket,SOL_SOCKET,SO_REUSEADDR,&optval,sizeof(optval));
	return unSocket;
}
void conectarseCon(char *dirIP,int puerto,int unSocket){
	struct sockaddr_in clienteDir;

	clienteDir.sin_addr.s_addr=inet_addr(dirIP);
	clienteDir.sin_family=AF_INET;
	clienteDir.sin_port=htons(puerto);
	memset(&(clienteDir.sin_zero),'\0',8);

	if(connect(unSocket,(struct sockaddr*)&clienteDir,sizeof(clienteDir))==-1){
		//ERROR
		printf("error en la funcion connect()\n");
	}
}
void recibirMsg(int unSocket,t_msg *msgRecibido){

	if (recv(unSocket,&(msgRecibido->encabezado),sizeof(t_encabezado),MSG_WAITALL)<0){//MSG_WAITALL)<0){
		//ERROR
		printf("error en la funcion recv()1\n");
	}
	msgRecibido->flujoDatos=(char*)realloc(msgRecibido->flujoDatos,sizeof(char)*msgRecibido->encabezado.longitud);
	//msgRecibido->flujoDatos=malloc(msgRecibido->encabezado.longitud);
	if(msgRecibido->encabezado.longitud>0){
		if(recv(unSocket,msgRecibido->flujoDatos,msgRecibido->encabezado.longitud,MSG_WAITALL)<0){
			//ERROR
			printf("error en la funcion recv()\n");
		}

	}
}
void enviarMsg(int unSocket,t_msg msgEnviado){
	int bytesTotales=0;
	int bytesPendientes=msgEnviado.encabezado.longitud+sizeof(t_encabezado);
    int tam=sizeof(t_encabezado)+msgEnviado.encabezado.longitud;
    int n;
    char *flujo=malloc(tam);
	memcpy(flujo,&msgEnviado.encabezado,sizeof(t_encabezado));
	memcpy(&flujo[sizeof(t_encabezado)],msgEnviado.flujoDatos,msgEnviado.encabezado.longitud);
	while(bytesTotales<bytesPendientes){
		n=send(unSocket,flujo,msgEnviado.encabezado.longitud+sizeof(msgEnviado.encabezado),MSG_NOSIGNAL);//bytesPendientes,0);
		if(n<0){
			//ERROR
			printf("error en la funcion send()\n");
		}
		bytesTotales +=n;
		bytesPendientes -=n;
	}
	free(flujo);
}
void bindearSocket(int unSocket,char *dirIP,int puerto){
	struct sockaddr_in dirServidor;

	dirServidor.sin_addr.s_addr=inet_addr(dirIP);
	dirServidor.sin_port=htons(puerto);
	dirServidor.sin_family=AF_INET;
	memset(&(dirServidor.sin_zero),'\0',8);

	if(bind(unSocket,(struct sockaddr*)&dirServidor,sizeof(struct sockaddr))<0){
		//ERROR
		printf("error en la funcion bind()\n");
	}
}
void escucharSocket(int unSocket){
	if(listen(unSocket,10)<0){
		//ERROR
		printf("error en la funcion listen()\n");
	}
}
int aceptarConexion(int unSocketEscuchando){
	struct sockaddr_in dirClienteEntrante;
	int socketConexionNueva;
	int tam=sizeof(struct sockaddr_in);
	socketConexionNueva=accept(unSocketEscuchando,(struct sockaddr *)&dirClienteEntrante,(void*)&tam);
	return socketConexionNueva;
}
