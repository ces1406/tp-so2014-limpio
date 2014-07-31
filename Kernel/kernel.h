/*
 * kernel.h
 *
 *  Created on: 22/05/2014
 *      Author: utnso
 */

#ifndef KERNEL_H_
#define KERNEL_H_
#include <commons/config.h>
#include <commons/collections/list.h>
#include <commons/collections/queue.h>
#include <commons/log.h>
#include <parser/parser.h>
#include "uso-sockets.h"
#include "estructuras.h"
#include <pthread.h>
#include <semaphore.h>
#include <sys/poll.h>
#include <signal.h>

typedef struct{
	t_pcb     pcb;
	short int peso;
	short int soquet_prog;
}t_nodo_proceso;


typedef struct{
	pthread_mutex_t mutex_io;
	sem_t           sem_io;
	int             retardo;
	t_queue         *bloqueados;
}t_dataHilo;
typedef struct{
	pthread_t hiloID;
	t_dataHilo dataHilo;
}t_hiloIO;
typedef struct{
	t_nodo_proceso* proceso;
	int            espera;
}t_bloqueadoIO;

typedef struct{
	char    *nombre;
	uint32_t valor;
	t_queue *bloqueados;
	bool     desbloquear;
}t_semaforo;

typedef struct{
	char *nombre;
	uint32_t valor;
}t_varCompartida;

//static void destruirElementoColaBloq(t_bloqueadoIO *nodoBloq);
//static void liberarElementoDiccioIO(t_hiloIO * hiloIO);
//static void liberarElementoDiccioSem(t_semaforo *semaforo);
static void destruirNodoProceso(t_nodo_proceso *proceso);
static void destruirVarCompartida(t_varCompartida *varComp);
void liberarRecursos();

#endif /* KERNEL_H_ */
