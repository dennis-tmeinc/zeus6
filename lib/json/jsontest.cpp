#include "json.h"
#include <stdio.h>

void memtes()
{
	char * p ;
	p = new char [1] ;

	delete[] p ;

	p = new char ;
	delete  p ;


	int * pi ;
	pi = new int ;
	delete pi ;

	pi = new int [10] ;
	delete [] pi ;
	
}

static json stj ;

int main()
{
	int n;
	char* text;

	memtes();

	text = new char[8004];
	FILE* f = fopen("j.txt", "r");
	n = fread(text, 1, 8000, f);
	fclose(f);

	// text[n] = 0;

	n = 2 ;
	json * jt ;
	jt = new json[n] ;
	delete [] jt ;

	json* j = new json();
	j->parse(text);

	n = j->encode(text, 5000);

	fwrite(text, 1, n, stdout);
	fwrite("\n", 1, 1, stdout);

	json* j1 = new json();
	*j1 = *j;

	j1->addStringItem("No named string");
	j1->addStringItem("field1", "named string");

	n = j1->encode(text, 5000);
	fwrite(text, 1, n, stdout);
	fwrite("\n", 1, 1, stdout);

	delete j1;

	delete j;
	delete text;

	return 0;
}