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

void readConfigFile(int);
struct Node* storeLocal(struct Node* nextNode, int key, int value);
int printList(struct Node* n);
struct Node* searchLocal(struct Node* head, int key);
struct Node* initList(int key, int value);
enum functions  getInvokedCommand(char* command);
void handler (int sig);
void runServer(int port);
int store(int x, int y);
char *intToString(int a);
int executeCommands(char * buf);

int sd, sd1;

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

struct Server {//?Ho pensato che creare una lista di server fosse la cosa più sensata
	struct sockaddr_in address;
	struct Server* next;
};

struct Node* head;
struct Server* serverListHead = NULL;
int isEmptyList = 1;

int main( int argc, const char* argv[]){
	int configFileDescriptor;

	//controllo sul numero di input
	if (argc < 3) {
		write(STDOUT_FILENO, "Necessari file config,  ip e porta per l'esecuzione\n", sizeof("Necessari file config,  ip e porta per l'esecuzione\n"));
		return 0;
	}

	configFileDescriptor = open(argv[1], O_RDONLY);//apertura del file di config
	
	readConfigFile(configFileDescriptor);//legge gli address dal file config



	int port = atoi(argv[1]);
	write(STDOUT_FILENO, argv[1], sizeof(int)); //Prendeva solo il primo numero perché scriveva un carattere solo
	write(STDOUT_FILENO, "\n\n", sizeof("\n\n"));
	int isEmptyList = 1;
	head = NULL;

	//TODO decommentare quando si implementeranno i server concorrenti
	//runServer(port); //!I server concorrenti sono più lanci del file, non più socket nello stesso processo
	runServer(5200);
	return 0;

}

//funzione che legge il file di config e crea una lista di server
//!NOT WORKING/NOT TESTED
//?strtok sputtana tutto il dio
//?strncpy pure
//entrambi è come se cambiassero il buffer di base
void readConfigFile(int fileDescriptor){
	char* buffer = (char*) malloc (64 * sizeof(char *));

	char* add = (char*) malloc (64 * sizeof(char *));
	char* porta = (char*) malloc (64 * sizeof(char *));
	char delim[] = ":";
	int charead;
	long port;
	struct Server* lastServer = NULL;
	serverListHead = (struct Server*) malloc (sizeof(struct Server*));

	while((charead = read(fileDescriptor, buffer, sizeof(buffer))) > 0){
		write(STDOUT_FILENO, buffer, charead);
		

		serverListHead = (struct Server*) malloc (sizeof(struct Server*));
		serverListHead->address.sin_family = AF_INET;
		//inet_pton(AF_INET, (add=strtok(buffer, ":")), &serverListHead->address.sin_addr);
		//port = atoi((porta =strtok(buffer, ":")));//argv[2] è sempre la porta
    	serverListHead->address.sin_port = htons(port);
		if(lastServer != NULL){
			lastServer->next = serverListHead;
		}
		lastServer=serverListHead;
	
	}

}

void runServer(int port){
	struct sockaddr_in address;
	struct sockaddr_in claddress;
	socklen_t dimaddcl = sizeof(claddress);

	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	address.sin_addr.s_addr = htonl(INADDR_ANY);

	signal (SIGINT, handler);
	sd = socket(AF_INET, SOCK_STREAM, 0);//socket TCP ipv4
	if (bind(sd, (struct sockaddr *) &address, sizeof(address)) < 0) {//assegna l'address al socket
		perror ("errore bind");
		exit(1);
	}
	listen(sd, 10); // rende il servizio raggiungibile

	write(STDOUT_FILENO, "listen\n", sizeof("listen\n"));

	int exitCondition = 1;
	while (exitCondition == 1) {
		char * buf = (char *) malloc (128 *sizeof(char));
		char * sup = (char *) malloc (8 *sizeof(char));
		sd1 = accept(sd, (struct sockaddr *) NULL, NULL);// estrae una richieta di connessione
		if (sd1>1) {
			write(STDOUT_FILENO, "accepted\n", sizeof("accepted\n"));

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

}

int executeCommands(char * buf){
	int isSuccessInt = 0;
	switch (getInvokedCommand(buf)) {
		case STORE:
			isSuccessInt = store(0, 0);
			char *isSuccessString;
			if (isSuccessInt == 1) {
				isSuccessString = "SUCCESS";
			}else{
				isSuccessString = "ERROR";
			}
			write(STDOUT_FILENO, isSuccessString, strlen(isSuccessString));
			break;
		case LIST:                         // TODO IMPLEMENTARE LA VERA FUNZIONE LIST
			isSuccessInt = printList(head);
			if (isSuccessInt == 0) {
				write(STDOUT_FILENO, "There are no record", sizeof("There are no record"));
			}
			break;
		case SEARCH:                       // TODO IMPLEMENTARE LA VERA FUNZIONE SEARCH
			if(searchLocal(head, 5) != NULL) {
				write(STDOUT_FILENO, "trovato il 5\n\n", sizeof("trovato il 5\n\n"));
			}else{
				write(STDOUT_FILENO, "non trovato il 5\n\n", sizeof("non trovato il 5\n\n"));
			}
			break;
		case EXIT:                         // TODO PERCHÉ NON CI VA MAI?
			close(sd1); // chiude la connessione
			return 0;
			break;
		case CORRUPT:   
			// TODO IMPLEMENTARE LA VERA FUNZIONE CORRUPT
			write(STDOUT_FILENO, "corrupt", sizeof("corrupt"));
			write(sd1, "corrato", sizeof("corrato"));//!ho scritto una cacata per testare
			break;
		case COMMANDO_NOT_FOUND:
			write(STDOUT_FILENO, "Command not found", sizeof("Command not found"));
			break;
	}
	return 1;
}
//TODO non vengono mai liberate le stringhe. Eventualmente va tolta
char *intToString(int a){
	char *resStr =  (char *) malloc(sizeof(char) * 20);
	sprintf(resStr, "%d", a);
	return resStr;
}

int store(int x, int y){
	if (isEmptyList == 1) {
		head = initList(0,0);
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

enum functions getInvokedCommand(char* command){
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

struct Node* initList(int key, int value){
	struct Node* head = NULL;
	head = (struct Node*)malloc(sizeof(struct Node));
	head->key = key;
	head->value = value;
	return head;
}

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

int printList(struct Node* n) {
	if (n == NULL){
		return 0;
	}
	while (n != NULL) {
		char *key = intToString(n->key);
		char *value = intToString(n->value);
		write(STDOUT_FILENO, key, sizeof(key));
		write(STDOUT_FILENO, ", ", sizeof(", "));
		write(STDOUT_FILENO, value, sizeof(value));
		n = n->next;
	}
	return 1;
}

struct Node* searchLocal(struct Node* head, int key){

	struct Node* cursor = head;
	while(cursor!=NULL) {
		if(cursor->key == key)
			return cursor;
		cursor = cursor->next;
	}
	return NULL;
}

void handler (int sig){
	if (sig==SIGINT) {
		close(sd1);// chiude la connessione

		close(sd);// rende il servizio non raggiungibile
		exit(1);
	}
}
