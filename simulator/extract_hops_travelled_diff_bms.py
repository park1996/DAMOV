import os
import csv

benchmark_suites_and_benchmarks_functions = {"chai" : ["OOPPAD_OOPPAD"],
    "hashjoin" : ["NPO_probehashtable", "PRH_histogramjoin"],
    "ligra" : ["PageRank_edgeMapDenseUSA"],
    "phoenix" : ["Linearregression_main", "Stringmatch_main"],
    "polybench" : ["linear-algebra_3mm", "linear-algebra_doitgen", "linear-algebra_gemm", "linear-algebra_gramschmidt", "linear-algebra_gemver", "linear-algebra_symm", "stencil_convolution-2d", "stencil_fdtd-apml"],
    "rodinia" : ["BFS_BFS"],
    "stream" : ["Add_Add", "Copy_Copy", "Scale_Scale"]}

csv_header = ["Hops Travelled"]
maximum_hop = 10
processor_types = ["pim_ooo_memtrace"]
core_numbers = ["32"]
output_filename = "stats_hops_aggregated_benchmark.csv"
output_filename = os.path.join(os.getcwd(), output_filename)
stats_folders = os.path.join(os.getcwd(), "zsim_stats")
hop_requests_matrix = {}


with open(output_filename, mode='w') as csv_file:
    csv_writer = csv.writer(csv_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
    for processor_type in processor_types:
        for core_number in core_numbers:
            csv_writer.writerow([processor_type+" processor with "+core_number+" core(s)"])
            for suite in benchmark_suites_and_benchmarks_functions.keys():
                for benchmark_function in benchmark_suites_and_benchmarks_functions[suite]:
                    hop_requests = {}
                    for hop_number in range(0, maximum_hop+1):
                        hop_requests[hop_number] = 0
                    full_benchmark_name = suite+"_"+benchmark_function
                    stat_file_location = os.path.join(stats_folders, processor_type, core_number, full_benchmark_name+".ramulator.address_distribution")
                    if not os.path.isfile(stat_file_location):
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
                    hop_requests_matrix[full_benchmark_name] = hop_requests
            for suite in benchmark_suites_and_benchmarks_functions.keys():
                for benchmark_function in benchmark_suites_and_benchmarks_functions[suite]:
                    full_benchmark_name = suite+"_"+benchmark_function
                    csv_header.append(full_benchmark_name)
            csv_writer.writerow(csv_header)
            for hop_number in range(0, maximum_hop+1):
                current_line = [hop_number]
                for suite in benchmark_suites_and_benchmarks_functions.keys():
                    for benchmark_function in benchmark_suites_and_benchmarks_functions[suite]:
                        full_benchmark_name = suite+"_"+benchmark_function
                        current_line.append(hop_requests_matrix[full_benchmark_name][hop_number])
                csv_writer.writerow(current_line)

