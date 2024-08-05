#include "socket.h"

extern t_log* logger;

int start_client(char* ip, char* port)
{
	struct addrinfo hints, *server_info;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	/*LLAMADA AL SISTEMA que devuelve informacion de RED sobre la IP
	 * y el PUERTO que le pasemos, en este caso del Servidor. Inyecta en
	 * variable server_info los datos necesarios para la creacion del Socket.*/
	getaddrinfo(ip, port, &hints, &server_info);

	// Ahora vamos a crear el socket Cliente.
	int socket_client = socket(server_info->ai_family,
                            server_info->ai_socktype,
						    server_info->ai_protocol);

	// Ahora que tenemos el socket, vamos a conectarlo.
	if(connect(socket_client, server_info->ai_addr, server_info->ai_addrlen) == -1)
	{
		return -1;
	}
	// nuestros procesos ya estan conectados

	freeaddrinfo(server_info);

	return socket_client;
}

int start_server(char* ip, char* port) {
	int socket_servidor; //Guarda el File Descriptor(IDs) representado por un entero.

	struct addrinfo hints, *servinfo; // Estruc q Contendra información sobre la dirección de un proveedor de servicios.

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	getaddrinfo(ip, port, &hints, &servinfo); //Traduce el nombre de una ubicación de servicio a un conjunto de direcciones de socket.

	// Creamos el socket de escucha del servidor
	socket_servidor = socket(servinfo->ai_family,
							 servinfo->ai_socktype,
							 servinfo->ai_protocol);

	/* bind() y listen() son las llamadas al sistema que realiza
	 * la preparacion por parte del proceso servidor */

	int activado = 1;
	setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEADDR, &activado, sizeof(activado));

	// 1) Asociamos el socket creado a un puerto
	bind(socket_servidor,servinfo->ai_addr, servinfo->ai_addrlen);

	// 2) Escuchamos las conexiones entrantes a ese socket, cuya unica responsabilidad
	// es notificar cuando un nuevo cliente este intentando conectarse.
	listen(socket_servidor,SOMAXCONN); // El servidor esta listo para recibir a los clientes (ESTA ESCUCHANDO).
	freeaddrinfo(servinfo);
	return socket_servidor;
}

void get_ip_port_from_module(const char* module, char* path_config, char* ip, char* port)
{
	t_config* config = config_create(path_config);

	char* config_ip_key = string_from_format("IP_%s", module);
	char* config_port_key = string_from_format("PUERTO_%s", module);

	strcpy(ip, config_get_string_value(config, config_ip_key));
	strcpy(port, config_get_string_value(config, config_port_key));

	free(config_ip_key);
	free(config_port_key);

	config_destroy(config);
}

int start_client_module(char* module, char* config) {
	char* ip = malloc(sizeof(char) * 20);
	char* port = malloc(sizeof(char) * 20);

	get_ip_port_from_module(module, config, ip, port);

	//log_rect(logger, "Creo socket cliente", "MODULO [%s]\n IP [%s]\n PUERTO [%s]", module, ip, port);

	//log_info(logger, "Creacion de socket cliente -> MODULO: %s - IP: %s - PUERTO: %s", module, ip, port);

	int socket_client = start_client(ip, port);

	if(socket_client == -1)
	{
		log_error(logger, "No se puede establecer conexion con el modulo %s", module);
		exit(EXIT_FAILURE);
	}

	free(ip);
	free(port);

	return socket_client;
}

int start_server_module(char* module, char* config) {
	char* ip = malloc(sizeof(char) * 20);
	char* port = malloc(sizeof(char) * 20);

	get_ip_port_from_module(module, config, ip, port);
	//log_info(logger, "Creacion de socket servidor -> IP: %s - PUERTO: %s", ip, port);

	// log_trace(logger, "Inicializando servidor en IP: %s PUERTO: %s", ip, port);
	int socket_server = start_server(ip, port);

	free(ip);
	free(port);

	return socket_server;
}

void handshake(int socket) {
	t_buffer* buffer = crear_buffer();

	agregar_op_code(buffer, HANDSHAKE);
	enviar_buffer(socket, buffer);

	uint32_t codigo = 0;
	recv(socket, &codigo, sizeof(codigo), MSG_WAITALL);

	if(codigo == OK) {
		log_trace(logger, "Respuesta a HANDSHAKE: OK");
	}else {
		log_trace(logger, "HANDSHAKE rechazado");
		exit(1);
	}

	liberar_buffer(buffer);
}