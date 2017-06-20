#pragma once

#include <iostream>
using namespace std;
/**
    Struct para guardar as estatisticas, total e por genero
*/
typedef struct statistics {
	unsigned long total;    //contagem total desta estatistica
    unsigned long male;   //contagem por genero desta estatistica
    unsigned long female;   //contagem por genero desta estatistica
} Statistics;

/**
	Create a Statistics instance
*/
Statistics * newStatistics(unsigned long total, unsigned long male, unsigned long female){
    Statistics * s =  new Statistics();
    s->total = total;
    s->male = male;
    s->female = female;
    return s;
}

ostream& operator<<(ostream& out, const Statistics * s) {
    out << "   Total: " << s->total << endl;
    out << "    Male: " << s->male << endl;
    out << "  Female: " << s->female;
    return out;
}
