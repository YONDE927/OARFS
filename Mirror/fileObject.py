from typing import Union, List

from pydantic import BaseModel

class PathList(BaseModel):
    path: List[str]
