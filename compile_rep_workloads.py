import os
import subprocess

benchmark_suites = ["chai-cpu", "Darknet", "multicore-hashjoins-0.1", "ligra", "phoenix", "PolyBench-ACC", "rodinia_3.1", "STREAM"]
root_dir = os.getcwd()
for benchmark in benchmark_suites:
    os.chdir(root_dir+"/workload/"+benchmark)
    return_code = subprocess.call(["python2", "compile.py"])
    #print "The return code of compiling benchmark " + benchmark + " is " + str(return_code)
    if return_code is not 0:
        print "Error when compiling benchmark: " + benchmark
        exit(1)
os.chdir(root_dir+"/workload/hpcg")
# add -DHPGSym into setup/Make.GCC_OMP file, make build directory, run configure GCC_OMP in it
return_code = subprocess.call(["python2", "compile.py"])
os.chdir(root_dir+"/workload/parsec-3.0")
# Install MPI interface by using sudo apt install mpich
return_code = subprocess.call(["python2", "compile_parsec.py"])
if return_code is not 0:
    print "Error when compiling benchmark: parsec"
    exit(1)
return_code = subprocess.call(["python2", "compile_splash2x.py"])
# Remove commented functions
if return_code is not 0:
    print "Error when compiling benchmark: splash"
    exit(1)
