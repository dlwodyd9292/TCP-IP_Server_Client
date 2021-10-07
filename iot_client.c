#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <signal.h>

#define BUF_SIZE 100
#define NAME_SIZE 20
#define ARR_CNT 5

void * send_msg(void * arg);
void * recv_msg(void * arg);
void error_handling(char * msg);

char name[NAME_SIZE]="[Default]";
char msg[BUF_SIZE];

int main(int argc, char *argv[])
{
	int sock;
	struct sockaddr_in serv_addr;
	pthread_t snd_thread, rcv_thread;
	void * thread_return;

	if(argc != 4) {
		printf("Usage : %s <IP> <port> <name>\n",argv[0]);
		exit(1);
	}

	sprintf(name, "%s",argv[3]);

	sock = socket(PF_INET, SOCK_STREAM, 0);
	if(sock == -1)
		error_handling("socket() error");

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family=AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
	serv_addr.sin_port = htons(atoi(argv[2]));

	if(connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
			error_handling("connect() error");

	sprintf(msg,"[%s:PASSWD]",name);
	write(sock, msg, strlen(msg));//id와 pwd 전송 
	pthread_create(&rcv_thread, NULL, recv_msg, (void *)&sock);
	pthread_create(&snd_thread, NULL, send_msg, (void *)&sock);

	pthread_join(snd_thread, &thread_return);
//	pthread_join(rcv_thread, &thread_return);

	close(sock);
	return 0;
}

void * send_msg(void * arg)
{
	int *sock = (int *)arg;
	int str_len;
	int ret;
	fd_set initset, newset;
	struct timeval tv;
	char name_msg[NAME_SIZE + BUF_SIZE+2];

	FD_ZERO(&initset);
	FD_SET(STDIN_FILENO, &initset);//표준입력장치를 멀티플렉스 장치로 활용

	fputs("Input a message! [ID]msg (Default ID:ALLMSG)\n",stdout);
	while(1) {
		memset(msg,0,sizeof(msg));
		name_msg[0] = '\0';
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		newset = initset;
		ret = select(STDIN_FILENO + 1, &newset, NULL, NULL, &tv);
		//키보드로 아무것도 입력받지 못하면 1초마다 빠져나온다 
        	if(FD_ISSET(STDIN_FILENO, &newset))
		{
			fgets(msg, BUF_SIZE, stdin);
			if(!strncmp(msg,"quit\n",5)) {
  				*sock = -1;
				return NULL;
			}
			else if(msg[0] != '[') //배열 0번째가 대괄호 유무 확인 후
								   // = 수신자가 특정되었는지 확인한 후 
								   // 예) [1]hi~
			{
		    	strcat(name_msg,"[ALLMSG]");//없으면 ALLMSG로 보낸다
				strcat(name_msg,msg); //붙이기 
			}
			else
				strcpy(name_msg,msg);//복사
			if(write(*sock, name_msg, strlen(name_msg))<=0)//서버에게 전송
			{
  				*sock = -1;
				return NULL;
			}
		}
		if(ret == 0) //타임아웃 
		{
  			if(*sock == -1) //recv_msg로 sock에 -1이 들어오는지 확인하면
		  		return NULL; //if문 탈출 
		}
	}
}

void * recv_msg(void * arg)
{
	int * sock = (int *)arg;	
	int i;
    	char *pToken;
	char *pArray[ARR_CNT]={0};

	char name_msg[NAME_SIZE + BUF_SIZE +1];
	int str_len;
	while(1) {
		memset(name_msg,0x0,sizeof(name_msg));
		str_len = read(*sock, name_msg, NAME_SIZE + BUF_SIZE );
		if(str_len <= 0)//서버로부터 0을 리턴받으면 = 연결이 종료되면 
		{
  			*sock = -1; //
			return NULL;
		}
		name_msg[str_len] = 0;
		fputs(name_msg, stdout);

/*   	pToken = strtok(name_msg,"[:]");
		i = 0;
		while(pToken != NULL)
		{
			pArray[i] =  pToken;
			if(i++ >= ARR_CNT)
				break;
			pToken = strtok(NULL,"[:]");
		}
  
//		printf("id:%s, msg:%s,%s,%s,%s\n",pArray[0],pArray[1],pArray[2],pArray[3],pArray[4]);
		printf("id:%s, msg:%s\n",pArray[0],pArray[1]);
*/
	}
}

void error_handling(char * msg)
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}
