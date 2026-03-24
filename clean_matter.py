import re

with open("/opt/ai_builder_data/users/763684262/projects/MAX/main/matter_bridge.cpp", "r") as f:
    text = f.read()

# 1. Remove app_attribute_update_cb toggle logic
text = re.sub(r'if \(endpoint_id == dev->ep_auto\) \{.*?(?=else if \(endpoint_id == dev->endpoint_id && dev->type == MAX_DEV_TYPE_PLUG_ADAPTER\))', '', text, flags=re.DOTALL)

# 2. Remove child switch update logic
text = re.sub(r'// Update child switches\s+if \(dev->ep_auto > 0\) \{[\s\S]*?\}\s+if \(dev->ep_boost > 0\) \{[\s\S]*?\}', '', text)

# 3. Remove child switch creation logic
text = re.sub(r'// --- Add Child Switch for Auto Mode ---[\s\S]*?ESP_LOGI\(TAG, " -> Added Boost Switch \(Endpoint %d\)", device->ep_boost\);\s+\}', '', text)

with open("/opt/ai_builder_data/users/763684262/projects/MAX/main/matter_bridge.cpp", "w") as f:
    f.write(text)
