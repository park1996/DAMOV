# -*- coding: utf-8 -*-
import os
import csv
from datetime import datetime
from batch_prefetcher_generator import get_hops_thresholds, get_count_thresholds

hops_thresholds = get_hops_thresholds()
count_thresholds = get_count_thresholds()
hops_thresholds_str = [""]
count_thresholds_str = [""]
debug_tag = "debugoff"
for hops_threshold in hops_thresholds:
    hops_thresholds_str.append(str(hops_threshold)+" Hops")
for count_threshold in count_thresholds:
    count_thresholds_str.append(str(count_threshold)+" Counts")

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
    "hpcg" : ["HPCG_ComputePrologation", "HPCG_ComputeRestriction", "HPCG_ComputeSPMV", "HPCG_ComputeSYMGS"],
    "ligra" : ["PageRank_edgeMapDenseUSA", "Radii_edgeMapSparseUSA", "Triangle_edgeMapDenseRmat"],
    "parsec" : ["Fluidaminate_ProcessCollision2MT"],
    "phoenix" : ["Linearregression_main", "Stringmatch_main"],
    "polybench" : ["linear-algebra_3mm", "linear-algebra_doitgen", "linear-algebra_gemm", "linear-algebra_gramschmidt", "linear-algebra_gemver", "linear-algebra_symm", "stencil_convolution-2d", "stencil_fdtd-apml"], 
    "rodinia" : ["BFS_BFS"],
    "splash-2" : ["FFT_Reverse", "FFT_Transpose", "Oceanncp_jacobcalc", "Oceanncp_laplaccalc", "Oceancp_slave2", "Radix_slave_sort"],
    "stream" : ["Add_Add", "Copy_Copy", "Scale_Scale", "Triad_Triad"]}

processor_type_prefix = "pim_prefetch_netoh_"
baseline_processor_type = "pim_ooo_netoh"
prefetcher_types = ["allocate"]
core_number = "32"

output_filename = "stats_normalized_cycle_pim_pf_with_diff_thresholds_"+datetime.now().strftime("%Y-%m-%d_%H-%M-%S")+".csv"
output_filename = os.path.join(os.getcwd(), output_filename)
stats_folders = os.path.join(os.getcwd(), "zsim_stats")

with open(output_filename, mode='w') as csv_file:
    csv_writer = csv.writer(csv_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
    for suite in benchmark_suites_and_benchmarks_functions.keys():
        for benchmark_function in benchmark_suites_and_benchmarks_functions[suite]:
            full_benchmark_name = suite+"_"+benchmark_function
            baseline_cycle = 0
            baseline_stat_file_location = os.path.join(stats_folders, baseline_processor_type, core_number, full_benchmark_name+".zsim.out")
            if os.path.isfile(baseline_stat_file_location):
                baseline_cycle = extract_cycle(baseline_stat_file_location)
            if baseline_cycle == 0:
                continue
            for prefetcher_type in prefetcher_types:
                csv_writer.writerow([full_benchmark_name+" with "+prefetcher_type+" prefetcher"])
                csv_writer.writerow(count_thresholds_str)
                for hops_threshold in hops_thresholds:
                    current_line = [str(hops_threshold)+" Hops"]
                    for count_threshold in count_thresholds:
                        processor_type = processor_type_prefix+prefetcher_type
                        stat_file_location = os.path.join(stats_folders, processor_type, str(hops_threshold)+"h"+str(count_threshold)+"c_"+debug_tag, core_number, full_benchmark_name+".zsim.out")
                        if not os.path.isfile(stat_file_location):
                            current_line.append("N/A")
                            continue
                        cycle_val = extract_cycle(stat_file_location)
                        current_line.append("N/A" if cycle_val == 0 else str(float(baseline_cycle)/float(cycle_val)))
                    csv_writer.writerow(current_line)
                csv_writer.writerow('')


