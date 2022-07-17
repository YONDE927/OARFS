from pathlib import Path
import time

class Mypath():
    def __init__(self, path):
        self.path = Path(str(path)).resolve()
    
    def __repr__(self):
        return str(self.path)

    def __str__(self):
        return str(self.path)

    def myls(self):
        files = []
        if self.path.is_dir():
            files = [ Mypath(x) for x in self.path.iterdir() ]
        else:
            files = [ Mypath(x) for x in self.path.parent.iterdir() ]
        return files

    def mylsf(self):
        files = self.myls()
        return list(filter(lambda x: x.path.is_file(), files))

    def sizeofpath(self):
        if self.path.is_file() :
            #print(str(self.path), " : ", self.path.stat().st_size)
            return self.path.stat().st_size
        elif  self.path.is_dir() :
            return sum([ x.sizeofpath() for x in self.myls()])

    def sizeofwd(self):
        size = 0
        for fi in self.myls():
            if(fi.path.is_file()):
                size += fi.path.stat().st_size
        return size

if __name__ == "__main__":
    mp = Mypath(".")
    print(mp.myls())
    print(mp.mylsf())
    print("sizeofpath: ", mp.sizeofpath())
    print("sizeofwd: ", mp.sizeofwd())
