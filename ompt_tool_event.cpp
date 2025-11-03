#include <omp.h>
#include <ompt.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <mutex>
#include <vector>

static std::chrono::steady_clock::time_point reference_time;

struct TaskInfo {
    std::chrono::steady_clock::time_point start_time;
    void* codeptr = nullptr;
    bool active = false;
};

const int MAX_THREADS = 128;
std::vector<TaskInfo> thread_tasks(MAX_THREADS);
std::mutex task_mutex;
std::ofstream logFile("omp_events.log");
std::mutex logMutex;

std::string get_construct_type(ompt_work_t type) {
    switch (type) {
        case ompt_work_loop: return "for";
        case ompt_work_sections: return "sections";
        case ompt_work_single_executor: return "single";
        case ompt_work_single_other: return "single_other";
        default: return "unknown";
    }
}

void log_event(int thread_id, const std::string& type, void* pointer,
               std::chrono::steady_clock::time_point begin,
               std::chrono::steady_clock::time_point end) {
    std::lock_guard<std::mutex> guard(logMutex);
    auto begin_rel = std::chrono::duration_cast<std::chrono::microseconds>(begin - reference_time).count();
    auto end_rel = std::chrono::duration_cast<std::chrono::microseconds>(end - reference_time).count();

    logFile << thread_id << ":"
            << begin_rel << ":"
            << end_rel << ":"
            << type << ":"
            << pointer << std::endl;
}

// Work construct callback
static void on_work(
    ompt_work_t work_type,
    ompt_scope_endpoint_t endpoint,
    ompt_data_t *parallel_data,
    ompt_data_t *task_data,
    uint64_t count,
    const void *codeptr_ra)
{
    static thread_local std::chrono::steady_clock::time_point begin_time;

    if (endpoint == ompt_scope_begin) {
        begin_time = std::chrono::steady_clock::now();
    } else if (endpoint == ompt_scope_end) {
        auto end_time = std::chrono::steady_clock::now();
        int thread_id = omp_get_thread_num();
        log_event(thread_id, get_construct_type(work_type), (void*)codeptr_ra, begin_time, end_time);
    }
}

static void on_task_create(
    ompt_data_t *encountering_task_data,
    const ompt_frame_t *encountering_task_frame,
    ompt_data_t *new_task_data,
    int flags,
    int has_dependences,
    const void *codeptr_ra)
{
    // Store the code pointer in the task data
    new_task_data->ptr = (void*)codeptr_ra;
}

// Task execution callback (using task_schedule instead of deprecated task)
static void on_task_schedule(
    ompt_data_t *prior_task_data,
    ompt_task_status_t prior_task_status,
    ompt_data_t *next_task_data)
{
    auto now = std::chrono::steady_clock::now();
    int thread_id = omp_get_thread_num();

    // End previous task if active
    if (thread_tasks[thread_id].active) {
        log_event(thread_id, "task", thread_tasks[thread_id].codeptr,
                  thread_tasks[thread_id].start_time, now);
        thread_tasks[thread_id].active = false;
    }

    // Start new task only if valid
    if (next_task_data && next_task_data->ptr) {
        thread_tasks[thread_id].start_time = now;
        thread_tasks[thread_id].codeptr = next_task_data->ptr;
        thread_tasks[thread_id].active = true;
    }
}

// Mutex acquire callback (waiting)
static void on_mutex_acquire(
    ompt_mutex_t kind,
    unsigned int hint,
    unsigned int impl,
    ompt_wait_id_t wait_id,
    const void *codeptr_ra)
{
    auto now = std::chrono::steady_clock::now();
    int thread_id = omp_get_thread_num();
    log_event(thread_id, "acquiring", (void*)codeptr_ra, now, now);
}

// Mutex acquired callback (locked)
static void on_mutex_acquired(
    ompt_mutex_t kind,
    ompt_wait_id_t wait_id,
    const void *codeptr_ra)
{
    auto now = std::chrono::steady_clock::now();
    int thread_id = omp_get_thread_num();
    log_event(thread_id, "locked", (void*)codeptr_ra, now, now);
}

// Initialization
static int ompt_initialize(
    ompt_function_lookup_t lookup,
    int initial_device_num,
    ompt_data_t *tool_data)
{
   reference_time = std::chrono::steady_clock::now();
   auto ompt_set_callback = (ompt_set_callback_t)lookup("ompt_set_callback");

   ompt_set_callback(ompt_callback_work, (ompt_callback_t)&on_work);
   ompt_set_callback(ompt_callback_task_schedule, (ompt_callback_t)&on_task_schedule);
   ompt_set_callback(ompt_callback_mutex_acquire, (ompt_callback_t)&on_mutex_acquire);
   ompt_set_callback(ompt_callback_mutex_acquired, (ompt_callback_t)&on_mutex_acquired);
   ompt_set_callback(ompt_callback_task_create, (ompt_callback_t)&on_task_create);

   return 1; // success
}

static void ompt_finalize(ompt_data_t *tool_data) {
    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> guard(task_mutex);

    for (int thread_id = 0; thread_id < thread_tasks.size(); ++thread_id) {
        if (thread_tasks[thread_id].active) {
            log_event(thread_id, "task", thread_tasks[thread_id].codeptr,
                      thread_tasks[thread_id].start_time, now);
        }
    }

    logFile.close();
}

extern "C" ompt_start_tool_result_t* ompt_start_tool(
    unsigned int omp_version,
    const char *runtime_version)
{
    std::cerr << "[OMPT Tool] Library loaded. OMPT version: " << omp_version
              << ", Runtime version: " << runtime_version << std::endl;

    static ompt_start_tool_result_t ompt_start_tool_result = {
        &ompt_initialize,
        &ompt_finalize,
        0
    };
    return &ompt_start_tool_result;
}
