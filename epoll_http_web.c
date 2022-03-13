#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <ctype.h>
#include"epoll_http_web.h"


//获取一行 \r\n 结尾的数据  因为 http 发送的是以这个结尾的
int get_line(int cfd, char* buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;
    while ((i < size - 1) && (c != '\n')) {
        n = recv(cfd, &c, 1, 0);
        if (n > 0) {
            if (c == '\r') {
                n = recv(cfd, &c, 1, MSG_PEEK);
                if ((n > 0) && (c == '\n')) {
                    recv(cfd, &c, 1, 0);
                }
                else {
                    c = '\n';
                }
            }
            buf[i] = c;
            i++;
        }
        else {
            c = '\n';
        }
    }
    buf[i] = '\0';

    return i;
}

//通过文件名获取文件类型
const char* get_file_type(const char* name)
{
    char* dot;

    //从右往左找‘.’字符，不存在返回空  存在把类型存在 dot 里
    dot = strrchr(name, '.');
    if (dot == NULL)
        return "text/plain; charset=utf-8";
    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
        return "text/html; charset=utf-8";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(dot, ".gif") == 0)
        return "image/gif";
    if (strcmp(dot, ".png") == 0)
        return "image/png";
    if (strcmp(dot, ".css") == 0)
        return "text/css";
    if (strcmp(dot, ".au") == 0)
        return "audio/basic";
    if (strcmp(dot, ".wav") == 0)
        return "audio/wav";
    if (strcmp(dot, ".avi") == 0)
        return "video/x-msvideo";
    if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
        return "video/quicktime";
    if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
        return "video/mpeg";
    if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
        return "model/vrml";
    if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
        return "audio/midi";
    if (strcmp(dot, ".mp3") == 0)
        return "audio/mpeg";
    if (strcmp(dot, ".ogg") == 0)
        return "application/ogg";
    if (strcmp(dot, ".pac") == 0)
        return "application/x-ns-proxy-autoconfig";

    return "text/plain; charset=utf-8";
}

int init_listen_fd(int port, int epfd)
{
    int lfd = socket(AF_INET, SOCK_STREAM, 0);            //创建 socket
    if (lfd == -1)
    {
        perror("socket error");
        exit(1);
    }
    //创建服务器地址结构 ip+port
    struct sockaddr_in ser_addr;
    bzero(&ser_addr, sizeof(ser_addr));   //清空内存

    ser_addr.sin_family = AF_INET;
    ser_addr.sin_port = htons(port);
    ser_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    //端口复用
    int opt = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, (void*)&opt, sizeof(opt));

    //给 lfd 绑定地址结构
    int ret = bind(lfd, (struct sockaddr*)&ser_addr, sizeof(ser_addr));
    if (ret == -1)
    {
        perror("binfd error");
        exit(1);
    }
    ret = listen(lfd, 128);   //设置监听连接上限  
    if (ret == -1)
    {
        perror("listen error");
        exit(1);
    }
    //lfd 添加到树上
    struct epoll_event ev;  //ev 给ctl用
    ev.data.fd = lfd;
    ev.events = EPOLLIN;

    ret = epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev);
    if (ret == -1)
    {
        perror("epoll_ctl add lfd error");
        exit(1);
    }
    return lfd;
}

void do_accept(int lfd, int epfd)
{
    struct sockaddr_in cli_addr;
    socklen_t cli_addr_len = sizeof(cli_addr);

    int cfd = accept(lfd, (struct sockaddr*)&cli_addr, &cli_addr_len);
    if (cfd == -1) {
        perror("accept error");
        exit(1);
    }
    //打印客户端 ip+port
    char cli_ip[64] = { 0 };
    printf("client ip:%s  port:%d \n",
        inet_ntop(AF_INET, &cli_addr.sin_addr.s_addr, cli_ip, sizeof(cli_ip)),
        ntohs(cli_addr.sin_port));

    //设置cfd 非阻塞
    int flag = fcntl(cfd, F_GETFL);
    flag |= O_NONBLOCK;
    fcntl(cfd, F_SETFL, flag);

    //将新节点 cfd 挂到监听树上 
    struct epoll_event ev;  //ev 给 ctl用
    ev.data.fd = cfd;
    //边沿非阻塞模式
    ev.events = EPOLLIN | EPOLLET;
    int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);

    if (ret == -1)
    {
        perror("epoll_ctl add cfd error");
        exit(1);
    }
}
//断开连接
void discon(int cfd, int epfd)
{
    int ret = epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, NULL);
    if (ret != 0) {
        perror("epoll_ctl error");
        exit(1);
    }
    close(cfd);
}

//    HTTP/1.1 200 OK
//    Content - Type : text / html; charset = ISO - 8859 - 1
//    Content - Length: 122
//    \r\n 空行 也是必须的
//应答协议头     百度一下  根据协议头特性传参 三行必须 外加一行文件长度 
//              应答回去客户端的fd ， 错误号 ，错误描述符，回发文件类型，文件长度
void  send_respond(int cfd, int nob, char* disp, const char* type, int len)
{
    char buf[1024] = { 0 };
    sprintf(buf, "HTTP/1.1 %d %s\r\n", nob, disp);
    send(cfd, buf, strlen(buf), 0);
    sprintf(buf + strlen(buf), "Content-Type: %s\r\n", type);
    sprintf(buf + strlen(buf), "Content-Length:%d\r\n", len);
    send(cfd, buf, strlen(buf), 0);
    
    send(cfd, "\r\n", 2, 0);

}

//打开被请求的文件 发送文件里的数据给浏览器cfd
void send_filedate(int cfd, const char* file)
{
    int n = 0, ret = 0;
    char buf[4094];
    int fd = open(file, O_RDONLY);
    if (fd == -1) {
        // 404 打开文件错误
        perror("open error");
        exit(1);
    }
    while ((n = read(fd, buf, sizeof(buf))) > 0)      //如果有数据
    {
        ret = send(cfd, buf, n, 0);
        if (ret == -1) {
            printf("error = %d \n", errno);
            if (errno == EAGAIN) {
                printf("----------------EAGAIN\n");
                continue;
            }
            else if (errno == EINTR) {
                printf("----------------EINTR\n");
                continue;
            }
            else {
                perror("send error");
                //exit(1);
                break;
            }

        }
    }

    if (n == -1)
    {
        perror("read file error:");
    }

    close(fd);
}

//处理http请求  判断请求的是文件还是目录 回发
void  http_request(int cfd, const char* requst)
{
    //拆分 http 请求行
    char method[16], path[256], protocol[16];
    sscanf(requst, "%[^ ] %[^ ] %[^ ]", method, path, protocol);  //正则表达式
    printf("m=%s ,p=%s ,pro=%s", method, path, protocol);

    //解码  处理浏览器发过来的数据解码成中文
    decode_str(path, path);

    char* filepath = path + 1;  //  path 代表的是 /hello.c  

    //如果访问未指定路径   添加默认目录
    if (strcmp(path, "/") == 0) {
        filepath = "./";
    }

    //获取文件属性
    struct stat sbuf;
    int ret = stat(filepath, &sbuf);
    if (ret != 0) {
        send_error(cfd, 404, "Not Found", "NO such file or direntry");
        //perror("stat");  //文件不存在并返回错误信息
        //return;
    }
    if (S_ISREG(sbuf.st_mode)) {     //是一个普通文件
        //回发 http应答协议头
        send_respond(cfd, 200, "OK", get_file_type(filepath), -1);
        //回发客户端请求的数据内容
        send_filedate(cfd, filepath);
    }
    else if (S_ISDIR(sbuf.st_mode)) { //是目录
       //发送头信息
        send_respond(cfd, 200, "OK", get_file_type(".html"), sbuf.st_size);
        //发送目录信息
        send_dir(cfd, filepath);
    }

}

//发送目录内容
void send_dir(int cfd, const char* dirname)
{
    int i, ret;
    //拼一个 html 页面
    char buf[4094] = { 0 };

    sprintf(buf, "<html><head><title>目录名: %s</title></head>", dirname);
    sprintf(buf + strlen(buf), "<body><h1>当前目录: %s</h1><table>", dirname);

    char enstr[1024] = { 0 };
    char path[1024] = { 0 };

    //快速获取当前目录下的所有项存入ptr  递归目录
    struct dirent** ptr;
    int num = scandir(dirname, &ptr, NULL, alphasort());  //linux下 alphasort不能带（）

    //遍历ptr
    for (i = 0; i < num; ++i)
    {
        char* name = ptr[i]->d_name;

        //拼接文件的完整路径
        sprintf(path, "%s/%s", dirname, name);
        printf("path = %s ==================\n", path);

        struct stat st;
        stat(path, &st);

        //在浏览器显示发送过去的中文需要编码
        encode_str(enstr, sizeof(enstr), name);

        if (S_ISREG(st.st_mode)) {     //是一个普通文件
            sprintf(buf + strlen(buf),
                "<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>",
                enstr, name, (long)st.st_size);
        }
        else if (S_ISDIR(st.st_mode)) { //是目录
            sprintf(buf + strlen(buf),
                "<tr><td><a href=\"%s/\">%s/</a></td><td>%ld</td></tr>",
                enstr, name, (long)st.st_size);
        }
        ret = send(cfd, buf, strlen(buf), 0);
        if (ret == -1) {
            if (errno == EAGAIN) {
                perror("send error: ");
                continue;
            }
            else if (errno == EINTR) {
                perror("send error: ");
                continue;
            }
            else {
                perror("send error: ");
                //exit(1);
                break;
            }

        }
        memset(buf, 0, sizeof(buf)); //将buf 后sizeof(buf)个字节 设为 0 就是清空的意思
        
    }

    sprintf(buf + strlen(buf), "</table></body></html>");
    send(cfd, buf, strlen(buf), 0);

    printf("dir message send ok!! \n");
}
void do_read(int cfd, int epfd)
{
    //读 http 的状态行信息  拆分 获取get 文件名 /hello.c 协议号 HTTP/1.1
    char line[1024] = { 0 };
    int len = get_line(cfd, line, sizeof(line));  //获取 cfd的请求协议行 放在 line中
    if (len == 0) {
        printf("客服端关闭 \n");
        discon(cfd, epfd);
    }
    else {
        printf("=================== 请求头 ================\n");
        printf("请求行数据: %s", line);

        //读取客户端后续发来的所有数据
        while (1) {
            char buf[1024] = { 0 };
            len = get_line(cfd, buf, sizeof(buf));
            if (buf[0] == '\n') {
                break;
            }
            else if (len == -1)
            {
                break;
            }
        }           
        printf("====================== The End ==================\n");
    }
    //判断一下是不是 get 请求  strncasecmp 忽略大小写比较
    if (strncasecmp(line, "GET", 3) == 0) {
        http_request(cfd, line);      //处理http请求  判断文件是否存在 回发
        discon(cfd, epfd); //浏览器一个会发送多个请求什么小图标啊等等 这个太简便让他通过一个就关闭 就不会有错误页面
    }
    return;
}

//==================================
// 16进制转换10进制
int hexit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return 0;
}

/*
浏览器编解码
处理 %20 之类的东西
*/
//编码
void encode_str(char* to, int tosize, const char* from)
{
    int tolen;
    for (tolen = 0; *from != '\0' && tolen + 4 < tosize; ++from) {
        if (isalnum(*from) || strchr("/_.-~", *from) != (char*)0) {
            *to = *from;
            ++to;
            ++tolen;
        }
        else {
            sprintf(to, "%%%02x", (int)*from & 0xff);
            to += 3;
            tolen += 3;
        }
    }
    *to = '\0';
}
//解码
void decode_str(char* to, char* from)
{
    for (; *from != '\0'; ++to, ++from) {
        if (from[0] == '%' && isxdigit(from[1]) && isxdigit(from[2])) {
            *to = hexit(from[1]) * 16 + hexit(from[2]);
            from += 2;
        }
        else {
            *to = *from;
        }
    }
    *to = '\0';
}
//错误页面
void send_error(int cfd, int status, char* title, char* text)
{
    char buf[4096] = { 0 };

    sprintf(buf, "%s %d %s\r\n", "HTTP/1.1", status, title);
    sprintf(buf + strlen(buf), "Content-Type: %s\r\n", "text/html");
    sprintf(buf + strlen(buf), "Content-Length: %d\r\n", -1);
    sprintf(buf + strlen(buf), "Connection: close\r\n");
    sprintf(buf + strlen(buf), "\r\n");
    send(cfd, buf, strlen(buf), 0);

    memset(buf, 0, sizeof(buf));

    sprintf(buf, "<html><head><title>%d %s</title></head>\n", status, title);
    sprintf(buf + strlen(buf), "<body bgcolor=\"#cc99cc\"><h2 align=\"center\">%d %s</h4>\n", status, title);
    sprintf(buf + strlen(buf), "%s\n", text);
    sprintf(buf + strlen(buf), "<hr>\n</body>\n</html>\n");
    send(cfd, buf, strlen(buf), 0);

    return;
}

void* epoll_run(int port)
{
    int i = 0;
    struct epoll_event all_events[MAXSIZE];

    int epfd = epoll_create(1024);   //创建红黑树 设置最多节点1024 返回根节点的 fd
    if (epfd == -1)
    {
        perror("epoll_create error");
        exit(1);
    }
    //将初始化的套接字添加到监听树上
    int lfd = init_listen_fd(port, epfd);

    while (1) {
        //监听对应节点事件
        int ret = epoll_wait(epfd, all_events, MAXSIZE, -1);
        if (ret == -1)
        {
            perror("epoll_wait error");
            exit(1);
        }

        for (i = 0; i < ret; ++i)
        {
            //因为做web服务器  只处理读事件 其他默认不处理
            struct epoll_event* pev = &all_events[i];

            //不是读事件的处理
            if (!(pev->events & EPOLLIN))
            {
                continue;
            }
            if (pev->data.fd == lfd)    //说明有想建立连接的客户端
            {
                do_accept(lfd, epfd);
            }
            else {
                do_read(pev->data.fd, epfd);    //说明已有的客户端想进行读写操作
            }
        }
    }
}



