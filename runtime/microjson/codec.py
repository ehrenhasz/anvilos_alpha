import json
from typing import Any, Dict

class MicroJSON:
    @staticmethod
    def encode(id_code: int, data: Any) -> str:
        """
        Encodes data into the MicroJSON format.
        Schema: {"@ID": int, "data": Any}
        """
        payload = {
            "@ID": id_code,
            "data": data
        }
        return json.dumps(payload, ensure_ascii=False)

    @staticmethod
    def decode(payload: str) -> Dict[str, Any]:
        """
        Decodes a MicroJSON string.
        Returns the full dictionary if valid, raises ValueError otherwise.
        """
        try:
            data = json.loads(payload)
            if "@ID" not in data or "data" not in data:
                raise ValueError("Invalid MicroJSON format: Missing @ID or data field")
            return data
        except json.JSONDecodeError as e:
            raise ValueError(f"Invalid JSON: {e}")
