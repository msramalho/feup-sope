//C includes
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
//C++ includes
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <queue>
//Our includes
#include "utils.h"
#include "request.h"
#include "statistics.h"

using namespace std;

//Global variables
pid_t parentPid;                //the process id of the parent
queue<Request *> requestsQueue; //the queue that holds the requests to send
int fdFifoEntrada;              //file descriptor para o fifo de entrada
int fdFifoRejeitados;              //file descriptor para o fifo dos rejeitados
pthread_t generatorTid;         //requestGenerator thread id
pthread_t listenerTid;          //deniedRequestsListener thread id
pthread_t inactiveTid;          //inactiveTimeListener thread id
pthread_mutex_t mutexQueue = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexTime = PTHREAD_MUTEX_INITIALIZER;
clock_t startTime;              //variable to store the moment the program started
clock_t startTimeInactive;     //variable to store the moment the last relevant update was made, so as to determine when to end the program
Statistics * generated, * rejectedReceived, * rejectedTrashed; //this are structs {unsigned long total, unsigned long gender} to display statiscal information in the end
char outputName[100];           //filename for the output file
int nRequests, maxDuration; //cmd line arguments

/**
    Takes care of all the delete and closing operations, registered with atextit
*/
void terminateProgram(){
    cout << "Statistics:" << endl;
    cout << "Generated Requests:  " << endl << generated << endl;
    cout << "Received Rejections: " << endl << rejectedReceived << endl;
    cout << "Trashed Rejections:  " << endl << rejectedTrashed << endl;
    close(fdFifoEntrada);
    close(fdFifoRejeitados);
    delete generated;
    delete rejectedReceived;
    delete rejectedTrashed;
    cout << "Done" << endl;
}

void updateStatistics(Statistics * s, Request * r){
    if(r->id == -1) return;
    s->total++;//total de gerados
    if(r->gender == MALE){
        s->male++;//total de gerados para male
    }else if(r->gender == FEMALE){
        s->female++;//total de gerados para female
    }
}

/**
    Returns the amount of time passed since the beginning of the execution
*/
double getElapsedTime(){
    return ((double)(clock() - startTime) / CLOCKS_PER_SEC);
}

/**
*   Functions that manage the termination of the program by measuring the inactive time
*/
double getInactiveTime(){
    return ((double)(clock() - startTimeInactive) / CLOCKS_PER_SEC);
}

void resetInactiveTime(){//Called when the program is still doing relevant operations
    pthread_mutex_lock(&mutexTime);//block all other actions on the queue and wait for permission
    startTimeInactive = clock();
    pthread_mutex_unlock(&mutexTime);//enable all other actions on the queue
}

void * inactiveTimeListener(void *){
    while(true){
        pthread_mutex_lock(&mutexTime);//block all other actions on the queue and wait for permission
        if(getInactiveTime() >= 5){
            Request * r = newExitRequest();
            requestsQueue.push(r);
            pthread_mutex_unlock(&mutexTime);//enable all other actions on the queue
            while(!requestsQueue.empty()){}
            break;
        }else{
            pthread_mutex_unlock(&mutexTime);//enable all other actions on the queue
        }
    }
    return NULL;
}

void recreateFilesAndFifos(){
    sprintf(outputName, "/tmp/ger.%d", parentPid);
    FILE * file;
    file = fopen(outputName, "r");
    if (file){//file exists and can be opened
       fclose(file);
       unlink(outputName);//deletes the file
    }else{//file doesn't exist
        int fd = open(outputName, O_WRONLY | O_CREAT | O_EXCL, 0644 );
        close(fd);
    }

    struct stat st;             //vartiable to use for stat functions

    //Create the fifo to send the requests
    if (stat(fifoEntrada, &st) >= 0)//if the fifo already exists
        unlink(fifoEntrada);//delete it to create a new one
    if(mkfifo(fifoEntrada, 0660) == -1) {//attempt to create the fifo
        fprintf(stderr, "Unable to create fifo %s\n", fifoEntrada);
        exit(2);
    }

    //Create the fifo to rejected
    if (stat(fifoRejeitados, &st) >= 0)//if the fifo already exists
        unlink(fifoRejeitados);//delete it to create a new one
    if(mkfifo(fifoRejeitados, 0660) == -1) {//attempt to create the fifo
        fprintf(stderr, "Unable to create fifo %s\n", fifoRejeitados);
        exit(2);
    }
}

void outputRequestToLog(Request r, char tipo){
    if (r.id == -1) return;//the exiting request is not logged
    FILE * foutput = fopen(outputName, "a");
    pid_t myId = getpid();
    double currTime = getElapsedTime();
    char buffer[100];
    sprintf(buffer, "%10.2f – %5d – %3d: %c – %5lu – %s", currTime, (int) myId, r.id,  r.gender, r.duration, getTipoGerador(tipo));
    printf("%s\n",buffer);
    fprintf (foutput, "%s\n", buffer);
    fclose(foutput);
}

/**
*   Generates nRequests with a maxDuration
*   Send each request to sauna (sends the data insiderequestsQueue)
*   writes the output to /tmp/ger.pid, where pid is the process id of the parent -> global parentPid
*   @param nRequests the number of requests
*   @param maxDuration maximum amount of time a session can last for
*/
void * requestGenerator(void* args){
    int n;  //int to count the number of bytes written by write to fifo
    //create the requests
    for(int i  = 0; i < nRequests; i++){
        Request * r = newRandomRequest(maxDuration);
        updateStatistics(generated, r);
        requestsQueue.push(r);
    }

    while(true){//while there are still requests to process
        while(requestsQueue.empty()){}
        resetInactiveTime();
    	pthread_mutex_lock(&mutexQueue);//block all other actions on the queue and wait for permission
        Request r = *requestsQueue.front();
        if(r.rejectedCount <= 3){
            if((n = write(fdFifoEntrada, &r, sizeof(Request))) == -1)//attempt to write the request in the fifo
                fprintf(stderr, "Unable to send request with id %d\n", r.id);
            else
                outputRequestToLog(r, 'P');
                delete requestsQueue.front();
                requestsQueue.pop();//Remove request from the queue
        }else{
            delete requestsQueue.front();
            requestsQueue.pop();//Remove request from the queue
        }
        pthread_mutex_unlock(&mutexQueue);//enable all other actions on the queue
        //sleep(maxDuration / 1000);
        sleep(0.1);
    }
	return NULL;
}

/**
    Function executed in a new thread that reads the declined requests and sends them to the queue
*/
void * deniedRequestsListener(void* args){
    if((fdFifoRejeitados = open(fifoRejeitados, O_RDONLY | O_NONBLOCK)) == -1)
		fprintf(stderr, "Unable to open read end for fifo: %s\n", fifoRejeitados);

    while(true){
        Request * r = newEmptyRequest();
		while(read(fdFifoRejeitados, r, sizeof(Request)) < 1);
        resetInactiveTime();
        pthread_mutex_lock(&mutexQueue);//block all other actions on the queue and wait for permission
        updateStatistics(rejectedReceived, r);
        r->rejectedCount++;
        if(r->rejectedCount < 3){//does not resend
            outputRequestToLog(*r, 'R');
            requestsQueue.push(r);
        }else{
            updateStatistics(rejectedTrashed, r);
            outputRequestToLog(*r, 'D');
        }
        pthread_mutex_unlock(&mutexQueue);//enable all other actions on the queue
    }
	return NULL;
}


int main(int argc, char * argv[]){
    //startTime count
    resetInactiveTime();
    startTime = clock();
    //parentPid
    parentPid = getpid();
    //creates the output file
    recreateFilesAndFifos();
    //statistics variables
    generated = newStatistics(0,0,0);
    rejectedReceived = newStatistics(0,0,0);
    rejectedTrashed = newStatistics(0,0,0);
    //register cleaning function with atextit
    atexit(terminateProgram);

    //set random seed
    srand(time(NULL));

    //check for the correct number of arguments
    if(argc != 3){
        cout << "Invalid number of arguments, usage:\ngerador <nPedidos> <maxUtilizacao>" << endl;
        exit(1);
    }

    //open fifo de entrada
    if((fdFifoEntrada = open(fifoEntrada, O_WRONLY)) == -1)
        fprintf(stderr, "Nao foi possivel abrir o fifo %s para escrita\n", fifoEntrada);

    //create deniedRequestsListener
    pthread_create(&listenerTid, NULL, deniedRequestsListener, NULL);

    //send the data to the requests generator thread
    nRequests = atoi(argv[1]);
    maxDuration = atoi(argv[2]);
    //create the new thread and send the data
    pthread_create(&generatorTid, NULL, requestGenerator, NULL);

    pthread_create(&inactiveTid, NULL, inactiveTimeListener, NULL);
    pthread_join(inactiveTid, NULL);
}
