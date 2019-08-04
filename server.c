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

//ANCHOR firma funzioni
void readConfigFile(int, char*);//Legge il config e salva gli indirizzi in una lista
void createConnection();
void *connectionToServer(void *);//apre le connesisoni
void *acceptConnection(void *);//accetta le connesisoni
struct Node* storeLocal(struct Node* nextNode, int key, int value);
char * printList(struct Node* n);
struct Node* searchLocal(struct Node* head, int key);
struct Node* initList(int key, int value);
enum functions  getInvokedCommand(char* command);
void handler (int sig);
pthread_t runServer(int port);
int store(int x, int y);
char *intToString(int a);
int executeCommands(char * buf);
struct CommandStructure getCommandStructure (char *buf);
void printCommandStructure(struct CommandStructure commandStr);
struct Node* corrupt(int key, int value);

int sd, sd1;
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
	char *key;
	char *value;
	char *command;
};

struct Server {//?Ho pensato che creare una lista di server fosse la cosa più sensata
	struct sockaddr_in address;
	struct Server* next;
	int socketDescriptor;
};

struct Node* head;
struct Server* serverListHead = NULL;
int isEmptyList = 1;
//ANCHOR main
int main( int argc, const char* argv[]){
	int configFileDescriptor;
	pthread_t serverThreadId;
	signal (SIGINT, handler); //assegnazione dell'handler

	/*		STARTUP		*/
	//controllo sul numero di input
	if (argc < 3) {
		write(STDOUT_FILENO, "Necessari file config,  ip e porta per l'esecuzione\n", sizeof("Necessari file config,  ip e porta per l'esecuzione\n"));
		return 0;
	}
	configFileDescriptor = open(argv[1], O_RDONLY);//apertura del file di config
	readConfigFile(configFileDescriptor,(char *) argv[2]);//legge gli address dal file config

	int port = atoi(argv[1]);
	write(STDOUT_FILENO, argv[2], sizeof(int)); //Prendeva solo il primo numero perché scriveva un carattere solo
	write(STDOUT_FILENO, "\n\n", sizeof("\n\n"));
	int isEmptyList = 1;
	head = NULL;

	//TODO decommentare quando si implementeranno i server concorrenti
	//runServer(port); //!I server concorrenti sono più lanci del file, non più socket nello stesso processo
	serverThreadId = runServer(atoi(argv[2])); //attivazione del socket in ricezione
	createConnection(); //creazione delle connessioni agli altri server
	pthread_join(serverThreadId, NULL); //attende la fine del thread server
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

   //lettura degli address e separazione in tokens
   	while(read(fileDescriptor, buffer, addressBufferSize) > 0) { //finché vengono letti indirizzi
		add = strtok(buffer, delim); //stringa prima del delimitatore
   		porta = strtok(NULL, delim); //stringa dopo il delimitatore
		if (strncmp(porta, selfPort, 4) != 0){ //evita che il server salvi se stesso
			write(STDOUT_FILENO, "creazione server ", sizeof("creazione server "));
			write(STDOUT_FILENO, add, strlen(add));
			write(STDOUT_FILENO, ":", sizeof(":"));
			write(STDOUT_FILENO, porta, strlen(porta));		
			
			
			currentServer->address.sin_family = AF_INET;//famiglia dell'address del socket
			port = atoi(porta);//conversione della porta a long
			currentServer->address.sin_port = htons(port);//assegnazione porta
			inet_pton(AF_INET, add, &currentServer->address.sin_addr);//assegnazione address
			
			write(STDOUT_FILENO, "fine creazione server", sizeof("fine creazione server"));
			write(STDOUT_FILENO, "\n", sizeof("\n"));

			if(lastServer != NULL){ //l'head non ha predecessori
				lastServer->next = currentServer; //assegnzione del corrente al next precedente
			}
			lastServer = currentServer; //salviamo il current come precedente
			currentServer = (struct Server *) malloc (sizeof(struct Server*));//allocazione dell'elemento della lista
		}
		
   }
	return;
}
//ANCHOR createConnection
//crea i thread per le connessioni
void createConnection(){
	struct Server *currentServer = serverListHead;
	pthread_t threadId;

	write(STDOUT_FILENO, "Stabilimento delle connessioni\n", sizeof("Stabilimento delle connessioni\n"));
	while(currentServer != NULL){//finché vi sono server salvati
		if(pthread_create(&threadId, NULL, connectionToServer, currentServer) != 0){//crea un thread
        	perror("errore thread");
		} else {
			pthread_join(threadId, NULL); //join del thread con quello padre.
		}
		
		currentServer = currentServer->next;
	}
	write(STDOUT_FILENO, "Stabilite connessioni con tutti i server\n", sizeof("Stabilite connessioni con tutti i server\n"));
}

//ANCHOR connectionToServer
//apre le connesisoni con gli altri server
void *connectionToServer(void *server){
	int connectResult; //utile al controllo errori sulle connessioni
	struct Server* currentServer = (struct Server *)server; //cast del parametro passato
	char* buffer = (char*) malloc (BUFFSIZE * sizeof(char *));//buffer in lettura


	write(STDOUT_FILENO, "tentativo di connessione a:", sizeof("tentativo di connessione a:"));
	inet_ntop(AF_INET, &(currentServer->address.sin_addr), buffer, INET_ADDRSTRLEN);
	write(STDOUT_FILENO, buffer, strlen(buffer));
	write(STDOUT_FILENO, ":", sizeof(":"));
	sprintf(buffer, "%d", ntohs(currentServer->address.sin_port));
	write(STDOUT_FILENO, buffer, strlen(buffer));
	//connessione al socket assegnato in input
	currentServer->socketDescriptor = socket(AF_INET, SOCK_STREAM, 0);//socket tcp tramite stream di dati, connection-oriented
    connectResult = connect(currentServer->socketDescriptor, (struct sockaddr *)&currentServer->address, sizeof(currentServer->address)); //connessione
	while(connectResult != 0){ //finché la connessione non è staiblita
		write(STDOUT_FILENO, "\nconnessione non riuscita, nuovo tentativo", sizeof("\nconnessione non riuscita, nuovo tentativo"));
		write(STDOUT_FILENO, "\n", sizeof("\n"));
		sleep(10);
		connectResult = connect(currentServer->socketDescriptor, (struct sockaddr *)&currentServer->address, sizeof(currentServer->address)); //connessione
	}
	write(STDOUT_FILENO, "\n", sizeof("\n"));

}

//ANCHOR runServer
//! NOT PROPERLY TESTED
pthread_t runServer(int port){
	struct sockaddr_in address;
	struct sockaddr_in claddress;//TODO cancellare
	socklen_t dimaddcl = sizeof(claddress);//TODO cancellare

	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	address.sin_addr.s_addr = htonl(INADDR_ANY);

	sd = socket(AF_INET, SOCK_STREAM, 0);//socket TCP ipv4
	if (bind(sd, (struct sockaddr *) &address, sizeof(address)) < 0) {//assegna l'address al socket
		perror ("errore bind");
		exit(1);
	}
	listen(sd, 15); // rende il servizio raggiungibile

	write(STDOUT_FILENO, "listening...\n", sizeof("listening...\n"));

	pthread_t threadId;
	int exitCondition = 1;
//	while (exitCondition == 1) {
		//char * buf = (char *) malloc (128 *sizeof(char));
		//char * sup = (char *) malloc (8 *sizeof(char));
		if(pthread_create(&threadId, NULL, acceptConnection, NULL) != 0){//crea un thread
			perror("errore thread");
		} else {
			write(STDOUT_FILENO, "thread creato\n", sizeof("thread creato\n"));
			return threadId;
		}
		
	
			/* write(STDOUT_FILENO, "accepted\n", sizeof("accepted\n"));

			int r = read (sd1, buf, sizeof(buf));
			if (r>0) {
				int add = getpeername(sd1, (struct sockaddr *)&claddress, &dimaddcl);
				strcpy (sup, inet_ntoa(claddress.sin_addr));
				write(STDOUT_FILENO, sup, strlen(sup));
				write(STDOUT_FILENO, ": ", sizeof(": "));
				write(STDOUT_FILENO, buf, r *sizeof(char));
				exitCondition = executeCommands(buf);
				write(STDOUT_FILENO, "\n\n", sizeof("\n\n"));
			}

			free(buf);
			free(sup);

		}
		close(sd1);// chiude la connessione
	}

	close(sd);// rende il servizio non raggiungibile
	exit(1);
*/
//	}
}
//ANCHOR acceptConnection
//accetta le connessioni in attesa
void *acceptConnection(void *arg){
	char * messaggio = (char *) malloc( BUFFSIZE * sizeof(char *));
	char * buf = (char *) malloc( BUFFSIZE * sizeof(char *));
	char * sup = (char *) malloc (BUFFSIZE *sizeof(char));
	struct sockaddr_in claddress;
	socklen_t dimaddcl = sizeof(claddress);
	int exitCondition = 1;
	while (exitCondition == 1) {
		

		

		sd1 = accept(sd, (struct sockaddr *) NULL, NULL);// estrae una richieta di connessione
		if (sd1>1) { //in caso l'accettazione sia andata a buon fine
			int add = getsockname(sd1, (struct sockaddr *)&claddress, &dimaddcl);
			strcpy (sup, inet_ntoa(claddress.sin_addr));
			write(STDOUT_FILENO, "Connessione accettata da: ", sizeof("connessione accettata da:"));
			write(STDOUT_FILENO, sup, strlen(sup));
			write(STDOUT_FILENO, ":", sizeof(":"));
			sprintf(sup, "%d", ntohs(claddress.sin_port));
			write(STDOUT_FILENO, sup, strlen(sup));
			write(STDOUT_FILENO, "\n", sizeof("\n"));
			int r;

			//! tentativo di lettura START
			r = read (sd1, buf, BUFFSIZE);
			strcpy(sup, buf);
			int size = atoi(strtok(sup, ":"));
			write(STDOUT_FILENO, buf, r);
			printf("\n\nsize %d, r %d\n\n", size, r);
			if (size == r){
				write(STDOUT_FILENO, "dimensione corretta", sizeof("dimensione corretta"));
				strcpy(messaggio, buf);

			} else if( size < r){
				strncpy(messaggio, buf, size);

			} else {
				while (size > r){
					r = read (sd1, buf, BUFFSIZE);
					strncpy(sup, buf, size - (r-1));
					strcat(messaggio, sup);
					write(STDOUT_FILENO, messaggio, size);
				}
			}

			
			
			

			//! tentativo di lettura END
			exitCondition = executeCommands(messaggio);
			//free(buf);
			free(messaggio);
			free(sup);

		}
		close(sd1);// chiude la connessione
	}
}

void sendResponse(char* response){
	char * messaggio = (char *) malloc (BUFFSIZE *sizeof(char));

	//calcolo della dimensione del messaggio
    sprintf(messaggio, "%ld", strlen(response)); //salvo la dimensione del restante messaggio in una stringa
    int dim = strlen(response) + strlen(messaggio); //sommo il numero di caratteri
    sprintf(messaggio, "%d", dim);//metto la somma in una stringa
    write(STDOUT_FILENO, messaggio, strlen(messaggio));
	strcat(messaggio, response);//concateno il resto del messaggio alla dimensione
    strcat(messaggio, "\n");


	write(sd1, messaggio, dim); // sd1 identifica il client dal quale ha ricevuto il messaggio originale
	int charead = read(sd, messaggio, sizeof(response)); //?se è più di un carattere sarebbe meglio strlen
    write(STDOUT_FILENO, messaggio, charead); 
	free(messaggio);
}

//ANCHOR executeCommands
int executeCommands(char * buf){
	struct CommandStructure command = getCommandStructure(buf);
	write(STDOUT_FILENO, "@@@executeCommmands\n\n", sizeof("@@@executeCommmands\n\n"));
	int isSuccessInt = 0;
	char *response = (char *) malloc (BUFFSIZE *sizeof(char)); 
	strcat(response, ":s:");
	struct Node* node;
	switch (getInvokedCommand(command.command)) {
		case STORE:
			write(STDOUT_FILENO, "\n@STORE CASE\n", sizeof("@STORE CASE\n"));
			strcat(response, "STORE RESPONSE");
			isSuccessInt = store(atoi(command.key), atoi(command.value));
			if (isSuccessInt == 1) {
				strcat(response, "success");

			}else{
				strcat(response, "ERROR: KEY ALREADY EXISTS");
			}
			//strcat(response, "\n\n");
			
			break;
		case LIST:                         // TODO IMPLEMENTARE LA VERA FUNZIONE LIST
			write(STDOUT_FILENO, "\n@LIST CASE\n", sizeof("@LIST CASE\n"));
			strcat(response, "LIST RESPONSE");
			char *list = printList(head);
			strcat(response, list);
			//strcat(response, "\n\n");
			free(list);
			break;
		case SEARCH:                       // TODO IMPLEMENTARE LA VERA FUNZIONE SEARCH
			write(STDOUT_FILENO, "\n@SEARCH CASE\n", sizeof("@SEARCH CASE\n"));
			node = searchLocal(head, atoi(command.key));
			strcat(response, "SEARCH RESPONSE");
			if(node != NULL) {
				char* key = intToString(node->key);
				char* value = intToString(node->value);
				strcat(response, key);
				strcat(response, ", ");
				strcat(response, value);
				free(key);
				free(value);
				
			}else{
				strcat(response, "chiave non trovata");
			}
			//strcat(response, "\n\n");
			write(STDOUT_FILENO, "\n\n", sizeof("\n\n"));			
			break;
		case EXIT:                         // TODO PERCHÉ NON CI VA MAI?
			close(sd1); // chiude la connessione
			return 0;
			break;
		case CORRUPT:   
			// TODO IMPLEMENTARE LA VERA FUNZIONE CORRUPT
			write(STDOUT_FILENO, "\n@CORRUPT CASE\n", sizeof("@CORRUPT CASE\n"));
			strcat(response, "CORRUPT RESPONSE");
			node = corrupt(atoi(command.key), atoi(command.value));
			if (node != NULL) {
				strcat(response, "KEY REPLACED SUCCESSFULLY");
			}else{
				strcat(response, "ERROR: KEY NOT EXISTS");
			}
			//strcat(response, "\n\n");

			break;
		case COMMANDO_NOT_FOUND:
			write(STDOUT_FILENO, "Command not found", sizeof("Command not found"));
			break;
	}
	write(STDOUT_FILENO, response, strlen(response));
	sendResponse(response);
	free(response);
	return 1;
}

struct CommandStructure getCommandStructure (char *buf){
	write(STDOUT_FILENO, "\n@@@getCommandStructure\n\n", sizeof("\n@@@getCommandStructure\n\n"));
	struct CommandStructure commandStr;
	char *p;
	char *sizeOfMessageStr;
	p = strtok (buf,":-");
	int counter = 0;
	while (p!= NULL)
	
	{
		counter++;
		if (counter == 1){
			sizeOfMessageStr = p;
			commandStr.sizeOfMessage = atoi(sizeOfMessageStr);
		}else if (counter == 2)
		{
			p = strtok (NULL, ":-");
			commandStr.sender = p;
		}else if (counter == 3)
		{
			p = strtok (NULL, ":-");
			commandStr.command = p;
			if (strstr(commandStr.command, "LIST")){
				break;
			}

		}else if (counter == 4)
		{
			p = strtok (NULL, ":-");
			commandStr.key = p;
			if (strstr(commandStr.command, "SEARCH")){
				break;
			}
			
		}else if (counter == 5)
		{
			p = strtok (NULL, ":-");
			commandStr.value = p;
		}else
		{
			write(STDOUT_FILENO, "\n\nsono a 6 quindi metto p a NULL\n\n ", sizeof("\n\nsono a 6 quindi metto p a NULL\n\n "));
			//p = NULL; //!va in sig fault se decommentato
			break;
		}	
		
	}
	printf ("\n\nfine ciclo\ncounter: %d\n", counter);
	printCommandStructure(commandStr);
	write(STDOUT_FILENO, "\n\n", sizeof("\n\n"));
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
		struct Node* tmpNode = storeLocal(head,  x, y);
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
	
	printf("value: %d:", value);
	struct Node* head = NULL;
	head = (struct Node*)malloc(sizeof(struct Node));
	head->key = key;
	head->value = value;
	return head;
}

//ANCHOR storeLocal
struct Node* storeLocal(struct Node* nextNode, int key, int value){
	if(searchLocal(nextNode, key) == NULL) {
		struct Node* newNode = (struct Node*)malloc(sizeof(struct Node));
		newNode->value = value;
		newNode->key = key;
		newNode->next = nextNode;
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
		close(sd1);// chiude la connessione

		close(sd);// rende il servizio non raggiungibile
		exit(1);
	}
}
