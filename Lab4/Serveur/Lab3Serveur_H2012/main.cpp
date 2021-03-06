/*
 * Fichier : main.cpp 
 * Description : Code du serveur.
 * Auteurs : Erica Bugden, Yawen Hou (Avec des sections adapt�es du code du lab 3)
 */

#undef UNICODE

#include <winsock2.h>
#include <iostream>
#include <algorithm>
#include <strstream>
#include <locale>
#include <vector>
#include <map>
#include <queue>
#include <fstream>
#include <ws2tcpip.h>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <chrono>
#include <mutex>

#define MAX_MSG_LEN_BYTES 200
#define USERS_FILENAME "../utilisateurs.txt"
#define MESSAGE_LOG "../messageslog.txt"

using namespace std;

// link with Ws2_32.lib
#pragma comment( lib, "ws2_32.lib" )

typedef struct ClientInfo {
	SOCKET sd;
	string username;
	string IP;
};

typedef struct Message {
	ClientInfo sender;
	char* message;
};

// External functions
extern DWORD WINAPI ClientMessageHandler(void* sd_) ;
extern DWORD WINAPI MessageSendHandler(void* sd_);
extern bool isValidIP(char *IP);
extern string writeMessageToFile(ClientInfo sender, string msg);
extern void parseExistingUsers();
extern void readMessageLog();
extern bool verifyUser(SOCKET sd, string username, string password);

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

// Queue FIFO pour les nouvellles connexions
std::mutex nvClientsMutex;
std::queue<ClientInfo> *nouveauxClients = new std::queue<ClientInfo>();

// Liste des clients connectes
std::vector<ClientInfo> *clients = new std::vector<ClientInfo>;

// Queue FIFO pour contenir les messages
std::mutex msgWritingMutex;
std::queue<Message> *messageQueue = new std::queue<Message>();

// Deque to save the last 15 messages
std::deque<char*> *last15Messages = new std::deque<char*>();

// Gestion des utilisateurs et des mots de passe
std::mutex userFileMutex;
fstream userFile;
fstream msgFile;
map<string, string> users;

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

/*
 * Fonction : main
 * Description :
 *  - Initialisation du socket d'�coute du serveur
 *  - �couter pour les clients qui veulent de connecter
 *  - Cr�ation d'un fil pour traiter chaque client
 */
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
	// For local testing
	//char *host = "127.0.0.1";
	// int port = 5040;

	int port = 5040; 
	char host[15] = "";

	do {
		std::string tmp;
		std::cout << "Entrez l'adresse du serveur : ";
		std::cin >> tmp;
		std::cin.get();
		strcpy(host, tmp.c_str());
		if (!isValidIP(host))
			std::cout << "L'adresse IP est invalide! Reessayez s'il-vous-plait." << std::endl;
	} while (!isValidIP(host));

	bool portIsValid = false;
	
	do {
		std::string tmp;
		std::cout << "Entrez le port du serveur : ";
		std::cin >> tmp;
		std::cin.get();
		if (tmp.find_first_not_of("0123456789") != std::string::npos || std::stoi(tmp) > 5050 || std::stoi(tmp) < 5000) {
			std::cout << "Le port est invalide! Le port doit etre entre 5000 et 5050. Reessayez s'il-vous-plait." << std::endl;
			portIsValid = false;
		}
		else {
			portIsValid = true;
			port = std::stoi(tmp);
		}
	} while (!portIsValid);
    
	// Recuperation de l'adresse locale
	hostent *thisHost;
	thisHost = gethostbyname(host);
	char* ip;
	ip = inet_ntoa(*(struct in_addr*) *thisHost->h_addr_list);
	printf("Adresse locale trouvee %s : \n\n", ip);
	sockaddr_in service;
    service.sin_family = AF_INET;
	service.sin_addr.s_addr = inet_addr(ip);
    service.sin_port = htons(port);

	if (::bind(ServerSocket, (SOCKADDR*) &service, sizeof(service)) == SOCKET_ERROR) {
		cerr << WSAGetLastErrorMessage("bind() failed.") << endl;
		closesocket(ServerSocket);
		WSACleanup();
		return 1;
	}

	// Creation d'un consommateur pour envoyer des messages
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

	// Ouvrir et parser le contenu du fichier des utilisateurs et des mots de passe
	parseExistingUsers();

	printf("En attente des connections des clients sur le port %d...\n\n", ntohs(service.sin_port));

	// Ouvrir et lire l'historique de messages
	readMessageLog();
	cout << endl << "------------Fin de l'historique------------" << endl;

	while (true) {	
		sockaddr_in sinRemote;
		int nAddrSize = sizeof(sinRemote);
		// Create a SOCKET for accepting incoming requests.
		// Accept the connection.
		SOCKET sd = accept(ServerSocket, (sockaddr*)&sinRemote, &nAddrSize);

		if (sd == INVALID_SOCKET) {
			cerr << WSAGetLastErrorMessage("Echec d'une connection.") <<
				endl;
			closesocket(sd);
		}

		// Confirmation
		cout << "Connection acceptee De : " <<
                inet_ntoa(sinRemote.sin_addr) << ":" <<
                ntohs(sinRemote.sin_port) << "." <<
                endl;

		DWORD nThreadID;
		// Creer le producteur
        CreateThread(0, 0, ClientMessageHandler, (void*)sd, 0, &nThreadID);
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

/*
 * Fonction : parseExistingUsers
 * Description : Lire le fichier des utilisateurs et r�cup�rer les informations
 *  des utilisateurs qui ont d�j� �t� connect� au serveur.
 */
void parseExistingUsers() {
	string username;
	string password;

	userFile.open(USERS_FILENAME, fstream::in | fstream::out | fstream::app);  
	while (getline(userFile, username)) {
		getline(userFile, password);
		users.insert(pair<string, string>(username, password));
		//cout << "username: " << username << ", password: " << password << endl;  // Uncomment to display existing users on startup
	}
	userFile.close();
}

/*
 * Fonction : readMessageLog
 * Description : Lire le fichier des messages et r�cup�rer les 15 messages
 *  les plus r�cents.
 */
void readMessageLog() {
	msgFile.open(MESSAGE_LOG, fstream::in);

	string message = "";

	while(getline(msgFile, message)) {
		message += "\n";
		// Convertir en char*
		char * messageChar = new char[message.length() + 1];
		strcpy(messageChar, message.c_str());

		last15Messages->push_back(messageChar);

		if (last15Messages->size() > 15)
			last15Messages->pop_front();
	}

	// Imprimer les 15 messages dans le console
	for (auto it = last15Messages->begin(); it != last15Messages->end(); it++) {
		std::cout << *it;
	}

	msgFile.close();
}

/*
 * Fonction : verifyUser
 * Description : V�rifier si les informations du client correspondent � une
 *  entr�e dans le fichier des utilisateurs. Si l'utilisateur existe mais le
 *  mot de passe n'est pas valide, le client est rejet�e. Si l'utilisateur
 *  n'existe pas ses informations sont enregistr�es dans le fichier et la
 *  connexion est accept�e.
 */
bool verifyUser(SOCKET sd, string username, string password) {

	// Check if user exists in file
	int iResult;
	/// Critical section
	userFileMutex.lock();
	auto it = users.find(username);
	if (it != users.end()) {
		// Verify the password
		if (it->second == password) {
			userFileMutex.unlock();
			/// End critical section

			char serverResponse[2] = "1";
			iResult = send(sd, serverResponse, strlen(serverResponse) + 1 , 0);
			if (iResult == SOCKET_ERROR) {
				cout << "Error sending authentification response to socket" << endl;
				return false;
			}
			return true;
		} else {
			userFileMutex.unlock();
			/// End critical section

			char serverResponse[2] = "0";
			iResult = send(sd, serverResponse, strlen(serverResponse) + 1, 0);
			if (iResult == SOCKET_ERROR) {
				cout << "Error sending authentification response to socket" << endl;
			}
			return false;
		}
	}
	else {
		// Create the user entry
		userFile.open(USERS_FILENAME, fstream::in | fstream::out | fstream::app);  
		users.insert(pair<string, string>(username, password));
		userFile << username << endl << password << endl;  // IMPORTANT : The \n needs to be there before the end of the file (endl is necessary for correct parsing)
		userFile.close();
		userFileMutex.unlock();
		/// End critical section

		char serverResponse[2] = "1";
		iResult = send(sd, serverResponse, strlen(serverResponse) + 1, 0);
		if (iResult == SOCKET_ERROR) {
			cout << "Error sending authentification response to socket" << endl;
		}
		return true;
	}
}

// R�cup�re le nom du client associ� au socket
ClientInfo getClientFromSocket(SOCKET sd) {
	auto it = find_if(clients->begin(), clients->end(), [&](const ClientInfo& obj) {return obj.sd == sd; });
	return *it;
}

/*
 * Fonction : writeMessageToFile
 * Description : �crire le message sur la console du serveur et dans le fichier
 *  des messages pr�c�dents.
 * 
 * Format d'�criture : 
 *  [Nom d�utilisateur - Adresse IP : Port client - 2017-10-13@13:02:01]:
 */
string writeMessageToFile(ClientInfo sender , string msg) {
	// Get current time
	auto t = std::time(nullptr);
	auto tm = *std::localtime(&t);

	std::ostringstream oss;
	oss << std::put_time(&tm, "%Y-%m-%d@%H:%M:%S");
	string currentTime = oss.str();

	// TODO: Write to file, change str to username
	// Get rid of this after having username
	std::stringstream sstr;
	sstr << sender.username;
	std::string str = sstr.str();

	string log = "[" + str + " - " + sender.IP + " - " + currentTime + "]: " + msg + '\n';

	cout << log;

	// Ouvrir le fichier txt pour enregistrement de messages
	msgFile.open(MESSAGE_LOG, fstream::out | fstream::app);
	msgFile << log;
	msgFile.close();

	return log;
}

/*
 * Fonction : ClientMessageHandler
 * Description :
 *  - Effectuer l'authentification du client et rejeter la connexion si
 *     n�cessaire
 *  - Recevoir les messages du client une fois que la connexion a �t� accept�e
 */
DWORD WINAPI ClientMessageHandler(void* sd_)
{
	//Get IP adress
	SOCKET sd = (SOCKET)sd_;

	struct sockaddr_in client_ip;
	socklen_t addr_size = sizeof(struct sockaddr_in);
	int res = getpeername(sd, (struct sockaddr *)&client_ip, &addr_size);
	string IP = std::string(inet_ntoa(client_ip.sin_addr)) + " : " + std::to_string(ntohs(client_ip.sin_port));

	char readBuffer[MAX_MSG_LEN_BYTES + 1] = "";
	int readBytes;

	// Get username and password from user
	string password;
	string username;
	do {
		readBytes = recv(sd, readBuffer, MAX_MSG_LEN_BYTES, 0);
		if (readBytes <= 0) {
			cout << "Error receiving username. Closing client connection." << endl;
			closesocket(sd);
			return 0;
		}
		username = readBuffer;

		readBytes = recv(sd, readBuffer, MAX_MSG_LEN_BYTES, 0);
		if (readBytes <= 0) {
			cout << "Error receiving password. Closing client connection." << endl;
			closesocket(sd);
			return 0;
		}
		password = readBuffer;

	} while (!verifyUser(sd, username, password));

	// Creer le ClientInfo
	ClientInfo client = { sd, username, IP };

	/// Critical section
	nvClientsMutex.lock();
	nouveauxClients->push(client);
	nvClientsMutex.unlock();
	/// End critical section

	do {
		readBytes = recv(sd, readBuffer, MAX_MSG_LEN_BYTES + 1, 0);
		if (readBytes > 0) {
			// Change socket to IP as sender
			ClientInfo sender = getClientFromSocket(sd);
			Message msg = { sender, readBuffer };

			/// Critical section
			msgWritingMutex.lock();
			messageQueue->push(msg);
			msgWritingMutex.unlock();
			/// End critical section

		} else {
			//cout << WSAGetLastErrorMessage("Echec de la reception !") << endl;
			cout << "Client " << client.username << " a quitte" << endl;
			break;
		}
	} while (readBytes > 0);

	int ishutDown = shutdown(sd, SD_SEND);
	if (ishutDown == SOCKET_ERROR) {
		printf("shutdown failed with error: %d\n", WSAGetLastError());
		closesocket(ishutDown);
		return 1;
	}
	closesocket(sd);
	return 0;
}

/*
 * Fonction : MessageSendHandler
 * Description :
 *  - Envoyer les les messages aux autres clients
 *  - Sauvegarder les messages pr�c�dents
 */
DWORD WINAPI MessageSendHandler(void* sd_)
{

	while (true) {
		// Ajouter les nouveaux clients et envoyer les 15 derniers messages

		///Critical section
		nvClientsMutex.lock();
		while (!nouveauxClients->empty()) {
			ClientInfo nv = nouveauxClients->front();
			// Envoyer les 15 derniers messages
			for (std::deque<char*>::iterator it = last15Messages->begin(); it != last15Messages->end(); ++it) {
				int iSendResult = send(nv.sd, *it, strlen(*it), 0);
				Sleep(10); // Eviter d'envoyer trop vite pour avoir du beau formattage

				if (iSendResult == SOCKET_ERROR) {
					printf("send failed with error: %d\n", WSAGetLastError());
					cout << "Client " << nv.username << " a quitte" << endl;
				}
			}
			// Ajout dans la liste des clients
			clients->push_back(nv);
			std::cout << "Client " << nouveauxClients->front().username << " ajoute" << endl;
			nouveauxClients->pop();		  
		}
		nvClientsMutex.unlock();
		/// End critical section

		// Aller au prochaine it�ration si aucun message
		if (messageQueue->empty()) {
			continue;
		}

		// Envoyer un message a tout le monde sauf le client qui l'a envoy�
		Message msg = messageQueue->front();

		// �crire dans le fichiers le messages
		string formattedMsg = writeMessageToFile(msg.sender, msg.message);

		// Convert to char*
		char * formattedMsgChar = new char[formattedMsg.length() + 1];
		strcpy(formattedMsgChar, formattedMsg.c_str());

		// Ajouter le message aux 15 derniers msgs
		last15Messages->push_back(formattedMsgChar);
		if (last15Messages->size() > 15)
			last15Messages->pop_front();

		for (std::vector<ClientInfo>::iterator it = clients->begin(); it != clients->end(); ) {
			// Verifier que ce n'est pas le meme client qui a envoye le message
			if (msg.sender.sd == it->sd) {
				it++;
				continue;
			}

			int iSendResult = send(it->sd, formattedMsgChar, strlen(formattedMsgChar), 0);
			if (iSendResult == SOCKET_ERROR) {
				//printf("send failed with error: %d\n", WSAGetLastError());
				//cout << "Client " << it->username << " a quitte" << endl;
				it = clients->erase(it);
			} else {
				// Incr�menter si le client n'a pas �t� effac� (erase() retourne une it�rateur qui pointe d�j� vers le prochain)
				it++;
			}
		}
			
		messageQueue->pop();	
	}

	delete[] clients;

	return 0;
}
