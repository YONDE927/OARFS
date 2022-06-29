import uvicorn
from fastapi import FastAPI

import fileObject

proposeReceiver = FastAPI()

@proposeReceiver.post("/record/")
async def propose(files: PathList):
    #calculate something return mirroring file list.
    return None

if __name__ == "__main__":
    uvicorn.run(proposeReceiver, host="127.0.0.1", port=50081)
