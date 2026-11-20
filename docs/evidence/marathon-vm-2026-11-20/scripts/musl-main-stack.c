#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <string.h>
static void show(const char*w){pthread_attr_t a;pthread_getattr_np(pthread_self(),&a);void*o;size_t s;pthread_attr_getstack(&a,&o,&s);printf("%s low=%p size=%zu\n",w,o,s);}
static int deep(int n){volatile char b[4096];memset((void*)b,n,sizeof b);return n?deep(n-1)+b[0]:0;}
int main(){show("start");deep(1000);show("after 4MB touched");return 0;}
