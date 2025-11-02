#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <unistd.h>

void busy_wait(double seconds) {
    double start = omp_get_wtime();
    while (omp_get_wtime() - start < seconds);
}

void apply_filter(int id, double duration) {
    printf("Task %d started (%.2fs)\n", id, duration);
    busy_wait(duration);
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
                busy_wait(0.75);
                #pragma omp critical
                {
                   printf("Critical section (0.25s)\n");
                   busy_wait(0.25);
                }
            }
            #pragma omp section
            {
                printf("Section 2 (0.5s)\n");
                busy_wait(0.5);
                #pragma omp critical
                {
                   printf("Critical section (0.25s)\n");
                   busy_wait(0.25);
                }
                busy_wait(0.25);
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

        #pragma omp barrier

        #pragma omp single
        {
            for (int seq = 0; seq < 3; seq++) {
                for (int step = 0; step < 4; step++) {
                    double duration = 0.25 + 0.25 * (step % 3);
                    #pragma omp task depend(inout: seq) firstprivate(seq, step, duration)
                    {
                        int id = seq * 10 + step;
                        apply_filter(id, duration);
                    }
                }
            }
        }
    }

    return 0;
}
