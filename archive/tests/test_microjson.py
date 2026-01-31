import unittest
import json
import sys
import os
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'runtime')))
from microjson.codec import MicroJSON
class TestMicroJSON(unittest.TestCase):
    def test_encode(self):
        result = MicroJSON.encode(100, {"status": "ok"})
        data = json.loads(result)
        self.assertEqual(data["@ID"], 100)
        self.assertEqual(data["data"]["status"], "ok")
    def test_decode_valid(self):
        payload = '{"@ID": 200, "data": "success"}'
        result = MicroJSON.decode(payload)
        self.assertEqual(result["@ID"], 200)
        self.assertEqual(result["data"], "success")
    def test_decode_invalid(self):
        payload = '{"foo": "bar"}'
        with self.assertRaises(ValueError):
            MicroJSON.decode(payload)
if __name__ == '__main__':
    unittest.main()
