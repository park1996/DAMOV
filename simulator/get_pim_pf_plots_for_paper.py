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

hops_thresholds = get_hops_thresholds()
# count_thresholds = get_count_thresholds()
count_thresholds = [0, 63]
hops_thresholds_str = [""]
count_thresholds_str = [""]
debug_tag = "debugoff"

def extract_cycle(stat_file_location):
    cycle_val = -1
    with open(stat_file_location, mode='r') as stat:
        for line in stat:
            if(line.find("Simulated unhalted cycles")!= -1):
                current_cycle = int(line.split()[1])
                if(current_cycle > cycle_val):
                    cycle_val = current_cycle
    return cycle_val

def extract_l1_miss_rate(stat_file_location):
    l1d = False
    l2d = False
    l3d = False
    l1_hits = 0
    l1_misses = 0
    l1_miss_rate = 0
    with open(stat_file_location, mode='r') as stat:
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
    try:
        l1_miss_rate = (l1_misses/float((l1_misses+l1_hits)))*100.0
    except:
        l1_miss_rate = -1.0
    return l1_miss_rate

def extract_subscription_stats(stat_file_location, stat_name):
    value = -1
    with open(stat_file_location, mode='r') as stat:
        for line in stat:
            if(line.find(stat_name)!= -1):
                value = int(line.split()[1])
    return value

def extract_per_vault_count(stat_file_location, label):
    core_number = 32
    result = [0]*core_number
    with open(stat_file_location, "r") as csv_file:
        csv_reader = csv.DictReader(csv_file, delimiter=' ')
        for row in csv_reader:
            result[int(row[label])] += int(row["#Requests"])
    return result

def extract_from_vault_count(stat_file_location):
    return extract_per_vault_count(stat_file_location, "CoreID")

def extract_to_vault_count(stat_file_location):
    return extract_per_vault_count(stat_file_location, "VaultID")

def mkdir_p(directory):
    try:
        os.makedirs(directory)
    except OSError as exc:  # Python >2.5
        if exc.errno == errno.EEXIST and os.path.isdir(directory):
            pass
        else:
            raise

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

processor_type_prefix = "pim_prefetch_netoh_"
baseline_processor_type = "pim_ooo_netoh"
prefetcher_types = ["allocate"]
core_number = "32"
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
for suite in benchmark_suites_and_benchmarks_functions.keys():
    for benchmark_function in benchmark_suites_and_benchmarks_functions[suite]:
        full_benchmark_name = suite+"_"+benchmark_function
        benchmark_key = benchmark_suites_and_benchmarks_functions_to_short_name[suite][benchmark_function]
        baseline_stat_file_location = os.path.join(stats_folders, baseline_processor_type, core_number, full_benchmark_name+".zsim.out")
        baseline_cycle = -1
        if os.path.isfile(baseline_stat_file_location):
            baseline_cycle = extract_cycle(baseline_stat_file_location)
        baseline_cycles[benchmark_key] = 0 if baseline_cycle == -1 else baseline_cycle
        baseline_sub_stat_file_location = os.path.join(stats_folders, baseline_processor_type, core_number, full_benchmark_name+".ramulator.subscription_stats")
        baseline_access_hops = -1
        baseline_mem_access = -1
        baseline_hmc_latency = -1
        if os.path.isfile(baseline_sub_stat_file_location):
            baseline_access_hops = extract_subscription_stats(baseline_sub_stat_file_location, "AccessPktHopsTravelled")
            baseline_mem_access = extract_subscription_stats(baseline_sub_stat_file_location, "MemAccesses")
            baseline_hmc_latency = extract_subscription_stats(baseline_sub_stat_file_location, "TotalHMCLatency")
        baseline_avg_hops[benchmark_key] = 0 if baseline_access_hops == -1 or baseline_mem_access == -1 or baseline_mem_access == 0 else float(baseline_access_hops)/float(baseline_mem_access)
        baseline_avg_hmc_latency[benchmark_key] = 0 if baseline_hmc_latency == -1 or baseline_mem_access == -1 or baseline_mem_access == 0 else float(baseline_hmc_latency)/float(baseline_mem_access)
baseline_cycle_image_output_dir = os.path.join(output_dir,"baseline_cycle.png")
baseline_cycle_csv_output_dir = baseline_cycle_image_output_dir.replace(".png", ".csv")
baseline_cycle_x_lengend = "Workload"
baseline_cycle_y_lengend = "Cycle"
baseline_cycle_title = "Baseline Cycle by Workload"
plt.legend()
plt.bar(baseline_cycles.keys(), baseline_cycles.values(), color ='maroon',
        width = 0.5)
plt.xticks(rotation=90)
plt.xlabel(baseline_cycle_x_lengend)
plt.ylabel(baseline_cycle_y_lengend)
plt.title(baseline_cycle_title)
# plt.savefig(baseline_cycle_image_output_dir)
plt.clf()
with open(baseline_cycle_csv_output_dir, "w") as csv_file:
    csv_writer = csv.writer(csv_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
    csv_writer.writerow([baseline_cycle_title])
    csv_writer.writerow([baseline_cycle_x_lengend, baseline_cycle_y_lengend])
    for key in baseline_cycles.keys():
        csv_writer.writerow([str(key).replace("\n", " "), str(baseline_cycles[key])])

baseline_avg_hops_image_output_dir = os.path.join(output_dir,"baseline_avg_hops.png")
baseline_avg_hops_csv_output_dir = baseline_avg_hops_image_output_dir.replace(".png", ".csv")
baseline_avg_hops_x_lengend = "Workload"
baseline_avg_hops_y_lengend = "Hops"
baseline_avg_hops_title = "Baseline Average Hops by Workload"
plt.legend()
plt.bar(baseline_avg_hops.keys(), baseline_avg_hops.values(), color ='maroon',
        width = 0.5)
plt.xticks(rotation=90)
plt.xlabel(baseline_avg_hops_x_lengend)
plt.ylabel(baseline_avg_hops_y_lengend)
plt.title(baseline_avg_hops_title)
# plt.savefig(baseline_avg_hops_image_output_dir)
plt.clf()
with open(baseline_avg_hops_csv_output_dir, "w") as csv_file:
    csv_writer = csv.writer(csv_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
    csv_writer.writerow([baseline_avg_hops_title])
    csv_writer.writerow([baseline_avg_hops_x_lengend, baseline_avg_hops_y_lengend])
    for key in baseline_avg_hops.keys():
        csv_writer.writerow([str(key).replace("\n", " "), str(baseline_avg_hops[key])])

baseline_avg_hmc_latency_image_output_dir = os.path.join(output_dir,"baseline_avg_hmc_latency.png")
baseline_avg_hmc_latency_csv_output_dir = baseline_avg_hmc_latency_image_output_dir.replace(".png", ".csv")
baseline_avg_hmc_latency_x_lengend = "Workload"
baseline_avg_hmc_latency_y_lengend = "Latency (Cycle)"
baseline_avg_hmc_latency_title = "Baseline Average HMC Latency by Workload"
plt.legend()
plt.bar(baseline_avg_hmc_latency.keys(), baseline_avg_hmc_latency.values(), color ='maroon',
        width = 0.5)
plt.xticks(rotation=90)
plt.xlabel(baseline_avg_hmc_latency_x_lengend)
plt.ylabel(baseline_avg_hmc_latency_y_lengend)
plt.title(baseline_avg_hmc_latency_title)
# plt.savefig(baseline_avg_hmc_latency_image_output_dir)
plt.clf()
with open(baseline_avg_hmc_latency_csv_output_dir, "w") as csv_file:
    csv_writer = csv.writer(csv_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
    csv_writer.writerow([baseline_avg_hmc_latency_title])
    csv_writer.writerow([baseline_avg_hmc_latency_x_lengend, baseline_avg_hmc_latency_y_lengend])
    for key in baseline_avg_hmc_latency.keys():
        csv_writer.writerow([str(key).replace("\n", " "), str(baseline_avg_hmc_latency[key])])

plt.subplots_adjust(bottom=0.1)

# The following are benchmarks that impacted by our model
selected_benchmark_suites_and_benchmarks_functions = {"chai" : ["BS_BEZIER_KERNEL"],
    "darknet" : ["yolo_gemm_nn"],
    "hpcg" : ["HPCG_ComputePrologation", "HPCG_ComputeRestriction"],
    "phoenix" : ["Linearregression_main"],
    "polybench" : ["linear-algebra_3mm", "linear-algebra_doitgen", "linear-algebra_gemm", "linear-algebra_gemver"], 
    "splash-2" : ["FFT_Reverse", "FFT_Transpose", "Radix_slave_sort"]}

selected_normalized_cycle_csv_output_dir = os.path.join(output_dir,"selected_bm_normalized_cycle.csv")
selected_avg_access_hops_csv_output_dir = os.path.join(output_dir,"selected_bm_avg_access_hops.csv")
selected_total_traffic_csv_output_dir = os.path.join(output_dir,"selected_bm_total_traffic.csv")
selected_cov_csv_output_dir = os.path.join(output_dir,"selected_bm_cov.csv")
selected_avg_hmc_lat_csv_output_dir = os.path.join(output_dir,"selected_bm_avg_hmc_lat.csv")

selected_normalized_cycle_csv_file = open(selected_normalized_cycle_csv_output_dir, "w")
selected_avg_access_hops_csv_file = open(selected_avg_access_hops_csv_output_dir, "w")
selected_total_traffic_csv_file = open(selected_total_traffic_csv_output_dir, "w")
selected_cov_csv_file = open(selected_cov_csv_output_dir, "w")
selected_avg_hmc_lat_csv_file = open(selected_avg_hmc_lat_csv_output_dir, "w")

selected_normalized_cycle_csv_write = csv.writer(selected_normalized_cycle_csv_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
selected_avg_access_hops_csv_write = csv.writer(selected_avg_access_hops_csv_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
selected_total_traffic_csv_write =csv.writer(selected_total_traffic_csv_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
selected_cov_csv_write = csv.writer(selected_cov_csv_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
selected_avg_hmc_lat_csv_write = csv.writer(selected_avg_hmc_lat_csv_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)

for prefetcher_type in prefetcher_types:
    for hops_threshold in hops_thresholds:
        selected_normalized_cycle_csv_write.writerow(["Normalized Cycle for "+prefetcher_type+" "+str(hops_threshold)+" Hops"])
        selected_avg_access_hops_csv_write.writerow(["Average Access Hops for "+prefetcher_type+" "+str(hops_threshold)+" Hops"])
        selected_total_traffic_csv_write.writerow(["Total Traffic for "+prefetcher_type+" "+str(hops_threshold)+" Hops"])
        selected_cov_csv_write.writerow(["Access Distribution for "+prefetcher_type+" "+str(hops_threshold)+" Hops"])
        selected_avg_hmc_lat_csv_write.writerow(["Average HMC Latency for "+prefetcher_type+" "+str(hops_threshold)+" Hops"])
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
        selected_cov_csv_write.writerow(csv_header)
        selected_avg_hmc_lat_csv_write.writerow(csv_header)
        for suite in selected_benchmark_suites_and_benchmarks_functions.keys():
            for benchmark_function in selected_benchmark_suites_and_benchmarks_functions[suite]:
                full_benchmark_name = suite+"_"+benchmark_function
                benchmark_tag = benchmark_suites_and_benchmarks_functions_to_short_name[suite][benchmark_function]
                normalized_cycle_line = [benchmark_tag]
                avg_access_hops_line = [benchmark_tag]
                total_traffic_line = [benchmark_tag]
                cov_line = [benchmark_tag]
                avg_hmc_lat_line = [benchmark_tag]
                per_bm_dir = os.path.join(output_dir,full_benchmark_name)
                mkdir_p(per_bm_dir)
                baseline_cycle = -1
                baseline_stat_file_location = os.path.join(stats_folders, baseline_processor_type, core_number, full_benchmark_name+".zsim.out")
                if os.path.isfile(baseline_stat_file_location):
                    baseline_cycle = extract_cycle(baseline_stat_file_location)
                baseline_access_hops = -1
                baseline_mem_access = -1
                baseline_hmc_latency = -1
                baseline_sub_stat_file_location = os.path.join(stats_folders, baseline_processor_type, core_number, full_benchmark_name+".ramulator.subscription_stats")
                if os.path.isfile(baseline_stat_file_location):
                    baseline_access_hops = extract_subscription_stats(baseline_sub_stat_file_location, "AccessPktHopsTravelled")
                    baseline_mem_access = extract_subscription_stats(baseline_sub_stat_file_location, "MemAccesses")
                    baseline_hmc_latency = extract_subscription_stats(baseline_sub_stat_file_location, "TotalHMCLatency")   
                baseline_req_to_vault_mean = -1
                baseline_req_to_vault_std = -1
                baseline_req_to_vault_cov = -1
                baseline_addr_dist_file_location = os.path.join(stats_folders, baseline_processor_type, core_number, full_benchmark_name+".ramulator.address_distribution")
                if os.path.isfile(baseline_addr_dist_file_location):
                    baseline_accesses_to_vaults = extract_to_vault_count(baseline_addr_dist_file_location)
                    baseline_req_to_vault_mean = numpy.mean(baseline_accesses_to_vaults)
                    baseline_req_to_vault_std = numpy.std(baseline_accesses_to_vaults)
                    baseline_req_to_vault_cov = baseline_req_to_vault_std / baseline_req_to_vault_mean
                normalized_cycle_map = {"Baseline":0 if baseline_cycle == 0 or baseline_cycle == -1 else 1}
                normalized_cycle_line.append(0 if baseline_cycle == 0 or baseline_cycle == -1 else 1)
                avg_access_hops_map = {"Baseline":0 if baseline_access_hops == -1 or baseline_mem_access == 0 or baseline_mem_access == -1 else float(baseline_access_hops)/float(baseline_mem_access)}
                avg_access_hops_line.append(0 if baseline_access_hops == -1 or baseline_mem_access == 0 or baseline_mem_access == -1 else float(baseline_access_hops)/float(baseline_mem_access))
                total_hops_byte_map = {"Baseline":0 if baseline_access_hops == -1 else baseline_access_hops*byte_per_hop}
                total_traffic_line.append(0 if baseline_access_hops == -1 else baseline_access_hops*byte_per_hop)
                cov_map = {"Baseline":0 if baseline_req_to_vault_cov == -1 else baseline_req_to_vault_cov}
                cov_line.append(0 if baseline_req_to_vault_cov == -1 else baseline_req_to_vault_cov)
                avg_hmc_latency_map = {"Baseline":0 if baseline_hmc_latency == -1 or baseline_mem_access == 0 or baseline_mem_access == -1 else float(baseline_hmc_latency)/float(baseline_mem_access)}
                avg_hmc_lat_line.append(0 if baseline_hmc_latency == -1 or baseline_mem_access == 0 or baseline_mem_access == -1 else float(baseline_hmc_latency)/float(baseline_mem_access))
                for prefetcher_policy in prefetcher_policies:
                    processor_type = processor_type_prefix+prefetcher_type
                    stat_file_location = os.path.join(stats_folders, processor_type, prefetcher_policy+"_"+debug_tag, core_number, full_benchmark_name+".zsim.out")
                    sub_stat_file_location = os.path.join(stats_folders, processor_type, prefetcher_policy+"_"+debug_tag, core_number, full_benchmark_name+".ramulator.subscription_stats")
                    addr_dist_file_location = os.path.join(stats_folders, processor_type, prefetcher_policy+"_"+debug_tag, core_number, full_benchmark_name+".ramulator.address_distribution")
                    if not os.path.isfile(stat_file_location):
                        normalized_cycle_map[policy_name_to_label[prefetcher_policy]] = 0
                        normalized_cycle_line.append(0)
                    else:
                        cycle_val = extract_cycle(stat_file_location)
                        normalized_cycle_map[policy_name_to_label[prefetcher_policy]] = 0 if cycle_val == 0 or cycle_val == -1 or baseline_cycle == -1 else float(baseline_cycle)/float(cycle_val)
                        normalized_cycle_line.append(0 if cycle_val == 0 or cycle_val == -1 or baseline_cycle == -1 else float(baseline_cycle)/float(cycle_val))
                    if not os.path.isfile(sub_stat_file_location):
                        avg_access_hops_map[policy_name_to_label[prefetcher_policy]] = 0
                        avg_access_hops_line.append(0)
                        total_hops_byte_map[policy_name_to_label[prefetcher_policy]] = 0
                        total_traffic_line.append(0)
                        avg_hmc_latency_map[policy_name_to_label[prefetcher_policy]] = 0
                        avg_hmc_lat_line.append(0)
                    else:
                        access_hops = extract_subscription_stats(sub_stat_file_location, "AccessPktHopsTravelled")
                        sub_hops = extract_subscription_stats(sub_stat_file_location, "SubscriptionPktHopsTravelled")
                        hmc_latency = extract_subscription_stats(sub_stat_file_location, "TotalHMCLatency") 
                        total_hops = -1 if sub_hops == -1 or access_hops == -1 else (access_hops+sub_hops)
                        mem_access = extract_subscription_stats(sub_stat_file_location, "MemAccesses")
                        avg_access_hops_map[policy_name_to_label[prefetcher_policy]] = 0 if access_hops == -1 or mem_access == 0 or mem_access == -1 else float(access_hops)/float(mem_access)
                        avg_access_hops_line.append(0 if access_hops == -1 or mem_access == 0 or mem_access == -1 else float(access_hops)/float(mem_access))
                        total_hops_byte_map[policy_name_to_label[prefetcher_policy]] = 0 if total_hops == -1 else total_hops*byte_per_hop
                        total_traffic_line.append(0 if total_hops == -1 else total_hops*byte_per_hop)
                        avg_hmc_latency_map[policy_name_to_label[prefetcher_policy]] = 0 if hmc_latency == -1 or mem_access == 0 or mem_access == -1 else float(hmc_latency)/float(mem_access)
                        avg_hmc_lat_line.append(0 if hmc_latency == -1 or mem_access == 0 or mem_access == -1 else float(hmc_latency)/float(mem_access))
                    if not os.path.isfile(addr_dist_file_location):
                        cov_map[policy_name_to_label[prefetcher_policy]] = 0
                        cov_line.append(0)
                    else:
                        accesses_to_vaults = extract_to_vault_count(addr_dist_file_location)
                        req_to_vault_mean = numpy.mean(accesses_to_vaults)
                        req_to_vault_std = numpy.std(accesses_to_vaults)
                        req_to_vault_cov = req_to_vault_std / req_to_vault_mean
                        cov_map[policy_name_to_label[prefetcher_policy]] = req_to_vault_cov
                        cov_line.append(req_to_vault_cov)
                normalized_cycle_img_output_dir = os.path.join(per_bm_dir, prefetcher_type+"_"+str(hops_threshold)+"hops_normalized_cycle.png")
                normalized_cycle_csv_output_dir = normalized_cycle_img_output_dir.replace(".png", ".csv")
                normalized_cycle_x_legend = "Prefetch Policies"
                normalized_cycle_y_legend = "Cycle"
                normalized_cycle_title = benchmark_tag+" Normalized Cycles"
                plt.legend()
                plt.bar(normalized_cycle_map.keys(), normalized_cycle_map.values(), color ='maroon',
                        width = 0.4)
                plt.xlabel(normalized_cycle_x_legend)
                plt.title(normalized_cycle_title)
                # plt.savefig(normalized_cycle_img_output_dir)
                plt.clf()
                with open(normalized_cycle_csv_output_dir, "w") as csv_file:
                    csv_writer = csv.writer(csv_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
                    csv_writer.writerow([normalized_cycle_title])
                    csv_writer.writerow([normalized_cycle_x_legend, normalized_cycle_y_legend])
                    for key in normalized_cycle_map.keys():
                        csv_writer.writerow([str(key).replace("\n", " "), str(normalized_cycle_map[key])])

                avg_access_hops_img_output_dir = os.path.join(per_bm_dir, prefetcher_type+"_"+str(hops_threshold)+"hops_avg_access_hops.png")
                avg_access_hops_csv_output_dir = avg_access_hops_img_output_dir.replace(".png", ".csv")
                avg_access_hops_x_lengend = "Prefetch Policies"
                avg_access_hops_y_lengend = "Hops"
                avg_access_hops_title = benchmark_tag+" Average Access Hops"
                plt.bar(avg_access_hops_map.keys(), avg_access_hops_map.values(), color ='maroon',
                        width = 0.4)
                plt.xlabel(avg_access_hops_x_lengend)
                plt.title(avg_access_hops_title)
                # plt.savefig(avg_access_hops_img_output_dir)
                plt.clf()
                with open(avg_access_hops_csv_output_dir, "w") as csv_file:
                    csv_writer = csv.writer(csv_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
                    csv_writer.writerow([avg_access_hops_title])
                    csv_writer.writerow([avg_access_hops_x_lengend, avg_access_hops_y_lengend])
                    for key in avg_access_hops_map.keys():
                        csv_writer.writerow([str(key).replace("\n", " "), str(avg_access_hops_map[key])])

                total_hops_byte_img_output_dir = os.path.join(per_bm_dir, prefetcher_type+"_"+str(hops_threshold)+"hops_total_hops_bytes.png")
                total_hops_byte_csv_output_dir = total_hops_byte_img_output_dir.replace(".png", ".csv")
                total_hops_byte_x_lengend = "Prefetch Policies"
                total_hops_byte_y_lengend = "Total Traffic (bytes)"
                total_hops_byte_title = benchmark_tag+" Total Traffic"
                plt.bar(total_hops_byte_map.keys(), total_hops_byte_map.values(), color ='maroon',
                        width = 0.4)
                plt.xlabel(total_hops_byte_x_lengend)
                plt.ylabel(total_hops_byte_y_lengend)
                plt.title(total_hops_byte_title)
                # plt.savefig(total_hops_byte_img_output_dir)
                plt.clf()
                with open(total_hops_byte_csv_output_dir, "w") as csv_file:
                    csv_writer = csv.writer(csv_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
                    csv_writer.writerow([total_hops_byte_title])
                    csv_writer.writerow([total_hops_byte_x_lengend, total_hops_byte_y_lengend])
                    for key in total_hops_byte_map.keys():
                        csv_writer.writerow([str(key).replace("\n", " "), str(total_hops_byte_map[key])])

                cov_img_output_dir = os.path.join(per_bm_dir, prefetcher_type+"_"+str(hops_threshold)+"hops_access_distro_cov.png")
                cov_csv_output_dir = cov_img_output_dir.replace(".png", ".csv")
                cov_x_lengend = "Prefetch Policies"
                cov_y_lengend = "Coefficient of Variation"
                cov_title = benchmark_tag+" Access Distribution"
                plt.bar(cov_map.keys(), cov_map.values(), color ='maroon',
                        width = 0.4)
                plt.xlabel(cov_x_lengend)
                plt.ylabel(cov_y_lengend)
                plt.title(cov_title)
                # plt.savefig(cov_img_output_dir)
                plt.clf()
                with open(cov_csv_output_dir, "w") as csv_file:
                    csv_writer = csv.writer(csv_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
                    csv_writer.writerow([cov_title])
                    csv_writer.writerow([cov_x_lengend, cov_y_lengend])
                    for key in cov_map.keys():
                        csv_writer.writerow([str(key).replace("\n", " "), str(cov_map[key])])

                avg_hmc_latency_img_output_dir = os.path.join(per_bm_dir, prefetcher_type+"_"+str(hops_threshold)+"hops_avg_hmc_latency.png")
                avg_hmc_latency_csv_output_dir = avg_hmc_latency_img_output_dir.replace(".png", ".csv")
                avg_hmc_latency_x_lengend = "Prefetch Policies"
                avg_hmc_latency_y_lengend = "Latency (Cycle)"
                avg_hmc_latency_title = benchmark_tag+" Average HMC Latency"
                plt.bar(avg_hmc_latency_map.keys(), avg_hmc_latency_map.values(), color ='maroon',
                        width = 0.4)
                plt.xlabel(avg_hmc_latency_x_lengend)
                plt.ylabel(avg_hmc_latency_y_lengend)
                plt.title(avg_hmc_latency_title)
                # plt.savefig(avg_hmc_latency_img_output_dir)
                plt.clf()
                with open(avg_hmc_latency_csv_output_dir, "w") as csv_file:
                    csv_writer = csv.writer(csv_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
                    csv_writer.writerow([avg_hmc_latency_title])
                    csv_writer.writerow([avg_hmc_latency_x_lengend, avg_hmc_latency_y_lengend])
                    for key in avg_hmc_latency_map.keys():
                        csv_writer.writerow([str(key).replace("\n", " "), str(avg_hmc_latency_map[key])])
                
                selected_normalized_cycle_csv_write.writerow(normalized_cycle_line)
                selected_avg_access_hops_csv_write.writerow(avg_access_hops_line)
                selected_total_traffic_csv_write.writerow(total_traffic_line)
                selected_cov_csv_write.writerow(cov_line)
                selected_avg_hmc_lat_csv_write.writerow(avg_hmc_lat_line)


selected_normalized_cycle_csv_file.close()
selected_avg_access_hops_csv_file.close()
selected_total_traffic_csv_file.close()
selected_cov_csv_file.close()
selected_avg_hmc_lat_csv_file.close()