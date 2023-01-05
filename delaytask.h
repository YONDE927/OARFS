#include <thread>
#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>

#define TRY_INTERVAL 1

class DelayTask {
    public:
        virtual ~DelayTask(){};
        virtual int exec_();
};

class DelayTaskManager{
    public:
        std::queue<std::shared_ptr<DelayTask>> tasks;
        bool term{false};
        bool sterm{false};
        std::thread task_thread;
        std::mutex mtx;
        std::condition_variable cond;
    public: 
        ~DelayTaskManager();
        int stop(bool strict = false);
        int task_loop();
        int run();
        int add_task(std::shared_ptr<DelayTask> task);
};
