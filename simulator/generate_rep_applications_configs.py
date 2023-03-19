import os
import threading
import time

benchmark_suites = ["chai", "darknet", "hashjoin", "ligra", "phoenix", "polybench", "rodinia", "stream"]
#HPCG is included in the representative functions list but requires configuration
#Parsec is not completely built and neither is splash2 - To be investigated
generation_script = "scripts/generate_config_files.py"
prefetch_generation_script = "generate_pim_prefetch_config_files.py" #Prefetch is special
command_files = os.getcwd()+"/command_files/"
maximum_thread = 70
threads = []
def generate_benchmark_config(benchmark):
    os.system("python2 " + generation_script + " " + command_files + benchmark + "_cf")
def generate_prefetch_benchmark_config(benchmark):
    os.system("python2 " + prefetch_generation_script + " " + command_files + benchmark + "_cf")
for benchmark in benchmark_suites:
    current_thread = threading.Thread(target = generate_benchmark_config, args = (benchmark,))
    threads.append(current_thread)
    current_thread.start()
    while threading.active_count() >= maximum_thread + 1:
        time.sleep(1)
    current_thread = threading.Thread(target = generate_prefetch_benchmark_config, args = (benchmark,))
    threads.append(current_thread)
    current_thread.start()
    while threading.active_count() >= maximum_thread + 1:
        time.sleep(1)
for thread in threads:
    thread.join()