# -*- coding: utf-8 -*-
import os
import csv
import sys
from datetime import datetime

hops_thresholds = []
count_thresholds = []
hops_thresholds_str = ["CountThreshold(Column) | Hops Threshold(Row)"]
count_thresholds_str = ["HopsThreshold(Column) | Count Threshold(Row)"]
hops_threshold_start = 1
hops_threshold_stepping = 1
hops_threshold_maximum = 10
count_threshold_start = 1
count_threshold_stepping = 4
count_threshold_maximum = 65535

def next_hops(current_hops):
    return current_hops + hops_threshold_stepping

def next_count(current_count):
    return (current_count+1)*count_threshold_stepping-1

hops = hops_threshold_start
while hops <= hops_threshold_maximum:
    hops_thresholds.append(hops)
    hops_thresholds_str.append(str(hops))
    hops = next_hops(hops)

count = count_threshold_start
while count <= count_threshold_maximum:
    count_thresholds.append(count)
    count_thresholds_str.append(str(count))
    count = next_count(count)

def extract_cycle(stat_file_location):
    cycle_val = 0
    with open(stat_file_location, mode='r') as stat:
        for line in stat:
            if(line.find("Simulated unhalted cycles")!= -1):
                current_cycle = int(line.split()[1])
                if(current_cycle > cycle_val):
                    cycle_val = current_cycle
    return cycle_val

benchmark_suites_and_benchmarks_functions = {"chai" : ["BS_BEZIER_KERNEL", "HSTO_HSTO", "OOPPAD_OOPPAD"],
    "darknet" : ["resnet152_gemm_nn", "yolo_gemm_nn"],
    "hashjoin" : ["NPO_probehashtable", "PRH_histogramjoin"],
    "ligra" : ["PageRank_edgeMapDenseUSA", "Radii_edgeMapSparseUSA", "Triangle_edgeMapDenseRmat"],
    "phoenix" : ["Linearregression_main", "Stringmatch_main"],
    "polybench" : ["linear-algebra_3mm", "linear-algebra_doitgen", "linear-algebra_gemm", "linear-algebra_gramschmidt", "linear-algebra_gemver", "linear-algebra_symm", "stencil_convolution-2d", "stencil_fdtd-apml"], 
    "rodinia" : ["BFS_BFS"], "stream" : ["Add_Add", "Copy_Copy", "Scale_Scale", "Triad_Triad"]}

processor_type_prefix = "pim_prefetch_netoh_"
prefetcher_types = ["swap"]
core_number = "32"

output_filename = "stats_cycle_pim_pf_with_diff_thresholds_"+datetime.now().strftime("%Y-%d-%m_%H-%M-%S")+".csv"
output_filename = os.path.join(os.getcwd(), output_filename)
stats_folders = os.path.join(os.getcwd(), "zsim_stats")

with open(output_filename, mode='w') as csv_file:
    csv_writer = csv.writer(csv_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
    for suite in benchmark_suites_and_benchmarks_functions.keys():
        for benchmark_function in benchmark_suites_and_benchmarks_functions[suite]:
            for prefetcher_type in prefetcher_types:
                full_benchmark_name = suite+"_"+benchmark_function
                csv_writer.writerow([full_benchmark_name+" with "+prefetcher_type+" prefetcher"])
                csv_writer.writerow(count_thresholds_str)
                for hops_threshold in hops_thresholds:
                    current_line = [str(hops_threshold)]
                    for count_threshold in count_thresholds:
                        processor_type = processor_type_prefix+prefetcher_type+str(hops_threshold)+"h"+str(count_threshold)+"c"
                        stat_file_location = os.path.join(stats_folders, processor_type, core_number, full_benchmark_name+".zsim.out")
                        if not os.path.isfile(stat_file_location):
                            current_line.append("N/A")
                            continue
                        cycle_val = extract_cycle(stat_file_location)
                        current_line.append("N/A" if cycle_val == 0 else str(cycle_val))
                    csv_writer.writerow(current_line)
                csv_writer.writerow('')


