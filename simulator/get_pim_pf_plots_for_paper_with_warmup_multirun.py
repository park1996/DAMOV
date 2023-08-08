# -*- coding: utf-8 -*-
import os
import csv
from datetime import datetime
from batch_prefetcher_generator import get_hops_thresholds, get_count_thresholds
import numpy
import matplotlib
# Use non X-Window engine as we are executing it in terminal
matplotlib.use('Agg')
from matplotlib import pyplot as plt
import math

hops_thresholds = get_hops_thresholds()
# count_thresholds = get_count_thresholds()
count_thresholds = [0]
hops_thresholds_str = [""]
count_thresholds_str = [""]
debug_tag = "debugoff"
iterations = 5
core_number = 128

def check_stat_file_exist(stat_file_location):
    for i in range(iterations):
        if not os.path.isfile(stat_file_location+"."+str(i)):
            return False
    return True

def extract_cycle(stat_file_location):
    total_cycle_val = 0
    for i in range(iterations):
        cycle_val = -1
        cycle_val_at_end_of_warmup = -1
        with open(stat_file_location+"."+str(i), mode='r') as stat:
            for line in stat:
                if(line.find("RamulatorCycleAtFinish")!= -1):
                    cycle_val = int(line.split()[1])
        with open(stat_file_location+"."+str(i), mode='r') as stat:
            for line in stat:
                if(line.find("WarmupCycles")!= -1):
                    cycle_val_at_end_of_warmup = int(line.split()[1])
        if cycle_val == -1 or cycle_val_at_end_of_warmup == -1:
            return -1.0
        else:
            total_cycle_val+=(cycle_val - cycle_val_at_end_of_warmup)
    return float(total_cycle_val)/float(iterations)

def extract_l1_mpki(stat_file_location):
    l1_mpki = 0.0
    for i in range(iterations):
        l1d = False
        l2d = False
        l3d = False
        l1_hits = 0
        l1_misses = 0
        l1_miss_rate = 0
        instructions = 0
        with open(stat_file_location+"."+str(i), mode='r') as stat:
            for line in stat:
                if(line.find("l1d:")!=-1):
                    l1d = True
                    l2d = False
                    l3d = False
                if(line.find("l2: # Cache stats")!=-1):
                    l1d = False
                    l2d = True
                    l3d = False
                if(line.find("l3: # Cache stats")!=-1):
                    l1d = False
                    l2d = False
                    l3d = True
                if(line.find("sched: # Scheduler stats")!=-1):
                    l1d = False
                    l2d = False
                    l3d = False
                if(l1d == True):
                    if(line.find("# GETS hits")!=-1 or line.find("# GETX hits")!=-1):
                        l1_hits += int(line.split()[1])
                    if(line.find("# GETS misses")!=-1 or line.find("# GETX I->M misses")!=-1):
                        l1_misses += int(line.split()[1])
                if(line.find("instrs: ")!=-1):
                    instructions += int(line.split()[1])
        try:
            l1_mpki += (float(l1_misses)/float((instructions/1000.0)))
        except:
            return -1.0
    return float(l1_mpki)/float(iterations)

def extract_subscription_stats(stat_file_location, stat_name):
    total_value = 0.0
    for i in range(iterations):
        value = -1
        value_at_end_of_warmup = -1
        with open(stat_file_location+"."+str(i), mode='r') as stat:
            for line in stat:
                if(line.find(stat_name)!= -1):
                    value = int(line.split()[1])
        with open(stat_file_location+".end_of_warmup"+"."+str(i), mode='r') as stat:
            for line in stat:
                if(line.find(stat_name)!= -1):
                    value_at_end_of_warmup = int(line.split()[1])
        if value == -1 or value_at_end_of_warmup == -1:
            return -1.0
        else:
            total_value+=(value - value_at_end_of_warmup)
    return float(total_value)/float(iterations)

def extract_subscription_stats_float(stat_file_location, stat_name):
    total_value = 0.0
    for i in range(iterations):
        value = -1
        value_at_end_of_warmup = -1
        with open(stat_file_location+"."+str(i), mode='r') as stat:
            for line in stat:
                if(line.find(stat_name)!= -1):
                    value = float(line.split()[1])
        with open(stat_file_location+".end_of_warmup"+"."+str(i), mode='r') as stat:
            for line in stat:
                if(line.find(stat_name)!= -1):
                    value_at_end_of_warmup = float(line.split()[1])
        if value == -1 or value_at_end_of_warmup == -1:
            return -1.0
        else:
            total_value+=(value - value_at_end_of_warmup)
    return float(total_value)/float(iterations)

def extract_per_vault_count(stat_file_location, eow_stat_file_location, label, core_number):
    result = [0]*core_number
    result_at_end_of_warmup = [0]*core_number
    with open(stat_file_location, "r") as csv_file:
        csv_reader = csv.DictReader(csv_file, delimiter=' ')
        for row in csv_reader:
            result[int(row[label])] += int(row["#Requests"])
    with open(eow_stat_file_location, "r") as csv_file:
        csv_reader = csv.DictReader(csv_file, delimiter=' ')
        for row in csv_reader:
            result_at_end_of_warmup[int(row[label])] += int(row["#Requests"])
    return numpy.subtract(numpy.array(result), numpy.array(result_at_end_of_warmup))

def extract_from_vault_count(stat_file_location, eow_stat_file_location, core_number):
    return extract_per_vault_count(stat_file_location, eow_stat_file_location, "CoreID", core_number)

def extract_to_vault_count(stat_file_location, eow_stat_file_location, core_number):
    return extract_per_vault_count(stat_file_location, eow_stat_file_location, "VaultID", core_number)

def calculate_cov_to_vault(stat_file_location, core_number):
    total_cov = 0.0
    for i in range(iterations):
        accesses_to_vaults = extract_to_vault_count(stat_file_location+"."+str(i), stat_file_location+".end_of_warmup."+str(i), core_number)
        req_to_vault_mean = numpy.mean(accesses_to_vaults)
        req_to_vault_std = numpy.std(accesses_to_vaults)
        if numpy.isnan(req_to_vault_mean) or numpy.isnan(req_to_vault_std) or req_to_vault_mean == 0:
            return -1.0
        else:
            total_cov += (req_to_vault_std / req_to_vault_mean)
    return float(total_cov)/float(iterations)

def calculate_hops_travelled(src_vault, dst_vault, core_number):
    network_width = int(math.ceil(math.sqrt(core_number))) # Assuming square network
    vault_destination_x = dst_vault/network_width
    vault_destination_y = dst_vault%network_width
    vault_origin_x = src_vault/network_width
    vault_origin_y = src_vault%network_width
    hops = abs(vault_destination_x - vault_origin_x) + abs(vault_destination_y - vault_origin_y)
    return hops

def calculate_hops_travelled_distribution(stat_file_location, core_number):
    network_width = int(math.ceil(math.sqrt(core_number))) # Assuming square network
    max_hops = (network_width-1)*2
    result = [0]*(max_hops+1)
    for i in range(iterations):
        with open(stat_file_location+"."+str(i), "r") as csv_file:
            csv_reader = csv.DictReader(csv_file, delimiter=' ')
            for row in csv_reader:
                src_vault = int(row["CoreID"])
                dst_vault = int(row["VaultID"])
                hops_travelled = calculate_hops_travelled(src_vault, dst_vault, core_number)
                result[hops_travelled] += int(row["#Requests"])
        with open(stat_file_location+".end_of_warmup."+str(i), "r") as csv_file:
            csv_reader = csv.DictReader(csv_file, delimiter=' ')
            for row in csv_reader:
                src_vault = int(row["CoreID"])
                dst_vault = int(row["VaultID"])
                hops_travelled = calculate_hops_travelled(src_vault, dst_vault, core_number)
                result[hops_travelled] -= int(row["#Requests"])
    return [float(val)/float(iterations) for val in result]

def mkdir_p(directory):
    try:
        os.makedirs(directory)
    except OSError as exc:  # Python >2.5
        if exc.errno == errno.EEXIST and os.path.isdir(directory):
            pass
        else:
            raise

def write_stats_output_files(output_dir, filename, title, x_lengend, y_lengend, data_map):
    image_output_dir = os.path.join(output_dir,filename+".png")
    csv_output_dir = image_output_dir.replace(".png", ".csv")
    plt.legend()
    plt.bar(data_map.keys(), data_map.values(), color ='maroon',
            width = 0.5)
    plt.xticks(rotation=90)
    plt.xlabel(x_lengend)
    plt.ylabel(y_lengend)
    plt.title(title)
    plt.savefig(image_output_dir)
    plt.clf()
    with open(csv_output_dir, "w") as csv_file:
        csv_writer = csv.writer(csv_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
        csv_writer.writerow([title])
        csv_writer.writerow([x_lengend, y_lengend])
        for key in data_map.keys():
            csv_writer.writerow([str(key).replace("\n", " "), str(data_map[key])])

# Following are all the benchmark's short name
benchmark_suites_and_benchmarks_functions_to_short_name = {"chai" : {"BS_BEZIER_KERNEL":"CHABsBez", "HSTI_HSTI":"CHAHsti", "HSTO_HSTO":"CHAHsto", "OOPPAD_OOPPAD":"CHAOpad"},
    "darknet" : {"resnet152_gemm_nn":"DRKRes", "yolo_gemm_nn":"DRKYolo"},
    "hashjoin" : {"NPO_probehashtable":"HSJNPO", "PRH_histogramjoin":"HSJPRH"},
    "hpcg" : {"HPCG_ComputePrologation":"HPGProl", "HPCG_ComputeRestriction":"HPGRes", "HPCG_ComputeSPMV":"HPGSpm", "HPCG_ComputeSYMGS":"HPGSyms"},
    "ligra" : {"BC_edgeMapSparseUSAUserAdded":"LIGBcEms", "BFSCC_edgeMapSparseUSAUserAdded":"LIGBfscEms", "BFS_edgeMapSparseUSAUserAdded":"LIGBfsEms", "Components_edgeMapSparseUSAUserAdded":"LIGCompEms", "PageRank_edgeMapDenseUSA":"LIGPrkEmd", "Radii_edgeMapSparseUSA":"LIGRadiEms", "Triangle_edgeMapDenseRmat":"LIGTriEmd"},
    "parsec" : {"Fluidaminate_ProcessCollision2MT":"PRSFerr"},
    "phoenix" : {"Linearregression_main":"PHELinReg", "Stringmatch_main":"PHEStrMat"},
    "polybench" : {"linear-algebra_3mm":"PLY3mm", "linear-algebra_doitgen":"PLYDoitgen", "linear-algebra_gemm":"PLYgemm", "linear-algebra_gramschmidt":"PLYGramSch", "linear-algebra_gemver":"PLYgemver", "linear-algebra_symm":"PLYSymm", "stencil_convolution-2d":"PLYcon2d", "stencil_fdtd-apml":"PLYdtd"}, 
    "rodinia" : {"BFS_BFS":"RODBfs", "NW_UserAdded":"RODNw"},
    "splash-2" : {"FFT_Reverse":"SPLFftRev", "FFT_Transpose":"SPLFftTra", "Oceanncp_jacobcalc":"SPLOcnpJac", "Oceanncp_laplaccalc":"SPLOcnpLap", "Oceancp_slave2":"SPLOcpSlave", "Radix_slave_sort":"SPLRad"},
    "stream" : {"Add_Add":"STRAdd", "Copy_Copy":"STRCpy", "Scale_Scale":"STRSca", "Triad_Triad":"STRTriad"}}

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
benchmark_suites_and_benchmarks_functions = {"chai" : ["BS_BEZIER_KERNEL", "OOPPAD_OOPPAD"],
    "darknet" : ["yolo_gemm_nn"],
    "hashjoin" : ["NPO_probehashtable", "PRH_histogramjoin"],
    "hpcg" : ["HPCG_ComputeSPMV"],
    "ligra" : ["BC_edgeMapSparseUSAUserAdded", "BFSCC_edgeMapSparseUSAUserAdded", "BFS_edgeMapSparseUSAUserAdded", "PageRank_edgeMapDenseUSA",  "Triangle_edgeMapDenseRmat"],
    "phoenix" : ["Linearregression_main"],
    "polybench" : ["linear-algebra_3mm", "linear-algebra_doitgen", "linear-algebra_gemm", "linear-algebra_gramschmidt", "linear-algebra_gemver", "linear-algebra_symm", "stencil_convolution-2d", "stencil_fdtd-apml"], 
    "rodinia" : ["BFS_BFS", "NW_UserAdded"],
    "splash-2" : ["FFT_Reverse", "FFT_Transpose", "Oceanncp_jacobcalc", "Oceanncp_laplaccalc", "Oceancp_slave2", "Radix_slave_sort"],
    "stream" : ["Add_Add", "Copy_Copy", "Scale_Scale", "Triad_Triad"]}

processor_type_prefix = "pim_prefetch_netoh_"
baseline_processor_type = "pim_ooo_netoh"
prefetcher_types = ["allocate"]
plt.figure(figsize=(10.5,6))
plt.subplots_adjust(bottom=0.35, left=0.1, right=0.95, top=0.95)
byte_per_hop = 16

output_dir_name = "plots_for_paper_"+datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
output_dir = os.path.join(os.getcwd(), output_dir_name)
mkdir_p(output_dir)
stats_folders = os.path.join(os.getcwd(), "zsim_stats")
baseline_cycles = {}
baseline_avg_hops = {}
baseline_avg_hmc_latency = {}
baseline_mpkis = {}
baseline_covs = {}
baseline_hops_distributions = {}
baseline_avg_xfer_latency = {}
baseline_xfer_percentage = {}
baseline_avg_qing_latency = {}
baseline_qing_percentage = {}
for suite in benchmark_suites_and_benchmarks_functions.keys():
    for benchmark_function in benchmark_suites_and_benchmarks_functions[suite]:
        full_benchmark_name = suite+"_"+benchmark_function
        benchmark_key = benchmark_suites_and_benchmarks_functions_to_short_name[suite][benchmark_function]
        baseline_stat_file_location = os.path.join(stats_folders, baseline_processor_type, str(core_number), full_benchmark_name+".ramulator.cycle_stats")
        baseline_cycle = -1
        baseline_mpki = -1
        if check_stat_file_exist(baseline_stat_file_location):
            baseline_cycle = extract_cycle(baseline_stat_file_location)
        baseline_zsim_stat_file_location = os.path.join(stats_folders, baseline_processor_type, str(core_number), full_benchmark_name+".zsim.out")
        if check_stat_file_exist(baseline_zsim_stat_file_location):
            baseline_mpki = extract_l1_mpki(baseline_zsim_stat_file_location)
        baseline_cycles[benchmark_key] = 0 if baseline_cycle == -1 else baseline_cycle
        baseline_mpkis[benchmark_key] = 0 if baseline_mpki == -1 else baseline_mpki
        baseline_sub_stat_file_location = os.path.join(stats_folders, baseline_processor_type, str(core_number), full_benchmark_name+".ramulator.subscription_stats")
        baseline_access_hops = -1
        baseline_mem_access = -1
        baseline_hmc_latency = -1
        baseline_xfer_latency = -1
        baseline_inc_qing_latency = -1
        baseline_out_qing_latency = -1
        if check_stat_file_exist(baseline_sub_stat_file_location) and check_stat_file_exist(baseline_sub_stat_file_location+".end_of_warmup"):
            baseline_access_hops = extract_subscription_stats(baseline_sub_stat_file_location, "AccessPktHopsTravelled")
            baseline_mem_access = extract_subscription_stats(baseline_sub_stat_file_location, "MemAccesses")
            baseline_hmc_latency = extract_subscription_stats(baseline_sub_stat_file_location, "TotalHMCLatency")
            baseline_xfer_latency = extract_subscription_stats(baseline_sub_stat_file_location, "TotalTransferLatency")
            baseline_inc_qing_latency = extract_subscription_stats(baseline_sub_stat_file_location, "TotalIncomingQueuingLatency")
            baseline_out_qing_latency = extract_subscription_stats(baseline_sub_stat_file_location, "TotalOutgoingQueuingLatency")
        else:
            print baseline_sub_stat_file_location+" does not exist"
        baseline_avg_hops[benchmark_key] = 0 if baseline_access_hops == -1 or baseline_mem_access == -1 or baseline_mem_access == 0 else float(baseline_access_hops)/float(baseline_mem_access)
        baseline_avg_hmc_latency[benchmark_key] = 0 if baseline_hmc_latency == -1 or baseline_mem_access == -1 or baseline_mem_access == 0 else float(baseline_hmc_latency)/float(baseline_mem_access)
        baseline_avg_xfer_latency[benchmark_key] = 0 if baseline_xfer_latency == -1 or baseline_mem_access == -1 or baseline_mem_access == 0 else float(baseline_xfer_latency)/float(baseline_mem_access)
        baseline_avg_qing_latency[benchmark_key] = 0 if baseline_inc_qing_latency == -1 or baseline_out_qing_latency == -1 or baseline_mem_access == -1 or baseline_mem_access == 0 else float(baseline_inc_qing_latency+baseline_out_qing_latency)/float(baseline_mem_access)
        baseline_xfer_percentage[benchmark_key] = 0 if baseline_xfer_latency == -1 or baseline_hmc_latency == -1 or baseline_hmc_latency == 0 else float(baseline_xfer_latency)/float(baseline_hmc_latency)
        baseline_qing_percentage[benchmark_key] = 0 if baseline_inc_qing_latency == -1 or baseline_out_qing_latency == -1 or baseline_hmc_latency == 0 else float(baseline_inc_qing_latency+baseline_out_qing_latency)/float(baseline_hmc_latency)
        baseline_req_to_vault_cov = -1
        baseline_hops_distribution = []
        baseline_addr_dist_file_location = os.path.join(stats_folders, baseline_processor_type, str(core_number), full_benchmark_name+".ramulator.address_distribution")
        if check_stat_file_exist(baseline_addr_dist_file_location) and check_stat_file_exist(baseline_addr_dist_file_location+".end_of_warmup"):
            baseline_req_to_vault_cov = calculate_cov_to_vault(baseline_addr_dist_file_location, core_number)
            baseline_hops_distribution = calculate_hops_travelled_distribution(baseline_addr_dist_file_location, core_number)
        baseline_covs[benchmark_key] = baseline_req_to_vault_cov
        baseline_hops_distributions[benchmark_key] = baseline_hops_distribution

baseline_hops_distribution_csv_output_dir = os.path.join(output_dir,"baseline_hops_distribution.csv")
with open(baseline_hops_distribution_csv_output_dir, "w") as csv_file:
    header = ["Workload"]
    network_width = int(math.ceil(math.sqrt(core_number))) # Assuming square network
    max_hops = (network_width-1)*2
    for i in range(max_hops+1):
        header.append(str(i))
    csv_writer = csv.writer(csv_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
    csv_writer.writerow(header)
    for key in baseline_hops_distributions.keys():
        csv_writer.writerow([str(key).replace("\n", " ")]+baseline_hops_distributions[key])

write_stats_output_files(output_dir, "baseline_cycle", "Baseline Cycle by Workload", "Workload", "Cycle", baseline_cycles)
write_stats_output_files(output_dir, "baseline_avg_hops", "Baseline Average Hops by Workload", "Workload", "Hops", baseline_avg_hops)
write_stats_output_files(output_dir, "baseline_avg_hmc_latency", "Baseline Average HMC Latency by Workload", "Workload", "Latency (Cycle)", baseline_avg_hmc_latency)
write_stats_output_files(output_dir, "baseline_cov", "Coefficient of Variation by Workload", "Workload", "Coefficient of Variation", baseline_covs)
write_stats_output_files(output_dir, "baseline_mpki", "MPKI by Workload", "Workload", "MPKI", baseline_mpkis)
write_stats_output_files(output_dir, "baseline_avg_xfer_latency", "Baseline Average Transfer Latency by Workload", "Workload", "Latency (Cycle)", baseline_avg_xfer_latency)
write_stats_output_files(output_dir, "baseline_avg_qing_latency", "Baseline Average Queuing Latency by Workload", "Workload", "Latency (Cycle)", baseline_avg_qing_latency)
write_stats_output_files(output_dir, "baseline_xfer_percentage", "Baseline Transfer Latency Propotionate to Total Latency by Workload", "Workload", "Percentage", baseline_xfer_percentage)
write_stats_output_files(output_dir, "baseline_qing_percentage", "Baseline Queuing Latency Propotionate to Total Latency by Workload", "Workload", "Percentage", baseline_qing_percentage)

plotting_prefetcher_policies = ["1h0c", "adaptive"]
for prefetcher_type in prefetcher_types:
    for policy in plotting_prefetcher_policies:
        normalized_cycles = {}
        avg_sub_access = {}
        avg_loc_sub_access = {}
        for suite in benchmark_suites_and_benchmarks_functions.keys():
            for benchmark_function in benchmark_suites_and_benchmarks_functions[suite]:
                processor_type = processor_type_prefix+prefetcher_type
                full_benchmark_name = suite+"_"+benchmark_function
                benchmark_key = benchmark_suites_and_benchmarks_functions_to_short_name[suite][benchmark_function]
                stat_file_location = os.path.join(stats_folders, processor_type, policy+"_"+debug_tag, str(core_number), full_benchmark_name+".ramulator.cycle_stats")
                baseline_stat_file_location = os.path.join(stats_folders, baseline_processor_type, str(core_number), full_benchmark_name+".ramulator.cycle_stats")
                baseline_cycle = -1
                if check_stat_file_exist(baseline_stat_file_location):
                    baseline_cycle = extract_cycle(baseline_stat_file_location)
                cycle = -1
                if check_stat_file_exist(stat_file_location):
                    cycle = extract_cycle(stat_file_location)
                normalized_cycles[benchmark_key] = 0 if baseline_cycle == -1 or cycle == -1 or cycle == 0 else float(baseline_cycle)/float(cycle)
                sub_stat_file_location = os.path.join(stats_folders, processor_type, policy+"_"+debug_tag, str(core_number), full_benchmark_name+".ramulator.subscription_stats")
                successful_subscription = -1
                subscription_accesses = -1
                local_subscription_accesses = -1
                if check_stat_file_exist(sub_stat_file_location):
                    successful_subscription = extract_subscription_stats(sub_stat_file_location, "SuccessfulSubscriptions")
                    subscription_accesses = extract_subscription_stats(sub_stat_file_location, "SubAccesses")
                    local_subscription_accesses = extract_subscription_stats(sub_stat_file_location, "SubLocAccesses")
                avg_sub_access[benchmark_key] = 0 if successful_subscription == 0 or successful_subscription == -1 or subscription_accesses == -1 else float(subscription_accesses)/float(successful_subscription)
                avg_loc_sub_access[benchmark_key] = 0 if successful_subscription == 0 or successful_subscription == -1 or subscription_accesses == -1 else float(local_subscription_accesses)/float(successful_subscription)
        write_stats_output_files(output_dir, "normalized_cycles_"+prefetcher_type+"_"+policy, "Normalized Cycle by Workload", "Workload", "Normalized Cycle", normalized_cycles)
        write_stats_output_files(output_dir, "avg_sub_access_"+prefetcher_type+"_"+policy, "Average Subscription Accesses by Workload", "Workload", "Access", avg_sub_access)
        write_stats_output_files(output_dir, "avg_loc_sub_access_"+prefetcher_type+"_"+policy, "Average Local Subscription Accesses by Workload", "Workload", "Access", avg_loc_sub_access)
plt.subplots_adjust(bottom=0.1)

# The following are benchmarks that impacted by our model
selected_benchmark_suites_and_benchmarks_functions = {"chai" : ["BS_BEZIER_KERNEL"],
    "darknet" : ["yolo_gemm_nn"],
    "phoenix" : ["Linearregression_main"],
    "polybench" : ["linear-algebra_3mm", "linear-algebra_doitgen", "linear-algebra_gemm", "linear-algebra_gemver", "linear-algebra_symm", "stencil_convolution-2d"], 
    "splash-2" : ["FFT_Reverse", "FFT_Transpose", "Radix_slave_sort"],
    "stream" : ["Triad_Triad"]}

selected_normalized_cycle_csv_output_dir = os.path.join(output_dir,"selected_bm_normalized_cycle.csv")
selected_avg_access_hops_csv_output_dir = os.path.join(output_dir,"selected_bm_avg_access_hops.csv")
selected_total_traffic_csv_output_dir = os.path.join(output_dir,"selected_bm_total_traffic.csv")
selected_avg_traffic_csv_output_dir = os.path.join(output_dir,"selected_bm_avg_traffic.csv")
selected_cov_csv_output_dir = os.path.join(output_dir,"selected_bm_cov.csv")
selected_avg_hmc_lat_csv_output_dir = os.path.join(output_dir,"selected_bm_avg_hmc_lat.csv")
selected_total_rq_pending_output_dir = os.path.join(output_dir,"selected_total_rq_pending.csv")
selected_total_wq_pending_output_dir = os.path.join(output_dir,"selected_total_wq_pending.csv")
selected_total_aq_pending_output_dir = os.path.join(output_dir,"selected_total_aq_pending.csv")
selected_total_outgoing_q_pending_output_dir = os.path.join(output_dir,"selected_total_outgoing_q_pending.csv")
selected_avg_xfer_latency_output_dir = os.path.join(output_dir,"selected_bm_avg_xfer_latency.csv")
selected_avg_qing_latency_output_dir = os.path.join(output_dir,"selected_bm_avg_qing_latency.csv")
selected_xfer_percentage_output_dir = os.path.join(output_dir,"selected_bm_xfer_percentage.csv")
selected_qing_percentage_output_dir = os.path.join(output_dir,"selected_bm_qing_percentage.csv")

selected_normalized_cycle_csv_file = open(selected_normalized_cycle_csv_output_dir, "w")
selected_avg_access_hops_csv_file = open(selected_avg_access_hops_csv_output_dir, "w")
selected_total_traffic_csv_file = open(selected_total_traffic_csv_output_dir, "w")
selected_avg_traffic_csv_file = open(selected_avg_traffic_csv_output_dir, "w")
selected_cov_csv_file = open(selected_cov_csv_output_dir, "w")
selected_avg_hmc_lat_csv_file = open(selected_avg_hmc_lat_csv_output_dir, "w")
selected_total_rq_pending_csv_file = open(selected_total_rq_pending_output_dir, "w")
selected_total_wq_pending_csv_file = open(selected_total_wq_pending_output_dir, "w")
selected_total_aq_pending_csv_file = open(selected_total_aq_pending_output_dir, "w")
selected_total_outgoing_q_pending_csv_file = open(selected_total_outgoing_q_pending_output_dir, "w")
selected_avg_xfer_latency_csv_file = open(selected_avg_xfer_latency_output_dir, "w")
selected_avg_qing_latency_csv_file = open(selected_avg_qing_latency_output_dir, "w")
selected_xfer_percentage_csv_file = open(selected_xfer_percentage_output_dir, "w")
selected_qing_percentage_csv_file = open(selected_qing_percentage_output_dir, "w")

selected_normalized_cycle_csv_write = csv.writer(selected_normalized_cycle_csv_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
selected_avg_access_hops_csv_write = csv.writer(selected_avg_access_hops_csv_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
selected_total_traffic_csv_write =csv.writer(selected_total_traffic_csv_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
selected_avg_traffic_csv_write =csv.writer(selected_avg_traffic_csv_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
selected_cov_csv_write = csv.writer(selected_cov_csv_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
selected_avg_hmc_lat_csv_write = csv.writer(selected_avg_hmc_lat_csv_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
selected_total_rq_pending_csv_write = csv.writer(selected_total_rq_pending_csv_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
selected_total_wq_pending_csv_write = csv.writer(selected_total_wq_pending_csv_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
selected_total_aq_pending_csv_write = csv.writer(selected_total_aq_pending_csv_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
selected_total_outgoing_q_pending_csv_write = csv.writer(selected_total_outgoing_q_pending_csv_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
selected_avg_xfer_latency_csv_write = csv.writer(selected_avg_xfer_latency_csv_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
selected_avg_qing_latency_csv_write = csv.writer(selected_avg_qing_latency_csv_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
selected_xfer_percentage_csv_write = csv.writer(selected_xfer_percentage_csv_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
selected_qing_percentage_csv_write = csv.writer(selected_qing_percentage_csv_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)

for prefetcher_type in prefetcher_types:
    for hops_threshold in hops_thresholds:
        selected_normalized_cycle_csv_write.writerow(["Normalized Cycle for "+prefetcher_type+" "+str(hops_threshold)+" Hops"])
        selected_avg_access_hops_csv_write.writerow(["Average Access Hops for "+prefetcher_type+" "+str(hops_threshold)+" Hops"])
        selected_total_traffic_csv_write.writerow(["Total Traffic for "+prefetcher_type+" "+str(hops_threshold)+" Hops"])
        selected_avg_traffic_csv_write.writerow(["Average Traffic for "+prefetcher_type+" "+str(hops_threshold)+" Hops"])
        selected_cov_csv_write.writerow(["Access Distribution for "+prefetcher_type+" "+str(hops_threshold)+" Hops"])
        selected_avg_hmc_lat_csv_write.writerow(["Average HMC Latency for "+prefetcher_type+" "+str(hops_threshold)+" Hops"])
        selected_total_rq_pending_csv_write.writerow(["Total Read Queue Pending for "+prefetcher_type+" "+str(hops_threshold)+" Hops"])
        selected_total_wq_pending_csv_write.writerow(["Total Write Queue Pending for "+prefetcher_type+" "+str(hops_threshold)+" Hops"])
        selected_total_aq_pending_csv_write.writerow(["Total Incoming Queue Pending for "+prefetcher_type+" "+str(hops_threshold)+" Hops"])
        selected_total_outgoing_q_pending_csv_write.writerow(["Total Outgoing Queue Pending for "+prefetcher_type+" "+str(hops_threshold)+" Hops"])
        selected_avg_xfer_latency_csv_write.writerow(["Average Transfer Latency for "+prefetcher_type+" "+str(hops_threshold)+" Hops"])
        selected_avg_qing_latency_csv_write.writerow(["Average Queuing Latency for "+prefetcher_type+" "+str(hops_threshold)+" Hops"])
        selected_xfer_percentage_csv_write.writerow(["Transfer Latency Propotionate to Total Latency for "+prefetcher_type+" "+str(hops_threshold)+" Hops"])
        selected_qing_percentage_csv_write.writerow(["Queuing Latency Propotionate to Total Latency for "+prefetcher_type+" "+str(hops_threshold)+" Hops"])
        
        csv_header = ["Workload", "Baseline", "Adaptive"]
        policy_name_to_label = {}
        prefetcher_policies = ["adaptive"]
        policy_name_to_label["adaptive"] = "Adaptive"
        policy_name_to_label["baseline"] = "Baseline"
        for count_threshold in count_thresholds:
            policy_name = str(hops_threshold)+"h"+str(count_threshold)+"c"
            prefetcher_policies.append(policy_name)
            policy_name_to_label[policy_name] = str(count_threshold)+" Count"
            csv_header.append(str(count_threshold)+" Count")
        
        selected_normalized_cycle_csv_write.writerow(csv_header)
        selected_avg_access_hops_csv_write.writerow(csv_header)
        selected_total_traffic_csv_write.writerow(csv_header)
        selected_avg_traffic_csv_write.writerow(csv_header)
        selected_cov_csv_write.writerow(csv_header)
        selected_avg_hmc_lat_csv_write.writerow(csv_header)
        selected_total_rq_pending_csv_write.writerow(csv_header)
        selected_total_wq_pending_csv_write.writerow(csv_header)
        selected_total_aq_pending_csv_write.writerow(csv_header)
        selected_total_outgoing_q_pending_csv_write.writerow(csv_header)
        selected_avg_xfer_latency_csv_write.writerow(csv_header)
        selected_avg_qing_latency_csv_write.writerow(csv_header)
        selected_xfer_percentage_csv_write.writerow(csv_header)
        selected_qing_percentage_csv_write.writerow(csv_header)

        
        for suite in selected_benchmark_suites_and_benchmarks_functions.keys():
            for benchmark_function in selected_benchmark_suites_and_benchmarks_functions[suite]:
                full_benchmark_name = suite+"_"+benchmark_function
                benchmark_tag = benchmark_suites_and_benchmarks_functions_to_short_name[suite][benchmark_function]
                normalized_cycle_line = [benchmark_tag]
                avg_access_hops_line = [benchmark_tag]
                total_traffic_line = [benchmark_tag]
                avg_traffic_line = [benchmark_tag]
                cov_line = [benchmark_tag]
                avg_hmc_lat_line = [benchmark_tag]
                rq_pending_line = [benchmark_tag]
                wq_pending_line = [benchmark_tag]
                aq_pending_line = [benchmark_tag]
                outgoing_q_pending_line = [benchmark_tag]
                avg_xfer_latency_line = [benchmark_tag]
                avg_qing_latency_line = [benchmark_tag]
                xfer_percentage_line = [benchmark_tag]
                qing_percentage_line = [benchmark_tag]

                per_bm_dir = os.path.join(output_dir,full_benchmark_name)
                mkdir_p(per_bm_dir)

                baseline_cycle = -1
                baseline_stat_file_location = os.path.join(stats_folders, baseline_processor_type, str(core_number), full_benchmark_name+".ramulator.cycle_stats")
                if check_stat_file_exist(baseline_stat_file_location):
                    baseline_cycle = extract_cycle(baseline_stat_file_location)
                
                baseline_access_hops = -1
                baseline_mem_access = -1
                baseline_hmc_latency = -1
                baseline_rq_pending = -1
                baseline_wq_pending = -1
                baseline_outgoing_q_pending = -1
                baseline_xfer_latency = -1
                baseline_inc_qing_latency = -1
                baseline_out_qing_latency = -1
                baseline_sub_stat_file_location = os.path.join(stats_folders, baseline_processor_type, str(core_number), full_benchmark_name+".ramulator.subscription_stats")
                if check_stat_file_exist(baseline_sub_stat_file_location) and check_stat_file_exist(baseline_sub_stat_file_location+".end_of_warmup"):
                    baseline_access_hops = extract_subscription_stats(baseline_sub_stat_file_location, "AccessPktHopsTravelled")
                    baseline_mem_access = extract_subscription_stats(baseline_sub_stat_file_location, "MemAccesses")
                    baseline_hmc_latency = extract_subscription_stats(baseline_sub_stat_file_location, "TotalHMCLatency")
                    baseline_rq_pending = extract_subscription_stats(baseline_sub_stat_file_location, "TotalReadQPending")
                    baseline_wq_pending = extract_subscription_stats(baseline_sub_stat_file_location, "TotalWriteQPending")
                    baseline_outgoing_q_pending = extract_subscription_stats(baseline_sub_stat_file_location, "TotalPendingQPending")
                    baseline_xfer_latency = extract_subscription_stats(baseline_sub_stat_file_location, "TotalTransferLatency")
                    baseline_inc_qing_latency = extract_subscription_stats(baseline_sub_stat_file_location, "TotalIncomingQueuingLatency")
                    baseline_out_qing_latency = extract_subscription_stats(baseline_sub_stat_file_location, "TotalOutgoingQueuingLatency")
                
                baseline_req_to_vault_cov = -1
                baseline_addr_dist_file_location = os.path.join(stats_folders, baseline_processor_type, str(core_number), full_benchmark_name+".ramulator.address_distribution")
                if check_stat_file_exist(baseline_addr_dist_file_location) and check_stat_file_exist(baseline_addr_dist_file_location+".end_of_warmup"):
                    baseline_req_to_vault_cov = calculate_cov_to_vault(baseline_addr_dist_file_location, core_number)
                
                normalized_cycle_map = {"Baseline":0 if baseline_cycle == 0 or baseline_cycle == -1 else 1}
                normalized_cycle_line.append(0 if baseline_cycle == 0 or baseline_cycle == -1 else 1)
                avg_access_hops_map = {"Baseline":0 if baseline_access_hops == -1 or baseline_mem_access == 0 or baseline_mem_access == -1 else float(baseline_access_hops)/float(baseline_mem_access)}
                avg_access_hops_line.append(0 if baseline_access_hops == -1 or baseline_mem_access == 0 or baseline_mem_access == -1 else float(baseline_access_hops)/float(baseline_mem_access))
                total_hops_byte_map = {"Baseline":0 if baseline_access_hops == -1 else baseline_access_hops*byte_per_hop}
                total_traffic_line.append(0 if baseline_access_hops == -1 else baseline_access_hops*byte_per_hop)
                avg_traffic_line.append(0 if baseline_access_hops == -1 or baseline_cycle == 0 or baseline_cycle == -1 else float(baseline_access_hops*byte_per_hop)/float(baseline_cycle))
                cov_map = {"Baseline":0 if baseline_req_to_vault_cov == -1 else baseline_req_to_vault_cov}
                cov_line.append(0 if baseline_req_to_vault_cov == -1 else baseline_req_to_vault_cov)
                avg_hmc_latency_map = {"Baseline":0 if baseline_hmc_latency == -1 or baseline_mem_access == 0 or baseline_mem_access == -1 else float(baseline_hmc_latency)/float(baseline_mem_access)}
                avg_hmc_lat_line.append(0 if baseline_hmc_latency == -1 or baseline_mem_access == 0 or baseline_mem_access == -1 else float(baseline_hmc_latency)/float(baseline_mem_access))
                rq_pending_map = {"Baseline":0 if baseline_rq_pending == -1 else baseline_rq_pending}
                rq_pending_line.append(0 if baseline_rq_pending == -1 else baseline_rq_pending)
                wq_pending_map = {"Baseline":0 if baseline_wq_pending == -1 else baseline_wq_pending}
                wq_pending_line.append(0 if baseline_wq_pending == -1 else baseline_wq_pending)
                aq_pending_map = {"Baseline":0 if baseline_wq_pending == -1 or baseline_rq_pending == -1 else (baseline_rq_pending + baseline_wq_pending)}
                aq_pending_line.append(0 if baseline_wq_pending == -1 or baseline_rq_pending == -1 else (baseline_rq_pending + baseline_wq_pending))
                outgoing_q_pending_map = {"Baseline":0 if baseline_outgoing_q_pending == -1 else baseline_outgoing_q_pending}
                outgoing_q_pending_line.append(0 if baseline_outgoing_q_pending == -1 else baseline_outgoing_q_pending)
                avg_xfer_latency_line.append(0 if baseline_xfer_latency == -1 or baseline_mem_access == 0 or baseline_mem_access == -1 else float(baseline_xfer_latency)/float(baseline_mem_access))
                avg_qing_latency_line.append(0 if baseline_inc_qing_latency == -1 or baseline_out_qing_latency == -1 or baseline_mem_access == 0 or baseline_mem_access == -1 else float(baseline_inc_qing_latency+baseline_out_qing_latency)/float(baseline_mem_access))
                xfer_percentage_line.append(0 if baseline_xfer_latency == -1 or baseline_hmc_latency == 0 or baseline_hmc_latency == -1 else float(baseline_xfer_latency)/float(baseline_hmc_latency))
                qing_percentage_line.append(0 if baseline_inc_qing_latency == -1 or baseline_out_qing_latency == -1 or baseline_mem_access == 0 or baseline_mem_access == -1 else float(baseline_inc_qing_latency+baseline_out_qing_latency)/float(baseline_hmc_latency))

                for prefetcher_policy in prefetcher_policies:
                    processor_type = processor_type_prefix+prefetcher_type
                    stat_file_location = os.path.join(stats_folders, processor_type, prefetcher_policy+"_"+debug_tag, str(core_number), full_benchmark_name+".ramulator.cycle_stats")
                    sub_stat_file_location = os.path.join(stats_folders, processor_type, prefetcher_policy+"_"+debug_tag, str(core_number), full_benchmark_name+".ramulator.subscription_stats")
                    addr_dist_file_location = os.path.join(stats_folders, processor_type, prefetcher_policy+"_"+debug_tag, str(core_number), full_benchmark_name+".ramulator.address_distribution")
                    
                    cycle_val = -1
                    if not check_stat_file_exist(stat_file_location):
                        normalized_cycle_map[policy_name_to_label[prefetcher_policy]] = 0
                        normalized_cycle_line.append(0)
                    else:
                        cycle_val = extract_cycle(stat_file_location)
                        normalized_cycle_map[policy_name_to_label[prefetcher_policy]] = 0 if cycle_val == 0 or cycle_val == -1 or baseline_cycle == -1 else float(baseline_cycle)/float(cycle_val)
                        normalized_cycle_line.append(0 if cycle_val == 0 or cycle_val == -1 or baseline_cycle == -1 else float(baseline_cycle)/float(cycle_val))
                    
                    if not (check_stat_file_exist(sub_stat_file_location) and check_stat_file_exist(sub_stat_file_location+".end_of_warmup")):
                        avg_access_hops_map[policy_name_to_label[prefetcher_policy]] = 0
                        avg_access_hops_line.append(0)
                        total_hops_byte_map[policy_name_to_label[prefetcher_policy]] = 0
                        total_traffic_line.append(0)
                        avg_traffic_line.append(0)
                        avg_hmc_latency_map[policy_name_to_label[prefetcher_policy]] = 0
                        avg_hmc_lat_line.append(0)
                        rq_pending_map[policy_name_to_label[prefetcher_policy]] = 0
                        rq_pending_line.append(0)
                        wq_pending_map[policy_name_to_label[prefetcher_policy]] = 0
                        wq_pending_line.append(0)
                        aq_pending_map[policy_name_to_label[prefetcher_policy]] = 0
                        aq_pending_line.append(0)
                        outgoing_q_pending_map[policy_name_to_label[prefetcher_policy]] = 0
                        outgoing_q_pending_line.append(0)
                        avg_xfer_latency_line.append(0)
                        avg_qing_latency_line.append(0)
                        xfer_percentage_line.append(0)
                        qing_percentage_line.append(0)
                    else:
                        access_hops = extract_subscription_stats(sub_stat_file_location, "AccessPktHopsTravelled")
                        sub_hops = extract_subscription_stats(sub_stat_file_location, "SubscriptionPktHopsTravelled")
                        hmc_latency = extract_subscription_stats(sub_stat_file_location, "TotalHMCLatency") 
                        total_hops = -1 if sub_hops == -1 or access_hops == -1 else (access_hops+sub_hops)
                        mem_access = extract_subscription_stats(sub_stat_file_location, "MemAccesses")
                        rq_pending = extract_subscription_stats(sub_stat_file_location, "TotalReadQPending")
                        wq_pending = extract_subscription_stats(sub_stat_file_location, "TotalWriteQPending")
                        outgoing_q_pending = extract_subscription_stats(sub_stat_file_location, "TotalPendingQPending")
                        xfer_latency = extract_subscription_stats(sub_stat_file_location, "TotalTransferLatency")
                        inc_qing_latency = extract_subscription_stats(sub_stat_file_location, "TotalIncomingQueuingLatency")
                        out_qing_latency = extract_subscription_stats(sub_stat_file_location, "TotalOutgoingQueuingLatency")
                        avg_access_hops_map[policy_name_to_label[prefetcher_policy]] = 0 if access_hops == -1 or mem_access == 0 or mem_access == -1 else float(access_hops)/float(mem_access)
                        avg_access_hops_line.append(0 if access_hops == -1 or mem_access == 0 or mem_access == -1 else float(access_hops)/float(mem_access))
                        total_hops_byte_map[policy_name_to_label[prefetcher_policy]] = 0 if total_hops == -1 else total_hops*byte_per_hop
                        total_traffic_line.append(0 if total_hops == -1 else total_hops*byte_per_hop)
                        avg_traffic_line.append(0 if total_hops == -1 or cycle_val == 0 or cycle_val == -1 else float(total_hops*byte_per_hop)/float(cycle_val))
                        avg_hmc_latency_map[policy_name_to_label[prefetcher_policy]] = 0 if hmc_latency == -1 or mem_access == 0 or mem_access == -1 else float(hmc_latency)/float(mem_access)
                        avg_hmc_lat_line.append(0 if hmc_latency == -1 or mem_access == 0 or mem_access == -1 else float(hmc_latency)/float(mem_access))
                        rq_pending_map[policy_name_to_label[prefetcher_policy]] = 0 if rq_pending == -1 else rq_pending
                        rq_pending_line.append(0 if rq_pending == -1 else rq_pending)
                        wq_pending_map[policy_name_to_label[prefetcher_policy]] = 0 if wq_pending == -1 else wq_pending
                        wq_pending_line.append(0 if wq_pending == -1 else wq_pending)
                        aq_pending_map[policy_name_to_label[prefetcher_policy]] = 0 if wq_pending == -1 or rq_pending == -1 else (rq_pending + wq_pending)
                        aq_pending_line.append(0 if wq_pending == -1 or rq_pending == -1 else (rq_pending + wq_pending))
                        outgoing_q_pending_map[policy_name_to_label[prefetcher_policy]] = 0 if outgoing_q_pending == -1 else outgoing_q_pending
                        outgoing_q_pending_line.append(0 if outgoing_q_pending == -1 else outgoing_q_pending)
                        avg_xfer_latency_line.append(0 if xfer_latency == -1 or mem_access == 0 or mem_access == -1 else float(xfer_latency)/float(mem_access))
                        avg_qing_latency_line.append(0 if inc_qing_latency == -1 or out_qing_latency == -1 or mem_access == 0 or mem_access == -1 else float(inc_qing_latency+out_qing_latency)/float(mem_access))
                        xfer_percentage_line.append(0 if xfer_latency == -1 or hmc_latency == 0 or hmc_latency == -1 else float(xfer_latency)/float(hmc_latency))
                        qing_percentage_line.append(0 if inc_qing_latency == -1 or out_qing_latency == -1 or hmc_latency == 0 or hmc_latency == -1 else float(inc_qing_latency+out_qing_latency)/float(hmc_latency))
                    
                    if not (check_stat_file_exist(addr_dist_file_location) and check_stat_file_exist(addr_dist_file_location+".end_of_warmup")):
                        cov_map[policy_name_to_label[prefetcher_policy]] = 0
                        cov_line.append(0)
                    else:
                        req_to_vault_cov = calculate_cov_to_vault(addr_dist_file_location, core_number)
                        cov_map[policy_name_to_label[prefetcher_policy]] = req_to_vault_cov
                        cov_line.append(req_to_vault_cov)
                
                write_stats_output_files(per_bm_dir, prefetcher_type+"_"+str(hops_threshold)+"hops_normalized_cycle", benchmark_tag+" Normalized Cycles", "Prefetch Policies", "Cycle", normalized_cycle_map)
                write_stats_output_files(per_bm_dir, prefetcher_type+"_"+str(hops_threshold)+"hops_avg_access_hops", benchmark_tag+" Average Access Hops", "Prefetch Policies", "Hops", avg_access_hops_map)
                write_stats_output_files(per_bm_dir, prefetcher_type+"_"+str(hops_threshold)+"hops_total_hops_bytes", benchmark_tag+" Total Traffic", "Prefetch Policies", "Total Traffic (bytes)", total_hops_byte_map)
                write_stats_output_files(per_bm_dir, prefetcher_type+"_"+str(hops_threshold)+"hops_access_distro_cov", benchmark_tag+" Access Distribution", "Prefetch Policies", "Coefficient of Variation", cov_map)
                write_stats_output_files(per_bm_dir, prefetcher_type+"_"+str(hops_threshold)+"hops_avg_hmc_latency", benchmark_tag+" Average HMC Latency", "Prefetch Policies", "Latency (Cycle)", avg_hmc_latency_map)

                selected_normalized_cycle_csv_write.writerow(normalized_cycle_line)
                selected_avg_access_hops_csv_write.writerow(avg_access_hops_line)
                selected_total_traffic_csv_write.writerow(total_traffic_line)
                selected_avg_traffic_csv_write.writerow(avg_traffic_line)
                selected_cov_csv_write.writerow(cov_line)
                selected_avg_hmc_lat_csv_write.writerow(avg_hmc_lat_line)
                selected_total_rq_pending_csv_write.writerow(rq_pending_line)
                selected_total_wq_pending_csv_write.writerow(wq_pending_line)
                selected_total_aq_pending_csv_write.writerow(aq_pending_line)
                selected_total_outgoing_q_pending_csv_write.writerow(outgoing_q_pending_line)
                selected_avg_xfer_latency_csv_write.writerow(avg_xfer_latency_line)
                selected_avg_qing_latency_csv_write.writerow(avg_qing_latency_line)
                selected_xfer_percentage_csv_write.writerow(xfer_percentage_line)
                selected_qing_percentage_csv_write.writerow(qing_percentage_line)
        


selected_normalized_cycle_csv_file.close()
selected_avg_access_hops_csv_file.close()
selected_total_traffic_csv_file.close()
selected_avg_traffic_csv_file.close()
selected_cov_csv_file.close()
selected_avg_hmc_lat_csv_file.close()
selected_total_rq_pending_csv_file.close()
selected_total_wq_pending_csv_file.close()
selected_total_aq_pending_csv_file.close()
selected_total_outgoing_q_pending_csv_file.close()
selected_avg_xfer_latency_csv_file.close()
selected_avg_qing_latency_csv_file.close()
selected_xfer_percentage_csv_file.close()
selected_qing_percentage_csv_file.close()