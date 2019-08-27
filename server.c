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

#define BUFFSIZE 128 //dimensione del buffer per i messaggi
#define HANDSHAKE "11:s:hands" //messaggio standard di handshake
//messaggio standard di errore inviato al client se la richiesta arriva prima della fine dello startup
#define STARTUPNOTFINISHED "36:s:res:error:startup not finished" 
//messaggio standard di errore inviato in caso ci siano errori di congruenza tra dati nel forward
#define DATANOTMATCHING "39:s:res:error:incongruenze tra dati"
//messaggio standard di risposta in caso di inoltro della chiusura
#define CLOSING "29:s:res:success:spegnimento"

//ANCHOR strutture
//enum dei comandi possibili
enum functions {
	STORE,
	SEARCH,
	CORRUPT,
	LIST,
	EXIT,
	COMMANDO_NOT_FOUND
};

//nodo della lista di elementi da memorizzare in locale
struct Node {
	int key;
	int value;
	struct Node* next;
};

//struttura del messaggio
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

//lista dei server letta da config.txt
struct Server {
	struct sockaddr_in address;
	struct Server* next;
	int socketDescriptor;
};

//struttura utile a mandare i dati necessari al forward al thread di invio
struct Forward {
	char *message;
	int size;
	struct CommandStructure response;
	struct Server *server;
};

//lista dei risultati ottenuti dal forward
struct ForwardList {
	struct Forward fwd;
	struct ForwardList *next;
};

//ANCHOR firma funzioni
void readConfigFile(int, char*);//Legge il config e salva gli indirizzi in una lista
void createConnection();//crea le connessioni con gli altri server
void *connectionToServer(void *);//apre le connesisoni
void *acceptConnection(void *);//accetta le connesisoni
char *readFromPeer(int); //ritorna il messaggio letto dal socket
struct Node* storeLocal(struct Node*, int, int); //memorizza in locale i dati inviati tramite comando store
char *printList(struct Node*);//stampa la lista locale 
struct Node* searchLocal(struct Node*, int);//ricerca locale dei dati richiesti
struct Node* initList(int, int);//inizializza la lista di dati locali
enum functions getInvokedCommand(char*);//restituisce il tipo di comando inviato
void handler (int);//handler per i segnali
void runServer(int);//apre il socket in ricezione e apre un thread per ogni connessione accettata
int store(int, int);//funzione di store
char *intToString(int);//conversione intero a stringa
int executeCommands(struct CommandStructure, int);//esegue il comando arrivato dal client/server
struct CommandStructure getCommandStructure (char *);//scompone il messaggio nelle sue parti
struct Node* corrupt(int, int);//esegue la funzione di corrupt
int forwardMessage(struct CommandStructure, char *);//forward ai server
void *forwardToServers(void *);//funzione per il thread che invia il messaggio ai vari server
int checkForwardResult(struct ForwardList *, char *);//controlla i risultati del forward
void sendResponse(char*, int, int);//invia la risposta ad i messaggi arrivati

int sd; //socket Descriptor 
int handshakeCounter = 0; //counter degli handshake ricevuti
int serverNumber; //il numero di server presenti in config.txt
struct Node* head;//head della lista di valori in locale
struct Server* serverListHead = NULL;//head della lista di server
int isEmptyList = 1;
int selfPort;//porta del server corrente

//ANCHOR main
int main(int argc, const char* argv[]){
	int configFileDescriptor;
	pthread_t serverThreadId;
	signal (SIGINT, handler); //assegnazione dell'handler
	write(STDOUT_FILENO, "--- Inizio fase di Start-up del server ---\n", sizeof("--- Inizio fase di Start-up del server ---\n"));
	//controllo sul numero di input
	if (argc < 3) {
		write(STDOUT_FILENO, "Errore durante lo Start-up: Necessari file config, ip e porta per l'esecuzione\n", sizeof("Errore durante lo Start-up: Necessari file config,  ip e porta per l'esecuzione\n"));
		return 0;
	}
	configFileDescriptor = open(argv[1], O_RDONLY);//apertura del file di config
	readConfigFile(configFileDescriptor,(char *) argv[2]);//legge gli address dal file config

	write(STDOUT_FILENO, "Lettura del file di configurazione avvenuta con successo.\n", sizeof("lettura del file di configurazione avvenuta con successo.\n\n"));
	selfPort = atoi(argv[1]);
	int isEmptyList = 1;
	head = NULL;

	
	runServer(atoi(argv[2])); //attivazione del socket in ricezione
	return 0;

}

//funzione che legge il file di config e crea una lista di server
//ANCHOR readConfigFile
void readConfigFile(int fileDescriptor, char* selfPort){
	int addressBufferSize = 15;//dimensione del buffer di lettura. La dimensione di address:porta
	char* buffer = (char*) malloc (addressBufferSize * sizeof(char *));//buffer in lettura
	char* address = (char*) malloc (addressBufferSize * sizeof(char *));//stringa di supporto per salvare l'add
	char* porta = (char*) malloc (addressBufferSize * sizeof(char *));//stringa di supporto per salvare la porta
	const char delim[2] = ":";//delimitatore per lo strtok
	long port; //versione long del numero di porta da assegnare all'elemento della lista
	struct Server* currentServer =  NULL; //per scorrere la lista
	struct Server* lastServer = NULL; //per salvare il server precedente
	
	serverListHead = (struct Server *) malloc (sizeof(struct Server*));//allocazione dell'elemento della lista
	currentServer = serverListHead;//assegnazione del cursore
	
	write(STDOUT_FILENO, "\nLettura del file di configurazione...\n", sizeof("\nLettura del file di configurazione...\n"));

   //lettura degli address e separazione in tokens
   	while(read(fileDescriptor, buffer, addressBufferSize) > 0) { //finché vengono letti indirizzi
		address = strtok(buffer, delim); //stringa prima del delimitatore
   		porta = strtok(NULL, delim); //stringa dopo il delimitatore
		if (strncmp(porta, selfPort, 4) != 0){ //evita che il server salvi se stesso	
			currentServer->address.sin_family = AF_INET;//famiglia dell'address del socket
			port = atoi(porta);//conversione della porta a long
			currentServer->address.sin_port = htons(port);//assegnazione porta
			inet_pton(AF_INET, address, &currentServer->address.sin_addr);//assegnazione address
			

			if(lastServer != NULL){ //l'head non ha predecessori
				lastServer->next = currentServer; //assegnzione del corrente al next precedente
			}

			lastServer = currentServer; //salviamo il current come precedente
			currentServer = (struct Server *) malloc (sizeof(struct Server*));//allocazione dell'elemento della lista

			serverNumber ++;//aumento del counter di server
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

	
	write(STDOUT_FILENO, "\nStabilimento delle connessioni con gli altri server\n", sizeof("\nStabilimento delle connessioni con gli altri server\n"));
	
	while(currentServer != NULL){//finché vi sono server salvati
		if(pthread_create(&threadId, NULL, connectionToServer, currentServer) != 0){//crea un thread
			perror("errore thread");
		} else {
			//pthread_join(threadId, NULL);
		}
		//inserisce il nuovo elemento nella lista e passa al prossimo
		lastServer = currentServer;
		currentServer = currentServer->next;
		lastServer->next = currentServer;
	}
	write(STDOUT_FILENO, "Stabilite connessioni con tutti i server\n", sizeof("Stabilite connessioni con tutti i server\n"));
}

//ANCHOR connectionToServer
//apre le connesisoni con gli altri server
void *connectionToServer(void *server){
	int connectResult; //utile al controllo errori sulle connessioni
	char* response = (char*) malloc (BUFFSIZE * sizeof(char *));//buffer per la risposta
	char* addressString = (char*) malloc (BUFFSIZE * sizeof(char *));//stringa per la stampa dell'address
	struct Server * currentServer = (struct Server *) server;
	struct CommandStructure responseStructure;

	write(STDOUT_FILENO, "\ntentativo di connessione a:", sizeof("\ntentativo di connessione a:"));
	inet_ntop(AF_INET, &(currentServer->address.sin_addr), addressString, INET_ADDRSTRLEN);
	write(STDOUT_FILENO, addressString, strlen(addressString));
	write(STDOUT_FILENO, ":", sizeof(":"));
	sprintf(addressString, "%d", ntohs(currentServer->address.sin_port));
	write(STDOUT_FILENO, addressString, strlen(addressString));

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
		write(currentServer->socketDescriptor, HANDSHAKE, sizeof(HANDSHAKE));//invio dell'handshake
		response = readFromPeer(currentServer->socketDescriptor);//legge la risposta
		responseStructure = getCommandStructure(response);//scompone la risposta

		//se la risposta è un handshake stampa
		if (strcmp(responseStructure.type, "hands") == 0){
			write(STDOUT_FILENO, "\nConnessione stabilita con:", sizeof("\nConnessione stabilita con:"));
			inet_ntop(AF_INET, &(currentServer->address.sin_addr), addressString, INET_ADDRSTRLEN);
			write(STDOUT_FILENO, addressString, strlen(addressString));
			write(STDOUT_FILENO, ":", sizeof(":"));
			sprintf(addressString, "%d", ntohs(currentServer->address.sin_port));
			write(STDOUT_FILENO, addressString, strlen(addressString));
			write(STDOUT_FILENO, "\n", sizeof("\n"));
		}
		
	}
}

//ANCHOR runServer
void runServer(int port){
	struct sockaddr_in address; //address del server
	struct sockaddr_in claddress; //address del peer connesso
	socklen_t dimaddcl = sizeof(claddress); //dimensione dell'address
	char * addressString = (char *) malloc(BUFFSIZE * sizeof(char *)); //stringa per stampare l'address
	int socketDescriptor;//socket descriptor
	pthread_t threadId;
	int exitCondition = 1;

	
	write(STDOUT_FILENO, "\nApertura del socket in ricezione...\n", sizeof("\nApertura del socket in ricezione...\n"));

	//struttura dell'address
	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	address.sin_addr.s_addr = htonl(INADDR_ANY);

	sd = socket(AF_INET, SOCK_STREAM, 0);//socket TCP ipv4
	if (bind(sd, (struct sockaddr *) &address, sizeof(address)) < 0) {//assegna l'address al socket
		perror ("errore bind");//in caso il bind non vada a buon fine il programma termina
		exit(1);
	}
	listen(sd, 20); // rende il servizio raggiungibile

	write(STDOUT_FILENO, "Socket aperto...\n", sizeof("Socket aperto...\n"));

	createConnection();//prova a connettersi e inviare handshake agli altri server

	while (exitCondition == 1) {
		socketDescriptor = accept(sd, (struct sockaddr *) NULL, NULL);// estrae una richieta di connessione
		if (socketDescriptor>1) { //in caso l'accettazione sia andata a buon fine
			
			write(STDOUT_FILENO, "Accettata\n", sizeof("accettata\n"));
			int add = getpeername(socketDescriptor, (struct sockaddr *)&claddress, &dimaddcl);
			strcpy (addressString, inet_ntoa(claddress.sin_addr));
			write(STDOUT_FILENO, "Connessione accettata da: ", sizeof("connessione accettata da:"));
			write(STDOUT_FILENO, addressString, strlen(addressString));
			write(STDOUT_FILENO, ":", sizeof(":"));
			sprintf(addressString, "%d", ntohs(claddress.sin_port));
			write(STDOUT_FILENO, addressString, strlen(addressString));
			write(STDOUT_FILENO, "\n", sizeof("\n"));

			if(pthread_create(&threadId, NULL, acceptConnection, &socketDescriptor) != 0){//crea un thread
					perror("errore thread");
			} else {
				//pthread_join(threadId, NULL);
			}
		}
	}
}

//lettura dal socket
char * readFromPeer(int socketDescriptor){
	char * messaggio = (char *) malloc(BUFFSIZE * sizeof(char *));
	char * buffer = (char *) malloc(BUFFSIZE * sizeof(char *));
	char * support = (char *) malloc(BUFFSIZE * sizeof(char *));
	int exitCondition = 1;
	int r = 0;
	int size = 0;
	int sumSize = 0;

	while (exitCondition == 1) {
		r = read (socketDescriptor, buffer, BUFFSIZE);//lettura dal socket
		strcpy(support, buffer);//copia nella stringa di supporto per lo strtok
		size = atoi(strtok(support, ":"));//ottiene la dimensione
		if (size == r){//in caso corrisponda al numero di caratteri letti
			strcpy(messaggio, buffer);//copia in messaggio ed esce dal ciclo
			exitCondition = 0;
		} else if( size < r){//se ha letto più del necessario
			strncpy(messaggio, buffer, size);//copia la sottostringa ed esce
			exitCondition = 0;
		} else {
			//altrimenti ripete la lettura fino a pareggiare il numero di caratteri letti in totale
			//e la dimensione del messaggio inviato
			sumSize = r;
			while ((size > sumSize) && (r > 0)){
				r = read (socketDescriptor, buffer, 128);
				sumSize += r;
				strncpy(support, buffer, size - (r-1));
				strcat(messaggio, support);
			}
			exitCondition = 0;
		}
	}
	free(buffer);
	return messaggio;
}
//ANCHOR acceptConnection
//accetta le connessioni in attesa
void *acceptConnection(void *arg){
	char * messaggio = (char *) malloc(BUFFSIZE * sizeof(char *));
	char * support = (char *) malloc(BUFFSIZE * sizeof(char *));
	struct sockaddr_in claddress;
	socklen_t dimaddcl = sizeof(claddress);
	int exitCondition = 1;
	int *socketDescriptor = (int *) arg;
	
	messaggio = readFromPeer(*socketDescriptor);//legge dal socket
	struct CommandStructure command = getCommandStructure(messaggio);//ottiene la struttura

	if(strcmp(command.type, "req") == 0){//in caso sia una richiesta
		if (handshakeCounter < serverNumber){//ma lo startup non è finito
			//invia la risposta
			write(*socketDescriptor, STARTUPNOTFINISHED, 36);
		} else {
			write(STDOUT_FILENO, "comando ricevuto", sizeof("comando ricevuto"));
			exitCondition = executeCommands(command, *socketDescriptor);//esegue il comando
		}
	} else if(strcmp(command.type, "hands") == 0) {//in caso di arrivo di un handshake
		write(STDOUT_FILENO, "\nHandshake Ricevuto\n", sizeof("\nHandshake Ricevuto\n"));
		write(*socketDescriptor, HANDSHAKE, sizeof(HANDSHAKE));//invio dell'handshake
		handshakeCounter ++;
		if (handshakeCounter == serverNumber){
			write(STDOUT_FILENO, "\nFase di Start-up completata\n", 30);
		}
		//setHandShake(claddress.sin_port);
	}

	free(messaggio);
	close(*socketDescriptor);// chiude la connessione
}

//invia la risposta in base al risultato del forward, se c'è stato
void sendResponse(char* response, int socketDescriptor, int resoult){
	char * messaggio = (char *) malloc (BUFFSIZE *sizeof(char));
	if (resoult == 1){
		write(STDOUT_FILENO, "Invio risposta...\n", sizeof("invio risposta...\n"));
		//calcolo della dimensione del messaggio
		sprintf(messaggio, "%ld", strlen(response)); //salvo la dimensione del restante messaggio in una stringa
		int dim = strlen(response) + strlen(messaggio); //sommo il numero di caratteri
		sprintf(messaggio, "%d", dim);//metto la somma in una stringa
		
		strcat(messaggio, response);//concateno il resto del messaggio alla dimensione
		strcat(messaggio, "\n");
		write(socketDescriptor, messaggio, dim); //invia la risposta
		write(STDOUT_FILENO, "Risposta inviata\n", sizeof("Risposta inviata\n"));
		free(messaggio);
	} else {
		write(socketDescriptor, DATANOTMATCHING, 39);
	}
	
} 

//ANCHOR executeCommands
int executeCommands(struct CommandStructure command, int socketDescriptor){
	int isSuccessInt = 0;//per controllare il risultato del comando
	int resoult = 1;//per controllare il risultato del forward
	char *response = (char *) malloc (BUFFSIZE *sizeof(char)); 
	struct Node* node;

	write(STDOUT_FILENO, "\nEsecuzione Comando\n\n", sizeof("\nEsecuzione Comando\n\n"));

	//concatena una parte del comando
	strcat(response, ":s:");
	strcat(response, "res:");

	//in base al tipo di comando ricevuto
	switch (getInvokedCommand(command.command)) {
		case STORE:
			isSuccessInt = store(atoi(command.key), atoi(command.value));
			if (isSuccessInt == 1) {//in caso di successo
				strcat(response, "success:");
				strcat(response, "Successfully stored");
				if (strcmp(command.sender, "c") == 0){//se la richiesta viene da un client
					resoult = forwardMessage(command, response);//inoltra il comando
				}
			}else{
				//concatena l'errore
				strcat(response, "error:");
				strcat(response, "KEY ALREADY EXISTS");
			}
			break;
		case LIST:
			strcat(response, "list:\n");//parametro di risultato specifico per la lista
			char *list = printList(head); //valorizza il char con la lista
			strcat(response, list);//concatena la lista alla risposta
			free(list);
			break;
		case SEARCH:
			node = searchLocal(head, atoi(command.key));//ritorna il nodo cercato, se esiste
			if(node != NULL) {//se è trovato
				//concatena i dati alla risposta
				strcat(response, "success:");
				char* key = intToString(node->key);
				char* value = intToString(node->value);
				strcat(response, key);
				strcat(response, ", ");
				strcat(response, value);
				free(key);
				free(value);
				if (strcmp(command.sender, "c") == 0){//se il comando arriva dal client lo inoltra
					resoult = forwardMessage(command, response);
				}
			}else{//concatena il messaggio d'errore alla risposta
				strcat(response, "error:");
				strcat(response, "chiave non trovata");
			}
			break;
		case EXIT://comando in arrivo da altri server per lo spegnimento di tutto il sistema
			system("clear");                        
			write(STDOUT_FILENO, command.message, strlen(command.message));
			write(socketDescriptor, CLOSING, 29);
			close(socketDescriptor); // chiude la connessione
			close(sd);
			exit(1);
		case CORRUPT:   
			node = corrupt(atoi(command.key), atoi(command.value));//chiama la corrupt e ritorna il nodo
			if (node != NULL) {//in caso ci sia viene concatenato il successo
				strcat(response, "success:");
				strcat(response, "KEY REPLACED SUCCESSFULLY");
			}else{//concatena messaggio d'errore
				strcat(response, "error:");
				strcat(response, "KEY DOESN'T EXIST");
			}
			break;
			//in caso il comando non sia trovato
		case COMMANDO_NOT_FOUND:
			strcat(response, "error:");
			strcat(response, "COMMAND NOT FOUND");
			break;
	}
	//invia la risposta
	sendResponse(response, socketDescriptor, resoult);

	//free(response);
	return 1;
}

//scompone il messaggio
struct CommandStructure getCommandStructure (char *buf){
	struct CommandStructure commandStr;
	char *part;
	char *sizeOfMessageStr;
	int counter = 0;


	//prende il primo token
	part = strtok (buf,":-");
	
	while (part!= NULL){//finché trova token
		counter++;
		if (counter == 1){//primo token ottenuto
			sizeOfMessageStr = part;
			commandStr.sizeOfMessage = atoi(sizeOfMessageStr);
		}else if (counter == 2)//secondo token
		{
			part = strtok (NULL, ":-");
			commandStr.sender = part;
		}
		else if (counter == 3)//terzo token
		{
			part = strtok (NULL, ":-");
			commandStr.type = part;
		}
		//in base se sia una richiesta, una risposta o un handshake
		else if (strcmp(commandStr.type, "req") == 0){
			if (counter == 4)//quarto token
			{
				part = strtok (NULL, ":-");
				commandStr.command = part;
				if (strstr(commandStr.command, "LIST")){//se il comando è list non ci sono altri vlaori
					commandStr.key = NULL;
					commandStr.value = NULL;
					break;
				}

			}else if (counter == 5) //quinto token
			{
				part = strtok (NULL, ":-");
				commandStr.key = part;
				if (strstr(commandStr.command, "SEARCH")){//se il comando è search c'è un solo valore
					commandStr.value = NULL;
					break;
				}
				
			}else if (counter == 6)//sesto token
			{
				part = strtok (NULL, ":-");
				commandStr.value = part;
			}else
			{//non ci sono altri valori
				break;
			}
		}else if (strcmp(commandStr.type, "res") == 0){//in caso di risposta
			if (counter == 4)//quarto token
			{
				part = strtok (NULL, ":-");
				commandStr.resoult = part;

			}else if (counter == 5)//quinto token
			{
				part = strtok (NULL, ":-");
				commandStr.message = part;
			}else//fine messaggio
			{
				commandStr.command = NULL;
				commandStr.key = NULL;
				commandStr.value = NULL;
				break;
			}

		} else {//fine messaggio
			commandStr.command = NULL;
			commandStr.key = NULL;
			commandStr.value = NULL;
			commandStr.message = NULL;
			commandStr.resoult = NULL;
			break;
		}
		 	
		
	}
	return commandStr;
}

//ANCHOR intToString
char *intToString(int a){
	char *resStr =  (char *) malloc(sizeof(char) * 20);
	sprintf(resStr, "%d", a);
	return resStr;
}

//ANCHOR store
int store(int x, int y){//funzione store
	if (isEmptyList == 1) {//se la lista è vuota
		head = initList(x,y);//la inizializza con i nuovi valori
		isEmptyList = 0;//setta il flag
		return 1;//ritorna successo
	}else{
		//alloca il nuovo nodo
		struct Node* tmpNode = (struct Node*) malloc (sizeof(struct Node*));
		tmpNode = storeLocal(head, x, y);//memorizza in locale

		if (tmpNode != NULL) {//se il risultato non è nullo 
			head = tmpNode;//mette il nuovo nodo a head e ritorna successo
			return 1;
		}
		return 0;//altrimenti torna errore
	}
}

//ANCHOR getInvokedCommand
enum functions getInvokedCommand(char* command){//ritorna il comando ricevuto
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
struct Node* initList(int key, int value){//iniziaizza la lista
	//alloca la testa
	struct Node* head = NULL;
	head = (struct Node*)malloc(sizeof(struct Node));
	//valorizza la testa
	head->key = key;
	head->value = value;
	return head;//ritorna la testa
}

//ANCHOR storeLocal
struct Node* storeLocal(struct Node* nextNode, int key, int value){
	if(searchLocal(nextNode, key) == NULL) {//in caso il nodo non esista
		struct Node* newNode = (struct Node*) malloc(sizeof(struct Node));//alloca il nodo
		//valorizza il nodo
		newNode->value = value;
		newNode->key = key;
		newNode->next = nextNode;
		//ritorna il nodo
		return newNode;
	}else{
		return NULL;//torna null in caso contrario
	}
}

//ANCHOR corrupt
struct Node* corrupt(int key, int value){
	struct Node* newNode = searchLocal(head, key);//cerca il nodo
	if(newNode != NULL) {//se esiste lo modifica
		newNode->value = value;
		newNode->key = key;
		return newNode;
	}else{//null altrimenti
		return NULL;
	}
}

//ANCHOR printList
char *printList(struct Node* node) {
	char *list = (char *) malloc (1024 *sizeof(char)); 

	if (node == NULL){//se il nodo non esiste
		strcat(list, "There are no record\n");
		return list;
	}
	while (node != NULL) {
		//valorizza le stringhe
		char *key = intToString(node->key);
		char *value = intToString(node->value);
		//concatena al messaggio che andrà in risposta
		strcat(list, key);
		strcat(list, ", ");
		strcat(list, value);
		strcat(list, "\n");
		//avanza di nodo
		node = node->next;
	}
	return list;//ritorna la lista di valori
}

//ANCHOR searchLocal
struct Node* searchLocal(struct Node* head, int key){//ricerca locale
	struct Node* cursor = head;
	
	//finché il cursore non è nullo
	while(cursor!=NULL) {
		if(cursor->key == key)//se il cursore corrente ha chiave pari a quella cercata
			return cursor;//ritorna il nodo corrente
		cursor = cursor->next;//scorre
	}
	return NULL;//in caso non trovi nulla torna null
}

//ANCHOR handler
void handler (int sig){//handler segnali
	if (sig==SIGINT) {
		//crea la struttura del comando exit da inoltrare
		struct CommandStructure Exit;
		Exit.sizeOfMessage = 34;
		Exit.sender = "s";
		Exit.type = "req";
		Exit.message = "chiusura del Server";
		Exit.command = "EXIT";
		Exit.key = (char *)NULL;
		Exit.value = (char *)NULL;
		write(STDOUT_FILENO, "\nchiusura del server", sizeof("\nchiusura del server"));
		forwardMessage(Exit, NULL);//chiama l'inoltro a tutti i server

		close(sd);// rende il servizio non raggiungibile
		exit(1);
	}
}

int forwardMessage(struct CommandStructure command, char *response){//inoltro del comando agli altri server
	char * message = (char *) malloc (BUFFSIZE * sizeof(char *));
	char * sizeString = (char *) malloc (sizeof(int));
	char * address = (char *) malloc (sizeof(int));
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

	write(STDOUT_FILENO, "Preparazione per l'inoltro dei messaggio\n", sizeof ("Preparazione per l'inostro dei messaggio\n"));
	//crea il messaggio da inoltrare partendo da quello ricevuto 
	sprintf(sizeString, "%d", command.sizeOfMessage);
	strcat(fwdMessage.message, sizeString);
	strcat(fwdMessage.message, ":s:");
	strcat(fwdMessage.message, "req:");
	strcat(fwdMessage.message, command.command);
	if (command.key != NULL){//se vi è una key, nel comando la concatena
		strcat(fwdMessage.message, "-");
		strcat(fwdMessage.message, command.key);
	}
	
	if(command.value != NULL){//se vi è valore nel comando lo concatena
		strcat(fwdMessage.message, "-");
		strcat(fwdMessage.message, command.value);
	}
	
	//inserisce il risultato locale alla lista per i confronti
	if (response != NULL){
		strcpy(message, "24");
		strcat(message, response);
		currFwdList->fwd.response = getCommandStructure(message);
	}
	//currFwdList->fwd.server->address.sin_port = selfPort;
	currFwdList->next = (struct ForwardList *) malloc(BUFFSIZE * sizeof(struct ForwardList*));
	currFwdList = currFwdList->next;

	//finché ci sono server a cui non è stato inviato il messaggio, nella lista
	while(currentServer != NULL){
		write(STDOUT_FILENO, "\nInoltro del comando a ", sizeof("\nInoltro del comando a "));
		sprintf (address, "%d", ntohs(currentServer->address.sin_port));
		write(STDOUT_FILENO, address, sizeof(int));
		write(STDOUT_FILENO, "\n", sizeof("  "));

		fwdMessage.server = currentServer;
		fwdMessage.size = command.sizeOfMessage;

		//crea un thread per lo scambio del messaggio
		if(pthread_create(&threadId, NULL, forwardToServers, &fwdMessage) != 0){//crea un thread
			perror("errore thread");
		} else {
			pthread_join(threadId, NULL);//attende la chiusura del thread
			currFwdList->fwd = fwdMessage;//aggiunge il messaggio alla lista dei forward
			currentServer = currentServer->next;//passa al prossimo server
			if (currentServer != NULL){//in caso esista aggiunge elementi alla lista dei forward
				currFwdList->next = (struct ForwardList *) malloc(BUFFSIZE * sizeof(struct ForwardList*));
				currFwdList = currFwdList->next;
			}
		}
	}
	if (strcmp(command.command, "EXIT")!= 0){
		//controlla il risultati
		resoult = checkForwardResult(fwdList, command.command);
	}
	//ritorna quanto ottenuto
	return resoult;
}

int checkForwardResult(struct ForwardList *forwardList, char *command){//confronta i risultati degli inoltri
	struct ForwardList *currFwdList = forwardList;
	struct ForwardList *firstResoult;
	int result = 1;

	if (strcmp(command, "SEARCH") == 0){//in caso il comando sia di lista
		firstResoult = currFwdList;
		currFwdList = currFwdList->next;
		while (currFwdList != NULL){//finché ci sono elementi nella lista inoltri
			if (strcmp(currFwdList->fwd.response.resoult, "error") == 0){//se vi sono errori
				result = 0;//ritorna errore
			} else if(strcmp(currFwdList->fwd.response.message, firstResoult->fwd.response.message) != 0){
				//confronto tutti i risultati con quello locale, in caso di discordanze
				result = 0;//torna errore
			}
			currFwdList = currFwdList->next;//altrimenti avanza
		}
	} else {
		while (currFwdList != NULL){//finché ci sono elementi nella lista inoltri
			if (strcmp(currFwdList->fwd.response.resoult, "error") == 0){//se vi sono errori
				result == 0;//ritorna errore
			}
			currFwdList = currFwdList->next;//altrimenti avanza
		}
	}
	return result;//ritorna il risultato
}

void *forwardToServers(void *arg){//bisogna definire una struct con il messaggio, l'sd e la risposta del server stesso
	struct Forward *fwd = (struct Forward *)arg;
	char * response = (char *) malloc(BUFFSIZE * sizeof(char *));
	int exitCondition = 1;
	int connectResult;
	//crea il socket
	fwd->server->socketDescriptor = socket(AF_INET, SOCK_STREAM, 0);//socket tcp tramite stream di dati, connection-oriented
    //tentativo di creazione della connessione
	connectResult = connect(fwd->server->socketDescriptor, (struct sockaddr *)&fwd->server->address, sizeof(fwd->server->address)); //connessione
	if (connectResult == 0){//in caso di successo
		write(fwd->server->socketDescriptor, fwd->message, fwd->size);//invia il comando
		
		//aspetta la risposta
		response = readFromPeer(fwd->server->socketDescriptor);
		write(STDOUT_FILENO, "risposta ricevuta\n", 19);
		
		fwd->response = getCommandStructure(response);//scompone il messaggio
	} else {
		write(STDOUT_FILENO, "Errore connessione", sizeof("Errore connessione"));
	}
	
}