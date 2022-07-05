import requests
import fcntl
import time
import psycopg2
import threading

class RecordSender:
    def __init__(self, dbname, user, url):
        self.conn = psycopg2.connect("dbname=%s user=%s"%(dbname, user))
        self.cur = self.conn.cursor()
        self.dbname = dbname;
        self.url = url
        self.flag = 1
    def __del__(self):
        try:
            self.cur.close()
            self.conn.close()
        except:
            pass
    #DB系操作
    def num_records(self):
        self.cur.execute("select count(*) from test;");
        return self.cur.fetchone()[0]

    def pop_records(self, num :int):
        self.cur.execute("select * from records order by id limit %s",(num,))
        paths = self.cur.fetchall()
        self.cur.execute("delete from records where id IN (select id from records order by id limit %s);",(num,))
        return [i[2] for i in paths]

    def report(self):
        while(self.flag):
            print("RecordSender loop")
            size = self.num_records()
            if(size > 5):
                path = self.pop_records(5)
                sendRecords(self.url, path)
            time.sleep(5)
        return None

    def run(self):
        self.thread = threading.Thread(target=self.report) 
        self.thread.daemon = True                            
        self.thread.start()                                  

    def join(self):
        self.thread.join()

def sendRecords(url :str, records :list):
    tryout = 0
    payload = { "path" : records }
    while(tryout < 5):
        try:
            r = requests.post(url, data = json.dumps(payload))
            return r
        except:
            tryout += 1
            time.sleep(5)

if __name__ == "__main__":
    sender = RecordSender("yonde", "yonde", "127.0.0.1")
    print(sender.num_records())
    print(sender.pop_records(3))
    
