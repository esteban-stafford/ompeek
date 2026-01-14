# OMPeek

OMPeek is a lightweight tool for profiling and visualizing the execution of OpenMP applications. It uses the OMPT interface to intercept OpenMP events and record their timings. The collected data can be viewed as a simple log file or as an interactive HTML burst graph.

## Installation

To build, you need a C++ compiler that supports C++11 and OpenMP, `clang` is recommended. The provided `Makefile` uses `clang++`, but it can be easily adapted for other compilers.

To build the project, simply run `make`:

```bash
make
```

This will create the `libompeek.so` shared library and the `ompeek` executable script in the current directory.

## Usage

The `ompeek` script is the recommended way to run an OpenMP application with OMPeek. It handles the environment variables required to preload the OMPeek library and configure its output.

```bash
./ompeek [options] -- ./your_openmp_app [app-args...]
```

### Options

*   `--filename <name>`: Specifies the name of the output file.
*   `--file-format <fmt>`: Specifies the format of the output file. Possible values are `log` and `html`.
*   `-h`, `--help`: Shows the help message.

### Visualization

OMPeek can generate an interactive HTML file that visualizes the execution of the OpenMP application. Each thread is represented as a row, and the different OpenMP constructs are shown as colored bursts.

To generate an HTML visualization, set the `--file-format` option to `html`. The HTML viewer shows OpenMP execution bursts on a per-thread timeline. 

* **X-axis units**: Time can be displayed in seconds, milliseconds, or microseconds; the axis rescales accordingly.
* **Colouring**: Bursts can be coloured by OpenMP construct type, by code pointer, or by user annotations, helping identify where time is spent.
* **Parallel metrics**: If a serial reference time is provided, the tool can compute some parallel metrics: speedup, global efficiency (speedup per thread), computation efficiency (useful work vs total work), and load balance (uniformity of work across threads).

Together, these allow quick visual and quantitative assessment of parallel performance.

## Example

Here is a simple example of how to use OMPeek with the provided `demo` application.

1.  **Build the project:**

```bash
make
```

2.  **Run the `demo` application with OMPeek and generate an HTML trace:**

```bash
./ompeek --file-format html --filename demo.html -- ./demo
```

3.  **Open the `demo.html` file in your browser to see the trace.**

## Internals & Advanced Usage

This section explains the inner workings of OMPeek for users who want to customize its behavior or integrate it into their own build systems.

### Communication between the Script and the Library

The `ompeek` script communicates with the `libompeek.so` library through environment variables:

*   `OMP_TOOL_LIBRARIES`: This is a standard OpenMP environment variable that tells the OpenMP runtime to load the specified OMPT tool.
*   `LD_PRELOAD`: The script also uses `LD_PRELOAD` to ensure the library is loaded.
*   `OMPEEK_FILENAME`: This variable is used by `libompeek.so` to determine the name of the output file.
*   `OMPEEK_FILE_FORMAT`: This variable tells `libompeek.so` which format to use for the output (`log` or `html`).

### User Instrumentation

OMPeek allows you to add custom information to the trace using the `ompeek_set_id` and `ompeek_get_id` functions. To use them, you need to include the `ompeek.h` header in your source code.

*   `ompeek_set_id(int id, int level)`: Sets a user-defined ID and level for the current burst.
*   `ompeek_get_id(int *id, int *level)`: Retrieves the user-defined ID and level for the current burst.

These functions are defined as weak symbols, so your application will still run without OMPeek. When OMPeek is loaded, these calls will be resolved to the functions in the tool.

Here is an example of how to use the user instrumentation in your code:

```c
#include <omp.h>
#include "ompeek.h"

void my_function() {
    #pragma omp parallel for
    for (int i = 0; i < 10; i++) {
        ompeek_set_id(1, i);
        // your code here
    }
}
```

The first argument to `ompeek_set_id` can be used as an event type identifier, and the second argument is a level that can be used to show how code advances through different stages. In the HTML viewer, bursts with the same type will have the same color, and the lever will affect the brightness of the color.
