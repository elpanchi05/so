#include "parser.h"

extern t_log* logger;
t_dictionary* diccionario;

t_list* parsear_archivo(FILE* archivo) {
    if(diccionario == NULL) {
        crear_diccionario();
    }

    t_list* lista_instrucciones = list_create();
	while (!feof(archivo)) {
        crear_instruccion(archivo, lista_instrucciones); // Codigo de instruccion | Numero de parametros | Parametros
    }

    return lista_instrucciones;
}

void crear_instruccion(FILE* archivo, t_list* lista_instrucciones) {   
    t_instruccion* instruccion = malloc(sizeof(t_instruccion));
    t_buffer* buffer_aux = crear_buffer();
	buffer_aux->size = 100;
    buffer_aux->stream = (char*)malloc(buffer_aux->size * sizeof(char));
    fgets(buffer_aux->stream, buffer_aux->size, archivo);

    char *pos;
    if ((pos = strchr(buffer_aux->stream, '\n')) != NULL) *pos = '\0';

    char** words = (char**)malloc(4 * sizeof(char*));
    words = string_split(buffer_aux->stream, " ");
    char* codigo_instruccion = words[0];

    instruccion->codigo_instruccion = (t_codigo_instruccion) dictionary_get(diccionario, codigo_instruccion);
    //log_trace(logger, "Operacion %d", instruccion->codigo_instruccion);

    instruccion->parametros = list_create();
    size_t numero_elementos = tamano_vector(words);
    for (int pos = 1; pos < numero_elementos; pos++) 
	{
        list_add(instruccion->parametros, words[pos]); 
        //log_trace(logger, "Parametro %s", words[pos]);
	}
	list_add(lista_instrucciones, instruccion);

    liberar_buffer(buffer_aux);
}

void crear_diccionario() {
    diccionario = dictionary_create();
   	dictionary_put(diccionario, "SET", (void*)((int)SET));
	dictionary_put(diccionario, "SUM", (void*)((int)SUM));
	dictionary_put(diccionario, "SUB", (void*)((int)SUB));
	dictionary_put(diccionario, "JNZ", (void*)((int)JNZ));
	dictionary_put(diccionario, "SLEEP", (void*)((int)SLEEP));
	dictionary_put(diccionario, "WAIT", (void*)((int)WAIT));
	dictionary_put(diccionario, "SIGNAL", (void*)((int)SIGNAL));
    dictionary_put(diccionario, "RESIZE", (void*)((int)RESIZE));
	dictionary_put(diccionario, "MOV_IN", (void*)((int)MOV_IN));
	dictionary_put(diccionario, "MOV_OUT", (void*)((int)MOV_OUT));
	dictionary_put(diccionario, "COPY_STRING", (void*)((int)COPY_STRING));
	dictionary_put(diccionario, "IO_GEN_SLEEP", (void*)((int)IO_GEN_SLEEP));
	dictionary_put(diccionario, "IO_STDIN_READ", (void*)((int)IO_STDIN_READ));
	dictionary_put(diccionario, "IO_STDOUT_WRITE", (void*)((int)IO_STDOUT_WRITE));
    dictionary_put(diccionario, "IO_FS_CREATE", (void*)((int)IO_FS_CREATE));
	dictionary_put(diccionario, "IO_FS_DELETE", (void*)((int)IO_FS_DELETE));
	dictionary_put(diccionario, "IO_FS_TRUNCATE", (void*)((int)IO_FS_TRUNCATE));
    dictionary_put(diccionario, "IO_FS_WRITE", (void*)((int)IO_FS_WRITE));
	dictionary_put(diccionario, "IO_FS_READ", (void*)((int)IO_FS_READ));
    dictionary_put(diccionario, "EXIT", (void*)((int)EXIT));
}

int tamano_vector(char** vector) {
    int n = 0;
    while (vector[n] != 0) {
        n++;
    }
    return n;
}
