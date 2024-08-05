#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <commons/log.h>
#include <commons/config.h>
#include <commons/string.h>
#include "structures.h"
#include "paquete.h"

int start_client_module(char* module, char* config);
int start_server_module(char* module, char* config);
void handshake(int socket);