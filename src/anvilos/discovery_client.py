import os
import logging
from googleapiclient.discovery import build
from google.oauth2 import service_account
logger = logging.getLogger(__name__)
class DiscoveryClient:
    """
    Wrapper for Google Cloud Discovery Engine API using googleapiclient.
    """
    def __init__(self, key_path=None):
        self.service = None
        self.key_path = key_path or os.environ.get("GOOGLE_APPLICATION_CREDENTIALS")
        if not self.key_path:
            internal_token = os.path.join(os.path.dirname(__file__), '../../config/token')
            if os.path.exists(internal_token):
                with open(internal_token, 'r') as f:
                    self.api_key = f.read().strip()
            else:
                logger.warning("No credentials found for Discovery Client.")
    def connect(self):
        try:
            if self.key_path:
                creds = service_account.Credentials.from_service_account_file(self.key_path)
                self.service = build("discoveryengine", "v1beta", credentials=creds)
            elif hasattr(self, 'api_key'):
                self.service = build("discoveryengine", "v1beta", developerKey=self.api_key)
            else:
                self.service = build("discoveryengine", "v1beta")
            logger.info("Discovery Engine Client Connected (v1beta)")
            return True
        except Exception as e:
            logger.error(f"Failed to connect to Discovery Engine: {e}")
            return False
    def search(self, project_id, location, data_store_id, query):
        if not self.service: self.connect()
        try:
            parent = f"projects/{project_id}/locations/{location}/collections/default_collection/dataStores/{data_store_id}/servingConfigs/default_search"
            request = self.service.projects().locations().collections().dataStores().servingConfigs().search(
                servingConfig=parent,
                body={"query": query, "pageSize": 10}
            )
            response = request.execute()
            return response
        except Exception as e:
            logger.error(f"Search failed: {e}")
            return {"error": str(e)}
if __name__ == "__main__":
    client = DiscoveryClient()
    if client.connect():
        print("Discovery Engine Client Enabled.")
    else:
        print("Failed to enable Discovery Engine Client.")
