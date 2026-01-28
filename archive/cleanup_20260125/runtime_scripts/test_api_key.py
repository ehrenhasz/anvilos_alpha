import os
import sys
import json
from google import genai
API_KEY = "AIzaSyBABWH2xlKyec86z7fyioX_gs0q7wOYYFA"
try:
    client = genai.Client(api_key=API_KEY)
    response = client.models.generate_content(
        model="gemini-2.0-flash",
        contents="Say something short."
    )
    print(f"API key test successful. Response: {response.text.strip()}")
except Exception as e:
    print(f"API key test failed: {e}")
    sys.exit(1)
