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


//��ȡһ�� \r\n ��β������  ��Ϊ http ���͵����������β��
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

//ͨ���ļ�����ȡ�ļ�����
const char* get_file_type(const char* name)
{
    char* dot;

    //���������ҡ�.���ַ��������ڷ��ؿ�  ���ڰ����ʹ��� dot ��
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
    int lfd = socket(AF_INET, SOCK_STREAM, 0);            //���� socket
    if (lfd == -1)
    {
        perror("socket error");
        exit(1);
    }
    //������������ַ�ṹ ip+port
    struct sockaddr_in ser_addr;
    bzero(&ser_addr, sizeof(ser_addr));   //����ڴ�

    ser_addr.sin_family = AF_INET;
    ser_addr.sin_port = htons(port);
    ser_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    //�˿ڸ���
    int opt = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, (void*)&opt, sizeof(opt));

    //�� lfd �󶨵�ַ�ṹ
    int ret = bind(lfd, (struct sockaddr*)&ser_addr, sizeof(ser_addr));
    if (ret == -1)
    {
        perror("binfd error");
        exit(1);
    }
    ret = listen(lfd, 128);   //���ü�����������  
    if (ret == -1)
    {
        perror("listen error");
        exit(1);
    }
    //lfd ��ӵ�����
    struct epoll_event ev;  //ev ��ctl��
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
    //��ӡ�ͻ��� ip+port
    char cli_ip[64] = { 0 };
    printf("client ip:%s  port:%d \n",
        inet_ntop(AF_INET, &cli_addr.sin_addr.s_addr, cli_ip, sizeof(cli_ip)),
        ntohs(cli_addr.sin_port));

    //����cfd ������
    int flag = fcntl(cfd, F_GETFL);
    flag |= O_NONBLOCK;
    fcntl(cfd, F_SETFL, flag);

    //���½ڵ� cfd �ҵ��������� 
    struct epoll_event ev;  //ev �� ctl��
    ev.data.fd = cfd;
    //���ط�����ģʽ
    ev.events = EPOLLIN | EPOLLET;
    int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);

    if (ret == -1)
    {
        perror("epoll_ctl add cfd error");
        exit(1);
    }
}
//�Ͽ�����
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
//    \r\n ���� Ҳ�Ǳ����
//Ӧ��Э��ͷ     �ٶ�һ��  ����Э��ͷ���Դ��� ���б��� ���һ���ļ����� 
//              Ӧ���ȥ�ͻ��˵�fd �� ����� ���������������ط��ļ����ͣ��ļ�����
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

//�򿪱�������ļ� �����ļ�������ݸ������cfd
void send_filedate(int cfd, const char* file)
{
    int n = 0, ret = 0;
    char buf[4094];
    int fd = open(file, O_RDONLY);
    if (fd == -1) {
        // 404 ���ļ�����
        perror("open error");
        exit(1);
    }
    while ((n = read(fd, buf, sizeof(buf))) > 0)      //���������
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

//����http����  �ж���������ļ�����Ŀ¼ �ط�
void  http_request(int cfd, const char* requst)
{
    //��� http ������
    char method[16], path[256], protocol[16];
    sscanf(requst, "%[^ ] %[^ ] %[^ ]", method, path, protocol);  //������ʽ
    printf("m=%s ,p=%s ,pro=%s", method, path, protocol);

    //����  ��������������������ݽ��������
    decode_str(path, path);

    char* filepath = path + 1;  //  path ������� /hello.c  

    //�������δָ��·��   ���Ĭ��Ŀ¼
    if (strcmp(path, "/") == 0) {
        filepath = "./";
    }

    //��ȡ�ļ�����
    struct stat sbuf;
    int ret = stat(filepath, &sbuf);
    if (ret != 0) {
        send_error(cfd, 404, "Not Found", "NO such file or direntry");
        //perror("stat");  //�ļ������ڲ����ش�����Ϣ
        //return;
    }
    if (S_ISREG(sbuf.st_mode)) {     //��һ����ͨ�ļ�
        //�ط� httpӦ��Э��ͷ
        send_respond(cfd, 200, "OK", get_file_type(filepath), -1);
        //�ط��ͻ����������������
        send_filedate(cfd, filepath);
    }
    else if (S_ISDIR(sbuf.st_mode)) { //��Ŀ¼
       //����ͷ��Ϣ
        send_respond(cfd, 200, "OK", get_file_type(".html"), sbuf.st_size);
        //����Ŀ¼��Ϣ
        send_dir(cfd, filepath);
    }

}

//����Ŀ¼����
void send_dir(int cfd, const char* dirname)
{
    int i, ret;
    //ƴһ�� html ҳ��
    char buf[4094] = { 0 };

    sprintf(buf, "<html><head><title>Ŀ¼��: %s</title></head>", dirname);
    sprintf(buf + strlen(buf), "<body><h1>��ǰĿ¼: %s</h1><table>", dirname);

    char enstr[1024] = { 0 };
    char path[1024] = { 0 };

    //���ٻ�ȡ��ǰĿ¼�µ����������ptr  �ݹ�Ŀ¼
    struct dirent** ptr;
    int num = scandir(dirname, &ptr, NULL, alphasort());  //linux�� alphasort���ܴ�����

    //����ptr
    for (i = 0; i < num; ++i)
    {
        char* name = ptr[i]->d_name;

        //ƴ���ļ�������·��
        sprintf(path, "%s/%s", dirname, name);
        printf("path = %s ==================\n", path);

        struct stat st;
        stat(path, &st);

        //���������ʾ���͹�ȥ��������Ҫ����
        encode_str(enstr, sizeof(enstr), name);

        if (S_ISREG(st.st_mode)) {     //��һ����ͨ�ļ�
            sprintf(buf + strlen(buf),
                "<tr><td><a href=\"%s\">%s</a></td><td>%ld</td></tr>",
                enstr, name, (long)st.st_size);
        }
        else if (S_ISDIR(st.st_mode)) { //��Ŀ¼
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
        memset(buf, 0, sizeof(buf)); //��buf ��sizeof(buf)���ֽ� ��Ϊ 0 ������յ���˼
        
    }

    sprintf(buf + strlen(buf), "</table></body></html>");
    send(cfd, buf, strlen(buf), 0);

    printf("dir message send ok!! \n");
}
void do_read(int cfd, int epfd)
{
    //�� http ��״̬����Ϣ  ��� ��ȡget �ļ��� /hello.c Э��� HTTP/1.1
    char line[1024] = { 0 };
    int len = get_line(cfd, line, sizeof(line));  //��ȡ cfd������Э���� ���� line��
    if (len == 0) {
        printf("�ͷ��˹ر� \n");
        discon(cfd, epfd);
    }
    else {
        printf("=================== ����ͷ ================\n");
        printf("����������: %s", line);

        //��ȡ�ͻ��˺�����������������
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
    //�ж�һ���ǲ��� get ����  strncasecmp ���Դ�Сд�Ƚ�
    if (strncasecmp(line, "GET", 3) == 0) {
        http_request(cfd, line);      //����http����  �ж��ļ��Ƿ���� �ط�
        discon(cfd, epfd); //�����һ���ᷢ�Ͷ������ʲôСͼ�갡�ȵ� ���̫�������ͨ��һ���͹ر� �Ͳ����д���ҳ��
    }
    return;
}

//==================================
// 16����ת��10����
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
����������
���� %20 ֮��Ķ���
*/
//����
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
//����
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
//����ҳ��
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

    int epfd = epoll_create(1024);   //��������� �������ڵ�1024 ���ظ��ڵ�� fd
    if (epfd == -1)
    {
        perror("epoll_create error");
        exit(1);
    }
    //����ʼ�����׽�����ӵ���������
    int lfd = init_listen_fd(port, epfd);

    while (1) {
        //������Ӧ�ڵ��¼�
        int ret = epoll_wait(epfd, all_events, MAXSIZE, -1);
        if (ret == -1)
        {
            perror("epoll_wait error");
            exit(1);
        }

        for (i = 0; i < ret; ++i)
        {
            //��Ϊ��web������  ֻ������¼� ����Ĭ�ϲ�����
            struct epoll_event* pev = &all_events[i];

            //���Ƕ��¼��Ĵ���
            if (!(pev->events & EPOLLIN))
            {
                continue;
            }
            if (pev->data.fd == lfd)    //˵�����뽨�����ӵĿͻ���
            {
                do_accept(lfd, epfd);
            }
            else {
                do_read(pev->data.fd, epfd);    //˵�����еĿͻ�������ж�д����
            }
        }
    }
}



