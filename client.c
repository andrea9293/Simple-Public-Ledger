#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>


int main( int argc, const char* argv[]){
    // TODO SISTEMARE I CHECK PER I VARI PARAMETRI IN INGRESSO DALLA RIGA DI COMANDO
    if (argc == 1) {
		write(STDOUT_FILENO, "inserire parametri necessari e riprovare\n", sizeof("inserire parametri necessari e riprovare\n"));
		return 0;
	}

    int sd, sd1;
    struct sockaddr_in address;
    char * buf = (char *) malloc (8 *sizeof(char));
   
    
    address.sin_family = AF_INET;
    address.sin_port = htons(5200);
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr); // TODO METTERE L'IP PASSATO DALLA RIGA DI COMANDO
    int exitCondition = 1;
    while (exitCondition == 1){
        sd = socket(AF_INET, SOCK_STREAM, 0);//socket tcp tramite stream di dati, connection-oriented
        connect(sd, (struct sockaddr *)&address, sizeof(address));
        
        write(STDOUT_FILENO, "inserire comando: ", sizeof("inserire comando: "));
        int r = read (STDIN_FILENO, buf, sizeof(buf));
        write(sd, buf, sizeof(buf));
       
        if (strcmp(buf, "exit\n") == 0){
            write(STDOUT_FILENO, "exit", sizeof("exit"));
            exitCondition = 0;
            //break;
        }
    }
    
    close(sd);// rende il servizio non raggiungibile
    exit(1);



    return 0;
}