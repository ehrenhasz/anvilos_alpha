import pexpect
import sys
import os

HOST = "192.168.6.50"
USER = "ehren"
PASS = "6dD69Kio9S9o"
CMD = " ".join(sys.argv[1:])

if not CMD:
    print("Usage: python3 ssh_executor.py <command>")
    sys.exit(1)

ssh_cmd = f"ssh {USER}@{HOST} \"{CMD}\""

try:
    child = pexpect.spawn(ssh_cmd, encoding='utf-8')
    
    # Expect the password prompt. 
    # Note: SSH prompts can vary slightly, "password:" is standard.
    # We also handle the case where it might ask to verify the host key (yes/no).
    i = child.expect(['password:', 'Are you sure you want to continue connecting', pexpect.EOF, pexpect.TIMEOUT])
    
    if i == 1: # Host key verification
        child.sendline('yes')
        child.expect('password:')
        child.sendline(PASS)
    elif i == 0: # Password prompt
        child.sendline(PASS)
    elif i == 2: # EOF - command might have run without password (unlikely given context) or failed immediately
        print("Connection closed unexpectedly.")
        print(child.before)
        sys.exit(1)
    elif i == 3: # Timeout
        print("Connection timed out.")
        sys.exit(1)
        
    # Interact/Read output
    # We want to print the output of the command.
    # Since we passed the command in the ssh string, ssh will execute it and exit.
    # So we just read until EOF.
    output = child.read()
    print(output)
    
    child.close()
    sys.exit(child.exitstatus)

except Exception as e:
    print(f"An error occurred: {e}")
    sys.exit(1)

