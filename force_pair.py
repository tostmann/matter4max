import asyncio
from matter_server.server.server import MatterServer
from matter_server.server.helpers.options import MatterServerOptions

async def main():
    options = MatterServerOptions(storage_path="/data")
    server = MatterServer(options)
    await server.start()
    print("Server started")
    try:
        node = await server.device_controller.commission_on_network(setup_pin_code=20202021, filter_type=0, ip_addr="10.10.11.28")
        print(f"Commissioned! Node ID: {node.node_id}")
    except Exception as e:
        print(f"Error: {e}")
    finally:
        await server.stop()

asyncio.run(main())
