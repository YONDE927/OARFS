import uvicorn
from fastapi import FastAPI

import fileObject

recordReceiver = FastAPI()

@recordReceiver.post("/record/")
async def report(files: PathList):
    #calculate something return mirroring file list.
    return None

if __name__ == "__main__":
    uvicorn.run(recordReceiver, host="127.0.0.1", port=50081)
