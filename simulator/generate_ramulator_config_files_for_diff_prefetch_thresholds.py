import sys
import os
import errno
from batch_prefetcher_generator import get_hops_thresholds, get_count_thresholds

hops_thresholds = get_hops_thresholds()
count_thresholds = get_count_thresholds()
ramulator_config_dir = os.path.join(os.getcwd(), "ramulator-configs")
template_dir = os.path.join(ramulator_config_dir, "HMC-SwapSubscriptionPF-config-template.cfg")

def mkdir_p(directory):
    try:
        os.makedirs(directory)
    except OSError as exc:  # Python >2.5
        if exc.errno == errno.EEXIST and os.path.isdir(directory):
            pass
        else:
            raise

mkdir_p(os.path.join(ramulator_config_dir, "prefetcher"))

for hops in hops_thresholds:
    for count in count_thresholds:
        output_dir = os.path.join(ramulator_config_dir, "prefetcher", "HMC-SwapSubscriptionPF-"+str(hops)+"h"+str(count)+"c-config.cfg")
        with open(template_dir, "r") as ins:
            config_file = open(output_dir,"w")
            for line in ins:
                line = line.replace("COUNT_THRESHOLD_NUMBER", str(count))
                line = line.replace("HOPS_THRESHOLD_NUMBER", str(hops))
                config_file.write(line)
            config_file.close()
        ins.close()