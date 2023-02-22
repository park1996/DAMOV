import sys
import os

hops_thresholds = []
count_thresholds = []
hops_threshold_start = 2
hops_threshold_stepping = 4
hops_threshold_maximum = 10
count_threshold_start = 1
count_threshold_stepping = 8
count_threshold_maximum = 65535
ramulator_config_dir = os.path.join(os.getcwd(), "ramulator-configs")
template_dir = os.path.join(ramulator_config_dir, "HMC-SwapSubscriptionPF-config-template.cfg")

def next_hops(current_hops):
    return current_hops + hops_threshold_stepping

def next_count(current_count):
    return (current_count+1)*count_threshold_stepping-1

def mkdir_p(directory):
    try:
        os.makedirs(directory)
    except OSError as exc:  # Python >2.5
        if exc.errno == errno.EEXIST and os.path.isdir(directory):
            pass
        else:
            raise

mkdir_p(os.path.join(ramulator_config_dir, "prefetcher"))

hops = hops_threshold_start
while hops <= hops_threshold_maximum:
    hops_thresholds.append(hops)
    count = count_threshold_start
    while count <= count_threshold_maximum:
        count_thresholds.append(count)
        output_dir = os.path.join(ramulator_config_dir, "prefetcher", "HMC-SwapSubscriptionPF-"+str(hops)+"h"+str(count)+"c-config.cfg")
        with open(template_dir, "r") as ins:
            config_file = open(output_dir,"w")
            for line in ins:
                line = line.replace("COUNT_THRESHOLD_NUMBER", str(count))
                line = line.replace("HOPS_THRESHOLD_NUMBER", str(hops))
                config_file.write(line)
            config_file.close()
        ins.close()
        count = next_count(count)
    hops = next_hops(hops)


