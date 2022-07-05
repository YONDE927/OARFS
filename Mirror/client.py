#スレッドを用いて、レシーバーとセンダーを起動
#別プロセスにミラークライアントを実行する。(exec)
import recordSender
import proposeReceiver

def main():
    sender = recordSender.RecordSender("yonde", "yonde", "http://127.0.0.1:50082/record/")
    sender.run()
    pRthread = proposeReceiver.proposeRecMain()

    sender.join()
    pRthread.join()

if __name__ == "__main__":
    main()
