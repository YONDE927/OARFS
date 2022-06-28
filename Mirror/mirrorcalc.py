import uvicorn
from typing import Union, List

from fastapi import FastAPI
from pydantic import BaseModel


class PathList(BaseModel):
    path : List[str]

app = FastAPI()


@app.post("/history/")
async def create_item(files: PathList):
    #calculate something return mirroring file list.
    return files

if __name__ == "__main__":
    uvicorn.run(app, host="127.0.0.1", port=8080)