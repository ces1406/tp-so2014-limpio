/*
 * cpu_primitivas.c
 *
 *  Created on: 28/05/2014
 *      Author: utnso
 */

#include "cpu_primitivas.h"

extern bool           g_desconexion;
extern t_dictionary  *g_diccionario_var;
extern bool           g_expulsar;
extern char          *g_infoEtiquetas;
extern t_log         *g_logger;
extern t_msg          g_mensaje;
extern t_pcb          g_pcb;
extern int            g_quantum;
extern int            g_quantumGastado;
extern int            g_socketKernel;

t_puntero primitiva_definirVariable(t_nombre_variable variable){
	t_puntero dirRetornada=-1;           //---->indica ERROR ???
if(!g_expulsar){
	t_size tamanio    =sizeof(t_nombre_variable);
	t_puntero base    =g_pcb.segmento_pila;
	t_size offset     =dictionary_size(g_diccionario_var)*(sizeof(t_byte)+sizeof(t_palabra))+(g_pcb.cursor_de_pila-g_pcb.segmento_pila);     //cada varibale ocupa 5 bytes(1byte:nombre+4bytes:contenido)
	printf("***primitiva--->definir Varible***\n");
	printf("            --->mandando a guardar var:%c base:%i offset:%i tamanio:%i<---\n",variable,base,offset,tamanio);
	log_debug(g_logger,":::primitiva_definirVariable:::\n    mandando a guardar la var:%c base:%i offset:%i tam:%i",variable,base,offset,tamanio);

	//ENVIA MENSAJE A UMV CON: base+offset+tamaño+contenido a almacenar
	if(escrituraUMV(base,offset,tamanio,(t_byte *)&variable)==-1){//=>segmentation fault?
		//error--->segmento sobrepasado
		printf("error de escritura de nombre de variable en umv -sEGMENTATIoN FAuLT-\n");
		log_debug(g_logger,"Error en la escritura del nombre de una variable en umv...");
		falloMemoria();
	}else{
		t_var *var_diccio =crearVarDiccio(variable,offset+1);
		//printf("se guardara la variable var_diccio->nombre_var:%s var_diccio->dir:%i\n",var_diccio->nombre_var,var_diccio->direccion_var);
		dictionary_put(g_diccionario_var,var_diccio->nombre_var,var_diccio);
		g_pcb.tamanio_contexto_actual++;
		dirRetornada=offset+1;
	}
}
	return dirRetornada;
}
t_puntero primitiva_obtenerPosicionVariable(t_nombre_variable variable){
	t_puntero            ptr;
	extern bool          g_expulsar;

if(!g_expulsar){
	printf("===>PRIMITIVA: obtener posicion variable %c\n",variable);
	char *clave=malloc(sizeof(char)*2);
	memcpy(clave,&variable,sizeof(char)*2);
	clave[1]='\0';
	t_var *varBuscada=dictionary_get(g_diccionario_var,clave);
	ptr=varBuscada->direccion_var;
	printf("            --->variable:%s direccion_var-oFFset:%i<---\n",varBuscada->nombre_var,varBuscada->direccion_var);
	log_debug(g_logger,":::primitiva_obtenerPosicionVariable:::\n    se busco variable:%c y se encontro  t_var.nombre:%s  t_var.dirVar-offset-:%i",variable,varBuscada->nombre_var,varBuscada->direccion_var);
	free(clave);
}
	return ptr;
}
t_valor_variable primitiva_dereferenciar (t_puntero ptr_variable){
	t_valor_variable variable;
	u_int32_t var;

if(!g_expulsar){
	printf("===>PRIMITIVA: derreferenciar\n");
	log_debug(g_logger,":::primitiva_dereferenciar:::");
	//ENVIANDO PEDIDO DE LECTURA A UMV
	if(lecturaUMV(g_pcb.segmento_pila,ptr_variable,sizeof(t_valor_variable))==-1){
		//error
		printf("            --->eRRoR en la lectura de la variable en segmento pila<---\n");
		log_debug(g_logger,"Error en la lectura en umv de una variable...");
		falloMemoria();
	}else{
		memcpy(&var,g_mensaje.flujoDatos,sizeof(u_int32_t));
		printf("            --->dIr_de_variable:%i  valor deVuelto de uMv:%i<---\n",ptr_variable,var);
		log_debug(g_logger,"    primitiva_dereferenciar---> dir_de_variable:%i---> valor devuelto de umv:%i",ptr_variable,(int)*(g_mensaje.flujoDatos));
		memcpy(&variable,g_mensaje.flujoDatos,sizeof(t_valor_variable));
		liberarMsg();
	}
}
	return variable;
}
void primitiva_asignar(t_puntero ptr_variable, t_valor_variable variable){

if(!g_expulsar){
	printf("===>PRIMITIVA: asIgnar\n");
	printf("            --->escribir en la direccion de la var:%i  vaLor:%i\n",ptr_variable,variable);
	log_debug(g_logger,":::primitiva_asignar:::\n    se escribira en la dir de variable:%i  el valor_variable:%i",ptr_variable,variable);
	if(escrituraUMV(g_pcb.segmento_pila,ptr_variable,sizeof(t_palabra),(t_byte *)&variable)==-1){
		//ERROR DE ESCRITURA
		printf("            --->ERRoR en escritura en el segmento pila del valor de la variable\n");
		falloMemoria();
	}
}
}
void primitiva_finalizar(void){

if(!g_expulsar){
	//si segmento_pila=cursor_de_pila =>es el fin del programa
	if(g_pcb.segmento_pila==g_pcb.cursor_de_pila){
		if(g_desconexion){
			expulsarProceso();
		}
		printf("===>PRIMITIVA: finalizar -FIN dEL PRoGRAMA-\n");
		//FINALIZAR EJECUCION DEL PROGRAMA
		g_mensaje.encabezado.codMsg=K_EXPULSADO_FIN_PROG;
		cargarPcb();
		enviarMsg(g_socketKernel,g_mensaje);
		liberarMsg();
		g_quantumGastado=g_quantum;
		log_debug(g_logger,":::primitiva_finalizar -fin del programa-:::");
		liberarEstructuras();
		g_expulsar=true;
	}else{
		//FIN DE UNA FUNCION_SIN_RETORNO
		printf("               --->finalizar -fin dE una funcion sin retorno-:::\n");
		log_debug(g_logger,":::primitiva_finalizar -fin de una funcion sin retorno-:::");
		t_puntero cursorPila=g_pcb.cursor_de_pila;
		//-->Desapilando el program counter
		if(lecturaUMV(g_pcb.segmento_pila,g_pcb.cursor_de_pila-g_pcb.segmento_pila-sizeof(uint16_t),sizeof(uint16_t))==-1){
			//error---->Nunca deberia llegar aca
			printf("            --->ERRoR de lectura del program counter de retorno<---\n");
			log_debug(g_logger,"Error de lectura en umv del program counter para retornar...");
		}else{
			memcpy(&g_pcb.program_counter,g_mensaje.flujoDatos,sizeof(uint16_t));
			liberarMsg();
			printf("            --->nuevo program coUnter:%i\n",g_pcb.program_counter);
		}
		//-->Desapilando el cursor_de_pila
		if(lecturaUMV(g_pcb.segmento_pila,g_pcb.cursor_de_pila-g_pcb.segmento_pila-sizeof(t_palabra)-sizeof(uint16_t),sizeof(t_palabra))==-1){
			//error---->Nunca deberia llegar aca
			printf("            --->ERRoR de lectura del cursor pila de retorno<---\n");
			log_debug(g_logger,"Error de lectura en umv del cursor_de_pila");
		}else{
			memcpy(&g_pcb.cursor_de_pila,g_mensaje.flujoDatos,sizeof(t_palabra));
			liberarMsg();
			printf("            --->nuevo curSor_de_pIla:%i<---\n",g_pcb.cursor_de_pila);
		}
		//LIMPIAR EL DICCIONARIO DE VARIABLES DEL CONTEXTO A ABANDONAR
		 dictionary_clean_and_destroy_elements(g_diccionario_var,(void*)liberarElementoDiccio);
		//RECALCULANDO EL TAMANIO DEL CONTEXTO A RETORNAR
		 g_pcb.tamanio_contexto_actual=(cursorPila-sizeof(t_palabra)-sizeof(uint16_t)-g_pcb.cursor_de_pila)/(sizeof(t_byte)+sizeof(t_palabra));
		//RECREANDO EL DICCIONARIO DE VARIABLES DEL CONTEXTO A RETORNAR
		 recrearDiccioVars();
	}
}
}
void primitiva_irAlLabel(t_nombre_etiqueta nombreEtiqueta){
	t_puntero_instruccion siguienteInstruc;

if(!g_expulsar){

	printf("===>PRIMITIVA: irAlLabel -%s-\n",nombreEtiqueta);
	log_debug(g_logger,":::primitiva_irAlLabel -%s-:::",nombreEtiqueta);
	siguienteInstruc=metadata_buscar_etiqueta(nombreEtiqueta,g_infoEtiquetas,g_pcb.tamanio_indice_etiquetas);
	if(siguienteInstruc==-1){
		//ERROR
		printf("             --->ERRoR en la busqueda de la etiqueta:%s en el chaR*etiquetas\n",nombreEtiqueta);
	}else{
		//cambiar la linea de ejecucion a siguienteInstruc
		printf("    metadata_buscar_etiqueta() dio: siguienteInstruccion:%i\n",siguienteInstruc);
		g_pcb.program_counter=siguienteInstruc;
	}
}
}
void primitiva_llamarSinRetorno(t_nombre_etiqueta etiqueta){
	t_size                offset=0;
	uint16_t              pcAnterior=g_pcb.program_counter;

if(!g_expulsar){
	printf("===>PRIMITIVA: llamar sIn reTorno -%s-\n",etiqueta);
	log_debug(g_logger,":::primitiva_llamarSinRetorno -%s-:::",etiqueta);
	//PONER EL program_counter EN LA DIRECCION DE LA FUNCION
	primitiva_irAlLabel(etiqueta);
	//LIMPIAR EL DICCIONARIO DE VARIABLES
	dictionary_clean_and_destroy_elements(g_diccionario_var,(void*)liberarElementoDiccio);
	//APILAR EL pcb.cursor_del_stack VIEJO
	offset=g_pcb.tamanio_contexto_actual*(sizeof(t_palabra)+sizeof(t_byte));
	if(escrituraUMV(g_pcb.segmento_pila,g_pcb.cursor_de_pila-g_pcb.segmento_pila+offset,sizeof(t_palabra),(t_byte*)&g_pcb.cursor_de_pila)==-1){
		//error--->segemnto de pila sobrepasado
		printf("           --->ERRoR en la escritura en umv del cursor de pila\n");
		log_debug(g_logger,"Error de escritura en umv del cursor de pila viejo...");
		falloMemoria();
	}else{
		//APILAR pcb.program_counter incrementado en uno
		offset=offset+sizeof(t_palabra);
		printf("            --->prOgram cOUnter apiladO:%i<---\n",pcAnterior);
		if(escrituraUMV(g_pcb.segmento_pila,g_pcb.cursor_de_pila-g_pcb.segmento_pila+offset,sizeof(uint16_t),(t_byte*)&pcAnterior)==-1){
			//error--->segmento de pila sobrepasado
			printf("             --->ERRoR eN LA EscRITuRA dEL PRoGRAM couNTER\n");
			falloMemoria();
		}else{
			//SETEAR pcb.tamaño_contexto_actual
			g_pcb.tamanio_contexto_actual=0;
			//SETEAR EL NUEVO pcb.cursor_de_pila
			g_pcb.cursor_de_pila=g_pcb.cursor_de_pila+offset+sizeof(uint16_t);
			printf("             --->nuevo cursor de pila:%i\n",g_pcb.cursor_de_pila);
		}
	}
}
}
void primitiva_llamarConRetorno(t_nombre_etiqueta etiqueta, t_puntero donde_retornar){
	int                   offset;

if(!g_expulsar){
	printf("===>PRIMITIVA: llamar cOn retorno\n");
	log_debug(g_logger,":::primitiva_llamarConRetorno:::");
	primitiva_llamarSinRetorno(etiqueta);
	//ADEMAS APILAR LA DIR DE RETORNO
	offset=g_pcb.cursor_de_pila-g_pcb.segmento_pila;
	if(escrituraUMV(g_pcb.segmento_pila,offset,sizeof(t_puntero),(t_byte*)&donde_retornar)==-1){
		//error--->segmento de pila sobrepasado
		printf("            --->ERRoR Al ApILAR LA dIREccIoN DEL REsuLTADo dE lA FuNcIoN\n");
		falloMemoria();
	}else{
		//MOVER EL CURSOR DE PILA YA QUE AGREGAMOS ESA DIR DE RETORNO
		g_pcb.cursor_de_pila=g_pcb.cursor_de_pila+sizeof(t_puntero);
		printf("    cursor_de_pila quedo en:%i\n",g_pcb.cursor_de_pila);
	}
}
}
void primitiva_imprimir(t_valor_variable valor){

if(!g_expulsar){
	log_debug(g_logger,"***primitiva_imprimir***  VALOR: %i",(int)valor);
	printf("===>PRIMITIVA: imprimir\n");
	//printf("***************************************************************\n");
	printf("              ******VALOR: %i*****\n",(int)valor);
	//printf("***************************************************************\n");

	g_mensaje.encabezado.codMsg=K_IMPRIMIR_VAR;
	g_mensaje.encabezado.longitud=sizeof(t_valor_variable)+sizeof(uint16_t);
	g_mensaje.flujoDatos=realloc(g_mensaje.flujoDatos,g_mensaje.encabezado.longitud);
	memcpy(g_mensaje.flujoDatos,&valor,sizeof(t_valor_variable));
	memcpy(g_mensaje.flujoDatos+sizeof(t_valor_variable),&(g_pcb.id_proceso),sizeof(uint16_t));
	enviarMsg(g_socketKernel,g_mensaje);
	liberarMsg();
}
}
void primitiva_retornar(t_valor_variable retorno){
	t_puntero            dirRetorno;

	printf("===>PRIMITIVA: REToRNAR\n");
	log_debug(g_logger,":::primitiva_retornar:::\n");
	//desapilando la dir de retorno del resultado
	if(lecturaUMV(g_pcb.segmento_pila,g_pcb.cursor_de_pila-g_pcb.segmento_pila-sizeof(t_puntero),sizeof(t_puntero))==-1){
		//error---->no deberia llegar aca
		printf("            --->ERRoR EN LA LEcTuRA dE LA DIR DE REToRNo DEL REsuLTADo\n");
	}
	//guardando el resultado "retorno" en esa direccion
	memcpy(&dirRetorno,g_mensaje.flujoDatos,sizeof(t_puntero));
	if(escrituraUMV(g_pcb.segmento_pila,dirRetorno,sizeof(t_palabra),(t_byte *)&retorno)==-1){
		//error---->no deberia llegar aca
		printf("            --->ERRoR EN LA EscrITuRA dEL REsuLTADo dE lA FuNcIoN en la dir de devollucion\n");
	}
	g_pcb.cursor_de_pila=g_pcb.cursor_de_pila-sizeof(t_puntero);
	primitiva_finalizar();
}
t_var *crearVarDiccio(char nombVar,t_puntero dirLogica){
	t_var *newVar=malloc(sizeof(t_var));
	newVar->nombre_var=strdup(&nombVar);
	newVar->nombre_var[1]='\0';
	newVar->direccion_var=dirLogica;
	return newVar;
}
void liberarElementoDiccio(t_var* elemento){
	free(elemento->nombre_var);
	free(elemento);
}
/*void finalizarProceso(){
	extern t_pcb g_pcb;
	extern t_msg g_mensaje;

	printf("****aprovecho esta funcion para dar un parte del programa finalizado\n****");
	printf("id del programa finalizado -en pcb.id_proceso:%i\n",g_pcb.id_proceso);
	printf("program counter -en pcb.program_counter:%i\n",g_pcb.program_counter);
	printf("tamanio del contexto actual -cant. de variables -en pcb.tamanio_contexto_actual:%i\n",g_pcb.tamanio_contexto_actual);
	printf("dir logica del cursor de pila -en pcb.cursor_de_pila:%i\n",g_pcb.cursor_de_pila);
	printf("dir logica del segmento pila  -en pcb.segmento_pila:%i\n",g_pcb.segmento_pila);
	printf("dir logica del segmento etiquetas -en pcb.indice_etiquetas:%i\n",g_pcb.indice_etiquetas);
	printf("tamanio del indice de etiquetas -en pcb.tamanio_indice_etiquetas:%i\n",g_pcb.tamanio_indice_etiquetas);
	listarDiccio();
	expulsarProceso();
	free(g_mensaje.flujoDatos);//--->Borrar despues, es solo para probar 1 programa
	//--->Borrar despues, es solo para probar 1 programa
}*/
void primitiva_imprimirTexto(char* texto){
	printf("===>PRIMITIVA: imprimirTexto\n");
	log_debug(g_logger,":::primitiva_imprimirTexto:::\n  texto:%s",texto);

	g_mensaje.encabezado.codMsg=K_IMPRIMIR_TXT;
	g_mensaje.encabezado.longitud=sizeof(uint16_t)+strlen(texto);
	g_mensaje.flujoDatos=realloc(g_mensaje.flujoDatos,g_mensaje.encabezado.longitud);
	memcpy(g_mensaje.flujoDatos,&(g_pcb.id_proceso),sizeof(uint16_t));
	memcpy(g_mensaje.flujoDatos+sizeof(uint16_t),texto,strlen(texto));
	enviarMsg(g_socketKernel,g_mensaje);
	liberarMsg();
}
t_valor_variable primitiva_obtenerValorCompartida(t_nombre_compartida variable){
	t_valor_variable varCompartida;

if(!g_expulsar){
	log_debug(g_logger,":::primitiva--->obtenerValorCompartida:::   var:%s",variable);
	printf("===>PRIMITIVA: obtenerValorCompartida\n");

	g_mensaje.encabezado.codMsg=K_PEDIDO_VAR_GL;
	g_mensaje.encabezado.longitud=strlen(variable)+sizeof(uint16_t);
	g_mensaje.flujoDatos=realloc(g_mensaje.flujoDatos,g_mensaje.encabezado.longitud);
	memcpy(g_mensaje.flujoDatos,&g_pcb.id_proceso,sizeof(uint16_t));
	memcpy(g_mensaje.flujoDatos+sizeof(uint16_t),variable,strlen(variable));
	printf("            --->se pide el valor de la variable %s\n",variable);
	enviarMsg(g_socketKernel,g_mensaje);
	//recibiendo la contestacion
	recibirMsg(g_socketKernel,&g_mensaje);
	if(g_mensaje.encabezado.codMsg!=VALOR_COMPARTIDA_OK){
		//error de acceso a esa variable--->el codigo hace referencia a una var compartida que no existe
		log_debug(g_logger,"Error en la obtencion del valor de una variable compartida...");
		printf("            --->se referencio a una var compartida inexistente<---\n");
		g_expulsar=true;
		g_quantumGastado=g_quantum;
		liberarEstructuras();
		//=>kernel ya tomo nota y va a expulsar al proceso=>termino su ejecucion y nada mas...
	}else{
		memcpy(&varCompartida,g_mensaje.flujoDatos,sizeof(t_valor_variable));
	}
	liberarMsg();
	printf("            --->se trajo el valor %i<---\n",varCompartida);
}
	return varCompartida;
}
t_valor_variable primitiva_asignarValorCompartida(t_nombre_compartida variable, t_valor_variable valor){

if(!g_expulsar){
	printf("===>PRIMITIVA: asignarValorCompartida\n");
	log_debug(g_logger,":::primitiva_asignarValorCompartida:::");

	g_mensaje.encabezado.codMsg=K_ASIGNAR_VAR_GL;
	g_mensaje.encabezado.longitud=strlen(variable)+sizeof(t_valor_variable)+sizeof(uint16_t);//serializado:id_proceso+nombre+valor
	g_mensaje.flujoDatos=realloc(g_mensaje.flujoDatos,g_mensaje.encabezado.longitud);
	memcpy(g_mensaje.flujoDatos,&g_pcb.id_proceso,sizeof(uint16_t));
	memcpy(g_mensaje.flujoDatos+sizeof(uint16_t),variable,strlen(variable));
	memcpy(g_mensaje.flujoDatos+sizeof(uint16_t)+strlen(variable),&valor,sizeof(t_valor_variable));
	printf("            ---> enviando pedido de asignacion de la varComapartida:%s valor:%i<---\n",variable,valor);
	enviarMsg(g_socketKernel,g_mensaje);
	//recibiendo la contestacion
	recibirMsg(g_socketKernel,&g_mensaje);
	//printf("se recibio contestacion codMsg:%i\n",g_mensaje.encabezado.codMsg);
	if(g_mensaje.encabezado.codMsg!=VALOR_COMPARTIDA_OK){
		//error de acceso a esa variable
		log_debug(g_logger,"Error en la asignacion de un valor a una variable compartida...\n");
		printf("            --->se referencio a una var compartida inexistente<---\n");
		g_expulsar=true;
		g_quantumGastado=g_quantum;
		liberarEstructuras();
		//=>kernel ya tomo nota y va a expulsar al proceso=>termino su ejecucion y nada mas...
	}
	printf("la asignacion fue ok\n");
	liberarMsg();
}
	return valor;
}
void primitiva_entradaSalida(t_nombre_dispositivo dispositivo, int tiempo){

	log_debug(g_logger,":::primitiva_entradaSalida:::");
	printf("===>PRIMITIVA: entradaSalida\n");
	g_mensaje.encabezado.codMsg=K_EXPULSADO_ES;//serializado: pcb+dispositivo+tiempo
	cargarPcb();
	//contenidoPcb=malloc(strlen(g_mensaje.flujoDatos));
	//memcpy(contenidoPcb,g_mensaje.flujoDatos,strlen(g_mensaje.flujoDatos));
	//printf("el strlen(dispositivo):%i del dispositivo:%s\n",strlen(dispositivo),dispositivo);
	//printf("mensaje.longitud antes del offset:%i\n",g_mensaje.encabezado.longitud);
	int offset=g_mensaje.encabezado.longitud;
	g_mensaje.encabezado.longitud=offset+sizeof(uint32_t)+strlen(dispositivo);
	g_mensaje.flujoDatos=realloc(g_mensaje.flujoDatos,g_mensaje.encabezado.longitud);
	memcpy(g_mensaje.flujoDatos+offset,dispositivo,strlen(dispositivo));
	memcpy(g_mensaje.flujoDatos+offset+strlen(dispositivo),&tiempo,sizeof(uint32_t));
	//memcpy(g_mensaje.flujoDatos+strlen(dispositivo)+sizeof(uint32_t),contenidoPcb,strlen(contenidoPcb));
	enviarMsg(g_socketKernel,g_mensaje);
	liberarMsg();
	g_quantumGastado=g_quantum;
	liberarEstructuras();
	g_expulsar=true;
}
void primitiva_signal(t_nombre_semaforo id_semaforo){

	log_debug(g_logger,":::primitiva--->signal:::");
	printf("===>PRIMITIVA: signal\n");
	g_mensaje.encabezado.codMsg=K_SIGNAL;
	g_mensaje.encabezado.longitud=strlen(id_semaforo)+sizeof(uint16_t);//serializado:id_proceso+id_semaforo
	g_mensaje.flujoDatos=realloc(g_mensaje.flujoDatos,g_mensaje.encabezado.longitud);
	memcpy(g_mensaje.flujoDatos,&g_pcb.id_proceso,sizeof(uint16_t));
	memcpy(g_mensaje.flujoDatos+sizeof(uint16_t),id_semaforo,strlen(id_semaforo));
	enviarMsg(g_socketKernel,g_mensaje);
	liberarMsg();

	//recibiendo contestacion
	//printf("    esperando contestacion de kernel.....\n");
	recibirMsg(g_socketKernel,&g_mensaje);
	if(g_mensaje.encabezado.codMsg!=C_SIGNAL_OK){
		log_debug(g_logger,"Error en singal, semaforo inexistente...\n");
		printf("            --->se referencio a un semaforo inexistente<---\n");
		g_expulsar=true;
		g_quantumGastado=g_quantum;
		liberarEstructuras();
		//=>kernel ya tomo nota y va a expulsar al proceso=>termino su ejecucion y nada mas...
	}
}
void primitiva_wait(t_nombre_semaforo id_semaforo){

	log_debug(g_logger,":::primitiva_wait:::");
	printf("===>PRIMITIVA: wait\n");
	g_mensaje.encabezado.codMsg=K_WAIT;
	g_mensaje.encabezado.longitud=strlen(id_semaforo)+sizeof(uint16_t);//serializado:id_proceso+id_semaforo
	g_mensaje.flujoDatos=realloc(g_mensaje.flujoDatos,g_mensaje.encabezado.longitud);
	memcpy(g_mensaje.flujoDatos,&g_pcb.id_proceso,sizeof(uint16_t));
	memcpy(g_mensaje.flujoDatos+sizeof(uint16_t),id_semaforo,strlen(id_semaforo));

	enviarMsg(g_socketKernel,g_mensaje);
	//recibiendo contestacion
	//printf("    esperando contestacion de kernel.....\n");
	recibirMsg(g_socketKernel,&g_mensaje);

	if(g_mensaje.encabezado.codMsg!=C_WAIT_OK){
		if(g_mensaje.encabezado.codMsg==C_ERROR){
			log_debug(g_logger,"Error en singal, semaforo inexistente...\n");
			printf("            --->se referencio a un semaforo inexistente<---\n");
			g_expulsar=true;
			g_quantumGastado=g_quantum;
			liberarEstructuras();
			//=>kernel ya tomo nota y va a expulsar al proceso=>termino su ejecucion y nada mas...
		}else{
			printf("            --->semaforo ocupado<---\n");
			//hay que desalojar al proceso
			g_mensaje.encabezado.codMsg=K_EXPULSADO_WAIT;
			cargarPcb();
			enviarMsg(g_socketKernel,g_mensaje);
			liberarEstructuras();
			g_quantumGastado=g_quantum;
			g_expulsar=true;
		}
	}
}
void falloMemoria(){
		g_expulsar=true;
	g_mensaje.encabezado.codMsg=K_EXPULSADO_SEG_FAULT;
	cargarPcb();
	enviarMsg(g_socketKernel,g_mensaje);
	liberarMsg();
	g_quantumGastado=g_quantum;//salir de la ejecucion
}
