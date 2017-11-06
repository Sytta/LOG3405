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

// External functions
extern DWORD WINAPI MessageRecvHandler(void* sd_);

int __cdecl main(int argc, char **argv)
{
    WSADATA wsaData;
    struct addrinfo *result = NULL,
                    *ptr = NULL,
                    hints;
    char motEnvoye[200];
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
	char *host = "132.207.214.39";
	char *port = "5040";

	std::string tmp;
	std::cout << "Entrez l'adresse du serveur avec lequel vous voulez communiquer : ";
	std::cin >> tmp;
	std::cin.get();
	host = const_cast<char*>(tmp.c_str());

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

	printf("Connecte au serveur %s:%s\n\n", host, port);
    freeaddrinfo(result);

	// Creer le thread pour recevoir des messages
	DWORD msRecvTheadID;
	CreateThread(0, 0, MessageRecvHandler, NULL, 0, &msRecvTheadID);

	//----------------------------
	// Demander à l'usager un mot a envoyer au serveur
	//-----------------------------

	printf("Saisir un mot de 7 lettres pour envoyer au serveur: ");
	gets_s(motEnvoye);
	printf("Le mot envoye est: %s\n", motEnvoye);

	while (std::string(motEnvoye) != "exit!!!") {
		
		// Envoyer le mot au serveur
		iResult = send(leSocket, motEnvoye, 200, 0);
		if (iResult == SOCKET_ERROR) {
			printf("Erreur du send: %d\n", WSAGetLastError());
			closesocket(leSocket);
			WSACleanup();
			printf("Appuyez une touche pour finir\n");
			getchar();
			return 1;
		}

		printf("Nombre d'octets envoyes : %ld\n", iResult);

		//------------------------------
		// Maintenant, on va recevoir l' information envoyée par le serveur
		printf("Saisir un mot de 7 lettres pour envoyer au serveur: ");
		gets_s(motEnvoye);
		printf("Le mot envoye est: %s\n", motEnvoye);
	}

    // cleanup
    closesocket(leSocket);
    WSACleanup();

	printf("Appuyez une touche pour finir\n");
	getchar();
    return 0;
}

DWORD WINAPI MessageRecvHandler(void* sd_) 
{
	int iResult;
	char motRecu[200];

	while (true) {
		iResult = recv(leSocket, motRecu, 200, 0);
		if (iResult > 0) {
			//printf("Nombre d'octets recus: %d\n", iResult);
			motRecu[iResult - 1] = '\0';
			printf("\nLe mot recu est %s\n", motRecu);
		}
		else {
			printf("Erreur de reception : %d\n", WSAGetLastError());
		}
	}
}