
import time

fd = open("records.req", "r+")
while(1):
    fd.seek(0)
    x = fd.readlines()
    print(x)
    time.sleep(5)
