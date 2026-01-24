# Recipe: Fetch Linux Kernel Source
import os
import subprocess

KERNEL_VERSION = "6.9.3"

def run():
    print("Starting kernel fetch recipe...")
    os.system(f"mkdir -p /mnt/anvil_temp/ascii_oss/src")
    print("Created directory /mnt/anvil_temp/ascii_oss/src")
    os.chdir("/mnt/anvil_temp/ascii_oss/src")
    print("Changed directory to /mnt/anvil_temp/ascii_oss/src")
    command = f"wget https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-{KERNEL_VERSION}.tar.xz"
    print(f"Executing command: {command}")
    result = subprocess.run(command, shell=True, capture_output=True, text=True)
    print(f"wget output: {result.stdout}")
    print(f"wget errors: {result.stderr}")
    command = f"tar -xf linux-{KERNEL_VERSION}.tar.xz"
    print(f"Executing command: {command}")
    result = subprocess.run(command, shell=True, capture_output=True, text=True)
    print(f"tar output: {result.stdout}")
    print(f"tar errors: {result.stderr}")
    command = f"mv linux-{KERNEL_VERSION} linux"
    print(f"Executing command: {command}")
    result = subprocess.run(command, shell=True, capture_output=True, text=True)
    print(f"mv output: {result.stdout}")
    print(f"mv errors: {result.stderr}")

print("Running recipe...")
run()
print("Recipe finished.")
