#スレッドを用いて、レシーバーとセンダーを起動
#別プロセスにミラークライアントを実行する。(exec)

import recordReceiver
import proposeSender

def main():
    sender = proposeSender.ProposeSender("http://127.0.0.1:50081/propose/", "record.req")
    sender.run()
    rRthread = recordReceiver.recordRecMain()

    sender.join()
    rRthread.join()

if __name__ == "__main__":
    main()
