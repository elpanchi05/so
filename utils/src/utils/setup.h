#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <commons/log.h>
#include <commons/config.h>

void initialize_setup(char *config_path, char* modulo, t_log** logger, t_config** config);
void initialize_logger(t_log** logger, char* modulo);
void initialize_config(char *config_path, t_config** config, t_log** logger);