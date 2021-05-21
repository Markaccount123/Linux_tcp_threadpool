#pragma once 

#include<iostream>
#include<queue>
#include<math.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<string>
#include<map>

#define NUM 5

class Task
{
  public:
    int sock;
    std::map<std::string,std::string> dict;
  public:
    Task()
    {}

    Task(int _sock)
      :sock(_sock)
    {
      dict.insert(std::make_pair("apple","苹果"));
      dict.insert(std::make_pair("banana","香蕉"));
      dict.insert(std::make_pair("student","学生"));
      dict.insert(std::make_pair("boy","男孩"));
      dict.insert(std::make_pair("goat","山羊"));
    }



    
    void Run()
    {
      std::cout<< "task is running "<<std::endl;
      char buf[64];
      ssize_t s = recv(sock,buf,sizeof(buf)-1,0);
      if(s > 0){
        buf[s] = '\0';
        //让他返回翻译好的单词意思，所以还需要定义一个字典
        std::string key = buf;
        //send发送的类型必须是以C语言的形式
        send(sock,dict[key].c_str(),dict[key].size(),0);

        }
      else if(s == 0){
        std::cout<<"Client quit ..."<<std::endl;
        }
      else{
        std::cerr<<"recv error"<<std::endl;
        }
    }
    ~Task()
    {
      std::cout<<"server close sock"<<std::endl;
      close(sock);
    }
};

class ThreadPool
{
  private:
    std::queue<Task*> q; //线程池中需要一个任务队列，server端不断的发数据，线程池中的线程不断的处理数据，那么有可能你的Task越来越大，导致性能的损失，所以为了避免改用指针
    int max_num;//你需要线程池里面有多少个线程
    pthread_mutex_t lock;
    pthread_cond_t cond; //你需要server和线程池中的线程保持同步的关系，当没有任务的时候，需要让线程等待，但是不可以让server端等待，如果它等待问题就大了

  public:
    void LockQueue()
    {
      pthread_mutex_lock(&lock);
    }

    void UnlockQueue()
    {
      pthread_mutex_unlock(&lock);
    }

    bool isEmpty()
    {
      return q.size() == 0;
    }

    void ThreadWait()
    {
      pthread_cond_wait(&cond,&lock);
    }

    void ThreadWakeUp()
    {
      pthread_cond_signal(&cond);
    }
  public:
    ThreadPool(int _max = NUM):max_num(_max)
    {}
    //加了static 那么这个成员函数就属于类，只有一份，也就没有了this指针
    //且static只能够访问static函数，没有的不能访问
    static void *Routine(void *arg)// 此时他作为内部成员函数，第一个默认的参数是this指针，所以这里其实是2个参数   正常来说pthread_create的最后一个参数应该是一个，但是此时却变为了2个
    {
      ThreadPool* this_p = (ThreadPool*)arg;
      //这是线程池里面的线程，要他们一直都存在，不销毁
      while(true)
      {
        //线程池创造出来的目的就是去解决任务，但是又面临这同组线程之间的竞争以及在取任务的时候，server端放，所以需要一把锁
        this_p->LockQueue();
        while(this_p->isEmpty())//这里有可能是会被假唤醒的，所以需要循环的去判断
        {
          this_p->ThreadWait(); 
        }
        //如果任务队列中不是空，那就开始消化任务
        Task *t;
        this_p->Get(&t);
        this_p->UnlockQueue();
        t->Run();//这一步很厉害，我已经把任务从队列中拿出来了，就不需要在占有锁，在临界资源内处理了，可以先进行释放，让任务队列继续工作，我自己处理我拿出来的任务
        delete t;
      }
    }
    void ThreadPoolInit()
    {
      pthread_mutex_init(&lock,nullptr);
      pthread_cond_init(&cond,nullptr);

      pthread_t t[NUM];
      for(int i = 0;i<max_num;++i)
      {
        pthread_create(t+i,nullptr,Routine,this);
      }
    }
    //server
    void Put(Task& in)
    {
      //要往任务队列中塞任务
      LockQueue();
      q.push(&in);
      UnlockQueue();
      
      //至少要唤醒一个线程
      //pthread_cond_signal   pthread_cond_broadcast()唤醒所有的线程
      ThreadWakeUp();
    }

    void Get(Task **out)
    {
      //调用这个函数的时候他本来就已经在临界资源了，所以不需要在加锁了
      Task *t = q.front();
      q.pop();
      *out = t;
    }


    ~ThreadPool()
    {
      pthread_mutex_destroy(&lock);
      pthread_cond_destroy(&cond);
    }
};
