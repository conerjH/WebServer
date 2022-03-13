#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include"epoll_http_web.h"

int main(int argc, char *argv[])
{
    // 命令行参数获取 端口和 server提供的目录
    if (argc < 3)
    {
        printf("./a.out port path\n");
        return 0;
    }
    //获取用户输入端口
    int port = atoi(argv[1]);

    //改变进程工作目录
    int ret = chdir(argv[2]);
    if (ret != 0)
    {
        perror("chdir error");
        exit(1);
    }
    //启动 epoll监听
    epoll_run(port);
    return 0;
}