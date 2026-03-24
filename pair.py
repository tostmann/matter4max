import asyncio
import json
import websockets

async def main():
    async with websockets.connect("ws://127.0.0.1:5580/ws") as ws:
        await ws.recv()
        cmd = {
            "message_id": "1",
            "command": "commission_with_code",
            "args": {
                "code": "34970112332"
            }
        }
        await ws.send(json.dumps(cmd))
        print("Sent commission request...")
        while True:
            response = await ws.recv()
            print(f"Received: {response}")
            res = json.loads(response)
            if "error_code" in res or "result" in res:
                break

asyncio.run(main())
