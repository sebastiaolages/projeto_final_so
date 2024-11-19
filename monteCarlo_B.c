#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h> 
#include <sys/file.h>

#define MAX_POINTS 10000
#define LINE_BUFFER_SIZE 256 

typedef struct {
    double x;
    double y;
} Point;

const char *nomeArquivoResultados = "resultados.txt";

bool processarArquivo(const char *nomeArquivo, Point *points, int *num_points);
void generateAndTestPoints(Point polygon[], int n, int num_pontos, int *pointsInside);

/**
 * @brief Determines the orientation of an ordered triplet (p, q, r).
 * @param p First point of the triplet.
 * @param q Second point of the triplet.
 * @param r Third point of the triplet.
 * @return 0 if p, q, and r are colinear, 1 if clockwise, 2 if counterclockwise.
 */
int orientation(Point p, Point q, Point r) {
    double val = (q.y - p.y) * (r.x - q.x) - (q.x - p.x) * (r.y - q.y);

    if (val == 0) return 0;
    return (val > 0)? 1: 2;
}

/**
 * @brief Checks if point q lies on line segment pr.
 * @param p First point of the line segment.
 * @param q Point to check.
 * @param r Second point of the line segment.
 * @return true if point q lies on line segment pr, else false.
 */
bool onSegment(Point p, Point q, Point r) {
    if (q.x <= fmax(p.x, r.x) && q.x >= fmin(p.x, r.x) &&
        q.y <= fmax(p.y, r.y) && q.y >= fmin(p.y, r.y))
       return true;

    return false;
}

/**
 * @brief Checks if line segments p1q1 and p2q2 intersect.
 * @param p1 First point of the first line segment.
 * @param q1 Second point of the first line segment.
 * @param p2 First point of the second line segment.
 * @param q2 Second point of the second line segment.
 * @return true if line segments p1q1 and p2q2 intersect, else false.
 */
bool doIntersect(Point p1, Point q1, Point p2, Point q2) {

    int o1 = orientation(p1, q1, p2);
    int o2 = orientation(p1, q1, q2);
    int o3 = orientation(p2, q2, p1);
    int o4 = orientation(p2, q2, q1);

    if (o1 != o2 && o3 != o4)
        return true;

    
    if (o1 == 0 && onSegment(p1, p2, q1)) return true;

    
    if (o2 == 0 && onSegment(p1, q2, q1)) return true;

  
    if (o3 == 0 && onSegment(p2, p1, q2)) return true;

    
    if (o4 == 0 && onSegment(p2, q1, q2)) return true;

    return false;
}

/**
 * @brief Checks if a point p is inside a polygon of n points.
 * @param polygon[] Array of points forming the polygon.
 * @param n Number of points in the polygon.
 * @param p Point to check.
 * @return true if the point p is inside the polygon, else false.
 */
bool isInsidePolygon(Point polygon[], int n, Point p) {

    if (n < 3) return false;

    Point extreme = {2.5, p.y};

    int count = 0, i = 0;
    do {
        int next = (i+1)%n;

        if (doIntersect(polygon[i], polygon[next], p, extreme)) {
            if (orientation(polygon[i], p, polygon[next]) == 0)
               return onSegment(polygon[i], p, polygon[next]);
            count++;
        }
        i = next;
    } while (i != 0);

    return count&1;
}

/**
 * Main function
 */
int main(int argc, char *argv[]) {
	srand(time(NULL));

    if (argc != 4) {
        printf("Uso: %s <nome_do_arquivo> <numero_de_processos> <numero_de_pontos>\n", argv[0]);
        return 1;
    }

    Point points[MAX_POINTS];
    int num_points = 0;
    int num_processos = atoi(argv[2]);
    int num_pontos = atoi(argv[3]);

    if (!processarArquivo(argv[1], points, &num_points)) {
        printf("Falha ao processar o arquivo.\n");
        return 1;
    }

    if (num_points < 3) {
        printf("Não há pontos suficientes para formar um polígono.\n");
        return 1;
    }

    int pointsInside = 0;
    pid_t pids[num_processos];
    int pointsPerProcess = num_pontos / num_processos;
    int status;

    int fd_resultados = open(nomeArquivoResultados, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_resultados == -1) {
        perror("Erro ao abrir o arquivo de resultados");
        return 1;
    }

    for (int i = 0; i < num_processos; i++) {
        pids[i] = fork();
        if (pids[i] < 0) {
            perror("Fork falhou");
            exit(1);
        }

        if (pids[i] == 0) { // Código do processo filho
            
            srand(time(NULL) ^ (getpid() << 16));
            int childPointsInside = 0;
            generateAndTestPoints(points, num_points, pointsPerProcess, &childPointsInside);
            
            char resultado[100];
            snprintf(resultado, sizeof(resultado), "%d;%d;%d\n",
                     getpid(), pointsPerProcess, childPointsInside);

            
            flock(fd_resultados, LOCK_EX);
            // Escrever no arquivo de resultados
            write(fd_resultados, resultado, strlen(resultado));
            // Desbloquear o arquivo
            flock(fd_resultados, LOCK_UN);

            close(fd_resultados); // Fechar o descritor de arquivo no filho
            exit(0); // Terminar o processo filho
        }
    }

    // Código do processo pai
    // Esperar que todos os processos filhos terminem
    for (int i = 0; i < num_processos; i++) {
        waitpid(pids[i], NULL, 0);
    }

    

    close(fd_resultados);
    return 0;
}

//FUNÇÃO PROCESSAR ARQUIVO - REQUISITO A
bool processarArquivo(const char *nomeArquivo, Point *points, int *num_points) {
    int fd = open(nomeArquivo, O_RDONLY);
    if (fd == -1) {
        perror("Erro ao abrir o arquivo");
        return false;
    }

    char line[LINE_BUFFER_SIZE];
    ssize_t bytes_read;
    char *token, *endptr;
    *num_points = 0;

    
    while (*num_points < MAX_POINTS && (bytes_read = read(fd, line, LINE_BUFFER_SIZE - 1)) > 0) {
        
        line[bytes_read] = '\0';

       
        char *new_line_ptr = strchr(line, '\n');
        if (new_line_ptr) {
            *new_line_ptr = '\0';
        }

       
        token = strtok(line, ",");
        if (token == NULL) {
            break;
        }
        points[*num_points].x = strtod(token, &endptr);
        token = strtok(NULL, ",");
        if (token == NULL) {
            break;
        }
        points[*num_points].y = strtod(token, &endptr);

      
        (*num_points)++;

        
        if (new_line_ptr && *(new_line_ptr + 1) != '\0') {
            lseek(fd, -((bytes_read - (new_line_ptr - line)) - 1), SEEK_CUR);
        }
    }

    if (bytes_read == -1) {
        perror("Erro ao ler o arquivo");
        close(fd);
        return false;
    }

    close(fd);
    return true;
}


void generateAndTestPoints(Point polygon[], int n, int num_pontos, int *pointsInside) {
    for(int i = 0; i < num_pontos; i++) {
        Point p = {((double)rand()/RAND_MAX)*2 - 1, ((double)rand()/RAND_MAX)*2 - 1};
        if(isInsidePolygon(polygon, n, p)) {
            (*pointsInside)++;
        }
    }
}
















