#include "io.h"

int main(int argc, char *argv[])
{
    nombre_interfaz = argv[2];
    initialize_setup(argv[1], nombre_interfaz, &logger, &config);
    tipo_interfaz = config_get_string_value(config, "TIPO_INTERFAZ");
    tiempo_unidad_de_trabajo = config_get_int_value(config, "TIEMPO_UNIDAD_TRABAJO");
    bloques_iniciales = dictionary_create();
    conectar_modulos();

    while(true)
    {
        t_paquete *paquete = recibir_paquete(socket_kernel);
        
        uint32_t pid = 0;
        op_code operacion = paquete->codigo_operacion;
        void *aux = paquete->buffer->stream;
        memcpy(&pid, aux, sizeof(uint32_t));
        
        int indice = operacion - 8;
        log_info(logger, "PID: %d - Operacion: %s", pid, instruccion_string[indice]);

        iniciar_interfaz(paquete, pid);
        eliminar_paquete(paquete);
        op_code op_code = OK;
        send(socket_kernel, &op_code, sizeof(op_code), 0);
        //log_trace(logger, "Peticion terminada");
    }

    // destruir_cerrar_estructuras();
}

void conectar_modulos()
{
    socket_memoria = start_client_module("MEMORIA", IP_CONFIG_PATH);
    log_info(logger, "Conexion con servidor MEMORIA en socket %d", socket_memoria);

    recv(socket_memoria, &tam_pagina, sizeof(uint32_t), MSG_WAITALL);

    socket_kernel = start_client_module("KERNEL", IP_CONFIG_PATH);
    log_info(logger, "Conexion con servidor KERNEL en socket %d", socket_kernel);

    t_paquete *paquete = crear_paquete_codigo_operacion(OK);
    size_t largo = strlen(nombre_interfaz) + 1;
    agregar_a_buffer(paquete->buffer, &largo, sizeof(size_t));
    agregar_a_buffer(paquete->buffer, nombre_interfaz, largo);
    char* tipo_interfaz = config_get_string_value(config, "TIPO_INTERFAZ");
    if(strcmp(tipo_interfaz, "GENERICA") == 0) {
        uint32_t cantidad_operaciones = 1;
        agregar_a_buffer(paquete->buffer, &cantidad_operaciones, sizeof(uint32_t));
        op_code op = IO_GEN_SLEEP_OP_CODE;
        agregar_a_buffer(paquete->buffer, &op, sizeof(op_code));
    }
    if(strcmp(tipo_interfaz, "STDIN") == 0) {
        uint32_t cantidad_operaciones = 1;
        agregar_a_buffer(paquete->buffer, &cantidad_operaciones, sizeof(uint32_t));
        op_code op = IO_STDIN_READ_OP_CODE;
        agregar_a_buffer(paquete->buffer, &op, sizeof(op_code));
    }
    if(strcmp(tipo_interfaz, "STDOUT") == 0) {
        uint32_t cantidad_operaciones = 1;
        agregar_a_buffer(paquete->buffer, &cantidad_operaciones, sizeof(uint32_t));
        op_code op = IO_STDOUT_WRITE_OP_CODE;
        agregar_a_buffer(paquete->buffer, &op, sizeof(op_code));
    }
    if(strcmp(tipo_interfaz, "DIALFS") == 0) {
        uint32_t cantidad_operaciones = 5;
        agregar_a_buffer(paquete->buffer, &cantidad_operaciones, sizeof(uint32_t));
        op_code op = IO_FS_CREATE_OP_CODE;
        agregar_a_buffer(paquete->buffer, &op, sizeof(op_code));
        op = IO_FS_DELETE_OP_CODE;
        agregar_a_buffer(paquete->buffer, &op, sizeof(op_code));
        op = IO_FS_TRUNCATE_OP_CODE;
        agregar_a_buffer(paquete->buffer, &op, sizeof(op_code));
        op = IO_FS_WRITE_OP_CODE;
        agregar_a_buffer(paquete->buffer, &op, sizeof(op_code));
        op = IO_FS_READ_OP_CODE;
        agregar_a_buffer(paquete->buffer, &op, sizeof(op_code));
        block_size = config_get_int_value(config, "BLOCK_SIZE");
        block_count = config_get_int_value(config, "BLOCK_COUNT");
        path_base_dialfs = config_get_string_value(config, "PATH_BASE_DIALFS");
        retraso_compactacion = config_get_int_value(config, "RETRASO_COMPACTACION");
        inicializar_estructuras_fs();
    }
    enviar_paquete(paquete, socket_kernel);
}

void iniciar_interfaz(t_paquete *paquete, uint32_t pid)
{
    if (strcmp(tipo_interfaz, "GENERICA") == 0)
        manejar_interfaz_generica(paquete);
    else if (strcmp(tipo_interfaz, "STDIN") == 0)
        manejar_interfaz_STDIN(paquete);
    else if (strcmp(tipo_interfaz, "STDOUT") == 0)
        manejar_interfaz_STDOUT(paquete);
    else if (strcmp(tipo_interfaz, "DIALFS") == 0)
        manejar_interfaz_DialFS(paquete, pid);
    else
    {
        log_trace(logger, "Interfaz desconocida");
        exit(EXIT_FAILURE);
    }
}

void manejar_interfaz_generica(t_paquete *paquete)
{
    uint32_t valor_sleep = 0;
    memcpy(&valor_sleep, (paquete->buffer->stream) + 4, sizeof(uint32_t));
    usleep((1000 * tiempo_unidad_de_trabajo) * valor_sleep); 
}

void manejar_interfaz_STDIN(t_paquete *paquete_kernel)
{
    t_paquete* paquete_io = crear_paquete_codigo_operacion(ESCRITURA);
    uint32_t tamanio = 0;
    uint32_t pags_a_escribir = 0;
    void* aux = paquete_kernel->buffer->stream + sizeof(uint32_t);

    memcpy(&tamanio, aux, sizeof(uint32_t));
    aux += sizeof(uint32_t);

    agregar_a_buffer(paquete_io->buffer, aux, sizeof(uint32_t));
    aux += sizeof(uint32_t);

    char *buffer_texto = malloc(tamanio);
    printf("Ingresa un texto (máx. %d caracteres): ", tamanio); // TODO: el parametro tamaño es incluyendo \0? Hay que almacenarlo?
    fgets(buffer_texto, tamanio + 1, stdin);
    agregar_a_buffer(paquete_io->buffer, aux, sizeof(uint32_t)); 
    memcpy(&pags_a_escribir, aux, sizeof(uint32_t));
    aux += sizeof(uint32_t);
    clean_stdin();
    solicitar_escritura_en_memoria(paquete_io, aux, pags_a_escribir, tamanio, buffer_texto);
}

void clean_stdin() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

void solicitar_escritura_en_memoria(t_paquete* paquete_io, void* aux, uint32_t pags_a_escribir, uint32_t tamanio, char* buffer_texto) {
    uint32_t tam_primer_contenido = 0;
    uint32_t tam_ultimo_contenido = 0;

    for(int i = 0; i < pags_a_escribir; i++) { 
        if(i == 0) { 
            agregar_a_buffer(paquete_io->buffer, aux, sizeof(uint32_t)); // Primera DF
            aux += sizeof(uint32_t);
            agregar_a_buffer(paquete_io->buffer, aux, sizeof(uint32_t)); // Tamaño primer contenido
            memcpy(&tam_primer_contenido, aux, sizeof(uint32_t));
            aux += sizeof(uint32_t);

            tam_ultimo_contenido = calcular_ultimo_contenido(tamanio, tam_primer_contenido, pags_a_escribir, tam_pagina); // TODO: repetido en cpu: sacarlo de alla (incluido del buffer) o reestructurar aca para extraer la variable aca mismo
            //log_trace(logger, "Primer contenido %d", tam_primer_contenido);
            for(int j = 0; j < tam_primer_contenido; j++) {
                //log_trace(logger, "[1] Byte %d", buffer_texto[j]);
                agregar_a_buffer(paquete_io->buffer, &buffer_texto[j], sizeof(char)); // Primer contenido
            }
        }
        else {
            if(i == pags_a_escribir - 1) {
                agregar_a_buffer(paquete_io->buffer, aux, sizeof(uint32_t)); // Ultima DF
                // uint32_t d = 0;
                // memcpy(&d, aux, sizeof(uint32_t));
                //log_trace(logger, "Ultima df %d", d);
                aux += sizeof(uint32_t);
                agregar_a_buffer(paquete_io->buffer, aux, sizeof(uint32_t)); // Tamaño ultimo contenido
                memcpy(&tam_ultimo_contenido, aux, sizeof(uint32_t));
                aux += sizeof(uint32_t);
                int ultimos_bytes = tamanio - tam_ultimo_contenido;
                //log_trace(logger, "Ultimo contenido %d", tam_ultimo_contenido);
                //log_trace(logger, "Ultimos bytes %d", ultimos_bytes);
                for(int j = ultimos_bytes; tam_ultimo_contenido != 0; tam_ultimo_contenido--, j++) {
                    agregar_a_buffer(paquete_io->buffer, &buffer_texto[j], sizeof(char));
                    //log_trace(logger, "[3] Byte %d", buffer_texto[j]);
                }
            }
            else {
                uint32_t bytes_a_escribir = tamanio - tam_primer_contenido - tam_ultimo_contenido;
                i--;
                for(int j = 0; j < bytes_a_escribir; j++) {
                    if(j % tam_pagina == 0) {
                        agregar_a_buffer(paquete_io->buffer, aux, sizeof(uint32_t)); // DFs intermedias
                        aux += sizeof(uint32_t);
                        i++;
                    }
                    agregar_a_buffer(paquete_io->buffer, &buffer_texto[j + tam_primer_contenido], sizeof(char));
                    //log_trace(logger, "[2] Byte %d", buffer_texto[j + tam_primer_contenido]);
                }
            }
            if (i == 0) {
               perror("Evitando loop infinito");
            }
        }
    }
    enviar_paquete(paquete_io, socket_memoria);
    esperar_ok(socket_memoria, logger);
}

void manejar_interfaz_STDOUT(t_paquete *paquete_kernel)
{
    void* aux = paquete_kernel->buffer->stream + sizeof(uint32_t);

    t_paquete* paquete_memoria = crear_paquete_codigo_operacion(LECTURA);

    uint32_t tamanio = 0;
    memcpy(&tamanio, aux, sizeof(uint32_t));
    aux += sizeof(uint32_t);

    agregar_a_buffer(paquete_memoria->buffer, aux, sizeof(uint32_t)); // PID
    aux += sizeof(uint32_t);

    uint32_t size_nuevo_buffer = paquete_kernel->buffer->size - sizeof(uint32_t) * 2; 
    agregar_a_buffer(paquete_memoria->buffer, aux, size_nuevo_buffer); // PAGS A LEER | DFs

    enviar_paquete(paquete_memoria, socket_memoria);
    t_paquete *paquete_lectura = recibir_paquete(socket_memoria);

    char *buffer_texto = malloc(tamanio);
    memcpy(buffer_texto, paquete_lectura->buffer->stream, tamanio);

    log_trace(logger, "%s", mem_hexstring(buffer_texto, tamanio));
}

// ------------------------------------------------------------ //
// ------------------------ FILESYSTEM ------------------------ //    
// ------------------------------------------------------------ //            

void manejar_interfaz_DialFS(t_paquete *paquete, uint32_t pid) {
    usleep(tiempo_unidad_de_trabajo * 1000); 

    void* aux = paquete->buffer->stream + sizeof(uint32_t);
    size_t largo_string = 0;
    memcpy(&largo_string, aux, sizeof(size_t));
    aux += sizeof(size_t);
    char* nombre_archivo = malloc(largo_string);
    memcpy(nombre_archivo, aux, largo_string);
    aux += largo_string;

    switch (paquete->codigo_operacion) {
        case IO_FS_CREATE_OP_CODE: 
        {
            crear_archivo(pid, nombre_archivo);
            log_info(logger, "PID: %d - Crear Archivo: %s", pid, nombre_archivo);
            break;
        }
        case IO_FS_DELETE_OP_CODE: 
        {
            borrar_archivo(buscar_archivo(nombre_archivo));
            log_info(logger, "PID: %d - Eliminar Archivo: %s", pid, nombre_archivo);
            break;
        }
        case IO_FS_TRUNCATE_OP_CODE:
        {
            uint32_t tamanio_nuevo = 0;
            memcpy(&tamanio_nuevo, aux, sizeof(uint32_t));
            log_info(logger, "PID: %d - Truncar Archivo: %s - Tamaño: %d", pid, nombre_archivo, tamanio_nuevo);
            t_archivo* archivo = buscar_archivo(nombre_archivo);
            truncar_archivo(pid, tamanio_nuevo, archivo);
            break;
        }
        case IO_FS_WRITE_OP_CODE:
        {
            uint32_t tamanio = 0;
            memcpy(&tamanio, aux, sizeof(uint32_t));
            aux += sizeof(uint32_t);
            uint32_t ptr = 0;
            memcpy(&ptr, aux, sizeof(uint32_t));
            aux += sizeof(uint32_t);

            uint32_t size_nuevo_buffer = paquete->buffer->size - sizeof(size_t) - largo_string;
            t_paquete* p_envio_memoria = crear_paquete_codigo_operacion(LECTURA);
            agregar_a_buffer(p_envio_memoria->buffer, aux, size_nuevo_buffer);
            enviar_paquete(p_envio_memoria, socket_memoria);
            t_paquete* p_recib_memoria = recibir_paquete(socket_memoria);
            t_archivo* archivo = buscar_archivo(nombre_archivo);
            int bloque_local = ptr / block_size - 1; // ptr es puntero dentro del archivo
            if(bloque_local < 0) bloque_local = 0;
            int bloque_global = archivo->bloque_inicial + bloque_local;
            escribir_archivo_de_bloques(bloque_global, p_recib_memoria->buffer->stream, tamanio);

            eliminar_paquete(p_recib_memoria);
            log_info(logger, "PID: %d - Escribir Archivo: %s - Tamaño a Escribir: %d - Puntero Archivo: %d", pid, nombre_archivo, tamanio, ptr);
            break;
        }
        case IO_FS_READ_OP_CODE:
        {
            uint32_t tamanio = 0;
            memcpy(&tamanio, aux, sizeof(uint32_t));
            aux += sizeof(uint32_t);
            uint32_t ptr = 0;
            memcpy(&ptr, aux, sizeof(uint32_t));
            aux += sizeof(uint32_t);
            uint32_t pid = 0;
            memcpy(&pid, aux, sizeof(uint32_t));
            aux += sizeof(uint32_t);
            uint32_t pags_a_escribir = 0;
            memcpy(&pags_a_escribir, aux, sizeof(uint32_t));
            aux += sizeof(uint32_t); 
            // TAM | PTR | -- PID | PAGS A ESCRIBIR |-- DF
            t_archivo* archivo = buscar_archivo(nombre_archivo);
            int bloque_local = ptr / block_size - 1;
            if(bloque_local < 0) bloque_local = 0;
            int bloque_global = archivo->bloque_inicial + bloque_local;
            char* contenido = leer_archivo_de_bloques(bloque_global, tamanio);

            t_paquete* p_enviar_memoria = crear_paquete_codigo_operacion(ESCRITURA);
            agregar_a_buffer(p_enviar_memoria->buffer, &pid, sizeof(uint32_t));
            agregar_a_buffer(p_enviar_memoria->buffer, &pags_a_escribir, sizeof(uint32_t));
            solicitar_escritura_en_memoria(p_enviar_memoria, aux, pags_a_escribir, tamanio, contenido);
            free(contenido);
            log_info(logger, "PID: %d - Leer Archivo: %s - Tamaño a Leer: %d - Puntero Archivo: %d", pid, nombre_archivo, tamanio, ptr);
            break;
        }
        default:
        {
            log_error(logger, "Código desconocido");
            exit(EXIT_FAILURE);
        }

        free(nombre_archivo);
    }
}

void inicializar_estructuras_fs() {
    inicializar_archivo_de_bloques();
    inicializar_bitmap();
    archivos_abiertos = list_create();
    levantar_metadata_persistidos();
}

void inicializar_archivo_de_bloques() {
    tam_archivo_de_bloques = block_size * block_count;
    char* nombre_archivo_bloques = "/bloques.dat";
    size_t largo_path = strlen(path_base_dialfs) + strlen(nombre_archivo_bloques) + 1;
    char *path = malloc(largo_path);
    snprintf(path, largo_path, "%s%s", path_base_dialfs, nombre_archivo_bloques);

    fd_archivo_de_bloques = open(path, O_RDWR | O_CREAT, 0666);
    if (fd_archivo_de_bloques == -1) {
        log_error(logger, "Error al abrir bloques.dat");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(fd_archivo_de_bloques, tam_archivo_de_bloques) == -1) {
        perror("Error al truncar el archivo");
        exit(EXIT_FAILURE);
    }
    
    ptr_a_bloques = mmap(NULL, tam_archivo_de_bloques, PROT_READ | PROT_WRITE, MAP_SHARED, fd_archivo_de_bloques, 0);
    if (ptr_a_bloques == MAP_FAILED) {
        perror("Error al mapear archivo a memoria");
        exit(EXIT_FAILURE);
    }
}

void inicializar_bitmap() {
    char* nombre_archivo_bitmap = "/bitarray.dat";
    size_t largo_path = strlen(path_base_dialfs) + strlen(nombre_archivo_bitmap) + 1;
    char *path = malloc(largo_path);
    snprintf(path, largo_path, "%s%s", path_base_dialfs, nombre_archivo_bitmap);

    tamanio_bitmap = (int)ceil((double)block_count / 8); // redondea al proximo byte, pero usaremos solo el tamaño correspondiente
    fd_bitmap = open(path, O_RDWR | O_CREAT, 0666);
    if (fd_bitmap == -1) {
        log_error(logger, "Error al abrir bitarray.dat");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(fd_bitmap, tamanio_bitmap) == -1) {
        perror("Error al truncar el archivo");
        exit(EXIT_FAILURE);
    }
    
    puntero_bitmap = mmap(NULL, tamanio_bitmap, PROT_READ | PROT_WRITE, MAP_SHARED, fd_bitmap, 0);
    if (puntero_bitmap == MAP_FAILED) {
        perror("Error al mapear archivo a memoria");
        exit(EXIT_FAILURE);
    }
    
    bitarray_bloques = bitarray_create_with_mode(puntero_bitmap, tamanio_bitmap, LSB_FIRST);
    if (bitarray_bloques == NULL)
    {
        log_error(logger, "Error al crear el bitarray");
        exit(EXIT_FAILURE);
    }
    
    //log_trace(logger, "%s", mem_hexstring(puntero_bitmap, tamanio_bitmap));
}

void levantar_metadata_persistidos() {
    DIR *dir;
    struct dirent *entrada;
    size_t largo_directorio = strlen(path_base_dialfs) + strlen("/metadata") + 1;
    char *directory_path = malloc(largo_directorio);
    snprintf(directory_path, largo_directorio, "%s/metadata", path_base_dialfs);
    if ((dir = opendir(directory_path)) != NULL) {
        while ((entrada = readdir(dir)) != NULL) {
            // Saltear . y .. (current dir y parent dir)
            if (strcmp(entrada->d_name, ".") == 0 || strcmp(entrada->d_name, "..") == 0) {
                continue;
            }
            
            size_t largo_path = largo_directorio + strlen(entrada->d_name) + 1;
            char* file_path = malloc(largo_path); 
            snprintf(file_path, largo_path, "%s/%s", directory_path, entrada->d_name);
            
            // Abrir solo tipo de archivo común
            if (entrada->d_type == DT_REG) {
                FILE *file = fopen(file_path, "r");
                if(file) {
                    t_config* archivo_metadata_config = config_create(file_path);
                    t_archivo* nuevo_archivo = malloc(sizeof(t_archivo)); 
                    nuevo_archivo->config = archivo_metadata_config;
                    nuevo_archivo->nombre = strdup(entrada->d_name);
                    nuevo_archivo->path = strdup(file_path);
                    nuevo_archivo->bloque_inicial = config_get_int_value(archivo_metadata_config, "BLOQUE_INICIAL");
                    char bloque_inicial_str[5];
                    sprintf(bloque_inicial_str, "%d", nuevo_archivo->bloque_inicial);
                    dictionary_put(bloques_iniciales, bloque_inicial_str, (void*)nuevo_archivo);
                    nuevo_archivo->tamanio = config_get_int_value(archivo_metadata_config, "TAMANIO_ARCHIVO");
                    list_add(archivos_abiertos, nuevo_archivo); 
                    //log_trace(logger, "Metadata abierto: %s", nuevo_archivo->nombre);
                } else {
                    log_error(logger, "Error al abrir el archivo metadata");
                    exit(EXIT_FAILURE);
                }
                fclose(file);
            }
        }
        closedir(dir);
    } else {
        log_error(logger, "Error al abrir el directorio de archivos metadata");
        exit(EXIT_FAILURE);
    }
}

t_archivo* buscar_archivo(char* nombre) {
    t_archivo* a = NULL;
    for(int i = 0; i < list_size(archivos_abiertos); i++) {
        a = list_get(archivos_abiertos, i);
        if(!strcmp(a->nombre, nombre)) {
            return a;
        }
    }
    log_error(logger, "No existe el archivo solicitado");
    exit(EXIT_FAILURE);
}

void crear_archivo(uint32_t pid, char* nombre_archivo) {
    size_t largo_path = strlen(path_base_dialfs) + strlen(nombre_archivo) + 11;
    char *path = malloc(largo_path);
    sprintf(path, "%s/metadata/%s", path_base_dialfs, nombre_archivo);

    FILE* archivo_metadata = fopen(path, "w+"); 
    if (archivo_metadata == NULL)
    {
        log_error(logger, "Error al abrir el archivo %s", nombre_archivo);
        exit(EXIT_FAILURE);
    }
    fclose(archivo_metadata);

    t_config* archivo_metadata_config = config_create(path);
    t_archivo* nuevo_archivo = malloc(sizeof(t_archivo));
    nuevo_archivo->nombre = strdup(nombre_archivo);
    nuevo_archivo->path = strdup(path);
    nuevo_archivo->config = archivo_metadata_config;
    nuevo_archivo->tamanio = 0;
    nuevo_archivo->bloque_inicial = -1;
    asignar_bloques_contiguos(pid, 2 * block_size, nuevo_archivo); // para que el calculo de bloques nuevos dé 1
    char str[10];
    sprintf(str, "%d", nuevo_archivo->tamanio);
    config_set_value(nuevo_archivo->config, "TAMANIO_ARCHIVO", str);
    sprintf(str, "%d", nuevo_archivo->bloque_inicial);
    config_set_value(nuevo_archivo->config, "BLOQUE_INICIAL", str);
    config_save_in_file(nuevo_archivo->config, nuevo_archivo->path);
    list_add(archivos_abiertos, nuevo_archivo);
}

void cerrar_archivo(t_archivo* archivo) {
    char str[10];
    sprintf(str, "%d", archivo->bloque_inicial);
    config_set_value(archivo->config, "BLOQUE_INICIAL", str);
    sprintf(str, "%d", archivo->tamanio);
    config_set_value(archivo->config, "TAMANIO_ARCHIVO", str);
    config_save(archivo->config);
    config_destroy(archivo->config);
    list_remove_element(archivos_abiertos, archivo);
    log_trace(logger, "Archivo %s borrado", archivo->nombre);
    free(archivo->path);
    free(archivo);
}

void borrar_archivo(t_archivo* archivo) {
    sacar_bloques_bitmap(0, archivo);
    actualizar_bitarray();
    // eliminar metadata
    remove(archivo->path);
    // borrar struct
    config_destroy(archivo->config);
    list_remove_element(archivos_abiertos, archivo);
    free(archivo->path);
    free(archivo);
}

void actualizar_bitarray() {
    if (msync(puntero_bitmap, tamanio_bitmap, MS_SYNC) == -1) {
        perror("Error al sincronizar el archivo");
        munmap(puntero_bitmap, tamanio_bitmap);
        exit(EXIT_FAILURE);
    }
}

void actualizar_bloques() {
    if (msync(ptr_a_bloques, tam_archivo_de_bloques, MS_SYNC) == -1) { // TODO: todos los bloques o solo los modificados?
        perror("Error al sincronizar el archivo");
        munmap(ptr_a_bloques, tam_archivo_de_bloques);
        exit(EXIT_FAILURE);
    }
}

uint32_t buscar_bloque_libre() {
    for(uint32_t i = 0; i < block_count; i++) {
        if(bitarray_test_bit(bitarray_bloques, i) == 0) {
            return i;
        }
    }
    log_error(logger, "Filesystem lleno. No se puede crear/truncar archivo.");
    exit(EXIT_FAILURE);
}

uint32_t contar_bloques_libres() {
    uint32_t bloques_libres = 0;
    for(uint32_t i = 0; i < block_count; i++) {
        if(bitarray_test_bit(bitarray_bloques, i) == 0) {
            bloques_libres++;
        }
    }
    return bloques_libres;
}

void truncar_archivo(uint32_t pid, int tamanio_nuevo, t_archivo* archivo) {
    if(archivo->tamanio == tamanio_nuevo) 
        return;
    if (((int)archivo->tamanio - (int)tamanio_nuevo) < 0) 
        asignar_bloques_contiguos(pid, tamanio_nuevo, archivo);
    else
        sacar_bloques_bitmap(tamanio_nuevo, archivo);
    archivo->tamanio = tamanio_nuevo;
    actualizar_bitarray();

    // actualizar en archivo metadata
    char str[10];
    sprintf(str, "%d", archivo->tamanio);
    config_set_value(archivo->config, "TAMANIO_ARCHIVO", str);
    sprintf(str, "%d", archivo->bloque_inicial);
    config_set_value(archivo->config, "BLOQUE_INICIAL", str);
    config_save(archivo->config);
}

void asignar_bloques_contiguos(uint32_t pid, int tamanio_nuevo, t_archivo* archivo) {
    int tamanio_archivo = 0;
    if(archivo->tamanio == 0) tamanio_archivo = block_size;
    else tamanio_archivo = archivo->tamanio;
    int cant_bloques_nuevos = (int)ceil((double)(tamanio_nuevo - tamanio_archivo) / (double)block_size);
    int cant_bloques_asignados = (int)ceil((double)tamanio_archivo / (double)block_size);

    int bloque_inicial_contiguo = -1;
    int bloques_contiguos_encontrados = 0;
    int i = archivo->bloque_inicial + cant_bloques_asignados; // bloque siguiente al ultimo del archivo
    if(archivo->bloque_inicial == -1) i = buscar_bloque_libre(); // si no tiene bloques

    // busco bloques libres contiguos 
    while(bloques_contiguos_encontrados < cant_bloques_nuevos && i < block_count) {
        if(bitarray_test_bit(bitarray_bloques, i) == 0) {
            if(bloque_inicial_contiguo == -1) bloque_inicial_contiguo = i;
            bloques_contiguos_encontrados++;
        } 
        else break;
        i++;
    }

    if(bloques_contiguos_encontrados == cant_bloques_nuevos) {
        int cant_bloques_nuevos_aux = cant_bloques_nuevos;
        int bloque_contiguo_aux = bloque_inicial_contiguo;

        // marco bloques como ocupados en bitarray (bloques que asigno al archivo)
        while(cant_bloques_nuevos_aux > 0) {
            bitarray_set_bit(bitarray_bloques, bloque_contiguo_aux);
            bloque_contiguo_aux++;
            cant_bloques_nuevos_aux--;
        }

        // marco en el archivo su nuevo bloque inicial
        if(archivo->bloque_inicial == -1) {
            archivo->bloque_inicial = bloque_inicial_contiguo;
            char bloque_inicial_nuevo_str[5];
            sprintf(bloque_inicial_nuevo_str, "%d", bloque_inicial_contiguo);
            dictionary_put(bloques_iniciales, bloque_inicial_nuevo_str, (void*)archivo);
        }
    } 
    else {
        log_info(logger, "PID: %d - Inicio Compactación", pid);
        if(contar_bloques_libres() < cant_bloques_nuevos) {
            log_error(logger, "Filesystem lleno. No se puede truncar el archivo.");
            exit(EXIT_FAILURE);
        }
        compactar_fs(archivo);
        log_info(logger, "PID: %d - Fin Compactación", pid);
        asignar_bloques_contiguos(pid, tamanio_nuevo, archivo); 
    }
}

void sacar_bloques_bitmap(int tamanio_nuevo, t_archivo* archivo) {
    int cant_bloques_a_liberar = (int)ceil((double)(archivo->tamanio - tamanio_nuevo) / (double)block_size);
    int posicion_ultimo_bloque = archivo->bloque_inicial + (int)ceil((double)archivo->tamanio / (double)block_size) - 1;

    for (int i = posicion_ultimo_bloque; cant_bloques_a_liberar > 0; cant_bloques_a_liberar--, i--)
    {
        bitarray_clean_bit(bitarray_bloques, i);
        log_trace(logger, "Bloque %d liberado", i);
        // si elimino su bloque inicial, lo marco como -1 (no asignado)
        if (i == archivo->bloque_inicial) archivo->bloque_inicial = -1; 
    }
}

void compactar_fs(t_archivo* archivo_a_agrandar) {
    usleep(retraso_compactacion * 1000);

    void* contenido = malloc(block_count * block_size);
    void* aux = contenido;
    t_list* archivos = list_create(); // Lista ordenada por bloque inicial (distinta de lista de archivos abiertos)
    int bloques_a_mover = 0;

    // Cargar buffer con contenido de todos los bloques utilizados. Lleno lista de archivos por orden de aparición.
    int i = 0;
    int tamanio_archivo = 0;
    while(i < block_count) { 
        //log_trace(logger, "Bloque %d", i);
        if(bitarray_test_bit(bitarray_bloques, i) == 1) {
            t_archivo* archivo = NULL;
            char str[10];
            sprintf(str, "%d", i);
            archivo = (t_archivo*)dictionary_get(bloques_iniciales, str);
            //log_trace(logger, "Archivo %s", archivo->nombre);
            if(archivo->tamanio == 0) tamanio_archivo = block_size;
            else tamanio_archivo = archivo->tamanio;

            if(i != archivo_a_agrandar->bloque_inicial) {
                list_add(archivos, archivo); // para reasignar bloques iniciales mas adelante
                memcpy(aux, ptr_a_bloques + i * block_size, tamanio_archivo); 
                aux += tamanio_archivo;
                //log_trace(logger, "Bloque copiado");
            }
            else {
                archivo = archivo_a_agrandar;
            } 
            i += (int)ceil((double)tamanio_archivo / (double)block_size); // avanzar a siguiente archivo
            bloques_a_mover += (int)ceil((double)tamanio_archivo / (double)block_size);
        }
        else {
            //log_trace(logger, "Bloque libre");
            i++;
        }
    }

    list_add(archivos, archivo_a_agrandar);
    if(archivo_a_agrandar->tamanio == 0) memcpy(aux, ptr_a_bloques + archivo_a_agrandar->bloque_inicial * block_size, block_size); 
    else memcpy(aux, ptr_a_bloques + archivo_a_agrandar->bloque_inicial * block_size, archivo_a_agrandar->tamanio);
    // Limpio bitarray y marco ocupado desde el inicio hasta la cantidad de bloques utilizados.
    
    for(int i = 0; i < block_count; i++) {
        bitarray_clean_bit(bitarray_bloques, i);
    }
    for(int i = 0; i < bloques_a_mover; i++) {
        bitarray_set_bit(bitarray_bloques, i);
    }
    log_trace(logger, "%s", mem_hexstring(puntero_bitmap, tamanio_bitmap));
    actualizar_bitarray();

    // Escribir todos los bloques compactados desde el inicio del archivo.
    memcpy(ptr_a_bloques, contenido, block_count * block_size); 
    actualizar_bloques();

    // Asignar nuevo bloque inicial.
    dictionary_clean(bloques_iniciales);
    uint32_t nuevo_bloque_inicial = 0;
    for(int i = 0; i < list_size(archivos); i++) {
        //log_trace(logger, "Asignando nuevo bloque inicial %d", nuevo_bloque_inicial);
        t_archivo* archivo = list_get(archivos, i);
        archivo->bloque_inicial = nuevo_bloque_inicial;
        if(archivo->tamanio == 0) tamanio_archivo = block_size;
        else tamanio_archivo = archivo->tamanio;
        nuevo_bloque_inicial += (int)ceil((double)tamanio_archivo / (double)block_size); 

        char bloque_inicial_nuevo_str[5];
        sprintf(bloque_inicial_nuevo_str, "%d", archivo->bloque_inicial);
        dictionary_put(bloques_iniciales, bloque_inicial_nuevo_str, archivo);
        config_set_value(archivo->config, "BLOQUE_INICIAL", bloque_inicial_nuevo_str);
        config_save(archivo->config);
    }
}

void escribir_archivo_de_bloques(int bloque, void* contenido, uint32_t tamanio) {
    uint32_t bloques_a_escribir = ceil((double) tamanio / (double) block_size);
    memcpy(ptr_a_bloques + bloque * block_size, contenido, tamanio); 
    log_trace(logger, "%s", mem_hexstring(ptr_a_bloques + bloque * block_size, bloques_a_escribir * block_size));
    actualizar_bloques();
}

char* leer_archivo_de_bloques(int bloque, uint32_t tamanio) {
    uint32_t bloques_a_leer = ceil((double) tamanio / (double) block_size);
    void* contenido = malloc(bloques_a_leer * block_size);
    memcpy(contenido, ptr_a_bloques + bloque * block_size, bloques_a_leer * block_size); 
    log_trace(logger, "%s", mem_hexstring(ptr_a_bloques + bloque * block_size, bloques_a_leer * block_size));
    //log_trace(logger, "%s", mem_hexstring(contenido, bloques_a_leer * block_size));
    return contenido;
}
