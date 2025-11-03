#include <omp.h>
#include <ompt.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <mutex>
#include <vector>
#include <stack>

static std::chrono::steady_clock::time_point reference_time;

struct Burst {
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point end_time;
//    const std::string type;
    const void* codeptr = nullptr;
};

struct TaskInfo {
    std::chrono::steady_clock::time_point start_time;
    void* codeptr = nullptr;
    bool active = false;
};

const int MAX_THREADS = 128;
std::vector<std::stack<Burst>> thread_bursts(MAX_THREADS);
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

void log_burst(const Burst& burst, int thread_id) {
  std::lock_guard<std::mutex> guard(logMutex);
  auto begin_rel = std::chrono::duration_cast<std::chrono::microseconds>(burst.start_time - reference_time).count();
  auto end_rel = std::chrono::duration_cast<std::chrono::microseconds>(burst.end_time - reference_time).count();

  std::string construct_type = "";

  logFile << thread_id
          << ":" << begin_rel
          << ":" << end_rel
          << construct_type << ":"
          << burst.codeptr << std::endl;
}

static void on_work(
    ompt_work_t work_type,
    ompt_scope_endpoint_t endpoint,
    ompt_data_t *parallel_data,
    ompt_data_t *task_data,
    uint64_t count,
    const void *codeptr_ra)
{
    int thread_id = omp_get_thread_num();
    if (endpoint == ompt_scope_begin) {
        Burst& burst = thread_bursts[thread_id].emplace();
        burst.codeptr = (void*)codeptr_ra;
        burst.start_time = std::chrono::steady_clock::now();
    } else if (endpoint == ompt_scope_end) {
        Burst burst = thread_bursts[thread_id].top();
        burst.end_time = std::chrono::steady_clock::now();
        log_burst(burst, thread_id);
        thread_bursts[thread_id].pop();
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

static void on_task_schedule(
    ompt_data_t *prior_task_data,
    ompt_task_status_t prior_task_status,
    ompt_data_t *next_task_data)
{
    int thread_id = omp_get_thread_num();
    auto now = std::chrono::steady_clock::now();

    // End previous task if active
    if (thread_tasks[thread_id].active) {
        Burst& burst = thread_bursts[thread_id].top();
        burst.end_time = now;
        log_burst(burst, thread_id);
        thread_tasks[thread_id].active = false;
    }

    // Start new task only if valid
    if (next_task_data && next_task_data->ptr) {
        Burst& burst = thread_bursts[thread_id].emplace();
        burst.start_time = now;
        burst.codeptr = next_task_data->ptr;
        thread_tasks[thread_id].active = true;
    }
}

static void on_mutex_acquire(
    ompt_mutex_t kind,
    unsigned int hint,
    unsigned int impl,
    ompt_wait_id_t wait_id,
    const void *codeptr_ra)
{
    int thread_id = omp_get_thread_num();
    Burst& burst = thread_bursts[thread_id].top();
    auto now = std::chrono::steady_clock::now();
    burst.end_time = now;
    log_burst(burst, thread_id);

    Burst& wait_burst = thread_bursts[thread_id].emplace();
    wait_burst.start_time = now;
    wait_burst.codeptr = codeptr_ra;
}

static void on_mutex_acquired(
    ompt_mutex_t kind,
    ompt_wait_id_t wait_id,
    const void *codeptr_ra)
{
    int thread_id = omp_get_thread_num();
    Burst& wait_burst = thread_bursts[thread_id].top();
    auto now = std::chrono::steady_clock::now();
    wait_burst.end_time = now;
    //log_burst(wait_burst, thread_id);

    // These two stack operations could be combined, but keeping them separate for clarity
    thread_bursts[thread_id].pop();
    Burst& critical_burst = thread_bursts[thread_id].emplace();
    critical_burst.start_time = now;
}

static void on_mutex_released(
    ompt_mutex_t kind,
    ompt_wait_id_t wait_id,
    const void *codeptr_ra)
{
    int thread_id = omp_get_thread_num();
    Burst& critical_burst = thread_bursts[thread_id].top();
    auto now = std::chrono::steady_clock::now();
    critical_burst.end_time = now;
    log_burst(critical_burst, thread_id);
    thread_bursts[thread_id].pop();

    Burst& burst = thread_bursts[thread_id].top();
    burst.start_time = now;
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
   ompt_set_callback(ompt_callback_task_create, (ompt_callback_t)&on_task_create);
   ompt_set_callback(ompt_callback_task_schedule, (ompt_callback_t)&on_task_schedule);
   ompt_set_callback(ompt_callback_mutex_acquire, (ompt_callback_t)&on_mutex_acquire);
   ompt_set_callback(ompt_callback_mutex_acquired, (ompt_callback_t)&on_mutex_acquired);
   ompt_set_callback(ompt_callback_mutex_released, (ompt_callback_t)&on_mutex_released);

   return 1;
}

static void ompt_finalize(ompt_data_t *tool_data) {
    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> guard(task_mutex);

    std::cout << "[OMPT Tool] Finalizing and flushing remaining bursts." << std::endl;

    for (int thread_id = 0; thread_id < thread_bursts.size(); ++thread_id) {
      if (!thread_bursts[thread_id].empty()) {
          /* std::cerr << "  Flushing burst for thread " << thread_id << std::endl;
          Burst& burst = thread_bursts[thread_id].top();
          burst.end_time = now;
          log_burst(burst, thread_id); */
          //thread_bursts[thread_id].pop();
          // if(!thread_bursts[thread_id].empty()) {
          //     Burst& next_burst = thread_bursts[thread_id].top();
          //     next_burst.start_time = now;
          // }
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
