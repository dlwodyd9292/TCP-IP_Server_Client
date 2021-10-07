#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>

#define BUF_SIZE 100
#define MAX_CLNT 32
#define ID_SIZE 10
#define ARR_CNT 5

#define DEBUG
typedef struct {
  	char fd;//파일디스크립트
	char *from;  //누구에게
	char *to; //누가 
	char *msg; //어떤 메세지를 보낼 것인지
	int len;//메세지 길이
}MSG_INFO; //메세지 정보

typedef struct {
	int index; //몇번째 클라이언트인지
	int fd;//클라이언트의 fd 
    char ip[20];
	char id[ID_SIZE];
	char pw[ID_SIZE];
	//id pw는 외부에 저장해두는게 좋다 
}CLIENT_INFO;//클라이언트 정보

void * clnt_connection(void * arg);//쓰레드 생성시 구동될 함수
void send_msg(MSG_INFO * msg_info, CLIENT_INFO * first_client_info);
void error_handling(char * msg);
void log_file(char * msgstr);//수신된 채팅 로그를 저장할 곳, 날짜폴더(202107 폴더, 28 폴더 <- 예시)
//void getlocaltime(char * buf); //시계만들때 사용할 함수

int clnt_cnt=0; //클라이언트 갯수 저장
pthread_mutex_t mutx; //뮤텍스 변수 

int main(int argc, char *argv[])
{
	int serv_sock, clnt_sock;
	struct sockaddr_in serv_adr, clnt_adr;
	int clnt_adr_sz;
	int sock_option  = 1; //bind error 처리를 위한 옵션 
	pthread_t t_id[MAX_CLNT] = {0}; //쓰레드 아이디 저장하기 위한 배열 
	int str_len = 0; //리턴된 문자열을 저장하기 위한 변수
	int i; //for문 제어 변수
	char idpasswd[(ID_SIZE*2)+3];//id와 pwd 전체를 한번에 저장하기 위한 배열
	char *pToken; //문자열을 분리하여 인식하기 위한 포인터
	char *pArray[ARR_CNT]={0};//문자열을 분리하여 인식하기위한 포인터 배열 
	char msg[BUF_SIZE]; 

	CLIENT_INFO client_info[MAX_CLNT] = {{0,-1,"","LJY_LDP","PASSWD"}, \
			 {0,-1,"","LJY_SMP","PASSWD"},  {0,-1,"","LJY_LIN","PASSWD"}, \
			 {0,-1,"","LJY_ARD","PASSWD"},  {0,-1,"","LJY_QT","PASSWD"}, \
			 {0,-1,"","6","PASSWD"},  {0,-1,"","7","PASSWD"}, \
			 {0,-1,"","8","PASSWD"},  {0,-1,"","9","PASSWD"}, \
			 {0,-1,"","10","PASSWD"},  {0,-1,"","11","PASSWD"}, \
			 {0,-1,"","12","PASSWD"},  {0,-1,"","13","PASSWD"}, \
			 {0,-1,"","14","PASSWD"},  {0,-1,"","15","PASSWD"}, \
			 {0,-1,"","16","PASSWD"},  {0,-1,"","17","PASSWD"}, \
			 {0,-1,"","18","PASSWD"},  {0,-1,"","19","PASSWD"}, \
			 {0,-1,"","20","PASSWD"},  {0,-1,"","21","PASSWD"}, \
			 {0,-1,"","22","PASSWD"},  {0,-1,"","23","PASSWD"}, \
			 {0,-1,"","24","PASSWD"},  {0,-1,"","25","PASSWD"}, \
			 {0,-1,"","26","PASSWD"},  {0,-1,"","27","PASSWD"}, \
			 {0,-1,"","28","PASSWD"},  {0,-1,"","29","PASSWD"}, \
			 {0,-1,"","30","PASSWD"},  {0,-1,"","31","PASSWD"}, \
			 {0,-1,"","32","PASSWD"}}; //초기화 데이터 집어넣음

	if(argc != 2) {
		printf("Usage : %s <port>\n",argv[0]);
		exit(1);
	}
	fputs("IoT Server Start!!\n",stdout);

	if(pthread_mutex_init(&mutx, NULL)) //뮤텍스 초기화
		error_handling("mutex init error");

	serv_sock = socket(PF_INET, SOCK_STREAM, 0); //소켓 생성 

	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family=AF_INET;
	serv_adr.sin_addr.s_addr=htonl(INADDR_ANY);
	serv_adr.sin_port=htons(atoi(argv[1])); //전화번호 생성 

	 setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, (void*)&sock_option, sizeof(sock_option));
	 //SO_REUSEADDR -> bind error 없애는 옵션
	if(bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr))==-1) // 전화기와 전화번호 연결
		error_handling("bind() error");

	if(listen(serv_sock, 5) == -1) // 연결대기 큐에 보낼 거
		error_handling("listen() error");

	while(1) {
		clnt_adr_sz = sizeof(clnt_adr);//IPv4 IPv6에 따라 크기가 다르므로 주소체계에 맞게 사이즈 지정
		clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_adr, &clnt_adr_sz); //클라이언트로와 연결 
		if(clnt_cnt >= MAX_CLNT)//이미 클라이언트 연결 최대치인 32개 이상  클라이언트가 접속되어있었다면 
       	{
       		printf("socket full\n");
			shutdown(clnt_sock,SHUT_WR);//한쪽방향만 닫는 함수, 방금연결된 클라이언트의 write를 닫아버리고 0을 클라이언트에게 반환, 클라이언트는 read에서 블록되어 있다가 0을 리턴받고 종료, read는 살려두고 write만 죽임 
           	continue;//accept로 다시 올라감 
		}
		else if(clnt_sock < 0)//accept 함수 호출 오류면 0보다 작은 값 반환
		{
       		perror("accept()");
          	continue;
		}

		str_len = read(clnt_sock, idpasswd, sizeof(idpasswd));//클라이언트로부터 받은 id와 pwd 수신
		idpasswd[str_len] = '\0';

		if(str_len > 0) //종료인 0이 아닐 때
		{
			i=0;
											 // 클라이언트에게서 전송받은 "[id:pwd]"라는 데이터를 가공하는 부분
			pToken = strtok(idpasswd,"[:]"); // 문자열을 분리하는 함수, "[:]"로 문자열 구분, id와 pwd 분리
											 // pToken에 결과값을 주소로 리턴
											 // 문자가 없으면 Null, 0이 리턴(널포인터)
											 // 문자를 찾으면 문자가 있는 배열에 해당 문자가 들어있는 \0를 넣고 
											 // 그다음 배열의 주소를 리턴 
												
			while(pToken != NULL) // Null이 아니면= 문자를 찾았다면 
			{
				pArray[i] =  pToken; //pToken 주소(그 다음 배열의 주소)를 넣고
        		if(i++ >= ARR_CNT)//ARR_CNT는 5, i는 문자 하나, 5보다 인덱스 갯수가 크면 탈출
					break;	
				pToken = strtok(NULL,"[:]");//NULL = 이전에 문자열 파싱하던 주소에서 시작하라
			}
			for(i=0;i<MAX_CLNT;i++)//모든 클라이언트를 비교
			{
				if(!strcmp(client_info[i].id,pArray[0]))//아이디값을 가져와서 조금전에 분리된 아이디와 비교해서 같으면
				{
					if(client_info[i].fd != -1)//fd가 -1 = fd가 처음 접속하는 것, -1이 아니라는건 누군가가 이미 접속했다는 것 
					{
						sprintf(msg,"[%s] Already logged!\n",pArray[0]);//클라이언트에게 전송할 메세지
						write(clnt_sock, msg,strlen(msg)); //클라이언트에게 메세지 접속 
						log_file(msg); //메세지를 로그로 서버 화면에 출력 
						shutdown(clnt_sock,SHUT_WR); //위와 마찬가지로 write만 죽임, 클라이언트는 read를 통해  0을 반환받고 0을 반환 받으면 종료 
#if 1   //for MCU
     					shutdown(client_info[i].fd,SHUT_WR); 
						client_info[i].fd = -1;
#endif  
						break; //등록된 사용자가 아니면 탈출
					}
					if(!strcmp(client_info[i].pw,pArray[1]))//패스워드 값을 가져와서 조금전에 분리된 패스워드와 비교해서 같으면  
					{
						strcpy(client_info[i].ip,inet_ntoa(clnt_adr.sin_addr));//cleint_info[i]에 해당 클라이언트 주소 저장
						pthread_mutex_lock(&mutx);
						client_info[i].index = i; 
						client_info[i].fd = clnt_sock; //구조체에 해당 값 저장
						clnt_cnt++; //cnt값 증가 
						pthread_mutex_unlock(&mutx);
						sprintf(msg,"[%s] New connected! (ip:%s,fd:%d,sockcnt:%d)\n",pArray[0],inet_ntoa(clnt_adr.sin_addr),clnt_sock,clnt_cnt);
						//서버화면에 아이피, fd, sockcnt 값 출력
						log_file(msg);
						write(clnt_sock, msg,strlen(msg));//msg를 클라이언트들에게 전송 

  						pthread_create(t_id+i/*배열명에 i값 증가*/, NULL, clnt_connection, (void *)(client_info + i));//정상 접속시 쓰레드 생성
						pthread_detach(t_id[i]);
						break;
					}
				}
			}
			if(i == MAX_CLNT)// 최대 클라이언트 수까지 for문을 돌렸을때 = 미리 등록된 아이디가 없을때 
			{
				sprintf(msg,"[%s] Authentication Error!\n",pArray[0]);
				write(clnt_sock, msg,strlen(msg));
				log_file(msg);
				shutdown(clnt_sock,SHUT_WR);
			}
		}
		else //read를 통해 반환된 str_len값이 종료를 의미하는  0일때 
			shutdown(clnt_sock,SHUT_WR);

	}
	return 0;
}

void * clnt_connection(void *arg)//클라이언트 하나당 하나씩 생성 
{
   	CLIENT_INFO * client_info = (CLIENT_INFO *)arg;//위에서 쓰레드 만들기전에 담은 클라이언트 정보를 전달받고 저장 
	//포인터 변수 
	int str_len = 0;
	int index = client_info->index;//인덱스 값 i 저장
	char msg[BUF_SIZE];//read하면서 수신된 메세지 저장
  	char to_msg[MAX_CLNT*ID_SIZE+1]; 
	// 클라이언트간 통신 시 수신 
	//
	int i=0;
	char *pToken;
   	char *pArray[ARR_CNT]={0};
	char strBuff[130]={0};

	MSG_INFO msg_info;
	CLIENT_INFO  * first_client_info;

	first_client_info = (CLIENT_INFO *)((void *)client_info - (void *)( sizeof(CLIENT_INFO) * index ));
	//32개의 구조체 client_INFO에서 제일 첫번째 위치에 있는 fd를 꺼내와서 저장
	//현재위치에서 제일 첫번째 위치값을 빼면 첫번째 client_info 값이 나온다 
	//모둔 사용자에게 메세지를 뿌리기 위해 

	while(1)
	{
		memset(msg,0x0,sizeof(msg));
		str_len = read(client_info->fd, msg, sizeof(msg)-1); //클라이언트에서 write한 데이터를 read 
		if(str_len <= 0)
			break;//0이 리턴되면 쓰레드 조료 

		msg[str_len] = '\0';
		pToken = strtok(msg,"[:]"); //문자열을 분리 
		i = 0; 
		while(pToken != NULL)
		{
			pArray[i] =  pToken;
			if(i++ >= ARR_CNT)
				break;	
			pToken = strtok(NULL,"[:]");
		}

		msg_info.fd = client_info->fd;
		msg_info.from = client_info->id;//수신자 id 
		msg_info.to = pArray[0]; //송신자 id 
		sprintf(to_msg,"[%s]%s",msg_info.from,pArray[1]);
		msg_info.msg = to_msg;
		msg_info.len = strlen(to_msg);

		sprintf(strBuff,"msg : [%s->%s] %s",msg_info.from,msg_info.to,pArray[1]);
		log_file(strBuff);
		send_msg(&msg_info, first_client_info);//특정 id에게만 메세지를 보낸다 
	}

	close(client_info->fd);

	sprintf(strBuff,"Disconnect ID:%s (ip:%s,fd:%d,sockcnt:%d)\n",client_info->id,client_info->ip,client_info->fd,clnt_cnt-1);
	log_file(strBuff);

	pthread_mutex_lock(&mutx);
	clnt_cnt--;
	client_info->fd = -1; //3,4,....번이 들어가있는 fd를  초기화
	pthread_mutex_unlock(&mutx);

	return 0;
}

void send_msg(MSG_INFO * msg_info, CLIENT_INFO * first_client_info)
{
	int i=0;

	if(!strcmp(msg_info->to,"ALLMSG"))//모든 접속자에게 다 보낸다 
	{
		for(i=0;i<MAX_CLNT;i++)
			if((first_client_info+i)->fd != -1)	//32번까지 돌려서 접속된상태가 아니면(-) 쓰지않고 맞다면
     			write((first_client_info+i)->fd, msg_info->msg, msg_info->len); //메세지 전송
	}
	else if(!strcmp(msg_info->to,"IDLIST"))//IDLIST라고 입력되었을 경우
	{
		char* idlist = (char *)malloc(ID_SIZE * MAX_CLNT);
		msg_info->msg[strlen(msg_info->msg) - 1] = '\0';//전송될 메세지 끝에 널문자 넣고
		strcpy(idlist,msg_info->msg);//카피
		//idlist에는 수신자의 [id]가 있고 msg_info에
		for(i=0;i<MAX_CLNT;i++)
		{
			if((first_client_info+i)->fd != -1)	
			{
				strcat(idlist,(first_client_info+i)->id); //idlist에 id 붙이기
				strcat(idlist," ");
			}
		}
		strcat(idlist,"\n");
    	write(msg_info->fd, idlist, strlen(idlist));//요청자에게 idlist에 들어있는 내용을 전송 
		free(idlist);
	}
	else//수신자가 특정된 상태라면 
		for(i=0;i<MAX_CLNT;i++)
			if((first_client_info+i)->fd != -1)	//fd가 -1d이 아니어아 현재 접속되어 있는 것
				if(!strcmp(msg_info->to,(first_client_info+i)->id))//특정된 수신자를 찾고
     				write((first_client_info+i)->fd, msg_info->msg, msg_info->len);//있으면 전송 
}

void error_handling(char *msg)
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}

void log_file(char * msgstr)
{
	fputs(msgstr,stdout); //표준 출력장치로 서버 화면에  메세지 출력 
}
