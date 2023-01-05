#include "delaytask.h"
#include <memory>

int DelayTask::exec_(){
    printf("%s\n", __FUNCTION__);
    return 0;
}

DelayTaskManager::~DelayTaskManager(){}

int DelayTaskManager::stop(bool strict){
    {
        std::lock_guard<std::mutex> lock(mtx);
        term = true;
        sterm = strict;
        cond.notify_one();
    }
    task_thread.join();
    return 0;
}

int DelayTaskManager::task_loop(){
    while(!term){
        std::unique_lock<std::mutex> lock(mtx);
        cond.wait(lock, [this]{return !tasks.empty() | term;});
        lock.unlock();
        while(!sterm){
            if(tasks.empty()){
                break;
            }
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

#ifdef TEST_DT
class delay_sample_task: public DelayTask {
    public:
        int cnt{0};
        int thres;
    public:
        delay_sample_task(int thres_):thres{thres_}{};
        int exec_() override{
            if(cnt > thres){
                printf("%d\n", cnt);
                return 0;
            }
            printf("%d\n", cnt);
            cnt ++;
            return -1;
        };
};

int main(){
    DelayTaskManager dtm;
    dtm.run();
    std::shared_ptr<DelayTask> dt1(new delay_sample_task(3));
    //std::shared_ptr<DelayTask> dt2(new delay_sample_task(5));
    dtm.add_task(dt1);
    //dtm.add_task(dt2);
    sleep(5); 
    dtm.stop();
    return 0;
}
#endif

