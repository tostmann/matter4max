import sys

with open("/opt/ai_builder_data/users/763684262/projects/MAX/main/matter_bridge.cpp", "r") as f:
    lines = f.readlines()

out = []
skip = False
for i, line in enumerate(lines):
    # Remove update block
    if "if (endpoint_id == dev->ep_auto) {" in line:
        skip = True
        continue
    if skip and "} else if (endpoint_id == dev->endpoint_id && dev->type == MAX_DEV_TYPE_PLUG_ADAPTER) {" in line:
        skip = False
        out.append("                if (endpoint_id == dev->endpoint_id && dev->type == MAX_DEV_TYPE_PLUG_ADAPTER) {\n")
        continue
    
    # Remove state updates
    if "if (dev->ep_auto > 0) {" in line:
        skip = True
        continue
    if skip and "if (dev->ep_boost > 0) {" in line:
        continue
    if skip and "attribute::update(dev->ep_boost" in line:
        continue
    if skip and line.strip() == "}" and len(out) > 0 and "attribute::update" in lines[i-1]:
        skip = False
        continue
        
    # Remove endpoint creation
    if "// --- Add Child Switch for Auto Mode ---" in line:
        skip = True
        continue
    if skip and "device->ep_boost = endpoint::get_id(ep_boost);" in line:
        # read next two lines to skip log and closing brace
        continue
    if skip and "Added Boost Switch" in line:
        continue
    if skip and line.strip() == "}" and len(out) > 0 and "Added Boost Switch" in lines[i-1]:
        skip = False
        continue

    if not skip:
        out.append(line)

with open("/opt/ai_builder_data/users/763684262/projects/MAX/main/matter_bridge.cpp", "w") as f:
    f.writelines(out)
