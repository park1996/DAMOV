# We made a file to generate pim_prefetch separately due to it's difference in configuration
import sys
import os
import errno
from batch_prefetcher_generator import get_hops_thresholds, get_count_thresholds

hops_thresholds = get_hops_thresholds()
count_thresholds = get_count_thresholds()

os.chdir("../workloads")
PIM_ROOT = os.getcwd() +"/"
os.chdir("../simulator") 
ROOT = os.getcwd() +"/"

def mkdir_p(directory):
    try:
        os.makedirs(directory)
    except OSError as exc:  # Python >2.5
        if exc.errno == errno.EEXIST and os.path.isdir(directory):
            pass
        else:
            raise

def create_pim_prefetch_configs(benchmark, application, function, command):
    version = "prefetch"
    number_of_cores = [32]
    prefetcher_types = ["Swap"]
    postfixes = ["netoh", "memtrace"]

    for cores in number_of_cores:
        for prefetcher_type in prefetcher_types:
            for postfix in postfixes:
                for hops_threshold in hops_thresholds:
                    for count_threshold in count_thresholds:
                        mkdir_p(ROOT+"config_files/pim_"+version+"_"+postfix+"_"+prefetcher_type.lower()+str(hops_threshold)+"h"+str(count_threshold)+"c/"+benchmark+"/"+str(cores)+"/")

    for cores in number_of_cores:
        for prefetcher_type in prefetcher_types:
            for postfix in postfixes:
                for hops_threshold in hops_thresholds:
                    for count_threshold in count_thresholds:
                        mkdir_p(ROOT+"zsim_stats/pim_"+version+"_"+postfix+"_"+prefetcher_type.lower()+str(hops_threshold)+"h"+str(count_threshold)+"c/"+str(cores)+"/")

    for cores in number_of_cores:
        for prefetcher_type in prefetcher_types:
            for postfix in postfixes:
                for hops_threshold in hops_thresholds:
                    for count_threshold in count_thresholds:
                        with open(ROOT+"templates/template_pim_"+version+".cfg", "r") as ins:
                            config_file = open(ROOT+"config_files/pim_"+version+"_"+postfix+"_"+prefetcher_type.lower()+str(hops_threshold)+"h"+str(count_threshold)+"c/"+benchmark+"/"+str(cores)+"/"+application+"_"+function+".cfg","w")
                            for line in ins:
                                line = line.replace("NUMBER_CORES", str(cores))
                                line = line.replace("STATS_PATH", "zsim_stats/pim_"+version+"_"+postfix+"_"+prefetcher_type.lower()+str(hops_threshold)+"h"+str(count_threshold)+"c/"+str(cores)+"/"+benchmark+"_"+application+"_"+function)
                                line = line.replace("COMMAND_STRING", "\"" + command + "\";")
                                line = line.replace("THREADS", str(cores))
                                line = line.replace("PIM_ROOT",PIM_ROOT)
                                line = line.replace("RAMULATOR_CONFIG_FILENAME", "HMC-"+prefetcher_type+"SubscriptionPF-"+str(hops_threshold)+"h"+str(count_threshold)+"c-config.cfg")
                                line = line.replace("RECORD_MEMORY_TRACE_SWITCH", "true" if postfix == "memtrace" else "false")

                                config_file.write(line)
                            config_file.close()
                        ins.close()


if(len(sys.argv) < 2):
    print "Usage python generate_config_files.py command_file"
    print "command_file: benckmark,applicationm,function,command"
    exit(1)

with open(sys.argv[1], "r") as command_file:
    for line in command_file:
        line = line.split(",")
        benchmark = line[0]
        application = line[1]
        function = line[2]
        command = line[3]
        print line
        command = command.replace('\n','')

        ### Fixed LLC Size 
        create_pim_prefetch_configs(benchmark, application, function, command)
