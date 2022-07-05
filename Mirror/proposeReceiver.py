import fcntl
import uvicorn
from fastapi import FastAPI
import threading

import fileObject

proposeReceiver = FastAPI()

@proposeReceiver.post("/propose/")
async def propose(files : fileObject.PathList):
    #calculate something return mirroring file list.
    fd = open("mirror.req", "a")
    fcntl.flock(fd, fcntl.LOCK_EX)
    for path in files.path:
        print("received propose: ", path)
        fd.write(path)
        fd.write("\n")
    fcntl.flock(fd, fcntl.LOCK_UN)
    fd.close()
    return files

def receiveStarter():
    uvicorn.run(proposeReceiver, host="127.0.0.1", port=50081)

def proposeRecMain():
    thread = threading.Thread(target=receiveStarter) 
    thread.daemon = True
    thread.start()    
    return thread

if __name__ == "__main__":
    proposeRecMain()
