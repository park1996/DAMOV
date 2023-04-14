def next_hops(current_hops, hops_threshold_stepping):
    return current_hops + hops_threshold_stepping

def next_count(current_count, count_threshold_stepping):
    return (current_count+1)*count_threshold_stepping-1
    # return current_count + count_threshold_stepping

def get_hops_thresholds():
    hops_threshold_start = 1
    # hops_threshold_stepping = 1
    hops_threshold_stepping = 2
    hops_threshold_maximum = 10
    hops_thresholds = []
    hops = hops_threshold_start
    while hops <= hops_threshold_maximum:
        hops_thresholds.append(hops)
        hops = next_hops(hops, hops_threshold_stepping)
    return hops_thresholds

def get_count_thresholds():
    count_threshold_start = 0
    # count_threshold_stepping = 1
    # count_threshold_maximum = 15
    count_threshold_stepping = 2
    # count_threshold_maximum = 255
    count_threshold_maximum = 63
    count_thresholds = []
    count = count_threshold_start
    while count <= count_threshold_maximum:
        count_thresholds.append(count)
        count = next_count(count, count_threshold_stepping)
    return count_thresholds

def get_debug_flags():
    return ["true", "false"]
    # return ["false"]

def get_prefetcher_types():
    # return ["Allocate", "Swap"]
    return ["Allocate"]