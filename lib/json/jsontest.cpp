#include <stdio.h>
#include "json.h"

int main()
{

    int n;
    char * text ;
    text = new char [8004] ;
    FILE * f = fopen("j.txt","r") ;
    n=fread(text, 1, 8000, f);
    fclose(f);
    text[n] = 0;

    json * j = new json ();
    j->parse( text, NULL);

    n = j->encode(text, 5000);

    fwrite( text, 1, n, stdout);
    fwrite( "\n", 1, 1, stdout);

    json * j1 = new json();
    *j1 = *j ;

    j1->addStringItem("No named string");
    j1->addStringItem("field1", "named string");

    n=j1->encode( text, 5000);
    fwrite( text, 1, n, stdout);
    fwrite( "\n", 1, 1, stdout);

    delete j1 ;

    delete j ;
    delete text ;

    return 0;

}