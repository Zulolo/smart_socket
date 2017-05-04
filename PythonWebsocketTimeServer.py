#!/usr/bin/env python

import asyncio
import datetime
import random
import websockets

async def time(websocket, path):
  while True:
#        now = datetime.datetime.now().isoformat() + 'Z'
#        await websocket.send(now)
    await websocket.send('{"command":{"switch":{"status":0}}}')
    await asyncio.sleep(5)
    await websocket.send('{"command":{"switch":{"status":1}}}')
    await asyncio.sleep(5)

start_server = websockets.serve(time, 'localhost', 8765)

asyncio.get_event_loop().run_until_complete(start_server)
asyncio.get_event_loop().run_forever()