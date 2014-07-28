/*
 * estructuras.h
 *
 *  Created on: 25/04/2014
 *      Author: utnso
 */

#ifndef ESTRUCTURAS_H_
#define ESTRUCTURAS_H_
#include <parser/parser.h>
#include <parser/metadata_program.h>
#include <stdint.h>

typedef u_int8_t t_byte;

typedef struct{
	uint16_t id_proceso;
	uint16_t program_counter;
	t_puntero segmento_codigo;
	t_puntero segmento_pila;
	t_puntero indice_codigo;
	t_puntero indice_etiquetas;
	t_size tamanio_contexto_actual;
	t_puntero cursor_de_pila;
	t_size tamanio_indice_etiquetas;
}t_pcb;

#endif /* ESTRUCTURAS_H_ */
