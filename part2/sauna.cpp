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
#include <vector>
#include <algorithm>
//Our includes
#include "utils.h"
#include "request.h"
#include "statistics.h"

using namespace std;

//Global variables
pid_t parentPid;                //the process id of the parent
clock_t startTime;              //variable to store the moment the program started
Statistics * received, * rejected, * served; //this are structs {unsigned long total, unsigned long gender} to display statiscal information in the end
pthread_mutex_t mutexVector = PTHREAD_MUTEX_INITIALIZER;
vector<pthread_t> sessionThreads;   //vector with the id of each thread that sleeps for the duration
int occupied = 0;               //number of spots occupied
char currentGender = ' ';       //the currentGender accepted byt the sauna
char outputName[100];           //filename for the output file
int fdFifoEntrada;              //file descriptor para o fifo de entrada
int fdFifoRejeitados;           //file descriptor para o fifo dos rejeitados


/**
    Takes care of all the delete and closing operations, registered with atextit
*/
void terminateProgram(){
    cout << "Statistics:" << endl;
    cout << "Received Requests: " << endl << received << endl;
    cout << "Rejected Requests: " << endl << rejected << endl;
    cout << "Served Requests:   " << endl << served << endl;
    close(fdFifoEntrada);
    close(fdFifoRejeitados);
    delete received;
    delete rejected;
    delete served;
    cout << "Done" << endl;
}
/**
    Returns the amount of time passed since the beginning of the execution
*/
double getElapsedTime(){
    return ((double)(clock() - startTime) / CLOCKS_PER_SEC);
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

void recreateFilesAndFifos(){
    sprintf(outputName, "/tmp/bal.%d", parentPid);
    FILE * file;
    file = fopen(outputName, "r");
    if (file){//file exists and can be opened
       fclose(file);
       unlink(outputName);//deletes the file
    }else{//file doesn't exist
        int fd = open(outputName, O_WRONLY | O_CREAT | O_EXCL, 0644 );
        close(fd);
    }
}

void outputRequestToLog(Request r, char tipo){
    if (r.id == -1) return;//the exiting request is not logged
    FILE * foutput = fopen(outputName, "a");
    pid_t myId = getpid();
    double currTime = getElapsedTime();
    char buffer[100];
    sprintf(buffer, "%10.2f – %5d – %3d: %c – %5lu – %s", currTime, (int) myId, r.id,  r.gender, r.duration, getTipoSauna(tipo));
    printf("%s\n",buffer);
    fprintf (foutput, "%s\n", buffer);
    fclose(foutput);
}

void * sleepMilliseconds(void* args){
    unsigned long * mili = (unsigned long *) args;
    //cout << "start sleep: " << (*mili) << endl;
    (*mili) *= 1000;
    usleep(*mili);
    pthread_mutex_lock(&mutexVector);//block all other actions on the queue and wait for permission
    pthread_t self = pthread_self();
    sessionThreads.erase(remove(sessionThreads.begin(), sessionThreads.end(), self), sessionThreads.end());
    occupied--;//update the number of empty spots
    if(occupied == 0){
        currentGender = ' ';//reset the sauna to receive anyone
    }
    //cout << "sleep completed ("<< *mili<< ") - size: "<< sessionThreads.size() << endl;
    pthread_mutex_unlock(&mutexVector);//enable all other actions on the queue
    delete mili;
    return NULL;
}

int main(int argc, char * argv[]){
    //startTime count
	startTime = clock();
    //parentPid
    parentPid = getpid();
    recreateFilesAndFifos();
    //statistics variables
    received = newStatistics(0,0,0);
    rejected = newStatistics(0,0,0);
    served = newStatistics(0,0,0);
    //register cleaning function with atextit
    atexit(terminateProgram);
    //main variables
    int nLugares; //cmd line arguments
    struct stat st;                     //variable to use for stat functions

    //check for the correct number of arguments
    if(argc != 2){
        cout << "Invalid number of arguments, usage:\nsauna <nLugares>" << endl;
        exit(1);
    }

    nLugares = atoi(argv[1]);
    //wait for the creation of the fifo /tmp/entrada
    while(stat(fifoEntrada, &st) < 0);

    //abrir o fifo de entrada
    if((fdFifoEntrada = open(fifoEntrada, O_RDONLY | O_NONBLOCK)) == -1)
        fprintf(stderr, "Unable to open read end for fifo: %s\n", fifoEntrada);

    //abrir o fifo dos rejeitados
    if((fdFifoRejeitados = open(fifoRejeitados, O_WRONLY)) == -1)
        fprintf(stderr, "Nao foi possivel abrir o fifo %s para escrita\n", fifoRejeitados);


    Request r;
    while(true){
		while(read(fdFifoEntrada, &r, sizeof(Request)) < 1);
        updateStatistics(received, &r);
        outputRequestToLog(r, 'R');
        if(r.id == -1){//exit Request
            exit(0);
        }

        bool accept = false;
        if(currentGender == ' '){//empty -> no gender
            accept = true;
            currentGender = r.gender;
        }else if(currentGender == MALE && r.gender == MALE){
            accept = true;
        }else if(currentGender == FEMALE && r.gender == FEMALE){
            accept = true;
        }

        if(accept){//if the request is accepted-> process it
            while(occupied >= nLugares){}
            updateStatistics(served, &r);
            outputRequestToLog(r, 'D');
            occupied++;
            pthread_t tempId;
            unsigned long * durationMilis = new unsigned long;
            (*durationMilis) = r.duration;
            pthread_create(&tempId, NULL, sleepMilliseconds, (void *) durationMilis);
        	pthread_mutex_lock(&mutexVector);//block all other actions on the queue and wait for permission
            sessionThreads.push_back(tempId);
            pthread_mutex_unlock(&mutexVector);//enable all other actions on the queue
        }else{
            updateStatistics(rejected, &r);
            outputRequestToLog(r, 'S');
            int n;
            if((n = write(fdFifoRejeitados, &r, sizeof(Request))) == -1)//attempt to write the request in the fifo
    			fprintf(stderr, "Unable to send denied request with id %d\n", r.id);
        }
    }
}









//End
