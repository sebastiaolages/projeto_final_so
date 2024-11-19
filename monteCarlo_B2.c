#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <math.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>

#define NUM_POINTS 10000
#define MAX_THREADS 10
#define MAX_POLYGON_POINTS 1000

typedef struct {
    double x;
    double y;
} Point;

typedef struct {
    Point *polygon;
    int n; 
    int totalPoints;
    int start;
    int end;
    int *countInside;
    int *pointsChecked;
    pthread_mutex_t *mutex;
} ThreadData;

int orientation(Point p, Point q, Point r);
bool onSegment(Point p, Point q, Point r);
bool doIntersect(Point p1, Point q1, Point p2, Point q2);
bool isInsidePolygon(Point polygon[], int n, Point p);

void *countPointsInside(void *arg);
void *displayProgress(void *arg);

typedef struct {
    int *pointsChecked;
    int totalPoints;
    pthread_mutex_t *mutex;
} ProgressData;

volatile bool progressDone = false;

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s <number of threads> <number of points> <polygon file>\n", argv[0]);
        return 1;
    }

    int numThreads = atoi(argv[1]);
    int numPoints = atoi(argv[2]);
    const char *filename = argv[3];
    
    if (numThreads > MAX_THREADS) numThreads = MAX_THREADS;

  
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("Error opening file");
        return 1;
    }

    Point polygon[MAX_POLYGON_POINTS];
    int n = 0;
    char buffer[256];
    ssize_t bytesRead;
    char *ptr;

    while ((bytesRead = read(fd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytesRead] = '\0';
        ptr = buffer;
        while (sscanf(ptr, "%lf,%lf", &polygon[n].x, &polygon[n].y) == 2) {
            n++;
            if (n >= MAX_POLYGON_POINTS) {
                fprintf(stderr, "Polygon has too many points (max %d)\n", MAX_POLYGON_POINTS);
                close(fd);
                return 1;
            }
            while (*ptr && *ptr != '\n') ptr++;
            if (*ptr) ptr++;
        }
    }
    close(fd);

    if (n < 3) {
        fprintf(stderr, "The polygon must have at least 3 points\n");
        return 1;
    }

    int countInside = 0;
    int pointsChecked = 0;
    pthread_t threads[numThreads];
    ThreadData tdata[numThreads];
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);

    int pointsPerThread = numPoints / numThreads;

    
    ProgressData pdata = { &pointsChecked, numPoints, &mutex };
    pthread_t progressThread;
    pthread_create(&progressThread, NULL, displayProgress, &pdata);

    for (int i = 0; i < numThreads; i++) {
        tdata[i].polygon = polygon;
        tdata[i].n = n;
        tdata[i].totalPoints = numPoints;
        tdata[i].start = i * pointsPerThread;
        tdata[i].end = (i + 1) * pointsPerThread - 1;
        if (i == numThreads - 1) tdata[i].end = numPoints - 1;
        tdata[i].countInside = &countInside;
        tdata[i].pointsChecked = &pointsChecked;
        tdata[i].mutex = &mutex;

        pthread_create(&threads[i], NULL, countPointsInside, (void *)&tdata[i]);
    }

    for (int i = 0; i < numThreads; i++) {
        pthread_join(threads[i], NULL);
    }

 
    pthread_mutex_lock(&mutex);
    progressDone = true;
    pthread_mutex_unlock(&mutex);

   
    pthread_join(progressThread, NULL);

    pthread_mutex_destroy(&mutex);

    double squareArea = 4.0;  
    double estimatedArea = squareArea * ((double)countInside / numPoints);
    printf("Estimated area of the polygon: %.2f\n", estimatedArea);

    return 0;
}

void *countPointsInside(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    int localCount = 0;

    for (int i = data->start; i <= data->end; i++) {
        Point p = {(double)rand() / RAND_MAX * 2 - 1, (double)rand() / RAND_MAX * 2 - 1};
        if (isInsidePolygon(data->polygon, data->n, p)) {
            localCount++;
        }
        pthread_mutex_lock(data->mutex);
        (*(data->pointsChecked))++;
        pthread_mutex_unlock(data->mutex);

        
        usleep(100);  
    }

    pthread_mutex_lock(data->mutex);
    *(data->countInside) += localCount;
    pthread_mutex_unlock(data->mutex);

    pthread_exit(NULL);
}

void *displayProgress(void *arg) {
    ProgressData *pdata = (ProgressData *)arg;
    int *pointsChecked = pdata->pointsChecked;
    int totalPoints = pdata->totalPoints;
    pthread_mutex_t *mutex = pdata->mutex;

    int previousProgress = -1;

    while (!progressDone) {
        pthread_mutex_lock(mutex);
        int checked = *pointsChecked;
        pthread_mutex_unlock(mutex);

        int progress = (double)checked / totalPoints * 100;
        if (progress != previousProgress) {
            printf("Progress: %d%%\r", progress);
            fflush(stdout);
            previousProgress = progress;
        }

        usleep(100000);  
    }
    
    printf("Progress: 100%%\n");
    return NULL;
}

int orientation(Point p, Point q, Point r) {
    double val = (q.y - p.y) * (r.x - q.x) - (q.x - p.x) * (r.y - q.y);
    return (val == 0) ? 0 : (val > 0) ? 1 : 2;
}

bool onSegment(Point p, Point q, Point r) {
    return q.x <= fmax(p.x, r.x) && q.x >= fmin(p.x, r.x) && q.y <= fmax(p.y, r.y) && q.y >= fmin(p.y, r.y);
}

bool doIntersect(Point p1, Point q1, Point p2, Point q2) {
    int o1 = orientation(p1, q1, p2);
    int o2 = orientation(p1, q1, q2);
    int o3 = orientation(p2, q2, p1);
    int o4 = orientation(p2, q2, q1);

    if (o1 != o2 && o3 != o4) return true;
    if (o1 == 0 && onSegment(p1, p2, q1)) return true;
    if (o2 == 0 && onSegment(p1, q2, q1)) return true;
    if (o3 == 0 && onSegment(p2, p1, q2)) return true;
    if (o4 == 0 && onSegment(p2, q1, q2)) return true;
    return false;
}

bool isInsidePolygon(Point polygon[], int n, Point p) {
    if (n < 3) return false;

    Point extreme = {2.5, p.y};
    int count = 0;
    for (int i = 0; i < n; i++) {
        int next = (i + 1) % n;
        if (doIntersect(polygon[i], polygon[next], p, extreme)) {
            if (orientation(polygon[i], p, polygon[next]) == 0)
                return onSegment(polygon[i], p, polygon[next]);
            count++;
        }
    }
    return count & 1;  
}

