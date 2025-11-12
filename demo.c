#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <unistd.h>
#include "burst.h"

void busy_wait(double seconds) {
    double start = omp_get_wtime();
    while (omp_get_wtime() - start < seconds);
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
    omp_set_num_threads(4);

    // First parallel region
    #pragma omp parallel
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
    busy_wait(0.75);

    // Second parallel region
    #pragma omp parallel
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
                for (int step = 0; step < 3; step++) {
                    double duration = 0.25 + 0.25 * seq_id + 0.25 * step;
                    #pragma omp task depend(inout: seq[seq_id]) firstprivate(seq_id, step, duration)
                    {
                        int id = seq_id * 10 + step;
                        burst_set_id(id);
                        apply_filter(id, duration);
                    }
                }
            }
        }
        #pragma omp taskwait

        #pragma omp single
        {
            printf("Single section (0.25s)\n");
            busy_wait(0.25);
        }
    }

    return 0;
}
