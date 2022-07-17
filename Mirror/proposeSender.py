import requests
import fcntl
import time
import os
import threading
import json
from schedulModel import ScheduleModel

class ProposeSender:
    def __init__(self, url, file):
        self.url = url
        self.file = file
        self.flag = 1
        self.model = ScheduleModel()
        fd = open(self.file, "a")
        fd.close()
            

    def propose(self):
        while(self.flag):
            print("ProposeSender loop")
            fd = open(self.file, "r+")
            props = []
            fcntl.flock(fd, fcntl.LOCK_EX)
            path = [ s.replace('\n','') for s in fd.readlines() ]
            if(len(path) > 0):
                props = self.model.calc(path)
                sendPropose(self.url, props)
                fd.truncate(0)
            fcntl.flock(fd, fcntl.LOCK_UN)
            fd.close()
            time.sleep(5)
        return None

    def run(self):
        self.thread = threading.Thread(target=self.propose) 
        self.thread.start()                                  

    def join(self):
        self.thread.join()

def sendPropose(url :str, propose :list):
    tryout = 0
    payload = { "path" : propose }

    print("sending Propose :", json.dumps(payload))
    while(tryout < 5):
        try:
            r = requests.post(url, data = json.dumps(payload))
            return r
        except:
            tryout += 1
            time.sleep(5)

