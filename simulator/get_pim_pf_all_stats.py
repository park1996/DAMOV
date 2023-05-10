# -*- coding: utf-8 -*-
import os
import csv
from datetime import datetime
from batch_prefetcher_generator import get_hops_thresholds, get_count_thresholds, get_core_numbers
import numpy

hops_thresholds = get_hops_thresholds()
# count_thresholds = get_count_thresholds()
count_thresholds = [0, 1, 63]
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

output_filename = "stats_all_pim_pf_with_diff_thresholds_"+datetime.now().strftime("%Y-%m-%d_%H-%M-%S")+".csv"
output_filename = os.path.join(os.getcwd(), output_filename)
stats_folders = os.path.join(os.getcwd(), "zsim_stats")

with open(output_filename, mode='w') as csv_file:
    csv_writer = csv.writer(csv_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
    for suite in benchmark_suites_and_benchmarks_functions.keys():
        for benchmark_function in benchmark_suites_and_benchmarks_functions[suite]:
            for core_number in core_numbers:
                full_benchmark_name = suite+"_"+benchmark_function
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
                    baseline_req_to_vault_cov = 0.0 if numpy.isnan(baseline_req_to_vault_mean) or numpy.isnan(baseline_req_to_vault_std) or baseline_req_to_vault_mean == 0 else baseline_req_to_vault_std / baseline_req_to_vault_mean
                for prefetcher_type in prefetcher_types:
                    for hops_threshold in hops_thresholds:
                        prefetcher_policies = []
                        prefetcher_policies.append("adaptive")
                        for count_threshold in count_thresholds:
                            prefetcher_policies.append(str(hops_threshold)+"h"+str(count_threshold)+"c")
                        csv_header = ["", "baseline"]+prefetcher_policies
                        csv_writer.writerow([full_benchmark_name+" with "+prefetcher_type+" prefetcher and "+str(hops_threshold)+" hop threshold and "+str(core_number)+" cores"])
                        csv_writer.writerow(csv_header)
                        cycle_line = ["Total Cycle", "N/A" if baseline_cycle == -1 else str(baseline_cycle) ]
                        normalized_cycle_line = ["Normalized Cycle", "N/A" if baseline_cycle == -1 else str(1)]
                        l1_miss_rate_line = ["L1 Miss Rate", "N/A" if baseline_l1_cache_miss_rate == -1 else str(baseline_l1_cache_miss_rate)]
                        normalized_l1_miss_rate_line = ["Normalized L1 Miss Rate", "N/A" if baseline_l1_cache_miss_rate == -1 else str(1)]
                        access_hops_line = ["Access Pkt Hops", "N/A" if baseline_access_hops == -1 else str(baseline_access_hops)]
                        normalized_access_hops_line = ["Normalized Access Hops", "N/A" if baseline_access_hops == -1 else str(1)]
                        sub_hops_line = ["Subscription Pkt Hops", "0"]
                        total_hops_line = ["Total Hops",  "N/A" if baseline_access_hops == -1 else str(baseline_access_hops)]
                        normalized_total_hops_line = ["Normalized Total hops", "N/A" if baseline_access_hops == -1 else str(1)]
                        mem_access_line = ["Memory Access", "N/A" if baseline_mem_access == -1 else str(baseline_mem_access)]
                        normalized_memory_access_line = ["Normalized Memory Access", "N/A" if baseline_mem_access == -1 else str(1)]
                        access_hops_per_mem_line = ["Access Hops per Mem Access", "N/A" if baseline_mem_access == -1 or baseline_mem_access == 0 or baseline_access_hops == -1 else str(float(baseline_access_hops)/float(baseline_mem_access))]
                        total_hops_per_mem_line = ["Total Hops per Mem Access", "N/A" if baseline_mem_access == -1 or baseline_mem_access == 0 or baseline_access_hops == -1 else str(float(baseline_access_hops)/float(baseline_mem_access))]
                        submitted_sub_per_mem_access_line = ["Submitted Sub per Mem access", "N/A"]
                        submitted_subscriptions_line = ["Submitted Subscriptions", "N/A"]
                        successful_subscription_line = ["Successful Subscriptions", "N/A"]
                        subscription_from_buffer_line = ["Subscription from Buffer", "N/A"]
                        unsubscription_line = ["Unsubscriptions", "N/A"]
                        resubscription_line = ["Resubscriptions", "N/A"]
                        suc_ins_to_buffer_line = ["Successful Ins to Buffer", "N/A"]
                        unsuc_ins_to_buffer_line = ["Unsuccessful Ins to Buffer", "N/A"]
                        count_table_evic_line = ["Count Table Evictions", "N/A"]
                        count_table_max_count_line = ["Count Table Max Count", "N/A"]
                        count_table_avg_count_line = ["Count Table Avg Count", "N/A"]
                        requests_to_vault_mean_line = ["Requests to Each Vault Mean", "N/A" if baseline_req_to_vault_mean == -1 else str(baseline_req_to_vault_mean)]
                        requests_to_vault_std_line = ["Requests to Each Vault Std. Dev.", "N/A" if baseline_req_to_vault_std == -1 else str(baseline_req_to_vault_std)]
                        requests_to_vault_cov_line = ["Requests to Each Vault Coe. of Var.", "N/A" if baseline_req_to_vault_cov == -1 else str(baseline_req_to_vault_cov)]
                        request_latency_line = ["Total Request Latency", "N/A" if baseline_req_latency == -1 else str(baseline_req_latency)]
                        avg_request_latency_line = ["Average Request Latency", "N/A" if baseline_req_latency == -1 or baseline_mem_access == -1 or baseline_mem_access == 0 else str(float(baseline_req_latency)/float(baseline_mem_access))]
                        normalized_req_latency_line = ["Normalized Request Latency", "N/A" if baseline_req_latency == -1 else str(1)]
                        hmc_latency_line = ["Total HMC Latency", "N/A" if baseline_hmc_latency == -1 else str(baseline_hmc_latency)]
                        avg_hmc_latency_line = ["Average HMC Latency", "N/A" if baseline_hmc_latency == -1 or baseline_mem_access == -1 or baseline_mem_access == 0 else str(float(baseline_hmc_latency)/float(baseline_mem_access))]
                        normalized_hmc_latency_line = ["Normalized HMC Latency", "N/A" if baseline_hmc_latency == -1 else str(1)]
                        readq_pending_line = ["Total Read Queue Pending", "N/A" if baseline_readq_pending == -1 else str(baseline_readq_pending)]
                        writeq_pending_line = ["Total Write Queue Pending", "N/A" if baseline_writeq_pending == -1 else str(baseline_writeq_pending)]   
                        for prefetcher_policy in prefetcher_policies:
                            processor_type = processor_type_prefix+prefetcher_type
                            stat_file_location = os.path.join(stats_folders, processor_type, prefetcher_policy+"_"+debug_tag, str(core_number), full_benchmark_name+".zsim.out")
                            sub_stat_file_location = os.path.join(stats_folders, processor_type, prefetcher_policy+"_"+debug_tag, str(core_number), full_benchmark_name+".ramulator.subscription_stats")
                            addr_dist_file_location = os.path.join(stats_folders, processor_type, prefetcher_policy+"_"+debug_tag, str(core_number), full_benchmark_name+".ramulator.address_distribution")
                            if not os.path.isfile(stat_file_location):
                                cycle_line.append("N/A")
                                normalized_cycle_line.append("N/A")
                                l1_miss_rate_line.append("N/A")
                                normalized_l1_miss_rate_line.append("N/A")
                            else:
                                cycle_val = extract_cycle(stat_file_location)
                                l1_miss_rate = extract_l1_miss_rate(stat_file_location)
                                cycle_line.append("N/A" if cycle_val == -1 else str(cycle_val))
                                normalized_cycle_line.append("N/A" if cycle_val == -1 or cycle_val == 0 or baseline_cycle == -1 else str(float(baseline_cycle)/float(cycle_val)))
                                l1_miss_rate_line.append("N/A" if l1_miss_rate == -1 else str(l1_miss_rate))
                                normalized_l1_miss_rate_line.append("N/A" if l1_miss_rate == -1 or baseline_l1_cache_miss_rate == 0 or baseline_l1_cache_miss_rate == 0 else str(float(l1_miss_rate)/float(baseline_l1_cache_miss_rate)))
                            if not os.path.isfile(sub_stat_file_location):
                                access_hops_line.append("N/A")
                                normalized_access_hops_line.append("N/A")
                                sub_hops_line.append("N/A")
                                total_hops_line.append("N/A")
                                normalized_total_hops_line.append("N/A")
                                mem_access_line.append("N/A")
                                normalized_memory_access_line.append("N/A")
                                access_hops_per_mem_line.append("N/A")
                                total_hops_per_mem_line.append("N/A")
                                submitted_sub_per_mem_access_line.append("N/A")
                                submitted_subscriptions_line.append("N/A")
                                successful_subscription_line.append("N/A")
                                subscription_from_buffer_line.append("N/A")
                                unsubscription_line.append("N/A")
                                resubscription_line.append("N/A")
                                suc_ins_to_buffer_line.append("N/A")
                                unsuc_ins_to_buffer_line.append("N/A")
                                count_table_evic_line.append("N/A")
                                count_table_max_count_line.append("N/A")
                                count_table_avg_count_line.append("N/A")
                                request_latency_line.append("N/A")
                                avg_request_latency_line.append("N/A")
                                normalized_req_latency_line.append("N/A")
                                hmc_latency_line.append("N/A")
                                avg_hmc_latency_line.append("N/A")
                                normalized_hmc_latency_line.append("N/A")
                                readq_pending_line.append("N/A")
                                writeq_pending_line.append("N/A")
                            else:
                                access_hops = extract_subscription_stats(sub_stat_file_location, "AccessPktHopsTravelled")
                                sub_hops = extract_subscription_stats(sub_stat_file_location, "SubscriptionPktHopsTravelled")
                                total_hops = -1 if sub_hops == -1 or access_hops == -1 else (access_hops+sub_hops)
                                mem_access = extract_subscription_stats(sub_stat_file_location, "MemAccesses")
                                submitted_subscriptions = extract_subscription_stats(sub_stat_file_location, "SubmittedSubscriptions")
                                successful_subscription = extract_subscription_stats(sub_stat_file_location, "SuccessfulSubscriptions")
                                subscription_from_buffer = extract_subscription_stats(sub_stat_file_location, "SuccessfulSubscriptionFromBuffer")
                                unsubscription = extract_subscription_stats(sub_stat_file_location, "Unsubscriptions")
                                resubscription = extract_subscription_stats(sub_stat_file_location, "Resubscriptions")
                                suc_ins_to_buffer = extract_subscription_stats(sub_stat_file_location, "SuccessfulInsertationToBuffer")
                                unsuc_ins_to_buffer = extract_subscription_stats(sub_stat_file_location, "UnsuccessfulInsertationToBuffer")
                                count_table_evic = extract_subscription_stats(sub_stat_file_location, "CountTableEvictions")
                                count_table_max = extract_subscription_stats(sub_stat_file_location, "CountTableMaxCount")
                                count_table_total = extract_subscription_stats(sub_stat_file_location, "CountTableTotalCount")
                                req_latency = extract_subscription_stats(sub_stat_file_location, "TotalRequestLatency")
                                hmc_latency = extract_subscription_stats(sub_stat_file_location, "TotalHMCLatency")
                                readq_pending = extract_subscription_stats(sub_stat_file_location, "TotalReadQPending")
                                writeq_pending = extract_subscription_stats(sub_stat_file_location, "TotalWriteQPending")  
                                access_hops_line.append("N/A" if access_hops == -1 else str(access_hops))
                                normalized_access_hops_line.append("N/A" if access_hops == -1 or access_hops == 0 or baseline_access_hops == -1 else str(float(baseline_access_hops)/float(access_hops)))
                                sub_hops_line.append("N/A" if sub_hops == -1 else str(sub_hops))
                                total_hops_line.append("N/A" if total_hops == -1 else str(total_hops))
                                normalized_total_hops_line.append("N/A" if total_hops == -1 or total_hops == 0 or baseline_access_hops == -1 else str(float(baseline_access_hops)/float(total_hops)))
                                mem_access_line.append("N/A" if mem_access == -1 else str(mem_access))
                                normalized_memory_access_line.append("N/A" if mem_access == -1 or mem_access == 0 or baseline_mem_access == -1 else str(float(baseline_mem_access)/float(mem_access)))
                                access_hops_per_mem_line.append("N/A" if access_hops == -1 or mem_access == 0 or mem_access == -1 else str(float(access_hops)/float(mem_access)))
                                total_hops_per_mem_line.append("N/A" if total_hops == -1 or mem_access == 0 or mem_access == -1 else str(float(total_hops)/float(mem_access)))
                                submitted_subscriptions_line.append("N/A" if submitted_subscriptions == -1 else str(submitted_subscriptions))
                                submitted_sub_per_mem_access_line.append("N/A" if mem_access == 0 or mem_access == -1 or submitted_subscriptions == -1 else str(float(submitted_subscriptions)/float(mem_access)))
                                successful_subscription_line.append("N/A" if successful_subscription == -1 else str(successful_subscription))
                                subscription_from_buffer_line.append("N/A" if subscription_from_buffer == -1 else str(subscription_from_buffer))
                                unsubscription_line.append("N/A" if unsubscription == -1 else str(unsubscription))
                                resubscription_line.append("N/A" if resubscription == -1 else str(resubscription))
                                suc_ins_to_buffer_line.append("N/A" if suc_ins_to_buffer == -1 else str(suc_ins_to_buffer))
                                unsuc_ins_to_buffer_line.append("N/A" if unsuc_ins_to_buffer == -1 else str(unsuc_ins_to_buffer))
                                count_table_evic_line.append("N/A" if count_table_evic == -1 else str(count_table_evic))
                                count_table_max_count_line.append("N/A" if count_table_max == -1 else str(count_table_max))
                                count_table_avg_count_line.append("N/A" if count_table_total == -1 or count_table_evic == 0 or count_table_evic == -1 else str(float(count_table_total)/float(count_table_evic)))
                                request_latency_line.append("N/A" if req_latency == -1 else str(req_latency))
                                avg_request_latency_line.append("N/A" if req_latency == -1 or mem_access == 0 or mem_access == -1 else str(float(req_latency)/float(mem_access)))
                                normalized_req_latency_line.append("N/A" if req_latency == -1 or baseline_req_latency == -1 or req_latency == 0 else str(float(baseline_req_latency)/float(req_latency)))
                                hmc_latency_line.append("N/A" if hmc_latency == -1 else str(hmc_latency))
                                avg_hmc_latency_line.append("N/A" if hmc_latency == -1 or mem_access == 0 or mem_access == -1 else str(float(hmc_latency)/float(mem_access)))
                                normalized_hmc_latency_line.append("N/A" if hmc_latency == -1 or baseline_hmc_latency == -1 or hmc_latency == 0 else str(float(baseline_hmc_latency)/float(hmc_latency)))
                                readq_pending_line.append("N/A" if readq_pending == -1 else str(readq_pending))
                                writeq_pending_line.append("N/A" if writeq_pending == -1 else str(writeq_pending))
                            if not os.path.isfile(addr_dist_file_location):
                                requests_to_vault_mean_line.append("N/A")
                                requests_to_vault_std_line.append("N/A")
                                requests_to_vault_cov_line.append("N/A")
                            else:
                                accesses_to_vaults = extract_to_vault_count(addr_dist_file_location, core_number)
                                req_to_vault_mean = numpy.mean(accesses_to_vaults)
                                req_to_vault_std = numpy.std(accesses_to_vaults)
                                req_to_vault_cov = 0.0 if numpy.isnan(req_to_vault_std) or numpy.isnan(req_to_vault_mean) or req_to_vault_mean == 0 else req_to_vault_std / req_to_vault_mean
                                requests_to_vault_mean_line.append(str(req_to_vault_mean))
                                requests_to_vault_std_line.append(str(req_to_vault_std))
                                requests_to_vault_cov_line.append(str(req_to_vault_cov))
                        csv_writer.writerow(cycle_line)
                        csv_writer.writerow(normalized_cycle_line)
                        csv_writer.writerow(access_hops_line)
                        csv_writer.writerow(normalized_access_hops_line)
                        csv_writer.writerow(sub_hops_line)
                        csv_writer.writerow(total_hops_line)
                        csv_writer.writerow(normalized_total_hops_line)
                        csv_writer.writerow(l1_miss_rate_line)
                        csv_writer.writerow(normalized_l1_miss_rate_line)
                        csv_writer.writerow(mem_access_line)
                        csv_writer.writerow(normalized_memory_access_line)
                        csv_writer.writerow(access_hops_per_mem_line)
                        csv_writer.writerow(total_hops_per_mem_line)
                        csv_writer.writerow(submitted_subscriptions_line)
                        csv_writer.writerow(submitted_sub_per_mem_access_line)
                        csv_writer.writerow(successful_subscription_line)
                        csv_writer.writerow(subscription_from_buffer_line)
                        csv_writer.writerow(unsubscription_line)
                        csv_writer.writerow(resubscription_line)
                        csv_writer.writerow(suc_ins_to_buffer_line)
                        csv_writer.writerow(unsuc_ins_to_buffer_line)
                        csv_writer.writerow(count_table_evic_line)
                        csv_writer.writerow(count_table_max_count_line)
                        csv_writer.writerow(count_table_avg_count_line)
                        csv_writer.writerow(requests_to_vault_mean_line)
                        csv_writer.writerow(requests_to_vault_std_line)
                        csv_writer.writerow(requests_to_vault_cov_line)
                        csv_writer.writerow(request_latency_line)
                        csv_writer.writerow(avg_request_latency_line)
                        csv_writer.writerow(normalized_req_latency_line)
                        csv_writer.writerow(hmc_latency_line)
                        csv_writer.writerow(avg_hmc_latency_line)
                        csv_writer.writerow(normalized_hmc_latency_line)
                        csv_writer.writerow(readq_pending_line)
                        csv_writer.writerow(writeq_pending_line)
                    csv_writer.writerow('')


