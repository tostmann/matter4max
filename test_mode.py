import urllib.request
import json
import time

TOKEN = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiI0MzgzOTcxYzYyZDQ0NTM3OWNlNmQ3ZjBkYzUwNzk3NiIsImlhdCI6MTc3NDI2NTU4OCwiZXhwIjoyMDg5NjI1NTg4fQ.srn3sfwQlhOoMZy_peEVvQdHsVBfP6oJTnjvhmxU1R0"
HASS_URL = "http://localhost:8123/api/services/climate/set_hvac_mode"

req = urllib.request.Request(HASS_URL, 
                             data=json.dumps({"entity_id": "climate.max_thermo_01ca92", "hvac_mode": "off"}).encode('utf-8'),
                             headers={'Authorization': f'Bearer {TOKEN}', 'Content-Type': 'application/json'})
urllib.request.urlopen(req)
print("Set to off!")
