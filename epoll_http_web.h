#ifndef _EPOLL_SERVER_H
#define _EPOLL_SERVER_H

#define MAXSIZE 2048

int init_listen_fd(int port, int epfd);  //创建socket 初始化 绑定 将监听sock挂上红黑树


void* epoll_run(int port); //创建一个红黑树 将创建的socket挂上树  循环监听epoll_wait (条件变量) 看返回值是满足那种条件 处理对应事件

void do_accept(int lfd, int epfd);		//有建立连接请求通知 accept 返回新客户端cfd 再挂上树

void do_read(int cfd, int epfd);		//读取 http 状态行 以及后续数据

const char* get_file_type(const char* name);//通过文件名获取文件类型

int get_line(int cfd, char* buf, int size); //获取一行 \r\n 结尾的数据  因为 http 发送的是以这个结尾的

void discon(int cfd, int epfd);			//断开连接 从监听树上摘除

void  http_request(int cfd, const char* requst);//处理http请求  判断请求的是文件还是目录 回发

void  send_respond(int cfd, int nob, char* disp, const char* type, int len); //组建 http的应答协议头

void send_filedate(int cfd, const char* file);	//只读打开被请求的文件 发送文件里的数据给浏览器cfd

void send_dir(int cfd, const char* dirname);	//展示目录下所有项 拼接目录路径 显示在html中为一个标签 可再次选中被请求

void encode_str(char* to, int tosize, const char* from);	//编码  发送给浏览器的中文需要编码成它能存储的形式

void decode_str(char* to, char* from);						//解码 浏览器发送过来的中文解码成我们看得懂中文汉字

int hexit(char c);		// 16进制转换10进制

void send_error(int cfd, int status, char* title, char* text); //拼接一个html 错误页面 例如文件不存在啊  404 ！





#endif