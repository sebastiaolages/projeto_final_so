#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h> // added this header to use isdigit function
#include <arpa/inet.h> // added this header to use inet_addr function

#define SERVER_PORT 8080
#define BUFSIZE 8096
#define TIMER_START() gettimeofday(&tv1, NULL);
#define TIMER_STOP() gettimeofday(&tv2, NULL); timersub(&tv2, &tv1, &tv); time_delta = (float)tv.tv_sec + tv.tv_usec / 1000000.0
#define TIMER_START_GERAL() gettimeofday(&tv3, NULL);
#define TIMER_STOP_GERAL() gettimeofday(&tv4, NULL); timersub(&tv4, &tv3, &tv5); time_delta_geral = (float)tv5.tv_sec + tv5.tv_usec / 1000000.0;

void pexit(char * msg)
{
	perror(msg);
	exit(1);
}

void report_ficheiro(int pid, int bsn, int rsn, int rc, float t) {
    int file = open("report.txt", O_WRONLY | O_CREAT | O_APPEND, 0777);
    if (file == -1) {
        perror("Erro ao abrir o arquivo de relatÃ³rio.\n");
        exit(1);
    }

    char buffer[50];
    sprintf(buffer, "%d %d %d %d %f\n", pid, bsn, rsn, rc, t);
    write(file, buffer, strlen(buffer));

    close(file);
}

void client(int N, int M, char *argv[]){ // added argv as a parameter to pass the command line arguments
    
    int tempo_maior_batch[N/M];
    int tempo_menor_batch[N/M];
    int tempo_maior = 0, tempo_menor = 999;

    for (int i = 0; i < N/M; ++i) {
        
        tempo_maior_batch[i] = 0;
        tempo_menor_batch[i] = 999;

        printf("Batch numero %d:\n", i + 1);
        for (int j = 0; j < M; ++j) {
            
            int tempos_pedidos_batch[M];
            pid_t pid = fork();  
            
            if (pid == -1) {  
                pexit("Erro na criacao de um processo.\n");
            }
            else if (pid == 0) {  
                //printf("%d - Eu sou o processo filho = %d. Eu sou o processo pai = %d.\n", j + 1, getpid(), getppid());
                // child process
                int sockfd;
                char buffer[BUFSIZE];
                static struct sockaddr_in serv_addr;
                struct timeval tv1, tv2, tv;
                float time_delta;
                
                // initialize the server address
                serv_addr.sin_family = AF_INET;
                serv_addr.sin_addr.s_addr = inet_addr(argv[1]); // convert IP address to network byte order
                serv_addr.sin_port = htons(atoi(argv[2])); // convert port number to network byte order
                
                if (argv[3] == NULL){ // check if the third argument is null
                    printf("client trying to connect to IP = %s PORT = %s\n",argv[1],argv[2]);
                    sprintf(buffer,"GET /index.html HTTP/1.0 \r\n\r\n");
                    /* Note: spaces are delimiters and VERY important */
                }
                else{
                    printf("client trying to connect to IP = %s PORT = %s retrieving FILE= %s\n",argv[1],argv[2], argv[3]);
                    sprintf(buffer,"GET /%s HTTP/1.0 \r\n\r\n", argv[3]);
                    /* Note: spaces are delimiters and VERY important */
                }
                
                TIMER_START();
                if((sockfd = socket(AF_INET, SOCK_STREAM,0)) <0)
                    pexit("Erro ao criar o socket.\n");
                if(connect(sockfd, (struct sockaddr *)&serv_addr,sizeof(serv_addr)) <0)
                    pexit("Erro ao conectar-se ao servidor.\n");
                write(sockfd,buffer,strlen(buffer));
                read(sockfd,buffer,BUFSIZE);
                TIMER_STOP();

                tempos_pedidos_batch[j] = time_delta;
                
                if(tempos_pedidos_batch[j] > tempo_maior_batch[i]){
                    tempo_maior_batch[i] = tempos_pedidos_batch[j];
                }

                if(tempos_pedidos_batch[j] < tempo_menor_batch[i]){
                    tempo_menor_batch[i] = tempos_pedidos_batch[j];
                }

                close(sockfd);
                
                // parse the response code from the buffer
                int rc = -1; // default value for error
                char *ptr = strtok(buffer, " "); // split by space
                while (ptr != NULL) {
                    if (isdigit(ptr[0])) { // check if the token is a number
                        rc = atoi(ptr); // convert to integer
                        break;
                    }
                    ptr = strtok(NULL, " "); // get next token
                }
                
                // call the report function with the process id, batch number, request number, response code and time elapsed
                report_ficheiro(getpid(), i + 1, j + 1, rc, time_delta);
                
                exit(0); // terminate the child process
            }
        }
        for (int j = 0; j < M; ++j) {
            wait(NULL); // wait for all child processes to finish
        }
    } 

    for (int w = 0; w < N/M; ++w) {
        
        if(tempo_maior_batch[w] > tempo_maior){
            tempo_maior = tempo_maior_batch[w];
        }

        if(tempo_menor_batch[w] > tempo_menor){
            tempo_menor = tempo_menor_batch[w];
        }
    }

    printf("Maior tempo de um pedido: %d.\n", tempo_maior);
    printf("Menor tempo de um pedido: %d.\n", tempo_menor);
}



int main(int argc,char *argv[])
{
    int N,M;
    struct timeval tv3, tv4, tv5;
    float time_delta_geral;
    if(argc!=5){
        printf("Usage: ./client <SERVER IP ADDRESS> <LISTENING PORT> <N> <M>\n");
        printf("Example: ./client 127.0.0.1 8141 10 5\n");
        exit(1);
    }
    N = atoi(argv[3]); // convert the third argument to integer
    M = atoi(argv[4]); // convert the fourth argument to integer

    TIMER_START_GERAL()

    client(N, M, argv); // call the client function with N, M and argv
    
    TIMER_STOP_GERAL()

    int tempo_total = time_delta_geral;
    int tempo_medio = tempo_total / N;
    
    printf("Tempo total de todos os pedidos: %d.\n", tempo_total);
    printf("Tempo medio de todos os pedidos: %d.\n", tempo_medio);



    return 0; // return 0 to indicate successful termination
}
