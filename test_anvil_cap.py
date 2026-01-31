import anvil
import json

print("Testing Anvil Capabilities...")
try:
    print("JSON works:", json.loads('{"a":1}')["a"])
except Exception as e:
    print("JSON Fail:", e)

try:
    output = anvil.check_output("echo 'Shell Works'")
    print("Shell Works:", output.strip())
except Exception as e:
    print("Shell Fail:", e)
