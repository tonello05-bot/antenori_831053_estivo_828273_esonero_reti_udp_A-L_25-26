/*
 * main.c
 *
 * UDP Client - Template for Computer Networks assignment
 *
 * This file contains the boilerplate code for a UDP client
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

int main(int argc, char *argv[]) {

    char *server_address_str = "localhost";
    int port = SERVER_PORT;
    char *request_str = NULL;


    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            server_address_str = argv[i + 1];
            i++;
        }
        else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = atoi(argv[i + 1]);
            if (port <= 0 || port > 65535) {
                printf("Errore: numero di porta non valido\n");
                return 1;
            }
            i++;
        }
        else if (strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
            request_str = argv[i + 1];
            i++;
        }
        else {
            printf("Utilizzo: %s [-s server] [-p port] -r \"tipo citta\"\n", argv[0]);
            return 1;
        }
    }

    if (request_str == NULL) {
        printf("Errore: -r è obbligatorio!\n");
        printf("Uso: %s [-s server] [-p port] -r \"tipo citta\"\n", argv[0]);
        return 1;
    }


    char *space_ptr = strchr(request_str, ' ');
    if (space_ptr == NULL) {
        printf("Errore: formato richiesta non valido. Uso: \"t citta\"\n");
        return 1;
    }


    size_t type_len = space_ptr - request_str;
    if (type_len != 1) {
        printf("Errore: il tipo richiesta deve essere un singolo carattere.\n");
        return 1;
    }

    char type = request_str[0];
    char *city_ptr = space_ptr + 1;


    if (strlen(city_ptr) > 63) {
        printf("Errore: nome città troppo lungo (max 63 caratteri).\n");
        return 1;
    }


    if (strchr(request_str, '\t') != NULL) {
        printf("Errore: caratteri di tabulazione non ammessi.\n");
        return 1;
    }

#if defined WIN32
	WSADATA wsa_data;
	int result = WSAStartup(MAKEWORD(2,2), &wsa_data);
	if (result != NO_ERROR) {
		printf("Errore nella funzione WSAStartup()\n");
		return 0;
	}
#endif


    struct hostent *host;
    struct sockaddr_in sad;
    memset(&sad, 0, sizeof(sad));
    sad.sin_family = AF_INET;
    sad.sin_port = htons(port);


    unsigned long ip_addr = inet_addr(server_address_str);
    if (ip_addr != INADDR_NONE) {
        sad.sin_addr.s_addr = ip_addr;
    } else {

        host = gethostbyname(server_address_str);
        if (host == NULL) {
            printf("Errore: impossibile risolvere il nome del server %s\n", server_address_str);
            clearwinsock();
            return 1;
        }
        memcpy(&sad.sin_addr, host->h_addr, host->h_length);
    }


	int c_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (c_socket < 0) {
	    printf("Creazione della socket fallita\n");
	    clearwinsock();
	    return 1;
	}


	weather_request_t request;
	memset(&request, 0, sizeof(request));
	request.type = type;
	strncpy(request.city, city_ptr, 63);


	if (sendto(c_socket, (char*)&request, sizeof(request), 0, (struct sockaddr*)&sad, sizeof(sad)) < 0) {
	    printf("Errore: funzione sendto() fallita\n");
	    closesocket(c_socket);
	    clearwinsock();
	    return 1;
	}


    char buffer[sizeof(uint32_t) + sizeof(char) + sizeof(float)];
    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);

	int bytes_rcvd = recvfrom(c_socket, buffer, sizeof(buffer), 0, (struct sockaddr*)&from_addr, &from_len);

	if (bytes_rcvd <= 0) {
	    printf("Errore: funzione recvfrom() fallita o timeout\n");
	    closesocket(c_socket);
	    clearwinsock();
	    return 1;
	}


    weather_response_t response;
    int offset = 0;


    uint32_t net_status;
    memcpy(&net_status, buffer + offset, sizeof(uint32_t));
    response.status = ntohl(net_status);
    offset += sizeof(uint32_t);


    memcpy(&response.type, buffer + offset, sizeof(char));
    offset += sizeof(char);


    uint32_t net_value_int;
    memcpy(&net_value_int, buffer + offset, sizeof(float));
    net_value_int = ntohl(net_value_int);
    memcpy(&response.value, &net_value_int, sizeof(float));


    char server_name[256];
    char *server_ip_str = inet_ntoa(from_addr.sin_addr);
    struct hostent *remote_host = gethostbyaddr((const char*)&from_addr.sin_addr, sizeof(from_addr.sin_addr), AF_INET);

    if (remote_host != NULL) {
        strncpy(server_name, remote_host->h_name, 255);
        server_name[255] = '\0';
    } else {
        strcpy(server_name, server_ip_str);
    }


	if (response.status == STATUS_OK) {
        printf("Ricevuto risultato dal server %s (ip %s). %s: ", server_name, server_ip_str, request.city);

	    switch (response.type) {
	        case 't':
	            printf("Temperatura = %.1f°C\n", response.value);
	            break;
	        case 'h':
	            printf("Umidità = %.1f%%\n", response.value);
	            break;
	        case 'w':
	            printf("Vento = %.1f km/h\n", response.value);
	            break;
	        case 'p':
	            printf("Pressione = %.1f hPa\n", response.value);
	            break;
	        default:
	            printf("Tipo sconosciuto\n");
	            break;
	    }
	} else {

        printf("Ricevuto risultato dal server %s (ip %s). ", server_name, server_ip_str);
	    switch (response.status) {
	        case STATUS_CITY_NOT_FOUND:
	            printf("Città non disponibile\n");
	            break;
	        case STATUS_INVALID_REQUEST:
	            printf("Richiesta non valida\n");
	            break;
	        default:
	            printf("Errore sconosciuto\n");
	            break;
	    }
	}

	closesocket(c_socket);
	clearwinsock();
	return 0;
}
