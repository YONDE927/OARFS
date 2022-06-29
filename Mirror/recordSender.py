import requests
import fcntl
import time

senderflag = 1;

def reporter(file :str, url :str):
    try:
        fd = open(file, 'r+')
    except:
        return None

    while(senderflag):
        fcntl.flock(fd, LOCK_SH)
        path = fd.readlines()
        if(path > 5):
            sendRecords(url, path)
            fd.truncate(0) 
        time.sleep(5)
    return None


def sendRecords(url :str, records :list):
    payload = { path: records}
    r = requests.post(url, data = payload)
    return r
