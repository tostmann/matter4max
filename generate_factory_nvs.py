import os
import subprocess
import sys

def create_factory_nvs(pin, salt, verifier, discriminator, size="0x10000"):
    csv_content = f"""key,type,encoding,value
chip-factory,namespace,,
pin-code,data,u32,{pin}
iteration-count,data,u32,1000
salt,data,string,{salt}
verifier,data,string,{verifier}
discriminator,data,u32,{discriminator}
"""
    with open("factory_nvs.csv", "w") as f:
        f.write(csv_content)
        
    print("[+] factory_nvs.csv erstellt.")
    
    # Aufruf von nvs_partition_gen.py aus dem ESP-IDF
    idf_path = "/root/.platformio/packages/framework-espidf"
    gen_script = os.path.join(idf_path, "components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py")
    
    cmd = [
        "python3", gen_script,
        "generate",
        "factory_nvs.csv",
        "factory_nvs.bin",
        size
    ]
    
    try:
        subprocess.run(cmd, check=True)
        print("[+] factory_nvs.bin erfolgreich generiert!")
        print("\n=== FLASH BEFEHL ===")
        print(f"esptool.py --port /dev/ttyACM1 write_flash 0x9000 factory_nvs.bin")
        print("====================")
    except subprocess.CalledProcessError as e:
        print(f"[-] Fehler beim Generieren der NVS-Partition: {e}")

if __name__ == "__main__":
    # Werte aus unserem vorherigen Durchlauf
    create_factory_nvs(
        pin="29644537",
        salt="/nd5Fi/XSNmg7pUu1tn9EQ==",
        verifier="cir74WH+smG8d3agLS+riQHdYwlaV2RfPFQ25+QG8O4E2kVtiu1tp6AEcfyX4a/+WQs20aS9usYL8sCws297Gy54fBhkTFprIc1rLcWS3Uvo/ME+oXG6hS5h4rlzWkWIQA==",
        discriminator="3840"
    )
