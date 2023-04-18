import threading
import os
import subprocess
import time
from datetime import datetime
import csv
from batch_prefetcher_generator import get_hops_thresholds, get_count_thresholds, get_debug_flags, get_prefetcher_types

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
prefetcher_types = get_prefetcher_types()
print "Starting execution with debug traces off"
debug_tag = "debugoff"

start_time = datetime.now()
print "We are starting experiment on "+start_time.strftime("%Y-%m-%d_%H-%M-%S")
maximum_thread = 100
threads = []
output_dir_name = "execution_statuses_"+start_time.strftime("%Y-%m-%d_%H-%M-%S")
output_dir = os.path.join(os.getcwd(), output_dir_name)
mkdir_p(output_dir)
summary_file_name = "execution_statuses_summary.csv"
summary_file = os.path.join(output_dir, summary_file_name)
summary_file_header = ["Processor Type", "Core #", "Benchmark Suite", "Benchmark Function", "Status"]
failed_benchmarks = []


with open(summary_file, "a") as status_summary:
    csv_writer = csv.writer(status_summary, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
    csv_writer.writerow(summary_file_header)


def run_benchmark(processor_type, benchmark_suite, core_number, function):
    config_file = os.path.join("config_files", processor_type, benchmark_suite, core_number, function+".cfg")
    stdout_file = os.path.join(output_dir, processor_type.replace("/", "_")+"_"+core_number+"_"+benchmark_suite+"_"+function+".txt")
    return_code = 1
    with open(stdout_file, "w") as output_file:
        return_code = subprocess.call(["build/opt/zsim", config_file], stdout=output_file, stderr=output_file)
    with open(summary_file, "a") as status_summary:
        csv_writer = csv.writer(status_summary, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
        if return_code == 0:
            csv_writer.writerow([processor_type.replace("/", "_"), core_number, benchmark_suite, function, "Success"])
        else:
            csv_writer.writerow([processor_type.replace("/", "_"), core_number, benchmark_suite, function, "Failed"])
            if "pim_prefetch" in processor_type:
                failed_benchmarks.append([processor_type, benchmark_suite, core_number, function])

benchmark_suites_and_benchmarks_functions = {"chai" : ["BS_BEZIER_KERNEL", "HSTO_HSTO", "OOPPAD_OOPPAD"],
    "darknet" : ["resnet152_gemm_nn", "yolo_gemm_nn"],
    "hashjoin" : ["NPO_probehashtable", "PRH_histogramjoin"],
    "hpcg" : ["HPCG_ComputePrologation", "HPCG_ComputeRestriction", "HPCG_ComputeSPMV", "HPCG_ComputeSYMGS"],
    "ligra" : ["PageRank_edgeMapDenseUSA", "Radii_edgeMapSparseUSA", "Triangle_edgeMapDenseRmat"],
    "parsec" : ["Fluidaminate_ProcessCollision2MT"],
    "phoenix" : ["Linearregression_main", "Stringmatch_main"],
    "polybench" : ["linear-algebra_3mm", "linear-algebra_doitgen", "linear-algebra_gemm", "linear-algebra_gramschmidt", "linear-algebra_gemver", "linear-algebra_symm", "stencil_convolution-2d", "stencil_fdtd-apml"], 
    "rodinia" : ["BFS_BFS"],
    "splash-2" : ["FFT_Reverse", "FFT_Transpose", "Oceanncp_jacobcalc", "Oceanncp_laplaccalc", "Oceancp_slave2", "Radix_slave_sort"],
    "stream" : ["Add_Add", "Copy_Copy", "Scale_Scale", "Triad_Triad"]}
# benchmark_suites_and_benchmarks_functions = {"chai" : ["OOPPAD_OOPPAD"],
#     "hashjoin" : ["NPO_probehashtable"],
#     "ligra" : ["PageRank_edgeMapDenseUSA"],
#     "phoenix" : ["Linearregression_main", "Stringmatch_main"],
#     "polybench" : ["linear-algebra_3mm", "linear-algebra_doitgen", "linear-algebra_gemm", "linear-algebra_gramschmidt", "linear-algebra_gemver", "stencil_convolution-2d"], 
#     "stream" : ["Add_Add", "Copy_Copy", "Scale_Scale", "Triad_Triad"]}
# benchmark_suites_and_benchmarks_functions = {"hashjoin" : ["NPO_probehashtable"],
#     "phoenix" : ["Linearregression_main", "Stringmatch_main"],
#     "polybench" : ["linear-algebra_3mm", "linear-algebra_gemm", "linear-algebra_gemver"], 
#     "stream" : ["Triad_Triad"]}

processor_types = ["pim_ooo_netoh"] # Include one for baseline
core_numbers = ["32"]
processor_type_prefix = "pim_prefetch_netoh_"
prefetcher_types = ["allocate"]
for prefetcher_type in prefetcher_types:
    for hops_threshold in hops_thresholds:
        for count_threshold in count_thresholds:
            processor_types.append(processor_type_prefix+prefetcher_type+"/"+str(hops_threshold)+"h"+str(count_threshold)+"c_"+debug_tag)

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
                print "Starting experment of " + suite + " " + benchmark_function + " with processor " + processor_type.replace("/", "_") + " and " + core_number + " core(s) (" + str(scheduled_experiments) + "/" + str(total_experiment_count) + ")"
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

if len(failed_benchmarks) > 0:
    print "The following benchmark runs has failed "+str(failed_benchmarks)
    print "Re-executing them with benchmark on for debug information"
    total_experiment_count = len(failed_benchmarks)
    scheduled_experiments = 0
    for benchmark in failed_benchmarks:
        processor_type = benchmark[0]
        suite = benchmark[1]
        core_number = benchmark[2]
        benchmark_function = benchmark[3]
        # processor_type = processor_type.replace("debugoff", "debugon")
        scheduled_experiments += 1
        print "Starting experment of " + suite + " " + benchmark_function + " with processor " + processor_type.replace("/", "_") + " and " + core_number + " core(s) (" + str(scheduled_experiments) + "/" + str(total_experiment_count) + ")"
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
end_time = datetime.now()
print "We are finishing at "+end_time.strftime("%Y-%m-%d %H:%M:%S")
print "It took "+str(end_time - start_time)
