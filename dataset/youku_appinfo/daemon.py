import subprocess
import time

cmd = 'node .\\index.js'
print(cmd)
p = subprocess.Popen(cmd.split(' '), shell=True)

sec = 0

while True:
    if p.poll() is not None:
        print(cmd)
        p = subprocess.Popen(cmd.split(' '), shell=True)
    time.sleep(10)
