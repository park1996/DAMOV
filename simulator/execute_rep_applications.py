import threading
import os
import subprocess

maximum_thread = 20
running_num_threads = 0
thread_statuses = dict()
threads = []
experiment_status = []
def run_benchmark(processor_type, benchmark_suite, core_number, function):
    config_file = os.path.join("config_files", processor_type, benchmark_suite, core_number, function+".cfg")
    return_code = subprocess.call(["build/opt/zsim", config_file])
    if return_code == 0:
        thread_statuses[config_file] = "Success"
    else:
        thread_statuses[config_file] = "Failed"


# benchmark_suites_and_benchmarks_functions = {"chai" : ["BS_BEZIER_KERNEL", "HSTO_HSTO", "OOPPAD_OOPPAD"],
#     "darknet" : ["resnet152_gemm_nn", "yolo_gemm_nn"],
#     "hashjoin" : ["NPO_probehashtable", "PRH_histogramjoin"],
#     "ligra" : ["PageRank_edgeMapDenseUSA", "Radii_edgeMapSparseUSA", "Triangle_edgeMapDenseRmat"],
#     "phoenix" : ["Linearregression_main", "Stringmatch_main"],
#     "polybench" : ["linear-algebra_3mm", "linear-algebra_doitgen", "linear-algebra_gemm", "linear-algebra_gramschmidt", "linear-algebra_gemver", "linear-algebra_symm", "stencil_convolution-2d", "stencil_fdtd-apml"], 
#     "rodinia" : ["BFS_BFS"], "stream" : ["Add_Add", "Copy_Copy", "Scale_Scale", "Triad_Triad"]}
benchmark_suites_and_benchmarks_functions = {"chai" : ["OOPPAD_OOPPAD"],
    "hashjoin" : ["NPO_probehashtable", "PRH_histogramjoin"],
    "ligra" : ["PageRank_edgeMapDenseUSA"],
    "phoenix" : ["Linearregression_main", "Stringmatch_main"],
    "polybench" : ["linear-algebra_3mm", "linear-algebra_doitgen", "linear-algebra_gemm", "linear-algebra_gramschmidt", "linear-algebra_gemver", "linear-algebra_symm", "stencil_convolution-2d", "stencil_fdtd-apml"],
    "rodinia" : ["BFS_BFS"],
    "stream" : ["Add_Add", "Copy_Copy", "Scale_Scale"]}

# processor_types = ["host_ooo/prefetch", "host_ooo/no_prefetch", "pim_ooo"]
processor_types = ["pim_ooo_netoh_withswapsubpf"]
# core_numbers = ["1", "4", "16", "64", "256"]
core_numbers = ["32"]

for suite in benchmark_suites_and_benchmarks_functions.keys():
    for benchmark_function in benchmark_suites_and_benchmarks_functions[suite]:
        for processor_type in processor_types:
            for core_number in core_numbers:
                print "Starting experment of " + suite + " " + benchmark_function + " with processor " + processor_type + " and " + core_number + " core(s)"
                current_thread = threading.Thread(target = run_benchmark, args = (processor_type, suite, core_number, benchmark_function))
                threads.append(current_thread)
                current_thread.start()
                running_num_threads += 1
                if running_num_threads >= maximum_thread:
                    print "Reaching maximum allowed concurrent threads. Waiting for threads to finish..."
                    for thread in threads:
                        thread.join()
                    thread = []
                    running_num_threads = 0

with open("execution_statuses.txt", "w") as status_file:
    for experiment in thread_statuses.keys():
        status_file.write("Experiment " + experiment + " is completed with status " + thread_statuses[experiment])
