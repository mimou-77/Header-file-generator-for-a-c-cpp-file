#include <stdio.h>
#include "inttypes.h"
#include "stdbool.h"


//function that returns a bool
bool fn_bool(bool a)
{
    return(a);
}

//function that returns a size_t
size_t fn_size(size_t a)
{
    return(a);
}


//function that has static + const
static const int s_n_nn(const int n)
{
    const int nn = n * 10;
    return nn;
}


//function that has const in parameters and return
const int n_nn(const int n)
{
    const int nn = n * 10;
    return nn;
}


//private function (static)
static void stars()
{
    printf("******** \n");
}


//------------------------------------------


//function with a size_t parameter
void hehe(size_t s)
{
    printf("size fn");
}


//function with a uint8_t parameter
int sum(uint8_t a, int b, int c) {
    return (a + b + c);
}


//function with a char * return type + a char * param type
char * print_hello_sx(char * s)
{
    printf("hello, %s", s);
    char * ss = "done";
    return(ss);
}