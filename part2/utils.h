#pragma once

char fifoEntrada[]  = "/tmp/entrada";
char fifoRejeitados[]  = "/tmp/rejeitados";

const char statusPedido[] = "PEDIDO";
const char statusRejeitado[] = "REJEITADO";
const char statusDescartado[] = "DESCARTADO";
const char statusRecebido[] = "RECEBIDO";
const char statusServido[] = "SERVIDO";
const char statusNotFound[] = "TipoNaoEncontrado";

const char MALE = 'M';
const char FEMALE = 'F';

/**
*	Returns a random number between 0 and max
*/
int getRandomNumber(int max){
	return (max +1)*((double)rand()/RAND_MAX);
}

/**
*	Returns a random gender
*/
char getRandomGender(){
	return getRandomNumber(1) == 1?FEMALE:MALE;
}

const char * getTipoSauna(char tipo){
    return tipo == 'R'?statusRecebido:(tipo == 'D'? statusRejeitado: (tipo == 'S'?statusServido:statusNotFound));
}

const char * getTipoGerador(char tipo){
    return tipo == 'P'?statusPedido:(tipo == 'R'? statusRejeitado: (tipo == 'D'?statusDescartado:statusNotFound));
}
