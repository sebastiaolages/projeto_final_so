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
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#define SHM_KEY 0x1234
#define MAX_POINTS 10000
#define LINE_BUFFER_SIZE 256 
#define SOCKET_PATH "/tmp/monte_carlo_socket"

typedef struct {
    double x;
    double y;
} Point;

bool processarArquivo(const char *nomeArquivo, Point *points, int *num_points);
void generateAndTestPoints(Point polygon[], int n, int num_pontos, int process_id, int *progress);
double calculateBoundingBoxArea(Point polygon[], int n);
int *init_shared_memory();
void display_progress(int current, int total);
void cleanup_shared_memory(int *shm, int shmid);


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

void generateAndTestPoints(Point polygon[], int n, int num_pontos, int process_id, int *progress) {
    int pointsInside = 0;
    Point testPoint;
    double minX, maxX, minY, maxY;

    minX = maxX = polygon[0].x;
    minY = maxY = polygon[0].y;
    for (int i = 1; i < n; i++) {
        if (polygon[i].x < minX) minX = polygon[i].x;
        if (polygon[i].x > maxX) maxX = polygon[i].x;
        if (polygon[i].y < minY) minY = polygon[i].y;
        if (polygon[i].y > maxY) maxY = polygon[i].y;
    }

    srand(time(NULL) + process_id);

    for (int i = 0; i < num_pontos; i++) {
        testPoint.x = minX + (double)rand() / RAND_MAX * (maxX - minX);
        testPoint.y = minY + (double)rand() / RAND_MAX * (maxY - minY);

        if (isInsidePolygon(polygon, n, testPoint)) {
            pointsInside++;
        }

         if (i % (num_pontos / 100) == 0) { 
        __sync_add_and_fetch(progress, num_pontos / 100);
    }
}

    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    connect(sockfd, (struct sockaddr*)&addr, sizeof(struct sockaddr_un));
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "%d;%d", process_id, pointsInside);
    send(sockfd, buffer, strlen(buffer), 0);
    close(sockfd);
}


double calculateBoundingBoxArea(Point polygon[], int n) {
    if (n == 0) return 0;

    double minX = polygon[0].x;
    double maxX = polygon[0].x;
    double minY = polygon[0].y;
    double maxY = polygon[0].y;

    for (int i = 1; i < n; i++) {
        if (polygon[i].x < minX) minX = polygon[i].x;
        if (polygon[i].x > maxX) maxX = polygon[i].x;
        if (polygon[i].y < minY) minY = polygon[i].y;
        if (polygon[i].y > maxY) maxY = polygon[i].y;
    }

    return (maxX - minX) * (maxY - minY);
}

// Função para inicializar a memória compartilhada
int *init_shared_memory() {
    int shmid = shmget(SHM_KEY, sizeof(int), IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget failed");
        exit(1);
    }
    int *shm = (int *)shmat(shmid, NULL, 0);
    if (shm == (void *) -1) {
        perror("shmat failed");
        exit(1);
    }
    *shm = 0; // Inicializa o valor compartilhado como 0
    return shm;
}

// Função para exibir a barra de progresso
void display_progress(int current, int total) {
    int barWidth = 70;
    float progress = (float)current / total;
    int pos = barWidth * progress;

    printf("\r[");
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos) printf("=");
        else if (i == pos) printf(">");
        else printf(" ");
    }
    printf("] %d%%", (int)(progress * 100.0));
    fflush(stdout);
}

// Função para desanexar e destruir a memória compartilhada
void cleanup_shared_memory(int *shm, int shmid) {
    shmdt(shm);
    shmctl(shmid, IPC_RMID, NULL);
}

/**
 * Main function
 */
int main(int argc, char *argv[]) {
   
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <file_name> <num_processes> <num_points>\n", argv[0]);
        return 1;
    }

   
    int *progress = init_shared_memory();
    int shmid = shmget(SHM_KEY, sizeof(int), 0666);
    if (shmid < 0) {
        perror("shmget failed");
        return 1;
    }

    
    Point points[MAX_POINTS];
    int num_points = 0;
    int num_processes = atoi(argv[2]);
    int num_points_total = atoi(argv[3]);

    
    if (!processarArquivo(argv[1], points, &num_points) || num_points < 3) {
        fprintf(stderr, "Failed to process file or insufficient points to form a polygon.\n");
        cleanup_shared_memory(progress, shmid);
        return 1;
    }

    // Create the server socket
    int server_sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_sockfd < 0) {
        perror("Socket creation failed");
        cleanup_shared_memory(progress, shmid);
        return 1;
    }

    
    struct sockaddr_un server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, SOCKET_PATH, sizeof(server_addr.sun_path) - 1);
    unlink(SOCKET_PATH);

    if (bind(server_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1 || listen(server_sockfd, num_processes) == -1) {
        perror("Failed to bind or listen on socket");
        close(server_sockfd);
        cleanup_shared_memory(progress, shmid);
        return 1;
    }

    // Fork child processes
    pid_t pid;
    for (int i = 0; i < num_processes; i++) {
        pid = fork();
        if (pid == 0) {
            // Child process
            generateAndTestPoints(points, num_points, num_points_total / num_processes, i, progress);
            exit(EXIT_SUCCESS);
        } else if (pid < 0) {
            
            perror("Fork failed");
            close(server_sockfd);
            cleanup_shared_memory(progress, shmid);
            return 1;
        }
    }

    
    int status;
    pid_t wpid;
    bool all_children_completed = false;
    int completed_processes = 0;

    double points_inside = 0;

    while (!all_children_completed) {
        wpid = waitpid(-1, &status, WNOHANG);
        if (wpid == -1) {
            perror("waitpid failed");
            break;
        } else if (wpid == 0) {
            display_progress(*progress, num_points_total);
            usleep(500000); 
        } else {
            completed_processes++;
            if (completed_processes >= num_processes) {
                all_children_completed = true;
            }
        }
    }

  
    while (completed_processes > 0) {
        int client_sockfd = accept(server_sockfd, NULL, NULL);
        if (client_sockfd < 0) {
            perror("Accept failed");
            continue;
        }

        char buffer[1024];
        recv(client_sockfd, buffer, sizeof(buffer) - 1, 0);
        close(client_sockfd);

        int proc_id, points_in_proc;
        sscanf(buffer, "%d;%d", &proc_id, &points_in_proc);
        points_inside += points_in_proc;
        completed_processes--;
    }

    
    display_progress(num_points_total, num_points_total);
    printf("\n");

    
    double area = (points_inside / num_points_total) * calculateBoundingBoxArea(points, num_points);
    printf("Estimated area of the polygon: %f\n", area);

  
    close(server_sockfd);
    unlink(SOCKET_PATH);
    cleanup_shared_memory(progress, shmid);

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

        // Incrementa a contagem de pontos
        (*num_points)++;

        // Se a nova linha foi encontrada, reposiciona o descritor de arquivo
        // para o início da próxima linha
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

