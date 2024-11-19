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

#define MAX_POINTS 10000
#define LINE_BUFFER_SIZE 256 

typedef struct {
    double x;
    double y;
} Point;

bool processarArquivo(const char *nomeArquivo, Point *points, int *num_points);
void generateAndTestPoints(Point polygon[], int n, int num_pontos, int pipefd, bool isVerbose);
ssize_t readn(int fd, void *ptr, size_t n);
ssize_t writen(int fd, const void *ptr, size_t n);
void display_progress(int current, int total);

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

    if (argc != 5) {
        printf("Uso: %s <nome_do_arquivo> <numero_de_processos> <numero_de_pontos> <modo_verboso>\n", argv[0]);
        return 1;
    }

    Point points[MAX_POINTS];
    int num_points = 0;
    int num_processos = atoi(argv[2]);
    int num_pontos = atoi(argv[3]);
    bool isVerbose = atoi(argv[4]);

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("Erro ao criar pipe");
        return 1;
    }

    if (!processarArquivo(argv[1], points, &num_points)) {
        printf("Falha ao processar o arquivo.\n");
        return 1;
    }

    if (num_points < 3) {
        printf("Não há pontos suficientes para formar um polígono.\n");
        return 1;
    }

    pid_t pids[num_processos];
    int progress = 0;
    for (int i = 0; i < num_processos; i++) {
        pids[i] = fork();
        if (pids[i] < 0) {
            perror("Fork falhou");
            exit(1);
        }

        if (pids[i] == 0) {
            close(pipefd[0]); 
            generateAndTestPoints(points, num_points, num_pontos / num_processos, pipefd[1], isVerbose);
            close(pipefd[1]);
            exit(0);
        }
    }

    close(pipefd[1]); // Fecha o lado de escrita no processo pai

    int totalPointsInside = 0;  
    int processedPoints = 0;    

    if (isVerbose) {
        char buffer[100];
        while (readn(pipefd[0], buffer, sizeof(buffer)) > 0) {
            printf("Dados: %s", buffer);  
            totalPointsInside++;  
            processedPoints++;
            display_progress(processedPoints, num_pontos);
        }
    } else {
        int pointsRead;
        while (readn(pipefd[0], &pointsRead, sizeof(pointsRead)) > 0) {
            totalPointsInside += pointsRead;
            processedPoints += pointsRead;
            display_progress(processedPoints, num_pontos);
        }
    }

    close(pipefd[0]);

    
    display_progress(num_pontos, num_pontos);

    
    double squareArea = 4.0;
    double polygonArea = squareArea * ((double)totalPointsInside / num_pontos);
    printf("\nÁrea estimada do polígono: %f\n", polygonArea);

 
    for (int i = 0; i < num_processos; i++) {
        waitpid(pids[i], NULL, 0);
    }

    return 0;
}


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

 
    while ((bytes_read = read(fd, line, LINE_BUFFER_SIZE - 1)) > 0) {
        line[bytes_read] = '\0';

        
        token = strtok(line, ",");
        while (token != NULL && *num_points < MAX_POINTS) {
            points[*num_points].x = strtod(token, &endptr);
            if (*endptr != '\0') break;  
            token = strtok(NULL, ",");
            if (token == NULL) break;
            points[*num_points].y = strtod(token, &endptr);
            if (*endptr != '\0') break;  
            token = strtok(NULL, ",");
            (*num_points)++;
        }
    }

    close(fd);

    if (*num_points < 3) {
        printf("Erro: o arquivo deve conter pelo menos 3 pontos para formar um polígono.\n");
        return false;
    }

    return true;
}

void generateAndTestPoints(Point polygon[], int n, int num_pontos, int pipefd, bool isVerbose) {
    int pointsInside = 0;
    char buffer[100];

    for (int i = 0; i < num_pontos; i++) {
        Point p = {(double)rand() / RAND_MAX * 2 - 1, (double)rand() / RAND_MAX * 2 - 1}; // Gera ponto aleatório entre -1 e 1

        if (isInsidePolygon(polygon, n, p)) {
            pointsInside++;
            if (isVerbose) {
                snprintf(buffer, sizeof(buffer), "%d;%f;%f\n", getpid(), p.x, p.y);
                writen(pipefd, buffer, strlen(buffer));
            }
        }
    }

    if (!isVerbose) {
        writen(pipefd, &pointsInside, sizeof(pointsInside));
    }
}

ssize_t readn(int fd, void *ptr, size_t n) {
    size_t nleft;
    ssize_t nread;

    nleft = n;
    while (nleft > 0) {
        if ((nread = read(fd, ptr, nleft)) < 0) {
            if (nleft == n)
                return -1;
            else
                break;
        } else if (nread == 0) {
            break;
        }

        nleft -= nread;
        ptr += nread;
    }
    return (n - nleft);
}

ssize_t writen(int fd, const void *ptr, size_t n) {
    size_t nleft;
    ssize_t nwritten;

    nleft = n;
    while (nleft > 0) {
        if ((nwritten = write(fd, ptr, nleft)) <= 0) {
            if (nwritten < 0 && nleft == n)
                return -1;
            else
                break;
        }

        nleft -= nwritten;
        ptr += nwritten;
    }
    return (n - nleft);
}

void display_progress(int current, int total) {
    int width = 70; // Largura da barra de progresso
    int pos = (current * width) / total;
    printf("[");
    for (int i = 0; i < width; ++i) {
        if (i < pos) printf("=");
        else if (i == pos) printf(">");
        else printf(" ");
    }
    printf("] %d%%\r", (current * 100) / total);
    fflush(stdout);
    usleep(500000);
}

