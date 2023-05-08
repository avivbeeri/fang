#!/opt/homebrew/bin/python3

import subprocess

inName = "examples/helloworld.fg"
outName = "./testout"
try:
    subprocess.run(["./fgc", inName, outName], shell=False, check=True,  capture_output=True)
    complete = subprocess.run([ outName ], shell=False, check=True,  capture_output=True)
    # print (complete.stdout.decode("utf-8"))
    # print ("Match" if "hello world" == complete.stdout.decode("utf-8") else "No match")
    print ("Done\n")
except subprocess.CalledProcessError as ex:
    print(ex.stdout.decode("utf-8"))
    print(ex.stderr.decode("utf-8"))
