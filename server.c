#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <signal.h>
#include <arpa/inet.h>
#include <pthread.h>

#define BUFFSIZE 128
#define CLEAR_SCREEN_ANSI "\e[1;1H\e[2J"
#define HANDSHAKE "11:s:hands"
#define STARTUPNOTFINISHED "36:s:res:error:startup not finished"
#define DATANOTMATCHING "39:s:res:error:incongruenze tra dati"
#define CLOSING "29:s:res:success:spegnimento"

//ANCHOR strutture
enum functions {
	STORE,
	SEARCH,
	CORRUPT,
	LIST,
	EXIT,
	COMMANDO_NOT_FOUND
};

struct Node {
	int key;
	int value;
	struct Node* next;
};

struct CommandStructure {
	int sizeOfMessage;
	char *sender;
	char *type;
	char *key;
	char *value;
	char *command;
	char *message;
	char *resoult;
};

struct Server {//?Ho pensato che creare una lista di server fosse la cosa più sensata
	struct sockaddr_in address;
	struct Server* next;
	int handShakeReceived;
	int socketDescriptor;
};

struct Forward {
	char *message;
	int size;
	struct CommandStructure response;
	struct Server *server;
};

struct ForwardList {
	struct Forward fwd;
	struct ForwardList *next;
};

//ANCHOR firma funzioni
void readConfigFile(int, char*);//Legge il config e salva gli indirizzi in una lista
void createConnection();
void *connectionToServer(void *);//apre le connesisoni
void *acceptConnection(void *);//accetta le connesisoni
char * readFromPeer(int);
void setHandShake(int);
struct Node* storeLocal(struct Node*, int, int);
char * printList(struct Node*);
struct Node* searchLocal(struct Node*, int);
struct Node* initList(int, int);
enum functions  getInvokedCommand(char*);
void handler (int);
pthread_t runServer(int );
int store(int , int);
char *intToString(int);
int executeCommands(struct CommandStructure, int);
struct CommandStructure getCommandStructure (char *);
void printCommandStructure(struct CommandStructure );
struct Node* corrupt(int, int);
int forwardMessage(struct CommandStructure);
void *forwardToServers(void *);
int checkForwardResult(struct ForwardList *, char *);
void sendResponse(char* , int, int);


int sd, sd1;
int handshakeCounter = 0;
int serverNumber;
struct Node* head;
struct Server* serverListHead = NULL;
int isEmptyList = 1;

//ANCHOR main
int main( int argc, const char* argv[]){
	int configFileDescriptor;
	pthread_t serverThreadId;
	signal (SIGINT, handler); //assegnazione dell'handler
	write(STDOUT_FILENO, "--- Inizio fase di Start-up del server ---\n", sizeof("--- Inizio fase di Start-up del server ---\n"));
	//controllo sul numero di input
	if (argc < 3) {
		write(STDOUT_FILENO, CLEAR_SCREEN_ANSI, 12);
		write(STDOUT_FILENO, "Errore durante lo Start-up: Necessari file config, ip e porta per l'esecuzione\n", sizeof("Errore durante lo Start-up: Necessari file config,  ip e porta per l'esecuzione\n"));
		return 0;
	}
	configFileDescriptor = open(argv[1], O_RDONLY);//apertura del file di config
	readConfigFile(configFileDescriptor,(char *) argv[2]);//legge gli address dal file config

	write(STDOUT_FILENO, "\nLettura del file di configurazione avvenuta con successo.\n", sizeof("lettura del file di configurazione avvenuta con successo.\n\n"));
	system("clear");
	int port = atoi(argv[1]);
	int isEmptyList = 1;
	head = NULL;

	
	runServer(atoi(argv[2])); //attivazione del socket in ricezione
	return 0;

}

//funzione che legge il file di config e crea una lista di server
//!NOT TESTED
//ANCHOR readConfigFile
void readConfigFile(int fileDescriptor, char* selfPort){
	int addressBufferSize = 15;//dimensione del buffer di lettura. La dimensione di address:porta
	char* buffer = (char*) malloc (addressBufferSize * sizeof(char *));//buffer in lettura
	char* add = (char*) malloc (addressBufferSize * sizeof(char *));//stringa di supporto per salvare l'add
	char* porta = (char*) malloc (addressBufferSize * sizeof(char *));//stringa di supporto per salvare la porta
	const char delim[2] = ":";//delimitatore per lo strtok
	long port; //versione long del numero di porta da assegnare all'elemento della lista
	struct Server* currentServer =  NULL; //per scorrere la lista
	struct Server* lastServer = NULL; //per salvare il server precedente
	
	serverListHead = (struct Server *) malloc (sizeof(struct Server*));//allocazione dell'elemento della lista
	currentServer = serverListHead;
	system("clear");
	write(STDOUT_FILENO, "--- Inizio fase di Start-up del server ---\n", sizeof("--- Inizio fase di Start-up del server ---\n"));
	write(STDOUT_FILENO, "\nLettura del file di configurazione...\n", sizeof("\nLettura del file di configurazione...\n"));

   //lettura degli address e separazione in tokens
   	while(read(fileDescriptor, buffer, addressBufferSize) > 0) { //finché vengono letti indirizzi
		add = strtok(buffer, delim); //stringa prima del delimitatore
   		porta = strtok(NULL, delim); //stringa dopo il delimitatore
		if (strncmp(porta, selfPort, 4) != 0){ //evita che il server salvi se stesso	
			currentServer->address.sin_family = AF_INET;//famiglia dell'address del socket
			port = atoi(porta);//conversione della porta a long
			currentServer->address.sin_port = htons(port);//assegnazione porta
			inet_pton(AF_INET, add, &currentServer->address.sin_addr);//assegnazione address
			

			if(lastServer != NULL){ //l'head non ha predecessori
				lastServer->next = currentServer; //assegnzione del corrente al next precedente
			}

			lastServer = currentServer; //salviamo il current come precedente
			currentServer = (struct Server *) malloc (sizeof(struct Server*));//allocazione dell'elemento della lista

			serverNumber ++;
		}
		
   }
	return;
}
//ANCHOR createConnection
//crea i thread per le connessioni
void createConnection(){
	struct Server *currentServer = serverListHead;
	struct Server *lastServer = currentServer;

	pthread_t threadId;
	char* buffer = (char*) malloc (BUFFSIZE * sizeof(char *));//buffer in lettura

	system("clear");
	write(STDOUT_FILENO, "--- Inizio fase di Start-up del server ---\n", sizeof("--- Inizio fase di Start-up del server ---\n"));
	write(STDOUT_FILENO, "\nStabilimento delle connessioni con gli altri server\n", sizeof("\nStabilimento delle connessioni con gli altri server\n"));
	
	while(currentServer != NULL){//finché vi sono server salvati
		if(pthread_create(&threadId, NULL, connectionToServer, currentServer) != 0){//crea un thread
			perror("errore thread");
		} else {
			//pthread_join(threadId, NULL);
		}
		lastServer = currentServer;
		currentServer = currentServer->next;
		lastServer->next = currentServer;
	}
	write(STDOUT_FILENO, "\nStabilite connessioni con tutti i server\n", sizeof("\nStabilite connessioni con tutti i server\n"));
}

//ANCHOR connectionToServer
//apre le connesisoni con gli altri server
void *connectionToServer(void *server){
	int connectResult; //utile al controllo errori sulle connessioni
	char* buffer = (char*) malloc (BUFFSIZE * sizeof(char *));//buffer in lettura
	char* response = (char*) malloc (BUFFSIZE * sizeof(char *));
	struct Server * currentServer = (struct Server *) server;
	struct CommandStructure responseStructure;

	write(STDOUT_FILENO, "\ntentativo di connessione a:", sizeof("\ntentativo di connessione a:"));
	inet_ntop(AF_INET, &(currentServer->address.sin_addr), buffer, INET_ADDRSTRLEN);
	write(STDOUT_FILENO, buffer, strlen(buffer));
	write(STDOUT_FILENO, ":", sizeof(":"));
	sprintf(buffer, "%d", ntohs(currentServer->address.sin_port));
	write(STDOUT_FILENO, buffer, strlen(buffer));

	//connessione al server assegnato in input
	currentServer->socketDescriptor = socket(AF_INET, SOCK_STREAM, 0);//socket tcp tramite stream di dati, connection-oriented
    //tentativo di creazione della connessione
	connectResult = connect(currentServer->socketDescriptor, (struct sockaddr *)&currentServer->address, sizeof(currentServer->address)); //connessione
	while(connectResult != 0){ //finché la connessione non è staiblita
		write(STDOUT_FILENO, "\nConnessione non riuscita, nuovo tentativo\n", sizeof("\nConnessione non riuscita, nuovo tentativo\n"));
		sleep(4);
		connectResult = connect(currentServer->socketDescriptor, (struct sockaddr *)&currentServer->address, sizeof(currentServer->address)); //connessione
	}
	if (connectResult == 0){
		system("clear");
		write(currentServer->socketDescriptor, HANDSHAKE, sizeof(HANDSHAKE));//invio dell'handshake
		response = readFromPeer(currentServer->socketDescriptor);
		responseStructure = getCommandStructure(response);

		if (strcmp(responseStructure.type, "hands") == 0){
			write(STDOUT_FILENO, "\nConnessione stabilita con:", sizeof("\nConnessione stabilita con:"));
			inet_ntop(AF_INET, &(currentServer->address.sin_addr), buffer, INET_ADDRSTRLEN);
			write(STDOUT_FILENO, buffer, strlen(buffer));
			write(STDOUT_FILENO, ":", sizeof(":"));
			sprintf(buffer, "%d", ntohs(currentServer->address.sin_port));
			write(STDOUT_FILENO, buffer, strlen(buffer));
			write(STDOUT_FILENO, "\n", sizeof("\n"));
		}
		
	}
}

//ANCHOR runServer
//! NOT PROPERLY TESTED
pthread_t runServer(int port){
	struct sockaddr_in address;
	struct sockaddr_in claddress;
	socklen_t dimaddcl = sizeof(claddress);
	char * sup = (char *) malloc(BUFFSIZE * sizeof(char *));
	int socketDescriptor;
	
	write(STDOUT_FILENO, CLEAR_SCREEN_ANSI, 12);
	write(STDOUT_FILENO, "--- Inizio fase di Start-up del server ---\n", sizeof("--- Inizio fase di Start-up del server ---\n"));
	write(STDOUT_FILENO, "\nApertura del socket in ricezione...\n", sizeof("\nApertura del socket in ricezione...\n"));

	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	address.sin_addr.s_addr = htonl(INADDR_ANY);

	sd = socket(AF_INET, SOCK_STREAM, 0);//socket TCP ipv4
	if (bind(sd, (struct sockaddr *) &address, sizeof(address)) < 0) {//assegna l'address al socket
		perror ("errore bind");
		exit(1);
	}
	listen(sd, 20); // rende il servizio raggiungibile

	write(STDOUT_FILENO, "Socket aperto...\n", sizeof("Socket aperto...\n"));

	pthread_t threadId;
	int exitCondition = 1;
	createConnection();

	while (exitCondition == 1) {
		
		socketDescriptor = accept(sd, (struct sockaddr *) NULL, NULL);// estrae una richieta di connessione
		if (socketDescriptor>1) { //in caso l'accettazione sia andata a buon fine
			
			write(STDOUT_FILENO, "accettata\n", sizeof("accettata\n"));
			int add = getpeername(socketDescriptor, (struct sockaddr *)&claddress, &dimaddcl);
			strcpy (sup, inet_ntoa(claddress.sin_addr));
			write(STDOUT_FILENO, "Connessione accettata da: ", sizeof("connessione accettata da:"));
			write(STDOUT_FILENO, sup, strlen(sup));
			write(STDOUT_FILENO, ":", sizeof(":"));
			sprintf(sup, "%d", ntohs(claddress.sin_port));
			write(STDOUT_FILENO, sup, strlen(sup));
			write(STDOUT_FILENO, "\n", sizeof("\n"));

			if(pthread_create(&threadId, NULL, acceptConnection, &socketDescriptor) != 0){//crea un thread
					perror("errore thread");
			} else {
				//pthread_join(threadId, NULL);
			}
		}
	}
}

char * readFromPeer(int socketDescriptor){
	char * messaggio = (char *) malloc(BUFFSIZE * sizeof(char *));
	char * buf = (char *) malloc(BUFFSIZE * sizeof(char *));
	char * sup = (char *) malloc(BUFFSIZE * sizeof(char *));
	int exitCondition = 1;
	int r = 0;
	int size = 0;
	int sumSize = 0;

	while (exitCondition == 1) {
		r = read (socketDescriptor, buf, BUFFSIZE);
		strcpy(sup, buf);
		size = atoi(strtok(sup, ":"));
		if (size == r){
			strcpy(messaggio, buf);
			exitCondition = 0;
		} else if( size < r){
			strncpy(messaggio, buf, size);
			exitCondition = 0;
		} else {
			sumSize = r;
			while ((size > sumSize) && (r > 0)){
				r = read (socketDescriptor, buf, 128);
				sumSize += r;
				strncpy(sup, buf, size - (r-1));
				strcat(messaggio, sup);
			}
			exitCondition = 0;
		}
	}
	free(buf);
	return messaggio;
}
//ANCHOR acceptConnection
//accetta le connessioni in attesa
void *acceptConnection(void *arg){
	char * messaggio = (char *) malloc(BUFFSIZE * sizeof(char *));
	char * buf = (char *) malloc(BUFFSIZE * sizeof(char *));
	char * sup = (char *) malloc(BUFFSIZE * sizeof(char *));
	struct sockaddr_in claddress;
	socklen_t dimaddcl = sizeof(claddress);
	int exitCondition = 1;
	int *socketDescriptor = (int *) arg;
	
	buf = readFromPeer(*socketDescriptor);
	struct CommandStructure command = getCommandStructure(buf);

	if(strcmp(command.type, "req") == 0){
		if (handshakeCounter < serverNumber){
			write(STDOUT_FILENO, "startup not finished", 21);
			write(*socketDescriptor, STARTUPNOTFINISHED, 36);
		} else {
			write(STDOUT_FILENO, "comando ricevuto", sizeof("comando ricevuto"));
			sprintf(sup, "%d", *socketDescriptor);
			write(STDOUT_FILENO, sup, strlen(sup));
			exitCondition = executeCommands(command, *socketDescriptor);
		}
	} else if(strcmp(command.type, "hands") == 0) {
		write(STDOUT_FILENO, "\nHandshake Ricevuto\n", sizeof("\nHandshake Ricevuto\n"));
		write(*socketDescriptor, HANDSHAKE, sizeof(HANDSHAKE));//invio dell'handshake
		handshakeCounter ++;
		//setHandShake(claddress.sin_port);
	}

	free(buf);

	write(STDOUT_FILENO, "chiusura", sizeof("chiusura"));
	close(*socketDescriptor);// chiude la connessione
}

void setHandShake(int port){
	struct Server *currentServer = serverListHead;
/*
	while ((currentServer != NULL ) && (currentServer->address.sin_port != port )){
		currentServer = currentServer->next;
	}
	if (currentServer->address.sin_port == port){*/
		write(STDOUT_FILENO, "handshake contato", sizeof("handshake contato"));
		currentServer->handShakeReceived = 1;
		handshakeCounter ++;
//	}
	write(STDOUT_FILENO, "\nHandshake aggiunto\n", sizeof("\nHandshake aggiunto\n"));
	return;
}


void sendResponse(char* response, int socketDescriptor, int resoult){
	char * sup = (char *) malloc(BUFFSIZE * sizeof(char *));
	if (resoult == 1){
		sprintf(sup, "%d", socketDescriptor);
		write(STDOUT_FILENO, sup, strlen(sup));

		char * messaggio = (char *) malloc (BUFFSIZE *sizeof(char));
		write(STDOUT_FILENO, "\ninvio risposta\n", sizeof("\ninvio risposta\n"));
		//calcolo della dimensione del messaggio
		sprintf(messaggio, "%ld", strlen(response)); //salvo la dimensione del restante messaggio in una stringa
		int dim = strlen(response) + strlen(messaggio); //sommo il numero di caratteri
		sprintf(messaggio, "%d", dim);//metto la somma in una stringa
		
		strcat(messaggio, response);//concateno il resto del messaggio alla dimensione
		strcat(messaggio, "\n");

		write(socketDescriptor, messaggio, dim); // sd1 identifica il client dal quale ha ricevuto il messaggio originale
		write(STDOUT_FILENO, "\nRisposta inviata\n", sizeof("\nRisposta inviata\n"));
		free(messaggio);
	} else {
		write(socketDescriptor, DATANOTMATCHING, 39);
	}
	
} 


//ANCHOR executeCommands
int executeCommands(struct CommandStructure command, int socketDescriptor){
	int isSuccessInt = 0;
	int resoult = 1;
	char *response = (char *) malloc (BUFFSIZE *sizeof(char)); 
	struct Node* node;
	char * sup = (char *) malloc(BUFFSIZE * sizeof(char *));

	system("clear");
	write(STDOUT_FILENO, "Esecuzione Comando\n\n", sizeof("Esecuzione Comando\n\n"));

	strcat(response, ":s:");
	strcat(response, "res:");

	sprintf(sup, "%d", socketDescriptor);

	switch (getInvokedCommand(command.command)) {
		case STORE:
			write(STDOUT_FILENO, "\n@STORE CASE\n", sizeof("@STORE CASE\n"));
			isSuccessInt = store(atoi(command.key), atoi(command.value));
			if (isSuccessInt == 1) {
				strcat(response, "success:");
				if (strcmp(command.sender, "c") == 0){
					resoult = forwardMessage(command);
				}
			}else{
				strcat(response, "error:");
				strcat(response, "KEY ALREADY EXISTS");
			}
			break;
		case LIST:                         // TODO IMPLEMENTARE LA VERA FUNZIONE LIST
			write(STDOUT_FILENO, "\n@LIST CASE\n", sizeof("@LIST CASE\n"));
			strcat(response, "list:");
			char *list = printList(head);
			strcat(response, list);
			//strcat(response, "\n\n");
			free(list);
			break;
		case SEARCH:                       // TODO IMPLEMENTARE LA VERA FUNZIONE SEARCH
			write(STDOUT_FILENO, "\n@SEARCH CASE\n", sizeof("@SEARCH CASE\n"));
			node = searchLocal(head, atoi(command.key));
			if(node != NULL) {
				strcat(response, "success:");
				char* key = intToString(node->key);
				char* value = intToString(node->value);
				strcat(response, key);
				strcat(response, ", ");
				strcat(response, value);
				free(key);
				free(value);
				if (strcmp(command.sender, "c") == 0){
					resoult = forwardMessage(command);
				}
			}else{
				strcat(response, "error:");
				strcat(response, "chiave non trovata");
			}
			break;
		case EXIT:                        // TODO PERCHÉ NON CI VA MAI?
			write(STDOUT_FILENO, command.message, strlen(command.message));
			write(socketDescriptor, CLOSING, 29);
			close(sd1); // chiude la connessione
			close(sd);
			exit(1);
			//break;
		case CORRUPT:   
			// TODO IMPLEMENTARE LA VERA FUNZIONE CORRUPT
			write(STDOUT_FILENO, "\n@CORRUPT CASE\n", sizeof("@CORRUPT CASE\n"));
			node = corrupt(atoi(command.key), atoi(command.value));
			if (node != NULL) {
				strcat(response, "success:");
				strcat(response, "KEY REPLACED SUCCESSFULLY");
			}else{
				strcat(response, "error:");
				strcat(response, "KEY DOESN'T EXIST");
			}
			break;
		case COMMANDO_NOT_FOUND:
			strcat(response, "error:");
			strcat(response, "COMMAND NOT FOUND");
			break;
	}
	
	sendResponse(response, socketDescriptor, resoult);

	//free(response);
	return 1;
}

struct CommandStructure getCommandStructure (char *buf){
	write(STDOUT_FILENO, "\n@@@getCommandStructure\n\n", sizeof("\n@@@getCommandStructure\n\n"));
	struct CommandStructure commandStr;
	char *p;
	char *sizeOfMessageStr;
	p = strtok (buf,":-");
	int counter = 0;
	while (p!= NULL){
		counter++;
		if (counter == 1){
			sizeOfMessageStr = p;
			commandStr.sizeOfMessage = atoi(sizeOfMessageStr);
		}else if (counter == 2)
		{
			p = strtok (NULL, ":-");
			commandStr.sender = p;
		}
		else if (counter == 3)
		{
			p = strtok (NULL, ":-");
			commandStr.type = p;
		}
		else if (strcmp(commandStr.type, "req") == 0){
			if (counter == 4)
			{
				p = strtok (NULL, ":-");
				commandStr.command = p;
				if (strstr(commandStr.command, "LIST")){
					break;
				}

			}else if (counter == 5)
			{
				p = strtok (NULL, ":-");
				commandStr.key = p;
				if (strstr(commandStr.command, "SEARCH")){
					break;
				}
				
			}else if (counter == 6)
			{
				p = strtok (NULL, ":-");
				commandStr.value = p;
			}else
			{
				//p = NULL; //!va in sig fault se decommentato
				break;
			}
		}else if (strcmp(commandStr.type, "res") == 0){
			if (counter == 4)
			{
				p = strtok (NULL, ":-");
				commandStr.resoult = p;

			}else if (counter == 5)
			{
				p = strtok (NULL, ":-");
				commandStr.message = p;
			}else
			{
				//p = NULL; //!va in sig fault se decommentato
				break;
			}

		} else {
			break;
		}
		 	
		
	}
	//	printCommandStructure(commandStr);
	//write(STDOUT_FILENO, "\n\n", sizeof("\n\n"));
	return commandStr;
}

void printCommandStructure(struct CommandStructure commandStr){
	write(STDOUT_FILENO, "\n@@@printCommandStructure\n\n", sizeof("\n@@@printCommandStructure\n\n"));
	char *sizeOfMessageStr = intToString(commandStr.sizeOfMessage);
	write(STDOUT_FILENO, "\ncommandStr.sizeOfMessage: ", sizeof("\ncommandStr.sizeOfMessage: "));
	write(STDOUT_FILENO, sizeOfMessageStr, strlen(sizeOfMessageStr));
	free(sizeOfMessageStr);

	write(STDOUT_FILENO, "\ncommandStr.sender: ", sizeof("\ncommandStr.sender: "));
	write(STDOUT_FILENO, commandStr.sender, strlen(commandStr.sender));

	write(STDOUT_FILENO, "\ncommandStr.type: ", sizeof("\ncommandStr.type: "));
	write(STDOUT_FILENO, commandStr.type, strlen(commandStr.type));

	write(STDOUT_FILENO, "\ncommandStr.command: ", sizeof("\ncommandStr.command: "));
	write(STDOUT_FILENO, commandStr.command, strlen(commandStr.command));

	if (!strstr(commandStr.command, "LIST")){
		write(STDOUT_FILENO, "\ncommandStr.key: ", sizeof("\ncommandStr.key: "));
		write(STDOUT_FILENO, commandStr.key, strlen(commandStr.key));

		if (!strstr(commandStr.command, "SEARCH")){
			write(STDOUT_FILENO, "\ncommandStr.value: ", sizeof("\ncommandStr.value: "));
			write(STDOUT_FILENO, commandStr.value, strlen(commandStr.value));
		}
	}
	write(STDOUT_FILENO, "\n\n", sizeof("\n\n"));
}

//ANCHOR intToString
//TODO non vengono mai liberate le stringhe. Eventualmente va tolta
char *intToString(int a){
	char *resStr =  (char *) malloc(sizeof(char) * 20);
	sprintf(resStr, "%d", a);
	return resStr;
}

//ANCHOR store
int store(int x, int y){
	if (isEmptyList == 1) {
		head = initList(x,y);
		isEmptyList = 0;
		return 1;
	}else{
		struct Node* tmpNode = (struct Node*) malloc (sizeof(struct Node*));
		tmpNode = storeLocal(head, x, y);

		if (tmpNode != NULL) {
			head = tmpNode;
			return 1;
		}
		return 0;
	}
}

//ANCHOR getInvokedCommand
enum functions getInvokedCommand(char* command){
	write(STDOUT_FILENO, "@@@getInvokedCommand\n", sizeof("@@@getInvokedCommand\n"));
	write(STDOUT_FILENO, command, strlen(command));
	if(strstr(command, "STORE")) {
		return STORE;
	}
	if(strstr(command, "SEARCH")) {
		return SEARCH;
	}
	if(strstr(command, "LIST")) {
		return LIST;
	}
	if(strstr(command, "CORRUPT")) {
		return CORRUPT;
	}
	if(strstr(command, "EXIT")) {
		return EXIT;
	}
	return COMMANDO_NOT_FOUND;
}

//ANCHOR initList
struct Node* initList(int key, int value){
	write(STDOUT_FILENO, "\nkey: ", sizeof("\nkey: "));
	write(STDOUT_FILENO, intToString(key), strlen(intToString(key)));
	write(STDOUT_FILENO, "\nvalue: ", sizeof("\nvalue: "));
	write(STDOUT_FILENO, intToString(value), strlen(intToString(value)));
	
	struct Node* head = NULL;
	head = (struct Node*)malloc(sizeof(struct Node));
	head->key = key;
	head->value = value;
	return head;
}

//ANCHOR storeLocal
struct Node* storeLocal(struct Node* nextNode, int key, int value){
	if(searchLocal(nextNode, key) == NULL) {
		struct Node* newNode = (struct Node*) malloc(sizeof(struct Node));
		newNode->value = value;
		newNode->key = key;
		newNode->next = nextNode;
		char *prova =  (char *) malloc(sizeof(char) * 128);
		sprintf(prova, "%d", newNode->key);
		write(STDOUT_FILENO, prova, strlen(prova));
		return newNode;
	}else{
		return NULL;
	}
}

//ANCHOR corrupt
struct Node* corrupt(int key, int value){
	struct Node* newNode = searchLocal(head, key);
	if(newNode != NULL) {
		newNode->value = value;
		newNode->key = key;
		return newNode;
	}else{
		return NULL;
	}
}

//ANCHOR printList
char *printList(struct Node* n) {
	write(STDOUT_FILENO, "\n\n@@@printList\n", sizeof("\n\n@@@printList\n"));
	char *list = (char *) malloc (1024 *sizeof(char)); 
	if (n == NULL){
		strcat(list, "There are no record\n");
		return list;
	}
	while (n != NULL) {
		char *key = intToString(n->key);
		char *value = intToString(n->value);
		strcat(list, key);
		strcat(list, ", ");
		strcat(list, value);
		strcat(list, "\n");
		n = n->next;
	}
	return list;
}

//ANCHOR searchLocal
struct Node* searchLocal(struct Node* head, int key){
	write(STDOUT_FILENO, "\n\n@@@searchLocal\n", sizeof("\n\n@@@searchLocal\n"));
	struct Node* cursor = head;
	
	while(cursor!=NULL) {
		if(cursor->key == key)
			return cursor;
		cursor = cursor->next;
	}
	return NULL;
}

//ANCHOR handler
void handler (int sig){
	if (sig==SIGINT) {
		struct CommandStructure Exit;
		Exit.sizeOfMessage = 34;
		Exit.sender = "s";
		Exit.type = "req";
		Exit.message = "chiusura del Server";
		Exit.command = "EXIT";
		Exit.key = (char *)NULL;
		Exit.value = (char *)NULL;
		write(STDOUT_FILENO, "\nchiusura del server", sizeof("\nchiusura del server"));
		forwardMessage(Exit);
		write(STDOUT_FILENO, "pipino", sizeof("pipino"));
		close(sd1);// chiude la connessione

		close(sd);// rende il servizio non raggiungibile
		exit(1);
	}
}

int forwardMessage(struct CommandStructure command){
	char * message = (char *) malloc (BUFFSIZE * sizeof(char *));
	char * sup = (char *) malloc (sizeof(int));
	char * sup1 = (char *) malloc (sizeof(int));
	struct Server *currentServer = serverListHead;
	struct Forward fwdMessage;
	struct ForwardList *fwdList;
	struct ForwardList *currFwdList;
	int connectResult; //utile al controllo errori sulle connessioni

	pthread_t threadId;
	fwdMessage.message = (char *) malloc(BUFFSIZE * sizeof(char *));
	fwdList = (struct ForwardList *) malloc(BUFFSIZE * sizeof(struct ForwardList*));
	currFwdList = fwdList;
	int resoult = 1;

	write(STDOUT_FILENO, "prova", sizeof("prova"));
	sprintf(sup, "%d", command.sizeOfMessage);
	strcat(fwdMessage.message, sup);
	strcat(fwdMessage.message, ":s:");
	strcat(fwdMessage.message, "req:");
	strcat(fwdMessage.message, command.command);
	if (command.key != (char *)NULL){
		write(STDOUT_FILENO, "prova", sizeof("prova"));
		strcat(fwdMessage.message, "-");
		strcat(fwdMessage.message, command.key);
	}
	
	if(command.value != (char *)NULL){
		write(STDOUT_FILENO, "prova", sizeof("prova"));
		strcat(fwdMessage.message, "-");
		strcat(fwdMessage.message, command.value);
	}
	

	while(currentServer != NULL){
		write(STDOUT_FILENO, "\nInoltro del comando a ", sizeof("\nInoltro del comando a "));
		sprintf (sup1, "%d", ntohs(currentServer->address.sin_port));
		write(STDOUT_FILENO, sup1, sizeof(int));
		write(STDOUT_FILENO, "\n", sizeof("  "));
		fwdMessage.server = currentServer;
		fwdMessage.size = command.sizeOfMessage;


		if(pthread_create(&threadId, NULL, forwardToServers, &fwdMessage) != 0){//crea un thread
			perror("errore thread");
		} else {
			pthread_join(threadId, NULL);
			currFwdList->fwd = fwdMessage;
			currentServer = currentServer->next;
			if (currentServer != NULL){
				currFwdList->next = (struct ForwardList *) malloc(BUFFSIZE * sizeof(struct ForwardList*));
				currFwdList = currFwdList->next;
			}
		}
	}
	resoult = checkForwardResult(fwdList, command.command);
	return resoult;
}

int checkForwardResult(struct ForwardList *forwardList, char *command){
	struct ForwardList *currFwdList = forwardList;
	int result = 1;
	if (strcmp(command, "LIST") == 0){
		write(STDOUT_FILENO, "il controllo di list è un casino, poi lo faccio", sizeof("il controllo di list è un casino, poi lo faccio"));
	} else {
		while (currFwdList != NULL){
			if (strcmp(currFwdList->fwd.response.resoult, "error") == 0){
				result == 0;
			}
			currFwdList = currFwdList->next;
		}
	}
	return result;
}

void *forwardToServers(void *arg){//bisogna definire una struct con il messaggio, l'sd e la risposta del server stesso
	struct Forward *fwd = (struct Forward *)arg;
	char * response = (char *) malloc(BUFFSIZE * sizeof(char *));
	int exitCondition = 1;
	int connectResult;
	fwd->server->socketDescriptor = socket(AF_INET, SOCK_STREAM, 0);//socket tcp tramite stream di dati, connection-oriented
    //tentativo di creazione della connessione
	connectResult = connect(fwd->server->socketDescriptor, (struct sockaddr *)&fwd->server->address, sizeof(fwd->server->address)); //connessione


	write(fwd->server->socketDescriptor, fwd->message, fwd->size);
	write(STDOUT_FILENO, fwd->message, fwd->size);
	write(STDOUT_FILENO, "\n", sizeof("\n"));
	//aspetta la risposta
	response = readFromPeer(fwd->server->socketDescriptor);
	write(STDOUT_FILENO, "risposta ricevuta", 18);
	//capire come inviare il ris dei confronti
	fwd->response = getCommandStructure(response);
	write(STDOUT_FILENO, "struct ricevuta", 18);
}