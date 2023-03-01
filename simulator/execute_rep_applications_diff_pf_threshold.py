import threading
import os
import subprocess
import time
from datetime import datetime
import csv
from batch_prefetcher_generator import get_hops_thresholds, get_count_thresholds

def mkdir_p(directory):
    try:
        os.makedirs(directory)
    except OSError as exc:  # Python >2.5
        if exc.errno == errno.EEXIST and os.path.isdir(directory):
            pass
        else:
            raise


hops_thresholds = get_hops_thresholds()
count_thresholds = get_count_thresholds()

maximum_thread = 80
threads = []
output_dir_name = "execution_statuses_"+datetime.now().strftime("%Y-%n-%d_%H-%M-%S")
output_dir = os.path.join(os.getcwd(), output_dir_name)
mkdir_p(output_dir)
summary_file_name = "execution_statuses_summary.csv"
summary_file = os.path.join(output_dir, summary_file_name)
summary_file_header = ["Processor Type", "Core #", "Benchmark Suite", "Benchmark Function", "Status"]


with open(summary_file, "a") as status_summary:
    csv_writer = csv.writer(status_summary, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
    csv_writer.writerow(summary_file_header)


def run_benchmark(processor_type, benchmark_suite, core_number, function):
    config_file = os.path.join("config_files", processor_type, benchmark_suite, core_number, function+".cfg")
    stdout_file = os.path.join(output_dir, processor_type+"_"+core_number+"_"+benchmark_suite+"_"+function+".txt")
    return_code = 1
    with open(stdout_file, "w") as output_file:
        return_code = subprocess.call(["build/opt/zsim", config_file], stdout=output_file, stderr=output_file)
    with open(summary_file, "a") as status_summary:
        csv_writer = csv.writer(status_summary, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
        if return_code == 0:
            csv_writer.writerow([processor_type, core_number, benchmark_suite, function, "Success"])
        else:
            csv_writer.writerow([processor_type, core_number, benchmark_suite, function, "Failed"])

# benchmark_suites_and_benchmarks_functions = {"chai" : ["BS_BEZIER_KERNEL", "HSTO_HSTO", "OOPPAD_OOPPAD"],
#     "darknet" : ["resnet152_gemm_nn", "yolo_gemm_nn"],
#     "hashjoin" : ["NPO_probehashtable", "PRH_histogramjoin"],
#     "ligra" : ["PageRank_edgeMapDenseUSA", "Radii_edgeMapSparseUSA", "Triangle_edgeMapDenseRmat"],
#     "phoenix" : ["Linearregression_main", "Stringmatch_main"],
#     "polybench" : ["linear-algebra_3mm", "linear-algebra_doitgen", "linear-algebra_gemm", "linear-algebra_gramschmidt", "linear-algebra_gemver", "linear-algebra_symm", "stencil_convolution-2d", "stencil_fdtd-apml"], 
#     "rodinia" : ["BFS_BFS"], "stream" : ["Add_Add", "Copy_Copy", "Scale_Scale", "Triad_Triad"]}
benchmark_suites_and_benchmarks_functions = {"chai" : ["OOPPAD_OOPPAD"],
    "hashjoin" : ["NPO_probehashtable"],
    "ligra" : ["PageRank_edgeMapDenseUSA"],
    "phoenix" : ["Linearregression_main", "Stringmatch_main"],
    "polybench" : ["linear-algebra_3mm", "linear-algebra_doitgen", "linear-algebra_gemm", "linear-algebra_gramschmidt", "linear-algebra_gemver", "stencil_convolution-2d"], 
    "stream" : ["Add_Add", "Copy_Copy", "Scale_Scale", "Triad_Triad"]}
# benchmark_suites_and_benchmarks_functions = {"hashjoin" : ["NPO_probehashtable"],
#     "phoenix" : ["Linearregression_main", "Stringmatch_main"],
#     "polybench" : ["linear-algebra_3mm", "linear-algebra_gemm", "linear-algebra_gemver"], 
#     "stream" : ["Triad_Triad"]}

processor_types = ["pim_ooo_netoh"] # Include one for baseline
core_numbers = ["32"]
processor_type_prefix = "pim_prefetch_netoh_"
prefetcher_types = ["swap"]
for prefetcher_type in prefetcher_types:
    for hops_threshold in hops_thresholds:
        for count_threshold in count_thresholds:
            processor_types.append(processor_type_prefix+prefetcher_type+str(hops_threshold)+"h"+str(count_threshold)+"c")

total_experiment_count = 0
for suite in benchmark_suites_and_benchmarks_functions.keys():
    total_experiment_count += len(benchmark_suites_and_benchmarks_functions[suite])
total_experiment_count *= len(processor_types)
total_experiment_count *= len(core_numbers)
print "Starting experiments. There are " + str(total_experiment_count) + " experiments to be scheduled, for cores: " + str(core_numbers) + " and processor types: " + str(processor_types)
scheduled_experiments = 0

for suite in benchmark_suites_and_benchmarks_functions.keys():
    for benchmark_function in benchmark_suites_and_benchmarks_functions[suite]:
        for processor_type in processor_types:
            for core_number in core_numbers:
                scheduled_experiments += 1
                print "Starting experment of " + suite + " " + benchmark_function + " with processor " + processor_type + " and " + core_number + " core(s) (" + str(scheduled_experiments) + "/" + str(total_experiment_count) + ")"
                current_thread = threading.Thread(target = run_benchmark, args = (processor_type, suite, core_number, benchmark_function))
                threads.append(current_thread)
                current_thread.start()
                if threading.active_count() >= maximum_thread + 1:
                    print "Reaching maximum allowed concurrent thread number of " + str(maximum_thread) + " threads. Waiting for threads to finish..."
                    while threading.active_count() >= maximum_thread + 1:
                        time.sleep(1)
print "The main thread has started all threads. Now waiting for threads to finish..."
for thread in threads:
    thread.join()