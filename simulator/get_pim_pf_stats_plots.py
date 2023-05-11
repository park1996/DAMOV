# -*- coding: utf-8 -*-
import os
import csv
from datetime import datetime
import matplotlib
# Use non X-Window engine as we are executing it in terminal
matplotlib.use('Agg')
from matplotlib import pyplot as plt
from batch_prefetcher_generator import get_hops_thresholds, get_count_thresholds, get_core_numbers
import numpy

hops_thresholds = get_hops_thresholds()
# count_thresholds = get_count_thresholds()
count_thresholds = [0, 1, 63]
hops_thresholds_str = [""]
count_thresholds_str = [""]
debug_tag = "debugoff"

def mkdir_p(directory):
    try:
        os.makedirs(directory)
    except OSError as exc:  # Python >2.5
        if exc.errno == errno.EEXIST and os.path.isdir(directory):
            pass
        else:
            raise

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

def extract_per_vault_count(stat_file_location, core_number, label):
    result = [0]*core_number
    with open(stat_file_location, "r") as csv_file:
        csv_reader = csv.DictReader(csv_file, delimiter=' ')
        for row in csv_reader:
            result[int(row[label])] += int(row["#Requests"])
    return result

def extract_from_vault_count(stat_file_location, core_number):
    return extract_per_vault_count(stat_file_location, core_number, "CoreID")

def extract_to_vault_count(stat_file_location, core_number):
    return extract_per_vault_count(stat_file_location, core_number, "VaultID")

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
# The following are benchmarks that impacted by our model
# benchmark_suites_and_benchmarks_functions = {"chai" : ["BS_BEZIER_KERNEL", "HSTO_HSTO", "OOPPAD_OOPPAD"],
#     "darknet" : ["yolo_gemm_nn"],
#     "hpcg" : ["HPCG_ComputePrologation", "HPCG_ComputeRestriction", "HPCG_ComputeSPMV", "HPCG_ComputeSYMGS"],
#     "ligra" : ["BC_edgeMapSparseUSAUserAdded", "BFS_edgeMapSparseUSAUserAdded"],
#     "phoenix" : ["Linearregression_main"],
#     "polybench" : ["linear-algebra_3mm", "linear-algebra_doitgen", "linear-algebra_gemm", "linear-algebra_gramschmidt", "linear-algebra_gemver", "linear-algebra_symm", "stencil_fdtd-apml"], 
#     "rodinia" : ["BFS_BFS", "NW_UserAdded"],
#     "splash-2" : ["FFT_Reverse", "FFT_Transpose", "Oceanncp_laplaccalc", "Radix_slave_sort"], 
#     "stream" : ["Triad_Triad"]}

processor_type_prefix = "pim_prefetch_netoh_"
baseline_processor_type = "pim_ooo_netoh"
prefetcher_types = ["allocate"]
core_numbers = get_core_numbers()
# core_number = "32"

plt.figure(figsize=(10.5,6))
plt.subplots_adjust(bottom=0.3, left=0.1, right=0.95, top=0.95)

output_dir_name = "stats_plots_"+datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
output_dir = os.path.join(os.getcwd(), output_dir_name)
mkdir_p(output_dir)
normalized_cycles_output_dir = os.path.join(output_dir, "normalized_cycles")
mkdir_p(normalized_cycles_output_dir)
normalized_access_hops_output_dir = os.path.join(output_dir, "normalized_access_hops")
mkdir_p(normalized_access_hops_output_dir)
normalized_total_hops_output_dir = os.path.join(output_dir, "normalized_total_hops")
mkdir_p(normalized_total_hops_output_dir)
coefficient_of_variation_output_dir = os.path.join(output_dir, "coefficient_of_variation")
mkdir_p(coefficient_of_variation_output_dir)
avg_hmc_latency_output_dir = os.path.join(output_dir, "avg_hmc_latency")
mkdir_p(avg_hmc_latency_output_dir)
successful_subscriptions_output_dir = os.path.join(output_dir, "successful_subscriptions")
mkdir_p(successful_subscriptions_output_dir)

stats_folders = os.path.join(os.getcwd(), "zsim_stats")

for suite in benchmark_suites_and_benchmarks_functions.keys():
    for benchmark_function in benchmark_suites_and_benchmarks_functions[suite]:
        full_benchmark_name = suite+"_"+benchmark_function
        x_configs = []
        normalized_cycle_map = {}
        normalized_access_hops_map = {}
        normalized_total_hops_map = {}
        coefficient_of_variation_map = {}
        avg_hmc_latency_map = {}
        successful_subscriptions_map = {}
        for core_number in core_numbers:
            baseline_cycle = -1
            baseline_l1_cache_miss_rate = -1
            baseline_stat_file_location = os.path.join(stats_folders, baseline_processor_type, str(core_number), full_benchmark_name+".zsim.out")
            if os.path.isfile(baseline_stat_file_location):
                baseline_cycle = extract_cycle(baseline_stat_file_location)
                baseline_l1_cache_miss_rate = extract_l1_miss_rate(baseline_stat_file_location)
            baseline_access_hops = -1
            baseline_mem_access = -1
            baseline_req_latency = -1
            baseline_hmc_latency = -1
            baseline_readq_pending = -1
            baseline_writeq_pending = -1
            baseline_sub_stat_file_location = os.path.join(stats_folders, baseline_processor_type, str(core_number), full_benchmark_name+".ramulator.subscription_stats")
            if os.path.isfile(baseline_sub_stat_file_location):
                baseline_access_hops = extract_subscription_stats(baseline_sub_stat_file_location, "AccessPktHopsTravelled")
                baseline_mem_access = extract_subscription_stats(baseline_sub_stat_file_location, "MemAccesses")
                baseline_req_latency = extract_subscription_stats(baseline_sub_stat_file_location, "TotalRequestLatency")
                baseline_hmc_latency = extract_subscription_stats(baseline_sub_stat_file_location, "TotalHMCLatency")
                baseline_readq_pending = extract_subscription_stats(baseline_sub_stat_file_location, "TotalReadQPending")
                baseline_writeq_pending = extract_subscription_stats(baseline_sub_stat_file_location, "TotalWriteQPending")   
            baseline_req_to_vault_mean = -1
            baseline_req_to_vault_std = -1
            baseline_req_to_vault_cov = -1
            baseline_addr_dist_file_location = os.path.join(stats_folders, baseline_processor_type, str(core_number), full_benchmark_name+".ramulator.address_distribution")
            if os.path.isfile(baseline_addr_dist_file_location):
                baseline_accesses_to_vaults = extract_to_vault_count(baseline_addr_dist_file_location, core_number)
                baseline_req_to_vault_mean = numpy.mean(baseline_accesses_to_vaults)
                baseline_req_to_vault_std = numpy.std(baseline_accesses_to_vaults)
                baseline_req_to_vault_cov = baseline_req_to_vault_std / baseline_req_to_vault_mean
            for prefetcher_type in prefetcher_types:
                for hops_threshold in hops_thresholds:
                    prefetcher_policies = []
                    # prefetcher_policies.append("adaptive")
                    config_tag = "baseline "+str(core_number)+" core"
                    x_configs.append(config_tag)
                    for count_threshold in count_thresholds:
                        prefetcher_policies.append(str(hops_threshold)+"h"+str(count_threshold)+"c")
                    normalized_cycle_map[config_tag] = 0 if baseline_cycle == -1 else 1
                    normalized_access_hops_map[config_tag] = 0 if baseline_access_hops == -1 else 1
                    normalized_total_hops_map[config_tag] = 0 if baseline_access_hops == -1 else 1
                    coefficient_of_variation_map[config_tag] = 0 if baseline_req_to_vault_cov == -1 else baseline_req_to_vault_cov
                    avg_hmc_latency_map[config_tag] = float(0) if baseline_hmc_latency == -1 or baseline_mem_access == -1 or baseline_mem_access == 0 else float(baseline_hmc_latency)/float(baseline_mem_access)
                    successful_subscriptions_map[config_tag] = 0
                    for prefetcher_policy in prefetcher_policies:
                        config_tag = prefetcher_policy+" "+str(core_number)+" core"
                        x_configs.append(config_tag)
                        processor_type = processor_type_prefix+prefetcher_type
                        stat_file_location = os.path.join(stats_folders, processor_type, prefetcher_policy+"_"+debug_tag, str(core_number), full_benchmark_name+".zsim.out")
                        sub_stat_file_location = os.path.join(stats_folders, processor_type, prefetcher_policy+"_"+debug_tag, str(core_number), full_benchmark_name+".ramulator.subscription_stats")
                        addr_dist_file_location = os.path.join(stats_folders, processor_type, prefetcher_policy+"_"+debug_tag, str(core_number), full_benchmark_name+".ramulator.address_distribution")
                        if not os.path.isfile(stat_file_location):
                            normalized_cycle_map[config_tag] = 0
                        else:
                            cycle_val = extract_cycle(stat_file_location)
                            normalized_cycle_map[config_tag] = float(0) if cycle_val == -1 or cycle_val == 0 or baseline_cycle == -1 else(float(baseline_cycle)/float(cycle_val))
                        if not os.path.isfile(sub_stat_file_location):
                            normalized_access_hops_map[config_tag] = 0
                            normalized_total_hops_map[config_tag] = 0
                            avg_hmc_latency_map[config_tag] = 0
                            successful_subscriptions_map[config_tag] = 0
                        else:
                            access_hops = extract_subscription_stats(sub_stat_file_location, "AccessPktHopsTravelled")
                            sub_hops = extract_subscription_stats(sub_stat_file_location, "SubscriptionPktHopsTravelled")
                            total_hops = -1 if sub_hops == -1 or access_hops == -1 else (access_hops+sub_hops)
                            mem_access = extract_subscription_stats(sub_stat_file_location, "MemAccesses")
                            successful_subscription = extract_subscription_stats(sub_stat_file_location, "SuccessfulSubscriptions")
                            req_latency = extract_subscription_stats(sub_stat_file_location, "TotalRequestLatency")
                            hmc_latency = extract_subscription_stats(sub_stat_file_location, "TotalHMCLatency")
                            normalized_access_hops_map[config_tag] = float(0) if access_hops == -1 or baseline_access_hops == 0 or baseline_access_hops == -1 else (float(baseline_access_hops)/float(access_hops))
                            normalized_total_hops_map[config_tag] = float(0) if total_hops == -1 or baseline_access_hops == 0 or baseline_access_hops == -1 else (float(baseline_access_hops)/float(total_hops))
                            avg_hmc_latency_map[config_tag] = float(0) if hmc_latency == -1 or mem_access == 0 or mem_access == -1 else (float(hmc_latency)/float(mem_access))
                            successful_subscriptions_map[config_tag] = 0 if successful_subscription == -1 else successful_subscription
                        if not os.path.isfile(addr_dist_file_location):
                            coefficient_of_variation_map[config_tag] = float(0)
                        else:
                            accesses_to_vaults = extract_to_vault_count(addr_dist_file_location, core_number)
                            req_to_vault_mean = numpy.mean(accesses_to_vaults)
                            req_to_vault_std = numpy.std(accesses_to_vaults)
                            req_to_vault_cov = 0 if numpy.isnan(req_to_vault_std) or numpy.isnan(req_to_vault_mean) or req_to_vault_mean == 0 else req_to_vault_std / req_to_vault_mean
                            coefficient_of_variation_map[config_tag] = req_to_vault_cov
        x_lengend = "Configurations"

        normalized_cycles_img_output_dir = os.path.join(normalized_cycles_output_dir, full_benchmark_name+".png")
        normalized_cycle_y = []
        for config in x_configs:
            normalized_cycle_y.append(normalized_cycle_map[config])
        # print "X axis: "+str(x_configs)+" Y axis: "+str(normalized_cycle_y)
        plt.legend()
        plt.bar(x_configs, normalized_cycle_y, color ='maroon',
                width = 0.5)
        plt.xticks(rotation=90)
        plt.xlabel(x_lengend)
        plt.ylabel("Normalized Cycles")
        plt.savefig(normalized_cycles_img_output_dir)
        plt.clf()

        normalized_access_hops_img_output_dir = os.path.join(normalized_access_hops_output_dir, full_benchmark_name+".png")
        normalized_access_hops_y = []
        for config in x_configs:
            normalized_access_hops_y.append(normalized_access_hops_map[config])
        # print "X axis: "+str(x_configs)+" Y axis: "+str(normalized_access_hops_y)
        plt.legend()
        plt.bar(x_configs, normalized_access_hops_y, color ='maroon',
                width = 0.5)
        plt.xticks(rotation=90)
        plt.xlabel(x_lengend)
        plt.ylabel("Normalized Access Hops")
        plt.savefig(normalized_access_hops_img_output_dir)
        plt.clf()

        normalized_total_hops_img_output_dir = os.path.join(normalized_total_hops_output_dir, full_benchmark_name+".png")
        normalized_total_hops_y = []
        for config in x_configs:
            normalized_total_hops_y.append(normalized_total_hops_map[config])
        # print "X axis: "+str(x_configs)+" Y axis: "+str(normalized_total_hops_y)
        plt.legend()
        plt.bar(x_configs, normalized_total_hops_y, color ='maroon',
                width = 0.5)
        plt.xticks(rotation=90)
        plt.xlabel(x_lengend)
        plt.ylabel("Normalized Total Hops")
        plt.savefig(normalized_total_hops_img_output_dir)
        plt.clf()

        coefficient_of_variation_img_output_dir = os.path.join(coefficient_of_variation_output_dir, full_benchmark_name+".png")
        coefficient_of_variation_y = []
        for config in x_configs:
            coefficient_of_variation_y.append(coefficient_of_variation_map[config])
        # print "X axis: "+str(x_configs)+" Y axis: "+str(coefficient_of_variation_y)
        plt.legend()
        plt.bar(x_configs, coefficient_of_variation_y, color ='maroon',
                width = 0.5)
        plt.xticks(rotation=90)
        plt.xlabel(x_lengend)
        plt.ylabel("Coefficient of Variation Hops")
        plt.savefig(coefficient_of_variation_img_output_dir)
        plt.clf()

        avg_hmc_latency_img_output_dir = os.path.join(avg_hmc_latency_output_dir, full_benchmark_name+".png")
        avg_hmc_latency_y = []
        for config in x_configs:
            avg_hmc_latency_y.append(avg_hmc_latency_map[config])
        # print "X axis: "+str(x_configs)+" Y axis: "+str(avg_hmc_latency_y)
        plt.legend()
        plt.bar(x_configs, avg_hmc_latency_y, color ='maroon',
                width = 0.5)
        plt.xticks(rotation=90)
        plt.xlabel(x_lengend)
        plt.ylabel("Average HMC Latency")
        plt.savefig(avg_hmc_latency_img_output_dir)
        plt.clf()

        successful_subscriptions_img_output_dir = os.path.join(successful_subscriptions_output_dir, full_benchmark_name+".png")
        successful_subscriptions_y = []
        for config in x_configs:
            successful_subscriptions_y.append(successful_subscriptions_map[config])
        # print "X axis: "+str(x_configs)+" Y axis: "+str(successful_subscriptions_y)
        plt.legend()
        plt.bar(x_configs, successful_subscriptions_y, color ='maroon',
                width = 0.5)
        plt.xticks(rotation=90)
        plt.xlabel(x_lengend)
        plt.ylabel("Successful Subscriptions")
        plt.savefig(successful_subscriptions_img_output_dir)
        plt.clf()