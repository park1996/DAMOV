import csv
import os

configurations = ["pim_ooo_memtrace"]
core_numbers = [4, 32]
request_types = ["GETS", "GETX", "PUTS", "PUTX"]
total_label = "Total"
address_label = "Address"
type_label = "Type"
cache_types = ["l1d"]

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
# benchmark_suites_and_benchmarks_functions = {"chai" : ["BS_BEZIER_KERNEL", "HSTI_HSTI", "OOPPAD_OOPPAD"],
#     "darknet" : ["yolo_gemm_nn"],
#     "hashjoin" : ["NPO_probehashtable", "PRH_histogramjoin"],
#     "hpcg" : ["HPCG_ComputePrologation", "HPCG_ComputeRestriction", "HPCG_ComputeSPMV", "HPCG_ComputeSYMGS"],
#     "ligra" : ["BC_edgeMapSparseUSAUserAdded", "BFSCC_edgeMapSparseUSAUserAdded", "BFS_edgeMapSparseUSAUserAdded", "PageRank_edgeMapDenseUSA",  "Triangle_edgeMapDenseRmat"],
#     "phoenix" : ["Linearregression_main", "Stringmatch_main"],
#     "polybench" : ["linear-algebra_3mm", "linear-algebra_doitgen", "linear-algebra_gemm", "linear-algebra_gramschmidt", "linear-algebra_gemver", "linear-algebra_symm", "stencil_convolution-2d", "stencil_fdtd-apml"], 
#     "rodinia" : ["BFS_BFS", "NW_UserAdded"],
#     "splash-2" : ["FFT_Reverse", "FFT_Transpose", "Oceanncp_jacobcalc", "Oceanncp_laplaccalc", "Oceancp_slave2", "Radix_slave_sort"],
#     "stream" : ["Add_Add", "Copy_Copy", "Scale_Scale", "Triad_Triad"]}
#Following are all benchmarks that has higher than warmup requests
# benchmark_suites_and_benchmarks_functions = {"chai" : ["BS_BEZIER_KERNEL", "OOPPAD_OOPPAD"],
#     "darknet" : ["yolo_gemm_nn"],
#     "hashjoin" : ["NPO_probehashtable", "PRH_histogramjoin"],
#     "hpcg" : ["HPCG_ComputeSPMV"],
#     "ligra" : ["BC_edgeMapSparseUSAUserAdded", "BFSCC_edgeMapSparseUSAUserAdded", "BFS_edgeMapSparseUSAUserAdded", "PageRank_edgeMapDenseUSA",  "Triangle_edgeMapDenseRmat"],
#     "phoenix" : ["Linearregression_main"],
#     "polybench" : ["linear-algebra_3mm", "linear-algebra_doitgen", "linear-algebra_gemm", "linear-algebra_gramschmidt", "linear-algebra_gemver", "linear-algebra_symm", "stencil_convolution-2d", "stencil_fdtd-apml"], 
#     "rodinia" : ["BFS_BFS", "NW_UserAdded"],
#     "splash-2" : ["FFT_Reverse", "FFT_Transpose", "Oceanncp_jacobcalc", "Oceanncp_laplaccalc", "Oceancp_slave2", "Radix_slave_sort"],
#     "stream" : ["Add_Add", "Copy_Copy", "Scale_Scale", "Triad_Triad"]}
benchmark_suites_and_benchmarks_functions = {"chai" : ["BS_BEZIER_KERNEL"],"splash-2" : ["Radix_slave_sort"],}

stats_folders = os.path.join(os.getcwd(), "zsim_stats")

for configuration in configurations:
    for core_number in core_numbers:
        for suite in benchmark_suites_and_benchmarks_functions.keys():
            for benchmark_function in benchmark_suites_and_benchmarks_functions[suite]:
                access_counts = {}
                for core in range(core_number):
                    for cache_type in cache_types:
                        csv_file_path = os.path.join(stats_folders, configuration, str(core_number), suite+"_"+benchmark_function+"."+cache_type+"-"+str(core)+".cache_access_trace.csv")
                        print "Reading "+csv_file_path
                        with open(csv_file_path) as csv_file:
                            csv_reader = csv.DictReader(csv_file, delimiter=',')
                            for row in csv_reader:
                                address = row[address_label]
                                type = row[type_label]
                                if address == None or len(address) <= 0:
                                    print "No address read"
                                    continue
                                if type == None or len(type) <= 0:
                                    print "No type read for address "+address
                                    continue
                                if not address in access_counts:
                                    access_counts[address] = {}
                                    for request_type in request_types:
                                        access_counts[address][request_type] = 0
                                    access_counts[address][total_label] = 0
                                access_counts[address][type]+=1
                                access_counts[address][total_label]+=1
                csv_output_path = os.path.join(stats_folders, configuration, str(core_number), suite+"_"+benchmark_function+".cache_access_count.csv")
                print "Writing to "+csv_output_path
                with open(csv_output_path, "w") as csv_output_file:
                    csv_header = [address_label]+request_types+[total_label]
                    csv_writer = csv.DictWriter(csv_output_file, fieldnames=csv_header)
                    csv_writer.writeheader()
                    for address in access_counts:
                        current_map = access_counts[address]
                        current_map[address_label] = address
                        csv_writer.writerow(current_map)
                    