/*
 * main.c
 *
 * UDP Server - Template for Computer Networks assignment
 *
 * This file contains the boilerplate code for a UDP server
 * portable across Windows, Linux, and macOS.
 */

#if defined WIN32
#include <winsock.h>
typedef int socklen_t;
#else
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#define closesocket close
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <stdint.h>
#include "protocol.h"

#ifndef NO_ERROR
#define NO_ERROR 0
#endif

void clearwinsock() {
#if defined WIN32
	WSACleanup();
#endif
}

int is_valid_city(const char *city) {
    const char* available_cities[] = {"bari", "roma", "milano", "napoli", "torino", "palermo", "genova", "bologna", "firenze", "venezia"};
    size_t n = sizeof(available_cities) / sizeof(available_cities[0]);

    for (size_t i = 0; i < n; i++) {
        #if defined WIN32
        if (_stricmp(city, available_cities[i]) == 0) {
        #else
        if (strcasecmp(city, available_cities[i]) == 0) {
        #endif
            return 1;
        }
    }
    return 0;
}

int is_valid_type(char type) {
    char lower_type = tolower(type);
    return (lower_type == 't' || lower_type == 'h' || lower_type == 'w' || lower_type == 'p');
}


int has_invalid_characters(const char *str) {
    int i;
    for (i = 0; str[i] != '\0'; i++) {

        if (!isalnum((unsigned char)str[i]) && str[i] != ' ') {
            return 1;
        }
    }
    return 0;
}


float get_temperature(void) { return -10.0 + (rand() / (float)RAND_MAX) * 50.0; }
float get_humidity(void) { return 20.0 + (rand() / (float)RAND_MAX) * 80.0; }
float get_wind(void) { return (rand() / (float)RAND_MAX) * 100.0; }
float get_pressure(void) { return 950.0 + (rand() / (float)RAND_MAX) * 100.0; }

int main(int argc, char *argv[]) {
	srand(time(NULL));

	int port = SERVER_PORT;

	if (argc == 3 && strcmp(argv[1], "-p") == 0) {
        port = atoi(argv[2]);
        if (port <= 0 || port > 65535) {
            printf("Errore: numero di porta non valido (deve essere tra 1-65535)\n");
            return 1;
        }
	} else if (argc != 1) {
	    printf("Uso: %s [-p port]\n", argv[0]);
	    return 1;
	}

#if defined WIN32
	WSADATA wsa_data;
	int result = WSAStartup(MAKEWORD(2,2), &wsa_data);
	if (result != NO_ERROR) {
		printf("Errore in WSAStartup()\n");
		return 0;
	}
#endif


	int my_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (my_socket < 0) {
		printf("Creazione socket fallita.\n");
	    clearwinsock();
	    return 1;
	}

	struct sockaddr_in sad;
	memset(&sad, 0, sizeof(sad));
	sad.sin_family = AF_INET;
	sad.sin_addr.s_addr = INADDR_ANY;
	sad.sin_port = htons(port);

	if (bind(my_socket, (struct sockaddr*) &sad, sizeof(sad)) < 0) {
	    printf("Errore: bind() fallita\n");
	    closesocket(my_socket);
	    clearwinsock();
	    return 1;
	}

	printf("Server in ascolto sulla porta %d...\n", port);

	struct sockaddr_in client_addr;
	socklen_t client_len;

	while (1) {
	    client_len = sizeof(client_addr);
	    weather_request_t request;


	    int bytes_rcvd = recvfrom(my_socket, (char*)&request, sizeof(request), 0, (struct sockaddr*)&client_addr, &client_len);

	    if (bytes_rcvd < 0) {
	        printf("Errore: recvfrom() fallita\n");
	        continue;
	    }


	    struct hostent *client_host;
	    char *client_ip = inet_ntoa(client_addr.sin_addr);
	    char client_name[256];

	    client_host = gethostbyaddr((const char*)&client_addr.sin_addr, sizeof(client_addr.sin_addr), AF_INET);
	    if (client_host != NULL) {
	        strncpy(client_name, client_host->h_name, 255);
	        client_name[255] = '\0';
	    } else {
	        strcpy(client_name, client_ip);
	    }


	    request.city[sizeof(request.city) - 1] = '\0';
	    printf("Richiesta ricevuta da %s (ip %s): type='%c', city='%s'\n",
	            client_name, client_ip, request.type, request.city);


	    weather_response_t response;
	    char req_type = (char) tolower((unsigned char) request.type);


        if (!is_valid_type(req_type)) {
            response.status = STATUS_INVALID_REQUEST;
            response.type = request.type;
            response.value = 0.0f;
        } else if (has_invalid_characters(request.city)) {
            response.status = STATUS_INVALID_REQUEST;
            response.type = req_type;
            response.value = 0.0f;
        } else if (!is_valid_city(request.city)) {
            response.status = STATUS_CITY_NOT_FOUND;
            response.type = req_type;
            response.value = 0.0f;
        } else {
            response.status = STATUS_OK;
            response.type = req_type;
            switch (req_type) {
                case 't': response.value = get_temperature(); break;
                case 'h': response.value = get_humidity(); break;
                case 'w': response.value = get_wind(); break;
                case 'p': response.value = get_pressure(); break;
                default: response.status = STATUS_INVALID_REQUEST; break;
            }
        }

        char buffer[sizeof(uint32_t) + sizeof(char) + sizeof(float)];
        int offset = 0;

        uint32_t net_status = htonl(response.status);
        memcpy(buffer + offset, &net_status, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        memcpy(buffer + offset, &response.type, sizeof(char));
        offset += sizeof(char);

        uint32_t net_value_int;
        memcpy(&net_value_int, &response.value, sizeof(float));
        net_value_int = htonl(net_value_int);
        memcpy(buffer + offset, &net_value_int, sizeof(float));
        offset += sizeof(float);

	    sendto(my_socket, buffer, offset, 0, (struct sockaddr*)&client_addr, client_len);
	}

	closesocket(my_socket);
	clearwinsock();
	return 0;
}
