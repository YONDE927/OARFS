import uvicorn
from fastapi import FastAPI
import fcntl
import threading

import fileObject

recordReceiver = FastAPI()

@recordReceiver.post("/record/")
async def report(files: fileObject.PathList):
    #calculate something return mirroring file list.
    fd = open("records.req", "a")
    fcntl.flock(fd, fcntl.LOCK_EX)
    for path in files.path:
        fd.write(path)
        fd.write("\n")
    fcntl.flock(fd, fcntl.LOCK_UN)
    fd.close()
    return files

def receiveStarter():
    uvicorn.run(recordReceiver, host="127.0.0.1", port=50082)

def recordRecMain():
    thread = threading.Thread(target=receiveStarter) 
    thread.start()    
    return thread

if __name__ == "__main__":
    uvicorn.run(recordReceiver, host="127.0.0.1", port=50082)
