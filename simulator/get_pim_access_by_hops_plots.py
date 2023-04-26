import os
import csv
from datetime import datetime
import matplotlib
# Use non X-Window engine as we are executing it in terminal
matplotlib.use('Agg')
from matplotlib import pyplot as plt
from batch_prefetcher_generator import get_hops_thresholds, get_count_thresholds, get_debug_flags, get_prefetcher_types

def mkdir_p(directory):
    try:
        os.makedirs(directory)
    except OSError as exc:  # Python >2.5
        if exc.errno == errno.EEXIST and os.path.isdir(directory):
            pass
        else:
            raise

def get_access_counts(csv_file_location):
    access_count = []
    with open(csv_file_location) as csv_file:
        csv_reader = csv.DictReader(csv_file)
        for row in csv_reader:
            access_count.append(int(row["Read"])+int(row["Write"])+int(row["Other"]))
    return access_count

def get_read_access_counts(csv_file_location):
    access_count = []
    with open(csv_file_location) as csv_file:
        csv_reader = csv.DictReader(csv_file)
        for row in csv_reader:
            access_count.append(int(row["Read"]))
    return access_count

def get_write_access_counts(csv_file_location):
    access_count = []
    with open(csv_file_location) as csv_file:
        csv_reader = csv.DictReader(csv_file)
        for row in csv_reader:
            access_count.append(int(row["Write"]))
    return access_count

def get_other_access_counts(csv_file_location):
    access_count = []
    with open(csv_file_location) as csv_file:
        csv_reader = csv.DictReader(csv_file)
        for row in csv_reader:
            access_count.append(int(row["Other"]))
    return access_count

def get_network_cycles(csv_file_location):
    network_cycles = []
    with open(csv_file_location) as csv_file:
        csv_reader = csv.DictReader(csv_file)
        for row in csv_reader:
            network_cycles.append(int(row["# Requests"]))
    return network_cycles

hops_thresholds = get_hops_thresholds()
count_thresholds = get_count_thresholds()
prefetcher_types = ["allocate"]
debug_tag = "debugoff"

processor_type_prefix = "pim_prefetch_netoh_"
baseline_processor_type = "pim_ooo_netoh"
core_number = "32"
stats_folders = os.path.join(os.getcwd(), "zsim_stats")
output_dir_name = "stats_access_by_hops_"+datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
output_dir = os.path.join(os.getcwd(), output_dir_name)
mkdir_p(output_dir)
total_plot_output_dir = os.path.join(output_dir, "total_access")
read_plot_output_dir = os.path.join(output_dir, "read_access")
write_plot_output_dir = os.path.join(output_dir, "write_access")
other_plot_output_dir = os.path.join(output_dir, "other_access")
network_cycle_csv_output_dir = os.path.join(output_dir, "network_cycle")
mkdir_p(total_plot_output_dir)
mkdir_p(read_plot_output_dir)
mkdir_p(write_plot_output_dir)
mkdir_p(network_cycle_csv_output_dir)
maximum_plotted_hop = 11
x_axis = []
for i in range(0, maximum_plotted_hop+1):
    x_axis.append(str(i))
maximum_network_cycles = 84
csv_header = [""]
for i in range(0, maximum_network_cycles+1):
    csv_header.append(str(i))

benchmark_suites_and_benchmarks_functions = {"chai" : ["BS_BEZIER_KERNEL", "HSTO_HSTO", "OOPPAD_OOPPAD"],
    "darknet" : ["yolo_gemm_nn"],
    "hashjoin" : ["NPO_probehashtable", "PRH_histogramjoin"],
    "hpcg" : ["HPCG_ComputePrologation", "HPCG_ComputeRestriction", "HPCG_ComputeSPMV", "HPCG_ComputeSYMGS"],
    "ligra" : ["BC_edgeMapSparseUSAUserAdded", "BFSCC_edgeMapSparseUSAUserAdded", "BFS_edgeMapSparseUSAUserAdded", "PageRank_edgeMapDenseUSA",  "Triangle_edgeMapDenseRmat"],
    "phoenix" : ["Linearregression_main", "Stringmatch_main"],
    "polybench" : ["linear-algebra_3mm", "linear-algebra_doitgen", "linear-algebra_gemm", "linear-algebra_gramschmidt", "linear-algebra_gemver", "linear-algebra_symm", "stencil_convolution-2d", "stencil_fdtd-apml"], 
    "rodinia" : ["BFS_BFS", "NW_UserAdded"],
    "splash-2" : ["FFT_Reverse", "FFT_Transpose", "Oceanncp_jacobcalc", "Oceanncp_laplaccalc", "Oceancp_slave2", "Radix_slave_sort"],
    "stream" : ["Add_Add", "Copy_Copy", "Scale_Scale", "Triad_Triad"]}

plt.figure(figsize=(10.5,6))

# Start plotting all benchmark's total requests
def plot_benchmark_requests(title_type_str, output_dir, retrival_function):
    for suite in benchmark_suites_and_benchmarks_functions.keys():
        for benchmark_function in benchmark_suites_and_benchmarks_functions[suite]:
            csv_file_dir = os.path.join(output_dir, suite+"_"+benchmark_function+".csv")
            with open(csv_file_dir, "w") as csv_file:
                csv_writer = csv.writer(csv_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
                csv_writer.writerow(["Hops"]+x_axis)
                access_counts = {}
                baseline_hops_distribution_csv_file_path = os.path.join(stats_folders, baseline_processor_type, core_number, suite+"_"+benchmark_function+".hops_distribution.csv")
                if os.path.isfile(baseline_hops_distribution_csv_file_path):
                    access_counts["baseline"] = retrival_function(baseline_hops_distribution_csv_file_path)
                else:
                    access_counts["baseline"] = [0]*(maximum_plotted_hop+1)
                plt.plot(x_axis, access_counts["baseline"], label = "baseline")
                csv_writer.writerow(["baseline"]+access_counts["baseline"])
                for prefetcher_type in prefetcher_types:
                    threshold_label = "adaptive"
                    hops_distribution_csv_file_path = os.path.join(stats_folders, processor_type_prefix+prefetcher_type, threshold_label+"_"+debug_tag, core_number, suite+"_"+benchmark_function+".hops_distribution.csv")
                    if os.path.isfile(hops_distribution_csv_file_path):
                        access_counts[threshold_label] = retrival_function(hops_distribution_csv_file_path)
                    else:
                        access_counts[threshold_label] = [0]*(maximum_plotted_hop+1)
                    plt.plot(x_axis, access_counts[threshold_label], label = threshold_label)
                    csv_writer.writerow([threshold_label]+access_counts[threshold_label])
                    for hops_threshold in hops_thresholds:
                        for count_threshold in count_thresholds:
                            threshold_label = str(hops_threshold)+"h"+str(count_threshold)+"c"
                            hops_distribution_csv_file_path = os.path.join(stats_folders, processor_type_prefix+prefetcher_type, threshold_label+"_"+debug_tag, core_number, suite+"_"+benchmark_function+".hops_distribution.csv")
                            if os.path.isfile(hops_distribution_csv_file_path):
                                access_counts[threshold_label] = retrival_function(hops_distribution_csv_file_path)
                            else:
                                access_counts[threshold_label] = [0]*(maximum_plotted_hop+1)
                            plt.plot(x_axis, access_counts[threshold_label], label = threshold_label)
                            csv_writer.writerow([threshold_label]+access_counts[threshold_label])
                output_image_dir = os.path.join(output_dir, suite+"_"+benchmark_function+".png")
                plt.legend()
                plt.xlabel("# of Hops")
                plt.ylabel("# of Requests")
                plt.title("Number of"+title_type_str+" Requests by hops travelled for "+ suite + " " + benchmark_function)
                plt.savefig(output_image_dir)
                plt.clf()

plot_benchmark_requests("", total_plot_output_dir, get_access_counts)
plot_benchmark_requests(" Read", read_plot_output_dir, get_read_access_counts)
plot_benchmark_requests(" Write", write_plot_output_dir, get_write_access_counts)
plot_benchmark_requests(" Other", other_plot_output_dir, get_other_access_counts)

for suite in benchmark_suites_and_benchmarks_functions.keys():
    for benchmark_function in benchmark_suites_and_benchmarks_functions[suite]:
        output_csv_dir = os.path.join(network_cycle_csv_output_dir, suite+"_"+benchmark_function+".csv")
        with open(output_csv_dir, "w") as csv_file:
            csv_writer = csv.writer(csv_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
            csv_writer.writerow(csv_header)
            baseline_row = ["baseline"]
            baseline_network_cycle_file_path = os.path.join(stats_folders, baseline_processor_type, core_number, suite+"_"+benchmark_function+".network_cycle.csv")
            if os.path.isfile(baseline_network_cycle_file_path):
                baseline_row += get_network_cycles(baseline_network_cycle_file_path)
            csv_writer.writerow(baseline_row)
            for prefetcher_type in prefetcher_types:
                threshold_label = "adaptive"
                hops_distribution_csv_file_path = os.path.join(stats_folders, processor_type_prefix+prefetcher_type, threshold_label+"_"+debug_tag, core_number, suite+"_"+benchmark_function+".hops_distribution.csv")
                current_row = [threshold_label]
                network_cycle_file_path = os.path.join(stats_folders, processor_type_prefix+prefetcher_type, threshold_label+"_"+debug_tag, core_number, suite+"_"+benchmark_function+".network_cycle.csv")
                if os.path.isfile(network_cycle_file_path):
                    current_row += get_network_cycles(network_cycle_file_path)
                csv_writer.writerow(current_row)
                for hops_threshold in hops_thresholds:
                    for count_threshold in count_thresholds:
                        threshold_label = str(hops_threshold)+"h"+str(count_threshold)+"c"
                        current_row = [threshold_label]
                        network_cycle_file_path = os.path.join(stats_folders, processor_type_prefix+prefetcher_type, threshold_label+"_"+debug_tag, core_number, suite+"_"+benchmark_function+".network_cycle.csv")
                        if os.path.isfile(network_cycle_file_path):
                            current_row += get_network_cycles(network_cycle_file_path)
                        csv_writer.writerow(current_row)