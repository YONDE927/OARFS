#include <thread>
#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>

#define TRY_INTERVAL 1

class DelayTask {
    public:
        virtual int exec_();
};

class DelayTaskManager{
    public:
        std::queue<std::shared_ptr<DelayTask>> tasks;
        bool term_dt{false};
        std::thread task_thread;
        std::mutex mtx;
        std::condition_variable cond;
    public: 
        ~DelayTaskManager();
        int task_loop();
        int run();
        int add_task(std::shared_ptr<DelayTask> task);
};

DelayTaskManager::~DelayTaskManager(){
    std::lock_guard<std::mutex> lock(mtx);
    term_dt = true;
    cond.notify_one();
    task_thread.join();
}

int DelayTaskManager::task_loop(){
    while(!term_dt){
        std::unique_lock<std::mutex> lock(mtx);
        cond.wait(lock, [this]{ return !tasks.empty(); });
        while(!term_dt){
            if(tasks.empty())
                break;
            std::shared_ptr<DelayTask> task = tasks.front();
            if(task->exec_() < 0){
                sleep(TRY_INTERVAL);
                continue;
            }
            tasks.pop();
        }
    }
    return 0;
}

int DelayTaskManager::run(){
    task_thread = std::thread(&DelayTaskManager::task_loop, this);
    return 0;
}

int DelayTaskManager::add_task(std::shared_ptr<DelayTask> task){
    std::lock_guard<std::mutex> lock(mtx);
    tasks.push(task);
    cond.notify_one();
    return 0;
}


