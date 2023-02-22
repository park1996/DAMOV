import os
import csv
import sys
from datetime import datetime

benchmark_suites_and_benchmarks_functions = {"chai" : ["BS_BEZIER_KERNEL", "HSTO_HSTO", "OOPPAD_OOPPAD"],
    "darknet" : ["resnet152_gemm_nn", "yolo_gemm_nn"],
    "hashjoin" : ["NPO_probehashtable", "PRH_histogramjoin"],
    "ligra" : ["PageRank_edgeMapDenseUSA", "Radii_edgeMapSparseUSA", "Triangle_edgeMapDenseRmat"],
    "phoenix" : ["Linearregression_main", "Stringmatch_main"],
    "polybench" : ["linear-algebra_3mm", "linear-algebra_doitgen", "linear-algebra_gemm", "linear-algebra_gramschmidt", "linear-algebra_gemver", "linear-algebra_symm", "stencil_convolution-2d", "stencil_fdtd-apml"], 
    "rodinia" : ["BFS_BFS"], "stream" : ["Add_Add", "Copy_Copy", "Scale_Scale", "Triad_Triad"]}

processor_types = ["pim_ooo_netoh", "pim_ooo_netoh_withswapsubpf", "pim_ooo"]
core_number = "32"
csv_header = ["Benchmark", "pim_ooo_netoh", "pim_ooo_netoh_withswapsubpf", "pim_ooo", "Improvement"]

output_filename = "stats_cycle_pim_netoh_and_pf_"+datetime.now().strftime("%Y-%d-%m_%H-%M-%S")+".csv"
output_filename = os.path.join(os.getcwd(), output_filename)
stats_folders = os.path.join(os.getcwd(), "zsim_stats")


with open(output_filename, mode='w') as csv_file:
    csv_writer = csv.writer(csv_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
    csv_writer.writerow(["The following results are generated based on " + core_number + " core(s) configuration"])
    csv_writer.writerow(csv_header)
    for suite in benchmark_suites_and_benchmarks_functions.keys():
        for benchmark_function in benchmark_suites_and_benchmarks_functions[suite]:
            full_benchmark_name = suite+"_"+benchmark_function
            current_line = [full_benchmark_name]
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
            if current_line[1] != "N/A" and current_line[2] != "N/A":
                current_line.append(str(float(current_line[1])/float(current_line[2])-1))
            else:
                current_line.append("N/A")
            csv_writer.writerow(current_line)
    csv_writer.writerow(["pim_ooo_netoh refers to the model with network connection overhead and no prefetch"])
    csv_writer.writerow(["pim_ooo_netoh_withswapsubpf refers to the model with a prefetcher that swaps the local vault page with remote vault's page at the same location"])
    csv_writer.writerow(["pim_ooo refers to the ideal model of no network connection overhead. Every memory access completes instantly"])
    csv_writer.writerow(["The numbers are the number of cycles taken for each benchmark to run (the lower the better)"])
    csv_writer.writerow(["The improvement refers to the cycle taken by pim_ooo_netoh divided by pim_ooo_netoh_withswapsubpf and minus one (the higher the better and negative value means worse performance with prefetcher)"])


