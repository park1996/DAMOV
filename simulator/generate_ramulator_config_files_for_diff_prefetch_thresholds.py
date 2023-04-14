import sys
import os
import errno
from batch_prefetcher_generator import get_hops_thresholds, get_count_thresholds, get_debug_flags, get_prefetcher_types

hops_thresholds = get_hops_thresholds()
count_thresholds = get_count_thresholds()
prefetcher_types = get_prefetcher_types()
debug_flags = get_debug_flags()
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
        for debug_flag in debug_flags:
            for prefetcher_type in prefetcher_types:
                debug_tag = "DebugOn" if debug_flag == "true" else "DebugOff"
                output_dir = os.path.join(ramulator_config_dir, "prefetcher", "HMC-SubscriptionPF-"+prefetcher_type+"-"+debug_tag+"-"+str(hops)+"h"+str(count)+"c-config.cfg")
                with open(template_dir, "r") as ins:
                    config_file = open(output_dir,"w")
                    for line in ins:
                        line = line.replace("PREFETCHER_TYPE", prefetcher_type)
                        line = line.replace("DEBUG_FLAG", debug_flag)
                        line = line.replace("COUNT_THRESHOLD_NUMBER", str(count))
                        line = line.replace("HOPS_THRESHOLD_NUMBER", str(hops))
                        config_file.write(line)
                    config_file.close()
                ins.close()