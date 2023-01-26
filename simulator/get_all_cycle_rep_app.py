import os
import csv
import sys

benchmark_suites_and_benchmarks_functions = {"chai" : ["BS_BEZIER_KERNEL", "HSTO_HSTO", "OOPPAD_OOPPAD"],
    "darknet" : ["resnet152_gemm_nn", "yolo_gemm_nn"],
    "hashjoin" : ["NPO_probehashtable", "PRH_histogramjoin"],
    "ligra" : ["PageRank_edgeMapDenseUSA", "Radii_edgeMapSparseUSA", "Triangle_edgeMapDenseRmat"],
    "phoenix" : ["Linearregression_main", "Stringmatch_main"],
    "polybench" : ["linear-algebra_3mm", "linear-algebra_doitgen", "linear-algebra_gemm", "linear-algebra_gramschmidt", "linear-algebra_gemver", "linear-algebra_symm", "stencil_convolution-2d", "stencil_fdtd-apml"], 
    "rodinia" : ["BFS_BFS"], "stream" : ["Add_Add", "Copy_Copy", "Scale_Scale", "Triad_Triad"]}

processor_types = ["host_ooo/prefetch", "host_ooo/no_prefetch", "pim_ooo", "pim_ooo_netoh"]
core_numbers = ["1", "4", "16", "64", "256"]
csv_header = ["Cores", "host_ooo/prefetch", "host_ooo/no_prefetch", "pim_ooo", "pim_ooo_netoh"]

output_filename = "stats_cycle.csv"
output_filename = os.path.join(os.getcwd(), output_filename)
stats_folders = os.path.join(os.getcwd(), "zsim_stats")

with open(output_filename, mode='w') as csv_file:
    csv_writer = csv.writer(csv_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
    for suite in benchmark_suites_and_benchmarks_functions.keys():
        for benchmark_function in benchmark_suites_and_benchmarks_functions[suite]:
            full_benchmark_name = suite+"_"+benchmark_function
            csv_writer.writerow([full_benchmark_name])
            csv_writer.writerow(csv_header)
            for core_number in core_numbers:
                current_line = [core_number]
                for processor_type in processor_types:
                    stat_file_location = os.path.join(stats_folders, processor_type, core_number, full_benchmark_name+".zsim.out")
                    cycle_val = 0
                    if not os.path.isfile(stat_file_location):
                        current_line.append("N/A")
                        continue
                    with open(stat_file_location, mode='r') as stat:
                        for line in stat:
                            if(line.find("Simulated unhalted cycles")!= -1):
                                current_cycle = int(line.split()[1])
                                if(current_cycle > cycle_val):
                                    cycle_val = current_cycle
                    current_line.append("N/A" if cycle_val == 0 else str(cycle_val))
                csv_writer.writerow(current_line)
            csv_writer.writerow('')


