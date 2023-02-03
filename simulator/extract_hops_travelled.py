import os
import csv

benchmark_suites_and_benchmarks_functions = {"chai" : ["OOPPAD_OOPPAD"],
    "hashjoin" : ["NPO_probehashtable", "PRH_histogramjoin"],
    "ligra" : ["PageRank_edgeMapDenseUSA"],
    "phoenix" : ["Linearregression_main", "Stringmatch_main"],
    "polybench" : ["linear-algebra_3mm", "linear-algebra_doitgen", "linear-algebra_gemm", "linear-algebra_gramschmidt", "linear-algebra_gemver", "linear-algebra_symm", "stencil_convolution-2d", "stencil_fdtd-apml"],
    "rodinia" : ["BFS_BFS"],
    "stream" : ["Add_Add", "Copy_Copy", "Scale_Scale"]}

csv_header = ["Hops Travelled", "Requests"]
maximum_hop = 10
processor_types = ["pim_ooo_memtrace"]
core_numbers = ["32"]
output_filename = "stats_hops.csv"
output_filename = os.path.join(os.getcwd(), output_filename)
stats_folders = os.path.join(os.getcwd(), "zsim_stats")

with open(output_filename, mode='w') as csv_file:
    csv_writer = csv.writer(csv_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
    for suite in benchmark_suites_and_benchmarks_functions.keys():
        for benchmark_function in benchmark_suites_and_benchmarks_functions[suite]:
            for core_number in core_numbers:
                for processor_type in processor_types:
                    hop_requests = {}
                    for hop_number in range(0, maximum_hop+1):
                        hop_requests[hop_number] = 0
                    full_benchmark_name = suite+"_"+benchmark_function
                    csv_writer.writerow([full_benchmark_name+"_"+processor_type+"_"+str(core_number)+"_cores"])
                    csv_writer.writerow(csv_header)
                    stat_file_location = os.path.join(stats_folders, processor_type, core_number, full_benchmark_name+".ramulator.address_distribution")
                    if not os.path.isfile(stat_file_location):
                        csv_writer.writerow(["Cannot find file "+stat_file_location])
                        continue
                    with open(stat_file_location, mode='r') as stat:
                        next(stat) # Skip the first line as it's the header
                        for line in stat:
                            stat_arr = line.split(" ")
                            if len(stat_arr) < 3:
                                continue
                            core_id = int(stat_arr[0])
                            vault_id = int(stat_arr[1])
                            request_number = int(stat_arr[2])
                            src_x = core_id / 6
                            src_y = core_id % 6
                            dst_x = vault_id / 6
                            dst_y = vault_id % 6
                            hops_travelled = abs(dst_x - src_x) + abs(dst_y - src_y)
                            hop_requests[hops_travelled] += request_number
                    for hop_number in range(0, maximum_hop+1):
                        csv_writer.writerow([hop_number, hop_requests[hop_number]])
                    csv_writer.writerow('')

