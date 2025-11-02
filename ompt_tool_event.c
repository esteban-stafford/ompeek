#include <omp-tools.h>
#include <stdint.h>  // for uintptr_t
#include <omp.h>
#include <stdio.h>

#define MAX_THREADS 128
static uint64_t thread_ids[MAX_THREADS] = {0};
#define MAX_TASKS 10000
static const void* task_codeptrs[MAX_TASKS] = {0};
static uint64_t task_counter = 0;

static double get_time() {
    return omp_get_wtime();
}

static void print_event(int tid, const char* event) {
    printf("%.6f:%lu:%s::\n", get_time(), thread_ids[tid], event);
    fflush(stdout);
}

static void on_thread_begin(
    ompt_thread_t thread_type,
    ompt_data_t *thread_data)
{
    int tid = omp_get_thread_num();
    thread_data->value = tid;
    thread_ids[tid] = tid;
}

static void on_sync_region(
    ompt_sync_region_t kind,
    ompt_scope_endpoint_t endpoint,
    ompt_data_t *parallel_data,
    ompt_data_t *task_data,
    const void *codeptr_ra)
{
    int tid = omp_get_thread_num();
    if (kind == ompt_sync_region_barrier) {
        if (endpoint == ompt_scope_begin)
            print_event(tid, "wait");
        else if (endpoint == ompt_scope_end)
            print_event(tid, "work");
    }
}

static void on_work(
    ompt_work_t wstype,
    ompt_scope_endpoint_t endpoint,
    ompt_data_t *parallel_data,
    ompt_data_t *task_data,
    uint64_t count,
    const void *codeptr_ra)
{
    int tid = omp_get_thread_num();
    const char* construct = NULL;

    switch (wstype) {
        case ompt_work_loop: construct = "for"; break;
        case ompt_work_sections: construct = "sections"; break;
        case ompt_work_single_executor: construct = "single"; break;
        case ompt_work_single_other: construct = "single_other"; break;
        default: construct = "unknown"; break;
    }

    const char* phase = (endpoint == ompt_scope_begin) ? "begin" : "end";

    printf("%.6f:%d:%s:%s:%lx\n", omp_get_wtime(), tid, construct, phase, (uintptr_t)codeptr_ra);
    fflush(stdout);
}

static void on_mutex_acquire(
    ompt_mutex_t kind,
    unsigned int hint,
    unsigned int impl,
    ompt_wait_id_t wait_id,
    const void *codeptr_ra)
{
    if (kind == ompt_mutex_critical) {
        int tid = omp_get_thread_num();
        print_event(tid, "wait");  // Thread is waiting to enter critical section
    }
}

static void on_mutex_released(
    ompt_mutex_t kind,
    ompt_wait_id_t wait_id,
    const void *codeptr_ra)
{
    if (kind == ompt_mutex_critical) {
        int tid = omp_get_thread_num();
        print_event(tid, "work");  // Thread has entered or exited critical section
    }
}

static void on_task_create(
    ompt_data_t *parent_task_data,
    const ompt_frame_t *parent_task_frame,
    ompt_data_t *new_task_data,
    int flags,
    int has_dependences,
    const void *codeptr_ra)
{
    new_task_data->value = task_counter;
    task_codeptrs[task_counter] = codeptr_ra;
    task_counter++;
}

static void on_task_schedule(
    ompt_data_t *prior_task_data,
    ompt_task_status_t prior_task_status,
    ompt_data_t *next_task_data)
{
    int tid = omp_get_thread_num();
    uint64_t prior_task_id = prior_task_data->value;
    const void* prior_codeptr = task_codeptrs[prior_task_id];

    // Log task end
    printf("%.6f:%d:task_schedule:end:%lx\n",
           omp_get_wtime(), tid, (uintptr_t)prior_codeptr);

    // Log task begin
    uint64_t next_task_id = next_task_data->value;
    const void* next_codeptr = task_codeptrs[next_task_id];
    printf("%.6f:%d:task_schedule:begin:%lx\n",
           omp_get_wtime(), tid, (uintptr_t)next_codeptr);

    fflush(stdout);
}

static void on_task_end(ompt_data_t *task_data)
{
    int tid = omp_get_thread_num();
    uint64_t task_id = task_data->value;
    const void* codeptr = task_codeptrs[task_id];

    printf("%.6f:%d:task_end:%lx\n", omp_get_wtime(), tid, (uintptr_t)codeptr);
    fflush(stdout);
}

static void ompt_finalize(ompt_data_t *tool_data) {
    printf("OMPT tool finalized\n");
}

static int ompt_initialize(
    ompt_function_lookup_t lookup,
    int initial_device_num,
    ompt_data_t *tool_data)
{
    ompt_set_callback_t ompt_set_callback =
        (ompt_set_callback_t) lookup("ompt_set_callback");

    ompt_set_callback(ompt_callback_thread_begin, (ompt_callback_t) on_thread_begin);
    ompt_set_callback(ompt_callback_sync_region, (ompt_callback_t) on_sync_region);
    ompt_set_callback(ompt_callback_work, (ompt_callback_t) on_work);
    ompt_set_callback(ompt_callback_mutex_acquire, (ompt_callback_t) on_mutex_acquire);
    ompt_set_callback(ompt_callback_mutex_released, (ompt_callback_t) on_mutex_released);
    ompt_set_callback(ompt_callback_task_create, (ompt_callback_t) on_task_create);
    ompt_set_callback(ompt_callback_task_schedule, (ompt_callback_t) on_task_schedule);
    return 1;
}

ompt_start_tool_result_t* ompt_start_tool(unsigned int omp_version, const char *runtime_version) {
    static ompt_start_tool_result_t result = {
        .initialize = ompt_initialize,
        .finalize = ompt_finalize,
        .tool_data = {0}
    };
    return &result;
}
