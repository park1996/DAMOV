import threading
import os
import subprocess

experiment_status = []

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

processor_types = ["pim_ooo_memtrace"]
core_numbers = ["32"]

for suite in benchmark_suites_and_benchmarks_functions.keys():
    for benchmark_function in benchmark_suites_and_benchmarks_functions[suite]:
        for processor_type in processor_types:
            for core_number in core_numbers:
                print "\n-----Starting experment of " + suite + " " + benchmark_function + " with processor " + processor_type + " and " + core_number + " core(s)-----"
                config_file = os.path.join("config_files", processor_type, suite, core_number, benchmark_function+".cfg")
                return_code = subprocess.call(["build/opt/zsim", config_file])
                if return_code == 0:
                    experiment_status.append("Experment of " + suite + " " + benchmark_function + " with processor " + processor_type + " and " + core_number + " core(s) completed successfully")
                else:
                    experiment_status.append("Experment of " + suite + " " + benchmark_function + " with processor " + processor_type + " and " + core_number + " core(s) failed")

for status in experiment_status:
    print(status)