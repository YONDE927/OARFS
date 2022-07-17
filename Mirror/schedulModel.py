from path import Mypath

class ScheduleModel:
    def __init__(self):
        pass

    def calc(self, pathlist :list):
        return self.calc1(pathlist)

    def calc1(self, pathlist :list):
        size = 0
        output = []
        files = [Mypath(x) for x in pathlist]
        for fi in files:
            size = fi.sizeofwd()
            if(size < 100000000):
                output += [str(p) for p in fi.mylsf()]
        return list(set(output))

if __name__ == "__main__":
    model = ScheduleModel()

    records = ["./path.py", "../client.sh"]

    preds = model.calc(records)

    print(preds)
