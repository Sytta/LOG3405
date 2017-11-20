#undef UNICODE

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string>

// Link avec ws2_32.lib
#pragma comment(lib, "ws2_32.lib")

SOCKET leSocket;

#define LONGEUR_MSG 200

// External functions
extern DWORD WINAPI MessageRecvHandler(void* sd_);
extern bool isValidIP(char* IP);

int __cdecl main(int argc, char **argv)
{
    WSADATA wsaData;
    struct addrinfo *result = NULL,
                    *ptr = NULL,
                    hints;
    char motEnvoye[LONGEUR_MSG];
	int iResult;

	//--------------------------------------------
    // Initialisation de Winsock
    iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0) {
        printf("Erreur de WSAStartup: %d\n", iResult);
        return 1;
    }

	// On va creer le socket pour communiquer avec le serveur
	leSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (leSocket == INVALID_SOCKET) {
        printf("Erreur de socket(): %ld\n\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
		printf("Appuyez une touche pour finir\n");
		getchar();
        return 1;
	}

	//--------------------------------------------
	// On va chercher l'adresse du serveur en utilisant la fonction getaddrinfo.
    ZeroMemory( &hints, sizeof(hints) );
    hints.ai_family = AF_INET;        // Famille d'adresses
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;  // Protocole utilisé par le serveur

	// On indique le nom et le port du serveur auquel on veut se connecter
	char host[15];
	char port[5];

	do {
		std::string tmp;
		std::cout << "Entrez l'adresse du serveur avec lequel vous voulez communiquer : ";
		std::cin >> tmp;
		std::cin.get();
		strcpy(host, tmp.c_str());
		if (!isValidIP(host))
			std::cout << "L'adresse IP est invalide! Reessayez s'il-vous-plait." << std::endl;
	} while (!isValidIP(host));

	bool portIsValid = false;
	do {
		std::string tmp;
		std::cout << "Entrez le port du serveur avec lequel vous voulez communiquer : ";
		std::cin >> tmp;
		std::cin.get();
		
		if (tmp.find_first_not_of("0123456789") != std::string::npos || std::stoi(tmp) > 5050 || std::stoi(tmp) < 5000) {
			std::cout << "Le port est invalide! Le port doit etre entre 5000 et 5050. Reessayez s'il-vous-plait." << std::endl;
			portIsValid = false;
		}
		else {
			portIsValid = true;
			strcpy(port, tmp.c_str());
		}
	} while (!portIsValid);

	//For local testing
	//char *host = "127.0.0.1";
	//char *port = "5040";

	// getaddrinfo obtient l'adresse IP du host donné
    iResult = getaddrinfo(host, port, &hints, &result);
    if ( iResult != 0 ) {
        printf("Erreur de getaddrinfo: %d\n", iResult);
        WSACleanup();
        return 1;
    }

	//---------------------------------------------------------------------		
	//On parcours les adresses retournees jusqu'a trouver la premiere adresse IPV4
	while((result != NULL) &&(result->ai_family!=AF_INET))   
			 result = result->ai_next; 

//	if ((result != NULL) &&(result->ai_family==AF_INET)) result = result->ai_next;  
	
	//-----------------------------------------
	if (((result == NULL) ||(result->ai_family!=AF_INET))) {
		freeaddrinfo(result);
		printf("Impossible de recuperer la bonne adresse\n\n");
        WSACleanup();
		printf("Appuyez une touche pour finir\n");
		getchar();
        return 1;
	}

	sockaddr_in *adresse;
	adresse=(struct sockaddr_in *) result->ai_addr;
	//----------------------------------------------------
	printf("Adresse trouvee pour le serveur %s : %s\n\n", host,inet_ntoa(adresse->sin_addr));
	printf("Tentative de connexion au serveur %s avec le port %s\n\n", inet_ntoa(adresse->sin_addr),port);
	
	// On va se connecter au serveur en utilisant l'adresse qui se trouve dans
	// la variable result.
	iResult = connect(leSocket, result->ai_addr, (int)(result->ai_addrlen));
	if (iResult == SOCKET_ERROR) {
        printf("Impossible de se connecter au serveur %s sur le port %s\n\n", inet_ntoa(adresse->sin_addr),port);
        freeaddrinfo(result);
        WSACleanup();
		printf("Appuyez une touche pour finir\n");
		getchar();
        return 1;
	}

	// Authentification avec le serveur
	char username[LONGEUR_MSG] = "";
	char password[LONGEUR_MSG] = "";
	bool connectionAccepted = false;

	do {
		std::cout << "username: ";
		gets_s(username);
		iResult = send(leSocket, username, strlen(username) + 1, 0);
		if (iResult == SOCKET_ERROR) {
			printf("Erreur du send: %d\n", WSAGetLastError());
			closesocket(leSocket);
			WSACleanup();
			printf("Appuyez une touche pour finir\n");
			getchar();
			return 1;
		}

		std::cout << "password: ";
		gets_s(password);
		iResult = send(leSocket, password, strlen(password) + 1, 0);
		if (iResult == SOCKET_ERROR) {
			printf("Erreur du send: %d\n", WSAGetLastError());
			closesocket(leSocket);
			WSACleanup();
			printf("Appuyez une touche pour finir\n");
			getchar();
			return 1;
		}

		// Attendre la réponse du serveur
		char readBuffer[LONGEUR_MSG + 1] = "";
		int readBytes;
		readBytes = recv(leSocket, readBuffer, LONGEUR_MSG, 0);
		if (readBytes > 0) {
			// Accepter ou rejeter le client selon la réponse du serveur
			connectionAccepted = (int)readBuffer[0] - 48;  // Conversion inspired by : https://stackoverflow.com/questions/27021039/how-to-convert-a-char-numeric-0-9-to-int-without-getnumericalvalue
			if (!connectionAccepted) {
				printf("Erreur d'authentification. Veuillez recommencer.\n");
			}
		}
		else {
			// Erreur : Fermer la connexion
			printf("Erreur de connexion au serveur.");
			closesocket(leSocket);
			WSACleanup();
			printf("Appuyez une touche pour finir\n");
			getchar();
			return 1;
		}

	} while (!connectionAccepted);

	printf("Connecte au serveur %s:%s\n\n", host, port);
    freeaddrinfo(result);

	std::cout << "Bienvenue!" << std::endl;

	// Creer le thread pour recevoir des messages
	DWORD msRecvTheadID;
	CreateThread(0, 0, MessageRecvHandler, NULL, 0, &msRecvTheadID);

	//----------------------------
	// Demander à l'usager un mot a envoyer au serveur
	//-----------------------------
	gets_s(motEnvoye);

	// Envoyer des messages au serveur tant que le mssage n'est pas vide
	while (std::string(motEnvoye).size() > 0 ) {
		
		iResult = send(leSocket, motEnvoye, strlen(motEnvoye) + 1, 0);
		if (iResult == SOCKET_ERROR) {
			//printf("Erreur du send: %d\n", WSAGetLastError());
			std::cout << "Au revoir! " << std::endl;
			closesocket(leSocket);
			WSACleanup();
			printf("Appuyez une touche pour finir\n");
			getchar();
			return 1;
		}

		gets_s(motEnvoye);
	}

	iResult = shutdown(leSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		printf("shutdown failed: %d\n", WSAGetLastError());
		closesocket(leSocket);
		WSACleanup();
		return 1;
	}

    // cleanup
    closesocket(leSocket);
    WSACleanup();

	printf("Appuyez une touche pour finir\n");
	getchar();
    return 0;
}

// Verifier la validite de l'IP
// Inspiration: https://stackoverflow.com/questions/791982/determine-if-a-string-is-a-valid-ip-address-in-c
bool isValidIP(char *IP)
{
	struct sockaddr_in iptest;
	int result = inet_pton(AF_INET, IP, &(iptest.sin_addr));
	return result != 0;
}

DWORD WINAPI MessageRecvHandler(void* sd_) 
{
	int iResult;
	char motRecu[LONGEUR_MSG + 1] = "";

	while (true) {
		iResult = recv(leSocket, motRecu, LONGEUR_MSG, 0);
		if (iResult > 0) {
			//printf("Nombre d'octets recus: %d\n", iResult);
			motRecu[iResult] = '\0';
			printf("%s", motRecu);
		}
		else {
			//printf("Erreur de reception : %d\n", WSAGetLastError());
			std::cout << "Au revoir! " << std::endl;
			return 0;
		}
	}
}