#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <unistd.h>
#include "ompeek.h"

#define DEBUG 0

void busy_wait(double seconds) {
  double start = omp_get_wtime();
#if DEBUG
  int event_id, event_level;
  ompeek_get_id(&event_id, &event_level);
  int thread_id = omp_get_thread_num();
  printf("  [busy_wait] thread=%d id=%d level=%d start %.2fs\n", thread_id, event_id, event_level, seconds);
#endif
  while (omp_get_wtime() - start < seconds);
#if DEBUG
  thread_id = omp_get_thread_num();
  printf("  [busy_wait] thread=%d id=%d level=%d end   %.2fs\n", thread_id, event_id, event_level, seconds);
#endif
}


void apply_filter(int id, double duration) {
    printf("Task %d started (%.2fs)\n", id, duration);
    busy_wait(duration/3);
    #pragma omp critical
    {
        printf("Task %d in critical section (%.2fs)\n", id, duration/3);
        busy_wait(duration/3);
    }
    busy_wait(duration/3);
    printf("Task %d finished\n", id);
}

int main() {
    printf("Serial (0.75s)\n");
    busy_wait(0.75); 

    // First parallel region
    #pragma omp parallel num_threads(4)
    {
        #pragma omp single
        {
            printf("Single section (0.25s)\n");
            busy_wait(0.25);
        }

        #pragma omp sections
        {
            #pragma omp section
            {
                printf("Section 1 (0.25s)\n");
                busy_wait(0.25);
                #pragma omp critical
                {
                   printf("Critical section (0.75s)\n");
                   busy_wait(0.75);
                }
                busy_wait(0.25);
            }
            #pragma omp section
            {
                printf("Section 2 (0.5s)\n");
                busy_wait(0.25);
                #pragma omp critical
                {
                   printf("Critical section (0.5s)\n");
                   busy_wait(0.5);
                }
                busy_wait(0.5);
            }
            #pragma omp section
            {
                printf("Section 3 (0.75s)\n");
                busy_wait(0.25);
                #pragma omp critical
                {
                   printf("Critical section (0.25s)\n");
                   busy_wait(0.25);
                }
                busy_wait(0.75);
            }
        }
    }

    printf("Serial (0.75s)\n");
    busy_wait(0.75); 
    // Second parallel region
    #pragma omp parallel num_threads(2)
    {
        #pragma omp for nowait
        for (int i = 0; i < 8; i++) {
            double duration = 0.1 + i * 0.05;
            printf("Loop iteration %d (%.2fs)\n", i, duration);
            busy_wait(duration);
        }

        #pragma omp single
        {
            int seq[5];
            for (int seq_id = 0; seq_id < 5; seq_id++) {
                seq[seq_id] = seq_id;
                ompeek_set_id(99,seq_id);
                busy_wait(0.5);
                for (int step = 0; step < 3; step++) {
                    double duration = 0.25 + 0.25 * seq_id + 0.25 * step;
                    #pragma omp task depend(inout: seq[seq_id]) firstprivate(seq_id, step, duration)
                    {
                        int id = seq_id * 10 + step;
                        ompeek_set_id(seq_id, step);
                        apply_filter(id, duration);
                    }
                }
            }
            #pragma omp taskwait
            busy_wait(0.25);
        }

        #pragma omp single
        {
            printf("Single section (0.25s)\n");
            busy_wait(0.25);
        } 

        #pragma omp single
        {
          int i = 0;
          do 
          {
            ompeek_set_id(1, i);
            printf("Single pre %d (0.25s)\n", i);
            busy_wait(0.25);

            #pragma omp task firstprivate(i)
            {
              ompeek_set_id(2, i);
              printf("  Task 2.%d start (0.75s)\n", i);
              busy_wait(.75);
            }
            #pragma omp task firstprivate(i)
            {
              ompeek_set_id(3, i);
              printf("  Task 3.%d start (1.0s)\n", i);
              busy_wait(1.0);
            }
            #pragma omp task firstprivate(i)
            {
              ompeek_set_id(4, i);
              printf("  Task 4.%d start (1.0s)\n", i);
              busy_wait(1.0);
            }
            #pragma omp taskwait
            #pragma omp task firstprivate(i)
            {
              ompeek_set_id(5, i);
              printf("  Task 5.%d start (0.5s)\n", i);
              busy_wait(0.5);
            }
          } while (i++ < 3);
          #pragma omp taskwait
          ompeek_set_id(6, 0);
          printf("Single post (0.33s)\n");
          busy_wait(0.33);
        }
    }
    busy_wait(0.33);

    return 0;
}
