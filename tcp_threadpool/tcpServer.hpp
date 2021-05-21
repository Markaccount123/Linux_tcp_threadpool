#pragma once 

#include<iostream>
#include<cstdlib>
#include<cstring>
#include<unistd.h>
#include<string>
#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<pthread.h>
#include"ThreadPool.hpp"
#define BACKLOG 5 //一般这个的值都设置的比较小，表示的意思是此时底层链接队列中等待链接的长度
class tcpServer{
  private:
    int port;
    int lsock;//表示监听的套接字
    ThreadPool *tp;
  public:

    tcpServer(int _port = 8080,int _lsock = -1)
      :port(_port),lsock(_lsock)
    {}

    void initServer()
    {
      lsock = socket(AF_INET,SOCK_STREAM,0);
      if(lsock < 0){
        std::cerr<<"socket error"<<std::endl;
        exit(2);
      }
      struct sockaddr_in local;
      local.sin_family = AF_INET;
      local.sin_port = htons(port);
      local.sin_addr.s_addr = htonl(INADDR_ANY);

      if(bind(lsock,(struct sockaddr*)&local,sizeof(local)) < 0){
        std::cerr<<"bind error!"<<std::endl;
        exit(3);
      }

      //走到这里说明bind成功了，但是tcp是面向链接的，所以还需要一个接口
      if(listen(lsock,BACKLOG) < 0){
        std::cout<<"listen error!"<<std::endl;
        exit(4);
      } 
      
      //tcp必须要将套接字设置为监听状态  这里可以想夜间你去吃饭，什么时候去都可以吃到，是为什么？是因为店里面一直有人在等待着你
      //任意时间有客户端来连接我
      //成功返回0，失败返回-1
      tp = new ThreadPool();
      tp->ThreadPoolInit();//初始化threadpool,创造出来一批线程
    }

    //echo服务器
    static void service(int sock)
    {
      while(true){
        //打开一个文件，也叫打开一个流，所以这里其实使用read和write也是可以的，但是这里是网络，最好使用tcp的recv和send
        char buf[64];
        ssize_t s = recv(sock,buf,sizeof(buf)-1,0);
        if(s > 0){
          buf[s] = '\0';
          std::cout<<"Client# "<< buf <<std::endl;
          send(sock,buf,strlen(buf),0);//此时的网络就是文件，你往文件中发送东西的格式，可不是以\0作为结束，加入\0会出现乱码的情况
        }
        else if(s == 0){
          std::cout<<"Client quit ..."<<std::endl;
          break;
        }else{
          std::cerr<<"recv error"<<std::endl;
          break;
        }
      }
      close(sock);
    }

    static void *serviceRoutine(void *arg)
    {
      pthread_detach(pthread_self());//采用线程分离，运行完了以后自动释放。
      std::cout<<"create a new thread"<<std::endl;
      int *p = (int*)arg;
      int sock =  *p; 
      service(sock);
      delete p;
    }

    void start()
    {
      struct sockaddr_in endpoint;
      while(true){
        socklen_t len = sizeof(endpoint);
        int sock = accept(lsock,(struct sockaddr*)&endpoint,&len);
        if(sock < 0){
          std::cerr<<"accept error!"<<std::endl;
          continue; //相当于拉客失败了，我需要继续拉客
        }
        std::string cli_info = inet_ntoa(endpoint.sin_addr);
        cli_info += ":";
        cli_info += std::to_string(endpoint.sin_port); //这里获取的端口号是一个整数，需要把该整数转换为字符串,当然这里也可以使用stringstream 比如定义一个对象 ， 此时有一个int变量，一个string 对象 你把int输入stringstream 然后再把stringstream在输入string的对象中，就进行了转换
        std::cout<<"get a new link "<< cli_info<<" sock: "<<sock<<std::endl;


        //当获得一个新链接的时候，就不要创建新线程等，只需要构建一个任务，然后把这个任务push到线程池中
        //线程池里面的线程都是从任务队列中取的，所以接收到的连接应该打包成为一个Task任务包
        Task *t = new Task(sock);
        tp->Put(*t);
      }
    }

    ~tcpServer()
    {}
};
