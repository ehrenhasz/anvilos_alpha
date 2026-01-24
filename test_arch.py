import os
import sys
import json
import uuid
import time
import logging
from google import genai
from google.genai import types

PROJECT_ROOT = os.path.abspath('.')
SYSTEM_DB = '/var/lib/anvilos/db/cortex.db'
TOKEN_PATH = os.path.join(PROJECT_ROOT, 'config', 'token')
sys.path.append(PROJECT_ROOT)

from runtime.cortex.db_interface import CortexDB

print('Imports OK')

if os.path.exists(TOKEN_PATH):
    with open(TOKEN_PATH, 'r') as f:
        API_KEY = f.read().strip()
    print(f'Token loaded: {len(API_KEY)} chars')
else:
    print('Token missing')

client = genai.Client(api_key=API_KEY)
print('Client init OK')

db = CortexDB(SYSTEM_DB)
print('DB init OK')
