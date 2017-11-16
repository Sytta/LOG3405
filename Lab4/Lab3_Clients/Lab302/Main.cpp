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
	// TODO: Set using user input (IP and port for the server)
	//char *host = "L4708-XX";
	//char *host = "L4708-XX.lerb.polymtl.ca";
	//char *host = "add_IP locale";
	/*char host[15];
	char port[5];

	do {
		std::string tmp;
		std::cout << "Entrez l'adresse du serveur avec lequel vous voulez communiquer : ";
		std::cin >> tmp;
		std::cin.get();
		strcpy(host, tmp.c_str());
	} while (!isValidIP(host));

	do {
		std::string tmp;
		std::cout << "Entrez le port du serveur avec lequel vous voulez communiquer : ";
		std::cin >> tmp;
		std::cin.get();
		strcpy(port, tmp.c_str());
	} while (std::stoi(port) > 5050 || std::stoi(port) < 5000);*/

	//char *host = "132.207.29.123";
	char *host = "127.0.0.1";
	char *port = "5040";

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
	char username[LONGEUR_MSG];
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
	char password[LONGEUR_MSG];
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
	char readBuffer[LONGEUR_MSG + 1];
	int readBytes;
	readBytes = recv(leSocket, readBuffer, LONGEUR_MSG, 0);
	if (readBytes > 0) {
		// Accepter ou rejeter le client selon la réponse du serveur
		bool connectionAccepted = (int) readBuffer[0] - 48;  // Conversion inspired by : https://stackoverflow.com/questions/27021039/how-to-convert-a-char-numeric-0-9-to-int-without-getnumericalvalue
		if (!connectionAccepted) {
			// Shutdown client
			printf("Erreur d'authentification.\n");
			closesocket(leSocket);
			WSACleanup();
			printf("Appuyez une touche pour finir\n");
			getchar();
			return 1;
		}
	} else {
		// Erreur : Fermer la connexion
		printf("Erreur de connexion au serveur.");
		closesocket(leSocket);
		WSACleanup();
		printf("Appuyez une touche pour finir\n");
		getchar();
		return 1;
	}

	printf("Connecte au serveur %s:%s\n\n", host, port);
    freeaddrinfo(result);

	// Creer le thread pour recevoir des messages
	DWORD msRecvTheadID;
	CreateThread(0, 0, MessageRecvHandler, NULL, 0, &msRecvTheadID);

	//----------------------------
	// Demander à l'usager un mot a envoyer au serveur
	//-----------------------------

	std::cout << "Enjoy votre chat!" << std::endl;
	std::cout << "Me: ";
	gets_s(motEnvoye);
	//printf("Le mot envoye est: %s\n", motEnvoye);

	while (std::string(motEnvoye).size() > 0 ) {
		
		// Envoyer le mot au serveur
		iResult = send(leSocket, motEnvoye, strlen(motEnvoye) + 1, 0);
		if (iResult == SOCKET_ERROR) {
			printf("Erreur du send: %d\n", WSAGetLastError());
			closesocket(leSocket);
			WSACleanup();
			printf("Appuyez une touche pour finir\n");
			getchar();
			return 1;
		}

		// Terminer le thread pour recevoir des mesages
		//ExitThread(msRecvTheadID);

		//printf("Nombre d'octets envoyes : %ld\n", iResult);

		//------------------------------
		// Maintenant, on va recevoir l' information envoyée par le serveur
		//printf("Saisir un mot de 7 lettres pour envoyer au serveur: ");
		std::cout << "Me: ";
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
	char motRecu[LONGEUR_MSG + 1];

	while (true) {
		iResult = recv(leSocket, motRecu, LONGEUR_MSG, 0);
		if (iResult > 0) {
			//printf("Nombre d'octets recus: %d\n", iResult);
			motRecu[iResult] = '\0';
			printf("Other: %s\n", motRecu);
		}
		else {
			printf("Erreur de reception : %d\n", WSAGetLastError());
			std::cout << "Au revoir!" << std::endl;
			return 0;
		}
	}
}