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
benchmark_suites_and_benchmarks_functions = {"chai" : ["BS_BEZIER_KERNEL", "HSTI_HSTI", "HSTO_HSTO", "OOPPAD_OOPPAD"],
    "darknet" : ["yolo_gemm_nn"],
    "hashjoin" : ["NPO_probehashtable"],
    "hpcg" : ["HPCG_ComputePrologation", "HPCG_ComputeRestriction", "HPCG_ComputeSYMGS"],
    "ligra" : ["PageRank_edgeMapDenseUSA", "Radii_edgeMapSparseUSA"],
    "parsec" : ["Fluidaminate_ProcessCollision2MT"],
    "phoenix" : ["Linearregression_main", "Stringmatch_main"],
    "polybench" : ["linear-algebra_3mm", "linear-algebra_doitgen", "linear-algebra_gemm", "linear-algebra_gramschmidt", "linear-algebra_gemver", "stencil_convolution-2d"], 
    "splash-2" : ["Oceanncp_jacobcalc", "Radix_slave_sort"],
    "stream" : ["Add_Add", "Copy_Copy", "Scale_Scale", "Triad_Triad"]}

processor_type_prefix = "pim_prefetch_netoh_"
baseline_processor_type = "pim_ooo_netoh"
prefetcher_types = ["allocate"]
core_number = "32"

output_filename = "stats_all_pim_pf_with_diff_thresholds_"+datetime.now().strftime("%Y-%m-%d_%H-%M-%S")+".csv"
output_filename = os.path.join(os.getcwd(), output_filename)
stats_folders = os.path.join(os.getcwd(), "zsim_stats")

with open(output_filename, mode='w') as csv_file:
    csv_writer = csv.writer(csv_file, delimiter=',', quotechar='"', quoting=csv.QUOTE_MINIMAL)
    for suite in benchmark_suites_and_benchmarks_functions.keys():
        for benchmark_function in benchmark_suites_and_benchmarks_functions[suite]:
            full_benchmark_name = suite+"_"+benchmark_function
            baseline_cycle = -1
            baseline_stat_file_location = os.path.join(stats_folders, baseline_processor_type, core_number, full_benchmark_name+".zsim.out")
            if os.path.isfile(baseline_stat_file_location):
                baseline_cycle = extract_cycle(baseline_stat_file_location)
            baseline_access_hops = -1
            baseline_sub_stat_file_location = os.path.join(stats_folders, baseline_processor_type, core_number, full_benchmark_name+".ramulator.subscription_stats")
            if os.path.isfile(baseline_sub_stat_file_location):
                baseline_access_hops = extract_subscription_stats(baseline_sub_stat_file_location, "AccessPktHopsTravelled")
            for prefetcher_type in prefetcher_types:
                for hops_threshold in hops_thresholds:
                    csv_writer.writerow([full_benchmark_name+" with "+prefetcher_type+" prefetcher and "+str(hops_threshold)+" hop threshold"])
                    csv_writer.writerow(count_thresholds_str)
                    cycle_line = ["Total Cycle"]
                    normalized_cycle_line = ["Normalized Cycle"]
                    l1_miss_rate_line = ["L1 Miss Rate"]
                    access_hops_line = ["Access Pkt Hops"]
                    normalized_access_hops_line = ["Normalized Access Hops"]
                    sub_hops_line = ["Subscription Pkt Hops"]
                    mem_access_line = ["Memory Access"]
                    submitted_sub_per_mem_access_line = ["Submitted Sub per Mem access"]
                    submitted_subscriptions_line = ["Submitted Subscriptions"]
                    successful_subscription_line = ["Successful Subscriptions"]
                    subscription_from_buffer_line = ["Subscription from Buffer"]
                    unsubscription_line = ["Unsubscriptions"]
                    resubscription_line = ["Resubscriptions"]
                    suc_ins_to_buffer_line = ["Successful Ins to Buffer"]
                    unsuc_ins_to_buffer_line = ["Unsuccessful Ins to Buffer"]
                    count_table_evic_line = ["Count Table Evictions"]
                    count_table_max_count = ["Count Table Max Count"]
                    for count_threshold in count_thresholds:
                        processor_type = processor_type_prefix+prefetcher_type
                        stat_file_location = os.path.join(stats_folders, processor_type, str(hops_threshold)+"h"+str(count_threshold)+"c_"+debug_tag, core_number, full_benchmark_name+".zsim.out")
                        sub_stat_file_location = os.path.join(stats_folders, processor_type, str(hops_threshold)+"h"+str(count_threshold)+"c_"+debug_tag, core_number, full_benchmark_name+".ramulator.subscription_stats")
                        if not os.path.isfile(stat_file_location):
                            cycle_line.append("N/A")
                            normalized_cycle_line.append("N/A")
                            l1_miss_rate_line.append("N/A")
                        else:
                            cycle_val = extract_cycle(stat_file_location)
                            l1_miss_rate = extract_l1_miss_rate(stat_file_location)
                            cycle_line.append("N/A" if cycle_val == -1 else str(cycle_val))
                            normalized_cycle_line.append("N/A" if cycle_val == -1 or baseline_cycle == 0 or baseline_cycle == -1 else str(float(baseline_cycle)/float(cycle_val)))
                            l1_miss_rate_line.append("N/A" if l1_miss_rate == -1 else str(l1_miss_rate))
                        if not os.path.isfile(sub_stat_file_location):
                            access_hops_line.append("N/A")
                            normalized_access_hops_line.append("N/A")
                            sub_hops_line.append("N/A")
                            mem_access_line.append("N/A")
                            submitted_sub_per_mem_access_line.append("N/A")
                            submitted_subscriptions_line.append("N/A")
                            successful_subscription_line.append("N/A")
                            subscription_from_buffer_line.append("N/A")
                            unsubscription_line.append("N/A")
                            resubscription_line.append("N/A")
                            suc_ins_to_buffer_line.append("N/A")
                            unsuc_ins_to_buffer_line.append("N/A")
                            count_table_evic_line.append("N/A")
                            count_table_max_count.append("N/A")
                        else:
                            access_hops = extract_subscription_stats(sub_stat_file_location, "AccessPktHopsTravelled")
                            sub_hops = extract_subscription_stats(sub_stat_file_location, "SubscriptionPktHopsTravelled")
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
                            access_hops_line.append("N/A" if access_hops == -1 else str(access_hops))
                            normalized_access_hops_line.append("N/A" if access_hops == -1 or baseline_access_hops == 0 or baseline_access_hops == -1 else str(float(baseline_access_hops)/float(access_hops)))
                            sub_hops_line.append("N/A" if sub_hops == -1 else str(sub_hops))
                            mem_access_line.append("N/A" if mem_access == -1 else str(mem_access))
                            submitted_subscriptions_line.append("N/A" if submitted_subscriptions == -1 else str(submitted_subscriptions))
                            submitted_sub_per_mem_access_line.append("N/A" if mem_access == 0 or mem_access == -1 or submitted_subscriptions == -1 else str(float(submitted_subscriptions)/float(mem_access)))
                            successful_subscription_line.append("N/A" if successful_subscription == -1 else str(successful_subscription))
                            subscription_from_buffer_line.append("N/A" if subscription_from_buffer == -1 else str(subscription_from_buffer))
                            unsubscription_line.append("N/A" if unsubscription == -1 else str(unsubscription))
                            resubscription_line.append("N/A" if resubscription == -1 else str(resubscription))
                            suc_ins_to_buffer_line.append("N/A" if suc_ins_to_buffer == -1 else str(suc_ins_to_buffer))
                            unsuc_ins_to_buffer_line.append("N/A" if unsuc_ins_to_buffer == -1 else str(unsuc_ins_to_buffer))
                            count_table_evic_line.append("N/A" if count_table_evic == -1 else str(count_table_evic))
                            count_table_max_count.append("N/A" if count_table_max == -1 else str(count_table_max))
                    csv_writer.writerow(cycle_line)
                    csv_writer.writerow(normalized_cycle_line)
                    csv_writer.writerow(access_hops_line)
                    csv_writer.writerow(normalized_access_hops_line)
                    csv_writer.writerow(sub_hops_line)
                    csv_writer.writerow(l1_miss_rate_line)
                    csv_writer.writerow(mem_access_line)
                    csv_writer.writerow(submitted_subscriptions_line)
                    csv_writer.writerow(submitted_sub_per_mem_access_line)
                    csv_writer.writerow(successful_subscription_line)
                    csv_writer.writerow(subscription_from_buffer_line)
                    csv_writer.writerow(unsubscription_line)
                    csv_writer.writerow(resubscription_line)
                    csv_writer.writerow(suc_ins_to_buffer_line)
                    csv_writer.writerow(unsuc_ins_to_buffer_line)
                    csv_writer.writerow(count_table_evic_line)
                    csv_writer.writerow(count_table_max_count)
                csv_writer.writerow('')


