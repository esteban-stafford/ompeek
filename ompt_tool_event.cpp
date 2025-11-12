#include <omp.h>
#include <ompt.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <mutex>
#include <vector>
#include <stack>
#include <cstdlib>
#include <string>
#include <cstring>
#include <sstream>

#include "burst_viewer_embed.cpp"

static std::chrono::steady_clock::time_point reference_time;

enum class OutputFormat { LOG, HTML };

static std::string g_tool_filename;
static OutputFormat g_tool_format = OutputFormat::LOG;

typedef enum {
    BURST_TYPE_FOR,
    BURST_TYPE_SECTION,
    BURST_TYPE_SINGLE,
    BURST_TYPE_SINGLE_OTHER,
    BURST_TYPE_TASK,
    BURST_TYPE_CRITICAL,
    BURST_TYPE_WAIT,
    BURST_TYPE_UNKNOWN
} BurstType;

struct Burst {
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point end_time;
    BurstType type = BURST_TYPE_UNKNOWN;
    const void* codeptr = nullptr;
    int user_data = 0;
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
std::mutex logMutex;
static std::ofstream logFile;

static void init_tool_output_config() {
  const char* fmt_env = std::getenv("TOOL_FILE_FORMAT");
  const char* file_env = std::getenv("TOOL_FILENAME");

  std::string fmt = fmt_env ? fmt_env : "log";
  if (fmt == "html" || fmt == "HTML")
    g_tool_format = OutputFormat::HTML;
  else
    g_tool_format = OutputFormat::LOG;

  if (file_env && std::strlen(file_env) > 0)
    g_tool_filename = file_env;
  else
    g_tool_filename = (g_tool_format == OutputFormat::HTML)
      ? "omp_events.html"
      : "omp_events.log";

  std::cerr << "[ompt_tool_event] Output file: " << g_tool_filename
    << " (" << (g_tool_format == OutputFormat::HTML ? "HTML" : "LOG") << ")\n";
}

static void open_log_file() {
  if (!logFile.is_open()) {
    logFile.open(g_tool_filename, std::ios::out | std::ios::trunc);
    if (!logFile) {
      std::cerr << "[ompt_tool_event] Failed to open output file: "
        << g_tool_filename << "\n";
    }
  }
  if (g_tool_format == OutputFormat::HTML) {
    logFile << BURST_VIEWER_HTML_HEADER;
    logFile << "<script id=\"embedded-events\" type=\"text/plain\">\n";
  }
}

static void close_log_file() {
  if (logFile.is_open()) {
    if (g_tool_format == OutputFormat::HTML) {
      logFile << "</script>\n";
      logFile << BURST_VIEWER_HTML_FOOTER;
    }
    logFile.close();
  }
}

void log_burst(const Burst& burst, int thread_id) {
  std::lock_guard<std::mutex> guard(logMutex);
  auto begin_rel = std::chrono::duration_cast<std::chrono::microseconds>(burst.start_time - reference_time).count();
  auto end_rel = std::chrono::duration_cast<std::chrono::microseconds>(burst.end_time - reference_time).count();
  logFile << thread_id
          << ":" << begin_rel
          << ":" << end_rel
          << ":" << burst.type
          << ":" << burst.codeptr
          << ":" << burst.user_data
          << std::endl;
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
        switch (work_type) {
            case ompt_work_loop:
                burst.type = BURST_TYPE_FOR;
                break;
            case ompt_work_sections:
                burst.type = BURST_TYPE_SECTION;
                break;
            case ompt_work_single_executor:
                burst.type = BURST_TYPE_SINGLE;
                break;
            case ompt_work_single_other:
                burst.type = BURST_TYPE_SINGLE_OTHER;
                break;
            default:
                burst.type = BURST_TYPE_UNKNOWN;
                break;
        }
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
        burst.type = BURST_TYPE_TASK;
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
    wait_burst.type = BURST_TYPE_WAIT;
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
    critical_burst.type = BURST_TYPE_CRITICAL;
    critical_burst.codeptr = codeptr_ra;
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

static void burst_set_id_tool(int id)
{
  int thread_id = omp_get_thread_num();
  Burst& burst = thread_bursts[thread_id].top();
  burst.user_data = id;
}


// Initialization
static int ompt_initialize(
    ompt_function_lookup_t lookup,
    int initial_device_num,
    ompt_data_t *tool_data)
{
   init_tool_output_config();
   open_log_file();
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
    close_log_file();
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
