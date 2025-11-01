import asyncio
import websockets

async def test():
    uri = "ws://10.229.202.72:5000/upload"
    print(f"Connecting to {uri}...")
    
    try:
        async with websockets.connect(uri) as websocket:
            print("✅ Connected!")
            
            # Send test data
            test_data = b"RIFF" + b"\x00" * 100
            await websocket.send(test_data)
            print(f"✅ Sent {len(test_data)} bytes")
            
            # Wait for response
            response = await asyncio.wait_for(websocket.recv(), timeout=5.0)
            print(f"✅ Received: {response}")
            
    except Exception as e:
        print(f"❌ Error: {e}")

asyncio.run(test())