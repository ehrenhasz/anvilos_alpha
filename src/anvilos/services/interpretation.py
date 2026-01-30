import os
import sys
import json
from google import genai
from google.genai import types
PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOKEN_PATH = os.path.join(PROJECT_ROOT, "..", "config", "token")
class Interpreter:
    def __init__(self):
        self.api_key = self._load_token()
        self.client = None
        if self.api_key:
            try:
                self.client = genai.Client(api_key=self.api_key)
            except Exception as e:
                print(f"[Interpreter] Init Error: {e}")
    def _load_token(self):
        try:
            if os.path.exists(TOKEN_PATH):
                with open(TOKEN_PATH, 'r') as f:
                    return f.read().strip()
            return os.environ.get("GOOGLE_API_KEY")
        except:
            return None
    def interpret(self, user_input: str) -> str:
        """
        Translates natural language to shell commands.
        """
        if not self.client:
            return f"echo 'Error: No LLM Client available. Input: {user_input}'"
        try:
            prompt = (
                "You are a Linux Shell Expert. Convert the following natural language request into a precise, "
                "safe, and efficient bash command. Output ONLY the command, no markdown, no explanation.\n" 
                "If the input is already a command, return it as is.\n" 
                f"Request: {user_input}"
            )
            response = self.client.models.generate_content(
                model="gemini-2.0-flash",
                contents=prompt,
                config=types.GenerateContentConfig(
                    temperature=0.1
                )
            )
            return response.text.strip()
        except Exception as e:
            return f"echo 'Interpretation Failed: {e}'"
if __name__ == "__main__":
    interp = Interpreter()
    print(interp.interpret("list all files in current dir sorted by size"))
