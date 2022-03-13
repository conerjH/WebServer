#ifndef _EPOLL_SERVER_H
#define _EPOLL_SERVER_H

#define MAXSIZE 2048

int init_listen_fd(int port, int epfd);  //����socket ��ʼ�� �� ������sock���Ϻ����


void* epoll_run(int port); //����һ������� ��������socket������  ѭ������epoll_wait (��������) ������ֵ�������������� �����Ӧ�¼�

void do_accept(int lfd, int epfd);		//�н�����������֪ͨ accept �����¿ͻ���cfd �ٹ�����

void do_read(int cfd, int epfd);		//��ȡ http ״̬�� �Լ���������

const char* get_file_type(const char* name);//ͨ���ļ�����ȡ�ļ�����

int get_line(int cfd, char* buf, int size); //��ȡһ�� \r\n ��β������  ��Ϊ http ���͵����������β��

void discon(int cfd, int epfd);			//�Ͽ����� �Ӽ�������ժ��

void  http_request(int cfd, const char* requst);//����http����  �ж���������ļ�����Ŀ¼ �ط�

void  send_respond(int cfd, int nob, char* disp, const char* type, int len); //�齨 http��Ӧ��Э��ͷ

void send_filedate(int cfd, const char* file);	//ֻ���򿪱�������ļ� �����ļ�������ݸ������cfd

void send_dir(int cfd, const char* dirname);	//չʾĿ¼�������� ƴ��Ŀ¼·�� ��ʾ��html��Ϊһ����ǩ ���ٴ�ѡ�б�����

void encode_str(char* to, int tosize, const char* from);	//����  ���͸��������������Ҫ��������ܴ洢����ʽ

void decode_str(char* to, char* from);						//���� ��������͹��������Ľ�������ǿ��ö����ĺ���

int hexit(char c);		// 16����ת��10����

void send_error(int cfd, int status, char* title, char* text); //ƴ��һ��html ����ҳ�� �����ļ������ڰ�  404 ��





#endif