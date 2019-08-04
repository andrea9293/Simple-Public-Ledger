#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>

//funzione che controlla che siano inseriti abbastanza parametri per il comando
int checkCorrectInput(int,  char*);
//funzione che controlla che il comando inserito sia valido
int checkCorrectCommand(char*);
//funzione che crea il messaggio "comando-param1-param2" da inviare al server
char* createMessage(int, const char*[]);
// funzione per la lettura del messaggio inviato dal server
void readFromServer(int sd);

int main( int argc, const char* argv[]){
    
    int sd, sd1; //socket descriptor
    struct sockaddr_in address; //indirizzo del socket
    char * message = (char *) malloc (128 *sizeof(char)); //buffer per la comunicazione con il server
    int charead;//caratteri letti tramite read
    int error = 0;//variabile utile a controllo errori sull'input
   

   // Controlli di correttezza input
    if (argc < 4) { //3 è il minimo di parametri necessari: ip, porta, comando (argv[0] è il nome programma)
		write(STDOUT_FILENO, "inserire parametri necessari e riprovare\n", sizeof("inserire parametri necessari e riprovare\n"));
        return 0;
	} else{
        //controlliamo che il comando inserito sia valido
        error = checkCorrectCommand((char *)argv[3]);//? separato per puilizia
        if(error == 1){
            write(STDOUT_FILENO, "comando non riconosciuto", sizeof("comando non riconosciuto"));
            return 0;
        }
        //controlliamo che il numero di parametri sia corretto per il comando dato
        error = checkCorrectInput(argc, (char *) argv[3]); //? l'ho voluto separare in una funzione a parte per pulizia
        if (error == 1){
            write(STDOUT_FILENO, "Il comando inserito richiede più parametri di quelli inseriti\n", sizeof("Il comando inserito richiede più parametri di quelli inseriti\n"));
            return 0;
        }
    } 

    
    //definizione dell'address per la connessione
    address.sin_family = AF_INET;//famiglia dell'address del socket
    long port = atoi(argv[2]);//argv[2] è sempre la porta
    address.sin_port = htons(port);
    inet_pton(AF_INET, argv[1], &address.sin_addr); // argv[1] è sempre l'ip
    write(STDOUT_FILENO, "Tentativo di conessione al server\n", sizeof("Tentativo di conessione al server\n"));
    
    sd = socket(AF_INET, SOCK_STREAM, 0);//socket tcp tramite stream di dati, connection-oriented
    connect(sd, (struct sockaddr *)&address, sizeof(address)); //connessione
    
    //Invio del messaggio al server    
    strcpy(message, createMessage(argc, argv));

    write(STDOUT_FILENO, message, strlen(message));

    write(sd, message, strlen(message));
    
    /*charead = read(sd, message, sizeof(message));
    write(STDOUT_FILENO, message, charead); */
    readFromServer(sd);
    
    free(message);
    
    close(sd);// chiusura del socket

    return 0;
}

void readFromServer(int sd){
    int r;
    char *buf = (char *) malloc( 128 * sizeof(char *));
    char * messaggio = (char *) malloc( 128 * sizeof(char *));
    char * sup = (char *) malloc (128 *sizeof(char));

	//! tentativo di lettura START
	r = read (sd, buf, 128);
	strcpy(messaggio, buf);
	strcpy(sup, messaggio);
	int size = atoi(strtok(sup, ":"));
	write(STDOUT_FILENO, messaggio, size);
	printf("\n\nsize %d, r %d\n\n", size, r);
	if (size == (r-1)){
		write(STDOUT_FILENO, "dimensione corretta", sizeof("dimensione corretta"));
	} else if( size < r-1){
		write(STDOUT_FILENO, "letto troppo", sizeof("letto troppo"));
	} else {
		write(STDOUT_FILENO, "letto poco", sizeof("letto poco"));
	}

    free(buf);
    free(messaggio);
    free(sup);
}

//controlla che siano stati dati abbastanza parametri per il comando (torna 1 in caso di errore)
int checkCorrectInput(int argc, char* command){
    if ((strcmp(command, "STORE") == 0) && (argc != 6)){
        return 1;
    }
    if ((strcmp(command, "CORRUPT") == 0) && (argc != 6)){
        return 1;
    }
    if ((strcmp(command, "SEARCH") == 0) && (argc != 5) ){
        return 1;
    }

    return 0;
}

//controlla che il comando dato sia valido, ritorna 1 in caso di errore
int checkCorrectCommand(char* command){
    if ((strcmp(command, "STORE") == 0)){
        return 0;
    }
    if ((strcmp(command, "CORRUPT") == 0)){
        return 0;
    }
    if ((strcmp(command, "SEARCH") == 0) ){
        return 0;
    }
    if ((strcmp(command, "LIST") == 0) ){
        return 0;
    }

    return 1;
}

/* 
NOTE il protocollo funziona con un messaggio strutturato come segue
<size>:<sender>:<command-par1-par2>
ad esempio
8:c:LIST
*/
char* createMessage(int argc, const char* argv[] ){
    char * buf = (char *) malloc (512 *sizeof(char)); 
    char * messaggio = (char *) malloc (512 *sizeof(char));
    
    write(STDOUT_FILENO, "creazione del messaggio\n", sizeof("creazione del messaggio\n"));

    //inserisce nel messaggio il segnale del sender ed il comando
    strcat(buf, ":c:");
    strcat(buf, argv[3]);

    if (argc == 5){ //inserisce un parametro in caso ce ne sia solo uno
        strcat(buf, "-");
        strcat(buf, argv[4]);

    }
    else if(argc == 6){ //inserisce due parametri altrimenti
        strcat(buf, "-");
        strcat(buf, argv[4]);
        strcat(buf, "-");      
        strcat(buf, argv[5]);
    }
    //calcolo della dimensione del messaggio
    sprintf(messaggio, "%ld", strlen(buf)); //salvo la dimensione del restante messagigo in una stringa
    int dim = strlen(buf) + strlen(messaggio); //sommo il numero di caratteri
    sprintf(messaggio, "%d", dim);//metto la somma in una stringa
    strcat(messaggio, buf);//concateno il resto del messaggio alla dimensione
    strcat(messaggio, "\n");

    return messaggio;
}