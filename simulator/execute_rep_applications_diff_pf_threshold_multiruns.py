import threading
import os
import glob
import subprocess
import time
from datetime import datetime
import psutil
import csv
from batch_prefetcher_generator import get_hops_thresholds, get_count_thresholds, get_debug_flags, get_prefetcher_types, get_core_numbers

def mkdir_p(directory):
    try:
        os.makedirs(directory)
    except OSError as exc:  # Python >2.5
        if exc.errno == errno.EEXIST and os.path.isdir(directory):
            pass
        else:
            raise


hops_thresholds = get_hops_thresholds()
# count_thresholds = get_count_thresholds()
count_thresholds = [0, 1, 63]
max_memory_gb = 200
print "Starting execution with debug traces off"
debug_tag = "debugoff"

start_time = datetime.now()
print "We are starting experiment on "+start_time.strftime("%Y-%m-%d %H:%M:%S")
maximum_thread = 20
threads = []
output_dir_name = "execution_statuses_"+start_time.strftime("%Y-%m-%d_%H-%M-%S")
output_dir = os.path.join(os.getcwd(), output_dir_name)
mkdir_p(output_dir)
summary_file_name = "execution_statuses_summary.csv"
summary_file = os.path.join(output_dir, summary_file_name)
summary_file_header = ["Processor Type", "Core #", "Benchmark Suite", "Benchmark Function", "Status", "Start Time", "Finish Time", "Total Time"]
failed_benchmarks = []
iterations = 5
stats_folders = os.path.join(os.getcwd(), "zsim_stats")

with open(summary_file, "a") as status_summary:
    csv_writer = csv.writer(status_summary, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
    csv_writer.writerow(summary_file_header)

def clean_darknet_chai_inputs():
    if os.path.exists(os.path.join(os.getcwd(), "data")):
        subprocess.call(["rm", "-r", os.path.join(os.getcwd(), "data")])
    if os.path.exists(os.path.join(os.getcwd(), "cfg")):
        subprocess.call(["rm", "-r", os.path.join(os.getcwd(), "cfg")])
    if os.path.exists(os.path.join(os.getcwd(), "input")):
        subprocess.call(["rm", "-r", os.path.join(os.getcwd(), "input")])
    if os.path.exists(os.path.join(os.getcwd(), "input_hsto")):
        subprocess.call(["rm", "-r", os.path.join(os.getcwd(), "input_hsto")])
    if os.path.exists(os.path.join(os.getcwd(), "input_hsti")):
        subprocess.call(["rm", "-r", os.path.join(os.getcwd(), "input_hsti")])

def clean_outputs():
    for f in glob.glob(os.path.join(os.getcwd(), "hpcg*.txt")):
        os.remove(f)
    for f in glob.glob(os.path.join(os.getcwd(), "HPCG*.txt")):
        os.remove(f)
    if os.path.isfile(os.path.join(os.getcwd(), "out.fluid")):
        subprocess.call(["rm", os.path.join(os.getcwd(), "out.fluid")])

def run_benchmark(processor_type, benchmark_suite, core_number, function, postfix, is_rerun):
    thread_start_time = datetime.now()
    config_file = os.path.join("config_files", processor_type, benchmark_suite, core_number, function+".cfg")
    stdout_file = os.path.join(output_dir, processor_type.replace("/", "_")+"_"+core_number+"_"+benchmark_suite+"_"+function+postfix+".txt")
    return_code = 1
    with open(stdout_file, "w") as output_file:
        return_code = subprocess.call(["build/opt/zsim", config_file], stdout=output_file, stderr=output_file)
    thread_end_time = datetime.now()
    with open(summary_file, "a") as status_summary:
        csv_writer = csv.writer(status_summary, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
        if return_code == 0:
            csv_writer.writerow([processor_type.replace("/", "_"), core_number, benchmark_suite, function, "Success", thread_start_time.strftime("%Y-%m-%d_%H-%M-%S"), thread_end_time.strftime("%Y-%m-%d_%H-%M-%S"), str(thread_end_time - thread_start_time)])
        else:
            csv_writer.writerow([processor_type.replace("/", "_"), core_number, benchmark_suite, function, "Failed", thread_start_time.strftime("%Y-%m-%d_%H-%M-%S"), thread_end_time.strftime("%Y-%m-%d_%H-%M-%S"), str(thread_end_time - thread_start_time)])
            if not is_rerun:
                failed_benchmarks.append([processor_type, benchmark_suite, core_number, function])

# Following are all the benchmark workloads that is currently compiling and available
# benchmark_suites_and_benchmarks_functions = {"chai" : ["BS_BEZIER_KERNEL", "HSTI_HSTI", "HSTO_HSTO", "OOPPAD_OOPPAD"],
#     "darknet" : ["resnet152_gemm_nn", "yolo_gemm_nn"],
#     "hashjoin" : ["NPO_probehashtable", "PRH_histogramjoin"],
#     "hpcg" : ["HPCG_ComputePrologation", "HPCG_ComputeRestriction", "HPCG_ComputeSPMV", "HPCG_ComputeSYMGS"],
#     "ligra" : ["BC_edgeMapSparseUSAUserAdded", "BFSCC_edgeMapSparseUSAUserAdded", "BFS_edgeMapSparseUSAUserAdded", "Components_edgeMapSparseUSAUserAdded", "PageRank_edgeMapDenseUSA", "Radii_edgeMapSparseUSA", "Triangle_edgeMapDenseRmat"],
#     "parsec" : ["Fluidaminate_ProcessCollision2MT"],
#     "phoenix" : ["Linearregression_main", "Stringmatch_main"],
#     "polybench" : ["linear-algebra_3mm", "linear-algebra_doitgen", "linear-algebra_gemm", "linear-algebra_gramschmidt", "linear-algebra_gemver", "linear-algebra_symm", "stencil_convolution-2d", "stencil_fdtd-apml"], 
#     "rodinia" : ["BFS_BFS", "NW_UserAdded"],
#     "splash-2" : ["FFT_Reverse", "FFT_Transpose", "Oceanncp_jacobcalc", "Oceanncp_laplaccalc", "Oceancp_slave2", "Radix_slave_sort"],
#     "stream" : ["Add_Add", "Copy_Copy", "Scale_Scale", "Triad_Triad"]}
# Following are all the benchmarks that currently runs
# benchmark_suites_and_benchmarks_functions = {"chai" : ["BS_BEZIER_KERNEL", "HSTO_HSTO", "OOPPAD_OOPPAD"],
#     "darknet" : ["yolo_gemm_nn"],
#     "hashjoin" : ["NPO_probehashtable", "PRH_histogramjoin"],
#     "hpcg" : ["HPCG_ComputePrologation", "HPCG_ComputeRestriction", "HPCG_ComputeSPMV", "HPCG_ComputeSYMGS"],
#     "ligra" : ["BC_edgeMapSparseUSAUserAdded", "BFSCC_edgeMapSparseUSAUserAdded", "BFS_edgeMapSparseUSAUserAdded", "PageRank_edgeMapDenseUSA",  "Triangle_edgeMapDenseRmat"],
#     "phoenix" : ["Linearregression_main", "Stringmatch_main"],
#     "polybench" : ["linear-algebra_3mm", "linear-algebra_doitgen", "linear-algebra_gemm", "linear-algebra_gramschmidt", "linear-algebra_gemver", "linear-algebra_symm", "stencil_convolution-2d", "stencil_fdtd-apml"], 
#     "rodinia" : ["BFS_BFS", "NW_UserAdded"],
#     "splash-2" : ["FFT_Reverse", "FFT_Transpose", "Oceanncp_jacobcalc", "Oceanncp_laplaccalc", "Oceancp_slave2", "Radix_slave_sort"],
#     "stream" : ["Add_Add", "Copy_Copy", "Scale_Scale", "Triad_Triad"]}
# The following benchmarks requires to be run serialized (or run multiple times?)
# serialized_benchmark_suites_and_benchmarks_functions = {
#     "hpcg" : ["HPCG_ComputeSYMGS"],
#     "hashjoin" : ["PRH_histogramjoin"], 
#     "splash-2":["FFT_Reverse","FFT_Transpose"],
#     "ligra" : ["BFSCC_edgeMapSparseUSAUserAdded", "BFS_edgeMapSparseUSAUserAdded"],}
# The following benchmarks are the set complement of the above benchmark
# benchmark_suites_and_benchmarks_functions = {"chai" : ["BS_BEZIER_KERNEL", "HSTO_HSTO", "OOPPAD_OOPPAD"],
#     "darknet" : ["yolo_gemm_nn"],
#     "hashjoin" : ["NPO_probehashtable"],
#     "hpcg" : ["HPCG_ComputePrologation", "HPCG_ComputeRestriction", "HPCG_ComputeSPMV"],
#     "ligra" : ["BC_edgeMapSparseUSAUserAdded", "PageRank_edgeMapDenseUSA",  "Triangle_edgeMapDenseRmat"],
#     "phoenix" : ["Linearregression_main", "Stringmatch_main"],
#     "polybench" : ["linear-algebra_3mm", "linear-algebra_doitgen", "linear-algebra_gemm", "linear-algebra_gramschmidt", "linear-algebra_gemver", "linear-algebra_symm", "stencil_convolution-2d", "stencil_fdtd-apml"], 
#     "rodinia" : ["BFS_BFS", "NW_UserAdded"],
#     "splash-2" : ["FFT_Reverse", "FFT_Transpose", "Oceanncp_jacobcalc", "Oceanncp_laplaccalc", "Oceancp_slave2", "Radix_slave_sort"],
#     "stream" : ["Add_Add", "Copy_Copy", "Scale_Scale", "Triad_Triad"]}
# The following are benchmarks that impacted by our model
# benchmark_suites_and_benchmarks_functions = {"chai" : ["BS_BEZIER_KERNEL", "HSTO_HSTO", "OOPPAD_OOPPAD"],
#     "darknet" : ["yolo_gemm_nn"],
#     "hpcg" : ["HPCG_ComputePrologation", "HPCG_ComputeRestriction", "HPCG_ComputeSPMV", "HPCG_ComputeSYMGS"],
#     "ligra" : ["BC_edgeMapSparseUSAUserAdded", "BFS_edgeMapSparseUSAUserAdded"],
#     "phoenix" : ["Linearregression_main"],
#     "polybench" : ["linear-algebra_3mm", "linear-algebra_doitgen", "linear-algebra_gemm", "linear-algebra_gramschmidt", "linear-algebra_gemver", "linear-algebra_symm", "stencil_fdtd-apml"], 
#     "rodinia" : ["BFS_BFS", "NW_UserAdded"],
#     "splash-2" : ["FFT_Reverse", "FFT_Transpose", "Oceanncp_laplaccalc", "Radix_slave_sort"], 
#     "stream" : ["Triad_Triad"]}
# Following are reserved for test runs of selected benchmarks
benchmark_suites_and_benchmarks_functions = {
    "hpcg" : ["HPCG_ComputePrologation", "HPCG_ComputeRestriction", "HPCG_ComputeSPMV", "HPCG_ComputeSYMGS"],
    "phoenix" : ["Linearregression_main"],
    "splash-2" : ["FFT_Reverse", "FFT_Transpose", "Oceanncp_laplaccalc", "Radix_slave_sort"],
}

clean_darknet_chai_inputs()

# Darknet requires some input to be placed in the cwd so we copy it over
if "darknet" in benchmark_suites_and_benchmarks_functions:
    print "Copying the required input files for darknet..."
    data_dir = os.path.abspath(os.path.join(os.getcwd(), "../workloads/Darknet/data/"))
    cfg_dir = os.path.abspath(os.path.join(os.getcwd(), "../workloads/Darknet/cfg/"))
    subprocess.call(["cp", "-r", data_dir, os.path.join(os.getcwd(), "data")])
    subprocess.call(["cp", "-r", cfg_dir, os.path.join(os.getcwd(), "cfg")])

# Chai's HSTO and BS also requires input to be in the cwd
if "chai" in benchmark_suites_and_benchmarks_functions:
    chai_benchmark_functions = benchmark_suites_and_benchmarks_functions["chai"]
    if "HSTO_HSTO" in chai_benchmark_functions:
        print "Copying the required input files for chai HSTO_HSTO..."
        input_dir = os.path.abspath(os.path.join(os.getcwd(), "../workloads/chai-cpu/HSTO/input/"))
        subprocess.call(["cp", "-r", input_dir, os.path.join(os.getcwd(), "input_hsto")])
    if "HSTI_HSTI" in chai_benchmark_functions:
        print "Copying the required input files for chai HSTI_HSTI..."
        input_dir = os.path.abspath(os.path.join(os.getcwd(), "../workloads/chai-cpu/HSTI/input_hsti/"))
        subprocess.call(["cp", "-r", input_dir, os.path.join(os.getcwd(), "input_hsti")])
    if "BS_BEZIER_KERNEL" in chai_benchmark_functions:
        print "Copying the required input files for chai BS_BEZIER_KERNEL..."
        input_dir = os.path.abspath(os.path.join(os.getcwd(), "../workloads/chai-cpu/BS/input/"))
        subprocess.call(["cp", "-r", input_dir, os.path.join(os.getcwd(), "input")])


processor_types = ["pim_ooo_netoh"] # Include one for baseline
# processor_types = []
core_numbers = get_core_numbers()
# core_numbers = [256]
processor_type_prefix = "pim_prefetch_netoh_"
prefetcher_types = ["allocate"]
for prefetcher_type in prefetcher_types:
    processor_types.append(processor_type_prefix+prefetcher_type+"/adaptive_"+debug_tag)
    for hops_threshold in hops_thresholds:
        for count_threshold in count_thresholds:
            processor_types.append(processor_type_prefix+prefetcher_type+"/"+str(hops_threshold)+"h"+str(count_threshold)+"c_"+debug_tag)

for current_iteration in range(iterations):
    total_experiment_count = 0
    for suite in benchmark_suites_and_benchmarks_functions.keys():
        total_experiment_count += len(benchmark_suites_and_benchmarks_functions[suite])
    total_experiment_count *= len(processor_types)
    total_experiment_count *= len(core_numbers)
    print "Starting experiments of iteration "+str(current_iteration+1)+". There are " + str(total_experiment_count) + " experiments to be scheduled, for cores: " + str(core_numbers) + " and processor types: " + str(processor_types)
    scheduled_experiments = 0
    with open(summary_file, "a") as status_summary:
        csv_writer = csv.writer(status_summary, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
        csv_writer.writerow(["Iteration "+str(current_iteration+1)])

    for suite in benchmark_suites_and_benchmarks_functions.keys():
        for benchmark_function in benchmark_suites_and_benchmarks_functions[suite]:
            for processor_type in processor_types:
                for core_number in core_numbers:
                    scheduled_experiments += 1
                    print "Starting experment of " + suite + " " + benchmark_function + " with processor " + processor_type.replace("/", "_") + " and " + str(core_number) + " core(s) (" + str(scheduled_experiments) + "/" + str(total_experiment_count) + ") iteration ("+str(current_iteration+1)+"/"+str(iterations)+")"
                    current_thread = threading.Thread(target = run_benchmark, args = (processor_type, suite, str(core_number), benchmark_function, "", False))
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
        with open(summary_file, "a") as status_summary:
            csv_writer = csv.writer(status_summary, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
            csv_writer.writerow(["Below are rerun of failed experiments"])
        total_experiment_count = len(failed_benchmarks)
        scheduled_experiments = 0
        for benchmark in failed_benchmarks:
            processor_type = benchmark[0]
            suite = benchmark[1]
            core_number = benchmark[2]
            benchmark_function = benchmark[3]
            # processor_type = processor_type.replace("debugoff", "debugon")
            scheduled_experiments += 1
            print "Starting experment of " + suite + " " + benchmark_function + " with processor " + processor_type.replace("/", "_") + " and " + str(core_number) + " core(s) (" + str(scheduled_experiments) + "/" + str(total_experiment_count) + ")"
            current_thread = threading.Thread(target = run_benchmark, args = (processor_type, suite, str(core_number), benchmark_function, "_rerun", True))
            threads.append(current_thread)
            current_thread.start()
            if threading.active_count() >= maximum_thread + 1 or psutil.virtual_memory()[3]/1000000000 >= max_memory_gb:
                print "Reaching maximum allowed concurrent thread number of " + str(maximum_thread) + " threads or maximum memory allocated. Waiting for threads to finish..."
                while threading.active_count() >= maximum_thread + 1:
                    time.sleep(1)
        print "The main thread has started all threads. Now waiting for threads to finish..."
        for thread in threads:
            thread.join()
        
    for suite in benchmark_suites_and_benchmarks_functions.keys():
        for benchmark_function in benchmark_suites_and_benchmarks_functions[suite]:
            for processor_type in processor_types:
                for core_number in core_numbers:
                    zsim_out_path = os.path.join(stats_folders, processor_type, str(core_number), suite+"_"+benchmark_function+".zsim.out")
                    new_zsim_out_path = zsim_out_path+"."+str(current_iteration)
                    subscription_stats_path = os.path.join(stats_folders, processor_type, str(core_number), suite+"_"+benchmark_function+".ramulator.subscription_stats")
                    new_subscription_stats_path = subscription_stats_path+"."+str(current_iteration)
                    address_dist_path = os.path.join(stats_folders, processor_type, str(core_number), suite+"_"+benchmark_function+".ramulator.address_distribution")
                    new_address_dist_path = address_dist_path+"."+str(current_iteration)
                    os.rename(zsim_out_path, new_zsim_out_path)
                    os.rename(subscription_stats_path, new_subscription_stats_path)
                    os.rename(address_dist_path, new_address_dist_path)

end_time = datetime.now()
print "We are finishing at "+end_time.strftime("%Y-%m-%d %H:%M:%S")
print "It took "+str(end_time - start_time)
clean_darknet_chai_inputs()
clean_outputs()