#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pwd.h>
#include <arpa/inet.h>

# define MAX 1024
#define DEFAULT_MULTICAST_TTL_VALUE  2
#ifndef	INADDR_NONE
#define	INADDR_NONE	0xffffffff
#endif	/* INADDR_NONE */
#define SA struct sockaddr
#define  h_addr h_addr_list[0]
#define MAX_LEN 4096
#define TRUE 1
#define FALSE 0


struct message
{
	uint8_t opcode;
	uint8_t name_len;
	char* name;
	uint16_t text_len;
	char* text;
	
};

void error(const char *msg){
	perror(msg);
	exit(1);
}



extern void leaveGroup(int recvSock, char *group);
extern void joinGroup(int s, char *group);
extern void reusePort(int sock);
extern void displayDaddr(int sock);
extern void setTTLvalue(int s,unsigned char *i);
extern void setLoopback(int s,int  loop);

/* external variables */

extern int	errno;

/* functions declarations */

int  CreateMcastSocket();
void chat(int McastSock);
void sendMessage(int inSock,int outSock,int *FLAG);
void getMessage(int inSock, int outSock);
void map_mcast_address(char * session_numb);

char *GroupIPaddress;//[MAX_LEN]; /* multicast group IP address */
int  UDPport; 
int McastSock;                /* port number */
unsigned char TimeToLive = DEFAULT_MULTICAST_TTL_VALUE;
char *chat_name; 
char *buffer;
char recvBuf[MAX_LEN];
struct sockaddr_in groupStruct;
struct ip_mreq  mreq; /* multicast group info structure */


void INThandler(int sig){
	char c;
	signal(sig,SIG_IGN);
	printf("\n Do you want to quit?(y/n) \n");
	c=getchar();
	if(c=='y'|| c=='Y'){

		leaveGroup(McastSock,GroupIPaddress);

	}
	else
		signal(SIGINT,INThandler);
}

void reusePort(int s)
{
	int one = 1;

	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *) &one, sizeof(one)) == -1) {
		error("error in setsockopt,SO_REUSEPORT ");
	}
}


void setTTLvalue(int s, unsigned char * ttl_value)
{
	if (setsockopt(s, IPPROTO_IP, IP_MULTICAST_TTL, (char *) ttl_value,
		sizeof(unsigned char)) == -1) {
		error("error in setting loopback value");
}
}

void setLoopback(int s, int loop)
{
	if (setsockopt(s, IPPROTO_IP, IP_MULTICAST_LOOP, (char *) &loop,
		sizeof(unsigned char)) == -1) {
		error("error in disabling loopback\n");
}
}


char* encodemsg(struct message *msg,char * name)
{

	msg->name_len=(uint8_t)strlen(name);
	msg->text_len=(uint16_t)strlen(msg->text);

	int size= 1+ 1+ msg->name_len + 2 + msg->text_len;
	char* encodemessage=(char *) malloc(size);
	memset(encodemessage,0,size);
	int bytes_written=0;

	uint8_t op_temp=(uint8_t) msg->opcode;
	memcpy(encodemessage+bytes_written,&op_temp,sizeof(op_temp));
	bytes_written=sizeof(uint8_t);
  //printf("Opcode check 0x%02x\n",op_temp);


	memcpy(encodemessage+bytes_written,&msg->name_len,sizeof(msg->name_len));
	bytes_written += sizeof(uint8_t);
  //printf("name length check 0x%02x\n",msg->name_len);

	char name_temp[msg->name_len];
	memcpy(name_temp,name,msg->name_len);
	memcpy(encodemessage+bytes_written,name_temp,msg->name_len);
	bytes_written += msg->name_len;
  //printf("name check %s\n",name_temp);



	memcpy(encodemessage+bytes_written,&msg->text_len,sizeof(msg->text_len));
	bytes_written =bytes_written+sizeof(uint16_t);
 // printf("text len check 0x%04x\n",msg->text_len);
	memcpy(encodemessage+bytes_written,msg->text,msg->text_len);

	return encodemessage; 


}

struct  message* decodemsg(char* recvmsg)
{

	int read_bytes=0;
	struct message* decoded_msg=(struct message*) malloc(sizeof (struct message));
	memset(decoded_msg,0,sizeof(struct message));

  //printf("Check point decoded_msg\n");
	uint8_t op_temp=*((uint8_t*) recvmsg);
	read_bytes=sizeof(uint8_t);
	decoded_msg->opcode=(uint8_t) op_temp;
  //printf("Check point decoded_msg op 0x%02x\n",decoded_msg->opcode);

	decoded_msg->name_len=*((uint8_t*) recvmsg+read_bytes);
	read_bytes = read_bytes + sizeof(uint8_t);

  //printf("Check point decoded_name_len 0x%02x\n",decoded_msg->name_len);

	char name_temp[decoded_msg->name_len];
	memcpy(name_temp,recvmsg+read_bytes,decoded_msg->name_len);
	decoded_msg->name=(char*)malloc(decoded_msg->name_len);
	memcpy(decoded_msg->name,name_temp,decoded_msg->name_len);
	read_bytes=read_bytes+decoded_msg->name_len;
  //printf("Check point decoded_name %s\n",decoded_msg->name);


  //decoded_msg->text_len=*((uint16_t*)recvmsg+read_bytes);
	memcpy(&decoded_msg->text_len,recvmsg+read_bytes,sizeof(uint16_t));
  //printf("%d\n",ntohs(decoded_msg->text_len));
	read_bytes = read_bytes + sizeof(uint16_t);
  //printf("Check point decoded text_len %d\n",decoded_msg->text_len);

	char text_temp[decoded_msg->text_len];
	decoded_msg->text=(char*)malloc(decoded_msg->text_len);
  //printf("Check point decoded_text1\n");
	memcpy(text_temp,recvmsg+read_bytes,decoded_msg->text_len);
	memcpy(decoded_msg->text,text_temp,decoded_msg->text_len);
  //printf("Check point decoded_text %s\n",decoded_msg->text);

	return decoded_msg;
	
}


void leaveGroup(int recvSock, char *group)
{
  //struct sockaddr_in groupStruct;
  //struct ip_mreq  dreq; /* multicast group info structure */
	struct message* leavemsg=(struct message*) malloc(sizeof(struct message));
	struct sockaddr_in dest;
	leavemsg->opcode=2;
	leavemsg->name=chat_name;
	leavemsg->text="bye";
	buffer=encodemsg(leavemsg,leavemsg->name);

	dest.sin_family = AF_INET;
	dest.sin_port = htons(UDPport);
	dest.sin_addr.s_addr = inet_addr(GroupIPaddress);
  //printf("%s\n", buffer);

	if ((groupStruct.sin_addr.s_addr = inet_addr(group)) == -1)
		error("error in inet_addr\n");

	mreq.imr_multiaddr = groupStruct.sin_addr;
	mreq.imr_interface.s_addr = INADDR_ANY;

	if (setsockopt(recvSock, IPPROTO_IP, IP_DROP_MEMBERSHIP,
		(char *) &mreq, sizeof(mreq)) == -1) {
		error("error in leaving group \n");
}
    else{
                if (sendto(recvSock, buffer,MAX_LEN,0,
                    (struct sockaddr *)&dest, sizeof(dest)) < 0 ){
                    error("error in sendto");
            }
        printf(" You left the multicast group %s \n", group);
        exit(0);
    }
}

void getMessage(int inSock, int outSock)
{
  //int bytes=0;
	struct message* recvmsg=(struct message*) malloc(sizeof (struct message));
	struct sockaddr_in addr;

	memset(recvBuf,0,sizeof(recvBuf));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(UDPport);
	addr.sin_addr.s_addr = inet_addr(GroupIPaddress);
	socklen_t addrlen=sizeof(addr);

	recvfrom(inSock,recvBuf,MAX_LEN,0,(struct sockaddr *)&addr, &addrlen);
	recvmsg=decodemsg(recvBuf);
	printf("%s > %s\n",recvmsg->name,recvmsg->text);


}

void sendMessage(int inSock, int outSock, int *flag)
{
	char sendBuf[MAX_LEN];
	memset(sendBuf,0,sizeof(sendBuf));
	int bytes=0;
	struct sockaddr_in dest;
	struct message* sendmsg=(struct message*) malloc(sizeof (struct message));

	sendmsg->name=chat_name;
	fgets(sendBuf,MAX_LEN,stdin);
	//printf("You >");
	sendmsg->text=sendBuf;
	if((strlen(sendmsg->text)-1)==3){
		if(strncmp(sendmsg->text,"bye ",3)==0 || strncmp(sendmsg->text,"Bye",3)==0 || strncmp(sendmsg->text,"BYE",3)==0)
		{
			leaveGroup(McastSock, GroupIPaddress);
		}
		else{
			sendmsg->opcode=1;
		}
	}   
  //printf("%s\n",sendmsg->text);
	buffer=encodemsg(sendmsg,sendmsg->name);
  //printf("%s\n",buffer);

	dest.sin_family = AF_INET;
	dest.sin_port = htons(UDPport);
	dest.sin_addr.s_addr = inet_addr(GroupIPaddress);
  //printf("%s\n", buffer);

	if (sendto(outSock, buffer,MAX_LEN,0,
		(struct sockaddr *)&dest, sizeof(dest)) < 0 ){
		error("error in sendto");
}
memset(buffer,0,sizeof(buffer));
memset(sendBuf,0,sizeof(sendBuf));


}


void chat(int McastSock)
{
	fd_set rfds, rfds_copy;
	struct timeval wait;
	int DONE=FALSE;
	int len;
	int nb=0;

/** set the bits corresponding to STDIN and the socket to be monitored */
	FD_ZERO(&rfds);
	FD_ZERO(&rfds_copy);
	FD_SET(0,&rfds);
	FD_SET(McastSock,&rfds);

	len = McastSock +1;
    /* set the monitoring frequency to 1 second */
	wait.tv_sec = 1;
	wait.tv_usec = 0;

	printf("Enter your name: ");
	fgets(chat_name,255,stdin);
	printf("You can start sending message \n");

	while(!DONE)
	{
		memcpy(&rfds_copy,&rfds,len);

		nb = select(len,&rfds_copy,(fd_set *) 0,(fd_set *) 0,&wait);
		if (nb<0)
		{
			error("error in select \n");
		}
		else if (nb > 0)
		{

			if(FD_ISSET(0,&rfds_copy)){
        //printf("You > \n");

				sendMessage(0,McastSock,&DONE);
			}
			if(FD_ISSET(McastSock,&rfds_copy)){
          //printf("recv started\n");
				getMessage(McastSock, 1);
			}

		}
	}
}



void joinGroup(int s, char *group)
{
	//struct sockaddr_in groupStruct;
	//struct ip_mreq  mreq;	/* multicast group info structure */

	if ((groupStruct.sin_addr.s_addr = inet_addr(group)) == -1)
		error("error in inet_addr\n");

	/* check if group address is indeed a Class D address */
	mreq.imr_multiaddr = groupStruct.sin_addr;
	mreq.imr_interface.s_addr = INADDR_ANY;

	if (setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *) &mreq,
		sizeof(mreq)) == -1) {
		error("error in joining group \n");
}
else
	printf("You have joined the group\n");
	//fgets(chat_name,255,stdin);
}


int CreateMcastSocket()
{
	int s,length,err;
	int  loop;  
         /* variable used to enable/disable loopback */
	struct sockaddr_in groupHost; 
         /* multicast group host info structure */

/** Get the multicast group host information */
	groupHost.sin_family=AF_INET;
	groupHost.sin_port=htons(UDPport);
	groupHost.sin_addr.s_addr = htonl(INADDR_ANY);

/** Allocate a UDP socket and set the multicast options */

	if ((s = socket(AF_INET,SOCK_DGRAM, 0)) < 0)
	{
		error("can't create socket \n");
	}

/** allow multiple processes to bind to same multicast port on the same host */
	reusePort(s);

/** bind the UDP socket to the mcast address to recv messages from the group */
	if((bind(s,(struct sockaddr *) &groupHost, sizeof(groupHost))== -1))
	{
		error("error in bind\n");
	}

	setTTLvalue(s,&TimeToLive);

  loop = 0;  /*disable loopback*/
  loop = 1;  /*enable loopback*/
	setLoopback(s,1);

	return s;  
}





int main(int argc,char *argv[])
{
  //int McastSock;
  //int session_num;
	signal(SIGINT ,INThandler); 
 // char* fname;

	chat_name=(char*)malloc(sizeof(char));
  /* parse the command line to get the multicast group address */
	if (argc != 5)
	{
		error("Usage: <programe name> -mcip <IP address> -port <port number>\n");
	}
	else{
  	/* assign mcast group and port number for the session */

		GroupIPaddress = argv[2];
		UDPport = atoi(argv[4]);
	}

	McastSock  = CreateMcastSocket();
	joinGroup(McastSock, GroupIPaddress);
	chat(McastSock);
	leaveGroup(McastSock, GroupIPaddress);
	close(McastSock);
}



