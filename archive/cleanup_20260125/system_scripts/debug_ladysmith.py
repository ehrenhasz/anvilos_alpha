#!/usr/bin/env python3
import os
import sys
import json
from google import genai
from google.genai import types

# Load Key
TOKEN_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'agent_ladysmith', 'token')
try:
    with open(TOKEN_PATH, 'r') as f:
        API_KEY = f.read().strip()
except:
    API_KEY = os.environ.get("GEMINI_API_KEY")

if not API_KEY:
    print("No API Key")
    sys.exit(1)

def submit_card(job_type: str, context: str, payload: str):
    """Submits a Task Card."""
    print(f"--- [MOCK] submit_card called: {job_type}, {context}, {payload}")
    return "SUCCESS: Job Queued"


client = genai.Client(api_key=API_KEY)

sys_instruction = "You are Ladysmith. You MUST use 'submit_card' if asked to create a card."

chat = client.chats.create(
    model="gemini-2.0-flash",
    config=types.GenerateContentConfig(
        tools=[submit_card],
        system_instruction=sys_instruction,
        temperature=0.1
    )
)

print("Sending 'card: hello world'...")
response = chat.send_message("card: hello world")

print(f"\nResponse Text: '{response.text}'")
print(f"Candidates: {len(response.candidates) if response.candidates else 0}")
if response.candidates:
    print(f"Finish Reason: {response.candidates[0].finish_reason}")
    for part in response.candidates[0].content.parts:
        print(f"Part: {part}")
