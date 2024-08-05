#include "setup.h"

void initialize_setup(char *config_path, char* modulo, t_log** logger, t_config** config)
{
    initialize_logger(logger, modulo);
    initialize_config(config_path, config, logger);
}

void initialize_logger(t_log** logger, char* modulo)
{
    size_t largo = strlen(modulo) + strlen(".log") + 1;
    char* log_file = (char*)malloc(largo);
    strcpy(log_file, modulo);
	strcat(log_file, ".log");

    *logger = log_create(log_file, modulo, true, LOG_LEVEL_TRACE);
    log_info(*logger, "Logger iniciado");
    free(log_file);
}

void initialize_config(char *config_path, t_config** config, t_log** logger)
{
	*config = config_create(config_path);
	if(*config == NULL) {
		log_error(*logger, "No se pudo abrir la config");
        exit(EXIT_FAILURE);
	}
	log_info(*logger, "Configuracion cargada");
}