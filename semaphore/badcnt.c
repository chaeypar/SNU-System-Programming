
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <stdlib.h>

volatile int cnt = 0;

void *thread(void *vargp){
    int i, niters = *((int *)vargp);
    for (i = 0;i<niters;i++){
        cnt++;
    }
    return NULL;

}
void Pthread_create(pthread_t *tidp, pthread_attr_t *attrp, 
		    void * (*routine)(void *), void *argp) 
{
    int rc;

    if ((rc = pthread_create(tidp, attrp, routine, argp)) != 0){
	    printf("pthread create error\n");
        exit(1);
    }
}

void Pthread_join(pthread_t tid, void **thread_return) {
    int rc;

    if ((rc = pthread_join(tid, thread_return)) != 0){
	    printf("pthread join error\n");
        exit(1);
    }
}

int main(int argc, char **argv){

    if (argc != 2){
        printf("An argument 'niters' was not passed.\n");
        return -1;
    }
    int niters = atoi(argv[1]);
    pthread_t tid1, tid2;
    time_t start, end;
    start = clock();
    Pthread_create(&tid1, NULL, thread, &niters);
    Pthread_create(&tid2, NULL, thread, &niters);

    Pthread_join(tid1, NULL);
    Pthread_join(tid2, NULL);

    end = clock();
    printf("Total Running Time : %lld\n", (end-start));
    if (cnt != (2*niters))
        printf("BOOM! cnt = %d\n", cnt);
    else    
        printf("OK cnt = %d\n", cnt);
    exit(0);
}