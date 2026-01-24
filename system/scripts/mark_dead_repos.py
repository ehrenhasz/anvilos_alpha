
import json
import os

files = ['runtime/card_archive_failed.json', 'runtime/card_queue.json']

for file_path in files:
    if not os.path.exists(file_path):
        continue
    
    with open(file_path, 'r') as f:
        try:
            data = json.load(f)
        except json.JSONDecodeError:
            print(f"Error decoding {file_path}")
            continue
    
    updated = False
    for item in data:
        if item.get('status') == 'failed':
            item['status'] = 'dead_repo'
            updated = True
            print(f"Marked {item.get('id')} as dead_repo in {file_path}")
    
    if updated:
        with open(file_path, 'w') as f:
            json.dump(data, f, indent=2)
            print(f"Updated {file_path}")
