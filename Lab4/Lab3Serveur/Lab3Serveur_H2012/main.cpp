#undef UNICODE

#include <winsock2.h>
#include <iostream>
#include <algorithm>
#include <strstream>
#include <locale>
#include <vector>
#include <queue>
#include <ws2tcpip.h>

#define MAX_MSG_LEN_BYTES 200

using namespace std;

// link with Ws2_32.lib
#pragma comment( lib, "ws2_32.lib" )

// External functions
extern DWORD WINAPI ClientMessageHandler(void* sd_) ;
extern DWORD WINAPI MessageSendHandler(void* sd_);
extern void DoSomething( char *src, char *dest );
extern bool isValidIP(char *IP);

// List of Winsock error constants mapped to an interpretation string.
// Note that this list must remain sorted by the error constants'
// values, because we do a binary search on the list when looking up
// items.
static struct ErrorEntry {
    int nID;
    const char* pcMessage;

    ErrorEntry(int id, const char* pc = 0) : 
    nID(id), 
    pcMessage(pc) 
    { 
    }

    bool operator<(const ErrorEntry& rhs) const
    {
        return nID < rhs.nID;
    }
} gaErrorList[] = {
    ErrorEntry(0,                  "No error"),
    ErrorEntry(WSAEINTR,           "Interrupted system call"),
    ErrorEntry(WSAEBADF,           "Bad file number"),
    ErrorEntry(WSAEACCES,          "Permission denied"),
    ErrorEntry(WSAEFAULT,          "Bad address"),
    ErrorEntry(WSAEINVAL,          "Invalid argument"),
    ErrorEntry(WSAEMFILE,          "Too many open sockets"),
    ErrorEntry(WSAEWOULDBLOCK,     "Operation would block"),
    ErrorEntry(WSAEINPROGRESS,     "Operation now in progress"),
    ErrorEntry(WSAEALREADY,        "Operation already in progress"),
    ErrorEntry(WSAENOTSOCK,        "Socket operation on non-socket"),
    ErrorEntry(WSAEDESTADDRREQ,    "Destination address required"),
    ErrorEntry(WSAEMSGSIZE,        "Message too long"),
    ErrorEntry(WSAEPROTOTYPE,      "Protocol wrong type for socket"),
    ErrorEntry(WSAENOPROTOOPT,     "Bad protocol option"),
    ErrorEntry(WSAEPROTONOSUPPORT, "Protocol not supported"),
    ErrorEntry(WSAESOCKTNOSUPPORT, "Socket type not supported"),
    ErrorEntry(WSAEOPNOTSUPP,      "Operation not supported on socket"),
    ErrorEntry(WSAEPFNOSUPPORT,    "Protocol family not supported"),
    ErrorEntry(WSAEAFNOSUPPORT,    "Address family not supported"),
    ErrorEntry(WSAEADDRINUSE,      "Address already in use"),
    ErrorEntry(WSAEADDRNOTAVAIL,   "Can't assign requested address"),
    ErrorEntry(WSAENETDOWN,        "Network is down"),
    ErrorEntry(WSAENETUNREACH,     "Network is unreachable"),
    ErrorEntry(WSAENETRESET,       "Net connection reset"),
    ErrorEntry(WSAECONNABORTED,    "Software caused connection abort"),
    ErrorEntry(WSAECONNRESET,      "Connection reset by peer"),
    ErrorEntry(WSAENOBUFS,         "No buffer space available"),
    ErrorEntry(WSAEISCONN,         "Socket is already connected"),
    ErrorEntry(WSAENOTCONN,        "Socket is not connected"),
    ErrorEntry(WSAESHUTDOWN,       "Can't send after socket shutdown"),
    ErrorEntry(WSAETOOMANYREFS,    "Too many references, can't splice"),
    ErrorEntry(WSAETIMEDOUT,       "Connection timed out"),
    ErrorEntry(WSAECONNREFUSED,    "Connection refused"),
    ErrorEntry(WSAELOOP,           "Too many levels of symbolic links"),
    ErrorEntry(WSAENAMETOOLONG,    "File name too long"),
    ErrorEntry(WSAEHOSTDOWN,       "Host is down"),
    ErrorEntry(WSAEHOSTUNREACH,    "No route to host"),
    ErrorEntry(WSAENOTEMPTY,       "Directory not empty"),
    ErrorEntry(WSAEPROCLIM,        "Too many processes"),
    ErrorEntry(WSAEUSERS,          "Too many users"),
    ErrorEntry(WSAEDQUOT,          "Disc quota exceeded"),
    ErrorEntry(WSAESTALE,          "Stale NFS file handle"),
    ErrorEntry(WSAEREMOTE,         "Too many levels of remote in path"),
    ErrorEntry(WSASYSNOTREADY,     "Network system is unavailable"),
    ErrorEntry(WSAVERNOTSUPPORTED, "Winsock version out of range"),
    ErrorEntry(WSANOTINITIALISED,  "WSAStartup not yet called"),
    ErrorEntry(WSAEDISCON,         "Graceful shutdown in progress"),
    ErrorEntry(WSAHOST_NOT_FOUND,  "Host not found"),
    ErrorEntry(WSANO_DATA,         "No host data of that type was found")
};
const int kNumMessages = sizeof(gaErrorList) / sizeof(ErrorEntry);

typedef struct ClientInfo {
	SOCKET sd;
	DWORD nThreadID;
};

typedef struct Message {
	SOCKET sender;
	char* message;
};

// Queue FIFO pour les nouvellles connexions
std::queue<ClientInfo> *nouveauxClients = new std::queue<ClientInfo>();

// Queue FIFO pour contenir les messages
std::queue<Message> *messageQueue = new std::queue<Message>();


//// WSAGetLastErrorMessage ////////////////////////////////////////////
// A function similar in spirit to Unix's perror() that tacks a canned 
// interpretation of the value of WSAGetLastError() onto the end of a
// passed string, separated by a ": ".  Generally, you should implement
// smarter error handling than this, but for default cases and simple
// programs, this function is sufficient.
//
// This function returns a pointer to an internal static buffer, so you
// must copy the data from this function before you call it again.  It
// follows that this function is also not thread-safe.
const char* WSAGetLastErrorMessage(const char* pcMessagePrefix, int nErrorID = 0)
{
    // Build basic error string
    static char acErrorBuffer[256];
    ostrstream outs(acErrorBuffer, sizeof(acErrorBuffer));
    outs << pcMessagePrefix << ": ";

    // Tack appropriate canned message onto end of supplied message 
    // prefix. Note that we do a binary search here: gaErrorList must be
	// sorted by the error constant's value.
	ErrorEntry* pEnd = gaErrorList + kNumMessages;
    ErrorEntry Target(nErrorID ? nErrorID : WSAGetLastError());
    ErrorEntry* it = lower_bound(gaErrorList, pEnd, Target);
    if ((it != pEnd) && (it->nID == Target.nID)) {
        outs << it->pcMessage;
    }
    else {
        // Didn't find error in list, so make up a generic one
        outs << "unknown error";
    }
    outs << " (" << Target.nID << ")";

    // Finish error message off and return it.
    outs << ends;
    acErrorBuffer[sizeof(acErrorBuffer) - 1] = '\0';
    return acErrorBuffer;
}

int main(void) 
{
	//----------------------
	// Initialize Winsock.
	WSADATA wsaData;
	int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	if (iResult != NO_ERROR) {
		cerr << "Error at WSAStartup()\n" << endl;
		return 1;
	}

	//----------------------
	// Create a SOCKET for listening for
	// incoming connection requests.
	// TODO: Get user input for socket selection at IP of the host computer
	SOCKET ServerSocket;
	ServerSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (ServerSocket == INVALID_SOCKET) {
        cerr << WSAGetLastErrorMessage("Error at socket()") << endl;
		WSACleanup();
		return 1;
	}
	char option[] = "1";
	setsockopt(ServerSocket, SOL_SOCKET, SO_REUSEADDR, option, sizeof(option));

	//----------------------
    // The sockaddr_in structure specifies the address family,
    // IP address, and port for the socket that is being bound.
	int port=5040; // TODO: Set port using the user input
	char host[15];

	do {
		std::string tmp;
		std::cout << "Entrez l'adresse du serveur : ";
		std::cin >> tmp;
		std::cin.get();
		strcpy(host, tmp.c_str());
	} while (!isValidIP(host));

	do {
		std::string tmp;
		std::cout << "Entrez le port du serveur : ";
		std::cin >> tmp;
		std::cin.get();
		port = std::stoi(tmp);
	} while (port > 5050 || port < 5000);
    
	// Recuperation de l'adresse locale
	hostent *thisHost;
	thisHost = gethostbyname(host);  // TODO: Set server IP using user input
	char* ip;
	ip = inet_ntoa(*(struct in_addr*) *thisHost->h_addr_list);
	printf("Adresse locale trouvee %s : \n\n", ip);
	sockaddr_in service;
    service.sin_family = AF_INET;
	service.sin_addr.s_addr = inet_addr(ip);
    service.sin_port = htons(port);

    if (bind(ServerSocket, (SOCKADDR*) &service, sizeof(service)) == SOCKET_ERROR) {
		cerr << WSAGetLastErrorMessage("bind() failed.") << endl;
		closesocket(ServerSocket);
		WSACleanup();
		return 1;
	}

	// TODO: Creation d'un consommateur pour envoyer des messages
	DWORD msSenderThreadID;
	CreateThread(0, 0, MessageSendHandler, NULL, 0, &msSenderThreadID);
	
	//----------------------
	// Listen for incoming connection requests.
	// on the created socket
	if (listen(ServerSocket, 30) == SOCKET_ERROR) {
		cerr << WSAGetLastErrorMessage("Error listening on socket.") << endl;
		closesocket(ServerSocket);
		WSACleanup();
		return 1;
	}

	printf("En attente des connections des clients sur le port %d...\n\n", ntohs(service.sin_port));

	while (true) {	
		sockaddr_in sinRemote;
		int nAddrSize = sizeof(sinRemote);
		// Create a SOCKET for accepting incoming requests.
		// Accept the connection.
		SOCKET sd = accept(ServerSocket, (sockaddr*)&sinRemote, &nAddrSize);
        if (sd != INVALID_SOCKET) {
			cout << "Connection acceptee De : " <<
                    inet_ntoa(sinRemote.sin_addr) << ":" <<
                    ntohs(sinRemote.sin_port) << "." <<
                    endl;

			DWORD nThreadID;
			// Creer le producteur
            CreateThread(0, 0, ClientMessageHandler, (void*)sd, 0, &nThreadID);

			// Creer le ClientInfo
			ClientInfo client = { sd, nThreadID };
			nouveauxClients->push(client);

        } else {
            cerr << WSAGetLastErrorMessage("Echec d'une connection.") << 
                    endl;
			closesocket(sd);
			WSACleanup();
        }
    }

	delete[] nouveauxClients;
	delete[] messageQueue;
}

// Verifier la validite de l'IP
// Inspiration: https://stackoverflow.com/questions/791982/determine-if-a-string-is-a-valid-ip-address-in-c
bool isValidIP(char *IP)
{
	struct sockaddr_in iptest;
	int result = inet_pton(AF_INET, IP, &(iptest.sin_addr));
	return result != 0;
}

//// ClientMessageHandler ///////////////////////////////////////////////////////
// TODO: Put relevant function description here

DWORD WINAPI ClientMessageHandler(void* sd_)
{
	SOCKET sd = (SOCKET)sd_;

	// Read Data from client
	/*// TODO: Change buffer sizes
	char readBuffer[10], outBuffer[10];
	int readBytes, sendBytes;

	do {
		readBytes = recv(sd, readBuffer, 7, 0);  // TODO: Change number of characters to read
		if (readBytes > 0) {
			cout << "Received " << readBytes << " bytes from client." << endl;
			cout << "Received " << readBuffer << " from client." << endl;
			DoSomething(readBuffer, outBuffer);
			sendBytes = send(sd, outBuffer, 7, 0);
			if (sendBytes == SOCKET_ERROR) {
				printf("send failed with error: %d\n", WSAGetLastError());
				closesocket(sd);
				WSACleanup();
				return 1;
			}
			printf("Bytes sent: %d\n", sendBytes);
		}
		else if (readBytes == 0) {
			cout << "Connection closing\n";
		} 
		else {
			cout << WSAGetLastErrorMessage("Echec de la reception !") << endl;
			closesocket(sd);
			WSACleanup();
			return 0;
		}

	} while (readBytes > 0);*/

	while (true) {
		char readBuffer[200];
		if (recv(sd, readBuffer, 200, 0)) {
			Message msg = { sd, readBuffer };
			messageQueue->push(msg);
			std::cout << GetCurrentThreadId() << " : " << readBuffer << std::endl;
			//send(sd, readBuffer, 200, 0);
		}
	}
	
	 // Shut down the socket
	//int ishutDown;

	int ishutDown = shutdown(sd, SD_SEND);
	if (ishutDown == SOCKET_ERROR) {
		printf("shutdown failed with error: %d\n", WSAGetLastError());
		closesocket(ishutDown);
		WSACleanup();
		return 1;
	}

	closesocket(sd);
	WSACleanup();

	return 0;
}

//// MessageSendHandler ///////////////////////////////////////////////////////
// TODO: Put relevant function description here

DWORD WINAPI MessageSendHandler(void* sd_)
{
	//msgQueue = (SOCKET)msgQueue_;

	// Liste des clients connectes
	std::vector<ClientInfo> *clients = new std::vector<ClientInfo>;

	while (true) {
		// Ajouter les nouveaux clients et envoyer les 15 derniers messages
		while (!nouveauxClients->empty()) {
			// TODO: Envoyer les 15 derniers messages

			// Ajout dans la liste des clients
			clients->push_back(nouveauxClients->front());
			std::cout << "Client" << nouveauxClients->front().nThreadID << "ajoute" << endl;
			nouveauxClients->pop();			
		}

		// Envoyer les messages a tous les clients sauf a celui qui l'a envoye
		while (!messageQueue->empty()) {
			Message msg = messageQueue->front();
			for (std::vector<ClientInfo>::iterator it = clients->begin(); it != clients->end(); ++it) {
				// Verifier que ce n'est pas le meme client qui a envoye le message
				if (msg.sender != it->sd) {
					int iSendResult = send(it->sd, msg.message, strlen(msg.message), 0);
					if (iSendResult == SOCKET_ERROR) {
						printf("send failed with error: %d\n", WSAGetLastError());
						cout << "Client " << it->nThreadID << " a quitte" << endl;
						clients->erase(it);
					}
				}
			}
			
			messageQueue->pop();
		}
		
	}

	// Shut down the socket
	/*int ishutDown;

	ishutDown = shutdown(sd, SD_SEND);
	if (ishutDown == SOCKET_ERROR) {
	printf("shutdown failed with error: %d\n", WSAGetLastError());
	closesocket(ishutDown);
	WSACleanup();
	return 1;
	}*/


	delete[] clients;
	//closesocket(sd);
	//WSACleanup();

	return 0;
}

// Do Something with the information
void DoSomething( char *src, char *dest )
{
	auto index = 0;
	for (auto i = 6; i >= 0; i--)
	{
		dest[index++] = (i % 2 != 0) ? src[i] : toupper(src[i]);
	}
}
