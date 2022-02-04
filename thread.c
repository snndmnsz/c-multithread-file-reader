#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <stdbool.h>
#include <semaphore.h>

//    KEREM KOSIF   
//   SINAN DUMANSIZ   

// in this part we defined our global variables and some boolen variables
bool isArrayFull = false;
bool stopThreadPool = false;

typedef struct Task{
    char * filename;
}Task;

Task taskQueue[256];
int taskCount = 0;

pthread_mutex_t mutexQueueThreads;
pthread_cond_t condQueueThreads;

pthread_mutex_t searchLock; // defining search lock mutex
pthread_mutex_t arrayFull;// defining adding extra space to array mutex
sem_t threadQueueeSemephore;// defining Queuee Semephore for the multiple works

int memorySize = 8;
int memoryIndex = 0;
int finishedTaskCount = 0;
int totalTask = 0;
int th_number;
char *path;

struct Record ** memory;
// created a struck for the words and their filenames
struct Record {
    char word[30];
    char location[30][30];
    int locationCount;
};

//This part creates the dynamcly allocated memeory for the project
struct Record ** createMemory() {
    struct Record ** memory = malloc(memorySize * sizeof(struct Record * ));
    printf("MAIN THREAD: Allocated initial array of %d pointers.\n", memorySize);
    return memory;
}

//When ever this function is called it otomaticly doubles the size of array 
int updateMemory() {
    memorySize *= 2;
    struct Record ** newMemory = realloc(memory, (memorySize * sizeof(struct Record * )));
    if (newMemory == NULL) {
        printf("Cannot allocate initial memory for data\n");
        exit(1);
    }
    memory = newMemory;
    return memorySize;
}

// Creates a struct object laster on we can store this value
struct Record * createHolder(char word[30], char location[30]) {
    struct Record * holder = malloc(sizeof(struct Record));
    strcpy(holder -> word, word);
    strcpy(holder -> location[0], location);
    holder -> locationCount = 1;
    return holder;
}

// When ever this function is called it otomatioly adds the stcut object to given index
void addToMemory(int index, char word[30], char location[30]) {
    memory[index] = createHolder(word, location);
}


void readFile(Task * task) {
    char fullPath[70]; //stores the full path 
    strcpy(fullPath, path);
    strcat(fullPath,task -> filename);

    printf("MAIN THREAD: Assigned \"%s\" to worker thread %lu.\n", task -> filename, syscall(SYS_gettid));
    sem_wait( & threadQueueeSemephore); // creaates a Semephore to sync all the threads
    int c;
    FILE * file;
    char word[50];
    file = fopen(fullPath, "r"); // reading the file
    if (file) {
        while (fscanf(file, "%s", word) != EOF) {

            /////////////   SEARCH PART ///////////// 
            int availableIndex; // storing the empty index
            int foundedIndex = 0;
            bool pass = false;

            pthread_mutex_lock( & searchLock); // locking the thread for search purpose
            if (memoryIndex == memorySize) {
                isArrayFull = true; // sending is array full signal to global variable
            }

            for (int i = 0; i < memoryIndex; i++) {
                if (strcmp(word, memory[i] -> word) == 0) {
                    int index = memory[i] -> locationCount; // if we found the same word
                    strcpy(memory[i] -> location[index],task -> filename); // then we store the fiolename also
                    memory[i] -> locationCount++;
                    printf("THREAD %ld: The word \"%s\" has already located at index %d.\n", syscall(SYS_gettid), word, i);
                    pass = true;
                }
            }

            if (pass == false) {
                availableIndex = memoryIndex; // if we cant find the word, it means its unique
                memoryIndex++;
            }
            pthread_mutex_unlock( & searchLock); // unlocking the mutex so that we can write at same time

            ///////// WRITE TO AVAILABLE INDEX  /////////
            if (isArrayFull) {
                pthread_mutex_lock( & arrayFull); // locking threads in order to update memeory
                updateMemory(); // if the array is full then simply allocate new memory size
                printf("THREAD %ld: Re-allocated array of %d pointers.\n", syscall(SYS_gettid), memorySize);
                // printing the message to console
                isArrayFull = false;
                pthread_mutex_unlock( & arrayFull);// unlocking mutex 
            }
            if (pass == false) {
                // writeing a word with all threas at same time with given empty index
                addToMemory(availableIndex, word, "xdxd.txt");
                printf("THREAD %ld: Added \"%s\" at index %d.\n", syscall(SYS_gettid), word, availableIndex);
                //printing the message to console
            }
        }
        fclose(file);
    }
    sem_post( & threadQueueeSemephore); // closing the Semephore
    free(task -> filename); // freeing the memory 
    //return NULL;
}


void submitTask(Task task) {
    // when ever a work is submitted automatically new thread takes it place
    pthread_mutex_lock( & mutexQueueThreads);
    taskQueue[taskCount] = task;
    taskCount++; // incrementing thread count
    pthread_mutex_unlock( & mutexQueueThreads);
    pthread_cond_signal( & condQueueThreads);
}

void * startThread(void * args) {
    // starting a thread to work for one file 
    while (stopThreadPool) {
        Task task;
        pthread_mutex_lock( & mutexQueueThreads); // locking mutex so that we can get the task number
        while (taskCount == 0) {
            pthread_cond_wait(& condQueueThreads, & mutexQueueThreads);
        }
        task = taskQueue[0];
        int i;
        for (i = 0; i < taskCount - 1; i++) {
            taskQueue[i] = taskQueue[i + 1];
        }
        taskCount--; // decrementing task number
        pthread_mutex_unlock(&mutexQueueThreads);
        readFile( & task);
        finishedTaskCount++; // incrementing the finished tasks 
        if (finishedTaskCount == totalTask) {
            // after all the worker threads are finished  freeing memory and printing info message
            printf("MAIN THREAD: All done (successfully read %d words with %d threads from %d files).\n", memoryIndex, th_number, totalTask);
            pthread_mutex_destroy(&searchLock);
            pthread_mutex_destroy(&arrayFull);
            free(memory);
            free( * memory);
            //exiting the code after all work is done.
            exit(0);
        }
    }
    return NULL;
}


int main(int argc, char ** argv) {

    if (argc > 5 || argc < 4) { // if 
        fprintf(stderr, "ERROR: Invalid arguments");
        fprintf(stderr, "USAGE: ./a.out -d <directoryName> -n <#ofThreads>");
        return 1;
    }
    memory = createMemory();// creating array with size of 8
    int counter = 0;
    char arr[100][80];
    int threadNumber = strtol(argv[4], NULL, 10); // getting values from argv
    th_number = threadNumber;
    pthread_t th[threadNumber];

    DIR * d;
    struct dirent * dir;
    d = opendir(argv[2]);  //getting path 
    path = argv[2];

    if (d) {
        while ((dir = readdir(d)) != NULL) {
            const size_t len = strlen(dir -> d_name);
            if (len > 4 && dir -> d_name[len - 4] == '.' && dir -> d_name[len - 3] == 't' && dir -> d_name[len - 2] == 'x' && dir -> d_name[len - 1] == 't') {
                //printf(" %s \n", dir -> d_name);
                strcpy(arr[counter], dir -> d_name); // reading all the filenames inside given directoy 
                // and then we soted all of them inside an array 
                totalTask++;
                counter++;
            }
        }
        closedir(d);
    }

    int max_file = counter + 1;
    sem_init( &threadQueueeSemephore, 0, th_number);

    pthread_mutex_init(& mutexQueueThreads, NULL); // define mutex
    pthread_cond_init(& condQueueThreads, NULL);
    stopThreadPool = true; // gicing true to search thread pool

    if (pthread_mutex_init( & searchLock, NULL) != 0) {
        printf("\n SearchLock Mutex init has failed.\n");
        return 1;
    }

    if (pthread_mutex_init( & arrayFull, NULL) != 0) {
        printf("\n arrayFull Mutex init has failed.\n");
        return 1;
    }

    for (int i = 0; i < th_number; ++i) {
        // creating threads for the multi thread 
        if (pthread_create( & th[i], NULL, & startThread, NULL)) {
            return 1;
        }
    }

    for (int i = 0; i < counter; i++) {
        //creating the work for the threads
        char * str = malloc(50 * sizeof(char));// creating malloc for  filename

        strcpy(str, arr[i]);
        Task t = {
                .filename = str // giving filename
        };
        submitTask(t);
    }

    for (int i = 0; i < th_number; ++i) {
        if (pthread_join(th[i], NULL)) {
            return 3;
        }
    }
    pthread_exit(NULL); // clearing the main thread

    // pthread_mutex_destroy( & searchLock);
    // pthread_mutex_destroy( & arrayFull);
    sem_destroy( & threadQueueeSemephore);
    pthread_mutex_destroy( & mutexQueueThreads);
    pthread_cond_destroy( & condQueueThreads);
    return (0);
}

