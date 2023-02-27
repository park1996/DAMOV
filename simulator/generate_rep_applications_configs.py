import os

benchmark_suites = ["chai", "darknet", "hashjoin", "ligra", "phoenix", "polybench", "rodinia", "stream"]
#HPCG is included in the representative functions list but requires configuration
#Parsec is not completely built and neither is splash2 - To be investigated
generation_script = "scripts/generate_config_files.py"
prefetch_generation_script = "generate_pim_prefetch_config_files.py" #Prefetch is special
command_files = os.getcwd()+"/command_files/"
for benchmark in benchmark_suites:
    os.system("python2 " + generation_script + " " + command_files + benchmark + "_cf")

for benchmark in benchmark_suites:
    os.system("python2 " + prefetch_generation_script + " " + command_files + benchmark + "_cf")