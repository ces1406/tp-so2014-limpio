/*
 * cpu_primitivas.h
 *
 *  Created on: 28/05/2014
 *      Author: utnso
 */

#ifndef CPU_PRIMITIVAS_H_
#define CPU_PRIMITIVAS_H_

#include <signal.h>
#include <commons/config.h>
#include <commons/log.h>
#include <parser/parser.h>
#include <semaphore.h>
#include <pthread.h>
#include "uso-sockets.h"
#include "estructuras.h"
#include <ctype.h>

typedef u_int8_t t_byte;
typedef u_int32_t t_palabra;
typedef struct{
	char *nombre_var;
	t_puntero direccion_var;
}t_var;

int    lecturaUMV(t_puntero,t_size,t_size);
int    escrituraUMV(t_puntero ,t_size ,t_size ,const t_byte *);
void   leerArchivoConf(char*);
void   desconectarse();
void   handshakeConKernel();
void   aceptarPcb();
void   handshakeConUmv();
void   avisoUMVprocesoActivo();
void   listarDiccio();
void   recrearDiccioVars();
void   ejecutarProceso();
void   levantarSegmentoEtiquetas();
void   finalizarProceso();
void   expulsarProceso();
void   liberarElementoDiccio(t_var*);
void   llenarDiccionario();
void   liberarEstructuras();
void   cargarPcb();
void   desconectarse();
void   liberarMsg();
void  *hiloHotPlug(void *sinUso);
t_log *crearLog(char *archivo);
t_var *crearVarDiccio(char,t_puntero);
void falloMemoria();

//PRIMITIVAS
t_puntero primitiva_definirVariable(t_nombre_variable);
t_puntero primitiva_obtenerPosicionVariable(t_nombre_variable);
t_valor_variable primitiva_dereferenciar (t_puntero);
t_valor_variable primitiva_obtenerValorCompartida(t_nombre_compartida);
t_valor_variable primitiva_asignarValorCompartida(t_nombre_compartida, t_valor_variable);
void primitiva_finalizar(void);
void primitiva_asignar(t_puntero, t_valor_variable);
void primitiva_irAlLabel(t_nombre_etiqueta);
void primitiva_llamarSinRetorno(t_nombre_etiqueta);
void primitiva_retornar(t_valor_variable);
void primitiva_imprimir(t_valor_variable);
void primitiva_llamarConRetorno (t_nombre_etiqueta, t_puntero);
void primitiva_imprimirTexto(char*);
void primitiva_entradaSalida(t_nombre_dispositivo, int);
void primitiva_wait(t_nombre_semaforo);
void primitiva_signal(t_nombre_semaforo);

#endif /* CPU_PRIMITIVAS_H_ */
