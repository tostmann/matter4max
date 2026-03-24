import asyncio
import json
import websockets

async def main():
    async with websockets.connect("ws://127.0.0.1:5580/ws") as ws:
        await ws.recv()
        cmd = {
            "message_id": "1",
            "command": "get_nodes",
            "args": {}
        }
        await ws.send(json.dumps(cmd))
        while True:
            response = await ws.recv()
            res = json.loads(response)
            if "error_code" in res or "result" in res:
                with open("nodes.json", "w") as f:
                    json.dump(res, f, indent=2)
                break

asyncio.run(main())
