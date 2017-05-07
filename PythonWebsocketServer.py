#!/usr/bin/env python

import syslog
import asyncio
import datetime
import websockets

async def time(websocket, path):
  while True:
#        now = datetime.datetime.now().isoformat() + 'Z'
#        await websocket.send(now)
    await websocket.send('post/config/switch/{"Response":{"status":0}}')
#    resp = await websocket.recv()
#    syslog.syslog("< {}".format(resp))
    await asyncio.sleep(5)
    await websocket.send('post/config/switch/{"Response":{"status":1}}')
#    resp = await websocket.recv()
#    syslog.syslog("< {}".format(resp))
    await asyncio.sleep(5)

start_server = websockets.serve(time, 'localhost', 8765)

asyncio.get_event_loop().run_until_complete(start_server)
asyncio.get_event_loop().run_forever()
