/* Copyright (c) 2015 The University of Edinburgh. */

/* 
* This software was developed as part of the                       
* EC FP7 funded project Adept (Project ID: 610490)                 
* www.adept-project.eu                                            
*/

/* Licensed under the Apache License, Version 2.0 (the "License"); */
/* you may not use this file except in compliance with the License. */
/* You may obtain a copy of the License at */

/*     http://www.apache.org/licenses/LICENSE-2.0 */

/* Unless required by applicable law or agreed to in writing, software */
/* distributed under the License is distributed on an "AS IS" BASIS, */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. */
/* See the License for the specific language governing permissions and */
/* limitations under the License. */

/*
 * Pointer-chasing code.
 * Nick Johnson, EPCC
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <math.h>

#ifdef ADEPTMEAS
#include "../../workpackage3/adeptlib/adeptpowerclient_c.h"
#include "../../workpackage3/adeptlib/adeptpowerclient_types.h"
#endif

#define NUM_EVENTS 2

#define ITER 100
#define MB (1024 * 1024)
#define NANO_SECOND 1000000000

#ifdef __linux__
#define HUGEPAGE_SIZE (2 * MB)
#endif

#ifndef MEASMACRO
#define MEASMACRO asm volatile ("add %%ebx,%%eax\n\t" ::: "eax", "memory");
#endif


/* Definition for our very basic linked list */
struct ll{
  struct ll* next;
  char pad[56]; // This is to pad the struct up to a cache line size, for 64bit machine, ptr* is 8 bytes, so need 56 bytes of pad
};


/* Prototypes */
int sub_time_hr(struct timespec*, struct timespec*, struct timespec*);
int elapsed_time_hr(struct timespec, struct timespec, int);
void sattolo(int *, int);


/* Compute the time for one event given the start time, stop time, and number of iterations */
int elapsed_time_hr(struct timespec t1, struct timespec t2, int len){

  struct timespec elapsed;
  int rv = 0;
  rv = sub_time_hr(&elapsed, &t1, &t2);
  if (rv!=0){
    printf("Error in timing routines. End pre-empts Start\n");
    return 1;
  }
  double elapsed_duration = 0;

  /* This could potentially lead to loss of precision dependant on the rounding in conversion to double */
  elapsed_duration = elapsed.tv_sec + ((double)elapsed.tv_nsec/NANO_SECOND);
  elapsed_duration = (elapsed_duration / ((double)len*ITER)) * NANO_SECOND;

  /* To match the graphing code in Moncef's code */
  printf("%ld %lf\n", len*sizeof(struct ll), elapsed_duration);
  return 0;
}


int sub_time_hr(struct timespec* result, struct timespec* start, struct timespec* end){

  if ((end->tv_nsec-start->tv_nsec)<0) {
    result->tv_sec = end->tv_sec-start->tv_sec-1;
    result->tv_nsec = 1000000000+end->tv_nsec-start->tv_nsec;
  } else {
    result->tv_sec = end->tv_sec-start->tv_sec;
    result->tv_nsec = end->tv_nsec-start->tv_nsec;
  }

  /*
   * If end.tv_sec < start.tv_sec, something is wrong
   * Normal operation is for the below to return 0
   */
  return end->tv_sec < start->tv_sec;
}


/* Sattolo Algorithm Shuffler */
void sattolo(int *array, int len){

  struct timeval tv;
  gettimeofday(&tv, NULL);
  int usec = tv.tv_usec;
  srand(usec);


  int i = 0;
  int j = 0;
  int t = 0;

  i = len;
  while (i>1){
    i-=1;
    j = rand() % i;
    t = array[j];
    array[j] = array[i];
    array[i] = t;
  }

}

/* Main */
int main(int argc, char**argv){

  int i = 0;
  int len = 0;
  int size = 64;
  int debug = 0;

  if (argc==3){
    size = atoi(argv[1]);
    debug = atoi(argv[2]);
  }
  else{
    printf("Usage %s <memory size in kB> <debug 1=TRUE>\n", argv[0]);
  }

  int s_size = (int)sizeof(struct ll);
  len = (size*1024)/s_size;


  if (debug>0){
    printf("Size of struct is %d\n", s_size);
    printf("This BM will use %dkB of memory.\n", (s_size*len)/1024);
    printf("Request was for  %dkB of memory.\n", size);
  }

  /*
   * arr is our linked list
   * a is the current pointer which traverses the list
   * ind is the array we use to track the "next" element to point to in the list
   */
  struct ll* arr;
  struct ll* a;
  int* ind;
  ind = (int*)calloc((size_t)len, sizeof(int));
  if (ind==NULL){
    printf("Error allocating memory for indices array. Exiting\n");
    return 1;
  }

  /* If the system doesn't support THP, use the traditional calls to allocate arr */
#ifndef THP
  arr = (struct ll*)calloc((size_t)len, sizeof(struct ll));
  if (arr==NULL){
    printf("Error allocating memory for linked list. Exiting\n");
    return 1;
  }
#endif


  /* Linux doesn't always allocate large pages when the alloc size is smaller than the size of a large pages */
  /* (or one of its multiples). */
  /* To ensure that it will use large pages, we allocate using a size which is a multiple of the page size. */
  /* It might look like a waste of space but this space would be wasted anyway when a large page is allocated */
#ifdef THP
  if (debug>0){
    printf("THP HPS=%d\n", HUGEPAGE_SIZE);
  }
  int array_size = s_size * len;
  const int size_aligned_on_large_page_size = (array_size + HUGEPAGE_SIZE - 1) & ~(HUGEPAGE_SIZE - 1);

  if (posix_memalign((void**) &arr, HUGEPAGE_SIZE, size_aligned_on_large_page_size)){
    perror("posix_memalign failed\n");
    exit(1);
  }
  if (arr==NULL){
    printf("Error allocating memory for linked list. Exiting\n");
    return 1;
  }

  madvise(arr, size_aligned_on_large_page_size, MADV_HUGEPAGE);
#endif






  /*
   * Seed ind with values of the "next" location for the linked list
   * Print these out for verification
   */
  for (i=0;i<len;i++){
    ind[i] = i;
  }

  /*
   * Shuffle the elements of ind such that the value at position i indicates the "next" position to jump to
   * You must use the Sattolo algorithm to ensure that all positions are touched and there are no loops or shorts
   */
  sattolo(&ind[0], len);


  /*
   * Set up the linked lists "next" pointers using the values in ind
   * For example, if ind[0] is 7, arr[0].next is the address of arr[7]
   * If we hit the last element, point to NULL as a termination condition
   */
  for(i=0;i<len;i++){
    if (ind[i] == 0){
      arr[i].next = NULL;
    }
    else{
      arr[i].next = &arr[ind[i]];
    }
  }

  /* Dispose of the memory for the ind array. */
  free(ind);


  /*
   * Step through arr once so that the main measurement loop is not hit with cold misses.
   */
  a = &arr[0];
  while(a!=NULL){
    a = a->next;
    /* CPU will stall here whilst waiting for a fetch from memory */
    asm volatile ("" :: "r"(a));
  }


  struct timespec start, end;


#ifdef ADEPTMEAS
  InitAdeptPowerClient();
  printf("Initialized.\n");
  ConnectHost("172.16.29.60", 9071);
  SetResultsPathNFS("BOUNTY", 0);
  sleep(3);
  Start();
#endif
  /*
   * The chasing loop
   * This is based on wikipedia entries, scraps in papers and my memory of Moncef's code
   */
  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  
  for (i=0;i<ITER;i++){
    a = &arr[0];
    while(a!=NULL){
      a = a->next;
      /* CPU will stall here whilst waiting for a fetch from memory */
      MEASMACRO
        asm volatile ("" :: "r"(a));
    }
  }
  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
  /*
   * Compute some intermediate values and process results.
   */
  elapsed_time_hr(start, end, len);

  /*
   * The chasing loop WITHOUT additional instructions
   * This is based on wikipedia entries, scraps in papers and my memory of Moncef's code
   */

  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  for (i=0;i<ITER;i++){
    a = &arr[0];
    while(a!=NULL){
      a = a->next;
      /* CPU will stall here whilst waiting for a fetch from memory */
      asm volatile ("" :: "r"(a));
    }
  }
  clock_gettime(CLOCK_MONOTONIC_RAW, &end);
#ifdef ADEPTMEAS
  Stop();
#endif
  /*
   * Compute some intermediate values and process results.
   */
  elapsed_time_hr(start, end, len);
#ifdef ADEPTMEAS
  CleanupAdeptPowerClient();
#endif
  return 0;
}
