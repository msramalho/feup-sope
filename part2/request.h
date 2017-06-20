#pragma once

#include <iostream>
#include <iomanip>
#include "utils.h"

using namespace std;

static int lastRequestId = 1;
/**
    Struct pedido - request
*/
typedef struct request {
    int id;                    //the request id
	unsigned long duration;    //duracao da sessao na sauna
	short rejectedCount;       //numero de vezes que o pedido foi rejeitado pela sauna
	char gender;               //genero do cliente associado ao pedido
}Request;

/**
	Create a Request instance
*/
Request * newRequestBuilder(int id, unsigned long duration, short rejectedCount, char gender){
    Request * r =  new Request();
    r->id = id;
    r->duration = duration;
    r->rejectedCount = rejectedCount;
    r->gender = gender;
    return r;
}

Request * newRequest(unsigned long duration, short rejectedCount, char gender){
    return newRequestBuilder(lastRequestId++, duration, rejectedCount, gender);
}

/**
	Create a Random Request instance
*/
Request * newRandomRequest(unsigned long maxDuration){
    return newRequest(getRandomNumber(maxDuration), 0, getRandomGender());
}

/**
	Create an empty request
*/
Request * newEmptyRequest(){
    return newRequestBuilder(0, 0, 0, ' ');
}
/**
    Create an exit request
*/
Request * newExitRequest(){
    return newRequestBuilder(-1, 0, 0, ' ');
}
ostream& operator<<(ostream& out, const Request * r) {
    out << setw(5) << r->id << ": " << setw(6) << r->duration << " - " << setw(6) << r->rejectedCount << " - " << setw(2) << r->gender;
    return out;
}
