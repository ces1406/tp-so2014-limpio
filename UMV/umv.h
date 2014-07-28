/*
 * umv.h
 *
 *  Created on: 21/05/2014
 *      Author: utnso
 */

#ifndef UMV_H_
#define UMV_H_
#include <ctype.h>
#include <pthread.h>
#include <commons/log.h>
#include <commons/config.h>
#include <commons/string.h>
#include <commons/collections/list.h>
#include "uso-sockets.h"
#include "estructuras.h"

#define FIRST_FIT 1
#define WORST_FIT 0

typedef struct{
	u_int32_t inicio;
	u_int32_t tamanio;
	u_int32_t dirLogica;
	u_int16_t idSegmento;
	uint16_t idProceso;
}t_segmento;

#endif /* UMV_H_ */
