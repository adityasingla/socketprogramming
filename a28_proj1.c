//**State University of New York - At BUffalo***////
//******Author: Aditya Singla******//
//******UbId:a28@buffalo.edu******//
//******References: Beej's Guide to Network Programming*****//
//******References: http://stackoverflow.com/questions/19149899/socket-programming-c-file-download*****//

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<errno.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netdb.h>
#include<arpa/inet.h>
#include<sys/wait.h>
#include<malloc.h>
#include<sys/stat.h>
#include<fcntl.h>


FILE * pfile;
struct file_detail
{
	int inserted;
};

typedef struct file_detail * fdl;

fdl file_array;
//structure for storing the details of the client addresses - list command
struct addr_detail
{
	char name[50];
	char ip[INET6_ADDRSTRLEN];
	char port[10];
	uint32_t conId;
	uint32_t connected;
	uint32_t fd;	
	struct addr_detail *next;
	
};


typedef struct addr_detail * n;

n cur,head,mynode;


//request structure for sending and recieving data among clients
struct request
{
	char type[10];
	char pckNo[15];
	char ip[INET6_ADDRSTRLEN];
	char data[1400];
	char filename[20];
	uint32_t pktSize;
	struct request *left;
	struct request *right;	
};

typedef struct request * req;

req file_root;

//Declarations for file descriptior sets
fd_set read_file_desc; 
fd_set write_file_desc;
fd_set error_file_desc;
fd_set serv_fd;
fd_set client_fd;
fd_set master_fd;

//Declarations for maximum value of file descriptors
int read_file_desc_max;
int write_file_desc_max;
int error_file_desc_max;
int fd_max;

//Max No of packet currently downloaded
long int max_pkt;

//total number of packetes to be downloaded
long int totalPkts;

//size of packet defined by the user
int pktSize;

//ip if the machine
char myIp[INET6_ADDRSTRLEN];

//port number of the machine
char portNum[10];

//hostname of the machine
char hostName[50];
size_t t;	

//listening socket of the machine
int socket_fd;

int MaxConId = 1;

//either client or server c for client and s for server
char * mode;

//whether already registered with the server or not
int alreadyRegistered =0;

//other variables
struct sockaddr_storage remoteaddr;
socklen_t addrLen;
int new_fd;
int nbytes;
char * fileNm;
long int size;
char fl[20];
char isComplete;

void createlist();
void set_write_file_desc(int);
void set_error_file_desc(int);
void set_master_fd(int);
void set_serv_fd(int);
void send_information_to_clients();
void send_deleted_info_clients(char *);
void addNode(n,int);
void removeClient(int);
void addServer(n);
void createMyNode();
void addConnection(char *,int);
void closeConnection(int);
void removeServer(int);
int searchNodePresent(long int);
void addtoTree(req);
void Download(char *);
void downloadSpecificPacket(long int,char *);
void DownloadAdditionalPackets(char *,int);
int isFileComplete();
void MakeFile();
void recieveRequest(req,int);
void askSize(char *, char *);
int isClientConnected(char *);
void Connect(char*, char *);
void Register(char *,char *);
void createSocket(char *);
void get_my_ip();
void DisplayList();
char* removeLastChar(char *);
void Terminate(char *);
void executecommand(char *);

void *get_addr(struct sockaddr *socket_address)
{
	if(socket_address->sa_family == AF_INET)
	{
		return &(((struct sockaddr_in *)socket_address)->sin_addr);
	}
	return &(((struct sockaddr_in6 *)socket_address)->sin6_addr);
}

//function for setting write file descriptor set
void set_write_file_desc(int file_desc)
{
	FD_SET(file_desc, &write_file_desc);
	if (write_file_desc_max <= file_desc)
	{
		write_file_desc_max = ++file_desc;
	}
}

//function for setting error file descriptor set
void set_error_file_desc(int file_desc)
{
	FD_SET(file_desc, &error_file_desc);
	if (error_file_desc_max <= file_desc)
	{
		error_file_desc_max = ++file_desc;
	}
}

//function for setting master file descriptor set

void set_master_fd(int fd)
{
	FD_SET(fd,&master_fd);
	if(fd_max <= fd)
	{
		fd_max = ++fd;
	}
}

//setting file descriptor set for server fd
void set_serv_fd(int fd)
{
	FD_SET(fd,&serv_fd);
	set_master_fd(fd);
}

//setting file descriptor for all client fd
void set_client_fd(int fd)
{
	FD_SET(fd,&client_fd);
	set_master_fd(fd);
}

// creates a linked list of all hosts in the network
// this function will only be called by the server initially. The client will add the list provided by the server
void createlist()
{
	mynode = (n)malloc(sizeof(struct addr_detail));
	strcpy(mynode->name,hostName);
	strcpy(mynode->port,portNum);
	strcpy(mynode->ip,myIp);
	mynode->conId = htons(1);
	mynode->next = NULL;
	mynode->connected = htons(0);
	
	head = mynode;
	
	cur = mynode;
}

//function for sending list to the clients. Only called by the server
void send_information_to_clients()
{
	n temp;
	int i=0;
	for(temp = head;temp!=NULL;temp=temp->next)
	{
		for(i=0;i<fd_max;i++)
		{	
			if(FD_ISSET(i,&client_fd) && i!= socket_fd)
			{
				if (send(i,temp,sizeof(struct addr_detail),0) == -1)
				{
					perror("error sending updated list to clients");
				}
			}
		}
	}
}

//function to send deleted clients to the other clients
void send_deleted_info_clients(char * name)
{
	int i = 0;
	n temp;		
	temp = (n)malloc(sizeof(struct addr_detail));
	for(i=0;i<fd_max;i++)
	{	
		
		if(FD_ISSET(i,&client_fd) && i!= socket_fd)
		{
			strcpy(temp->name,name);
			temp->connected = htons(2);
			if (send(i,temp,sizeof(struct addr_detail),0) == -1)
			{
				perror("error sending updated list to clients");
			}
		}
	}
	
}

//function for adding a node to the list(list command) - Server Function
void addNode(n node,int fd)
{
		if(head->next == NULL)
		{
			node->next = NULL;
			node->conId = htons(++MaxConId);
			node->fd = htons(fd);	
			head->next = node;
			cur = node;
		
		}
		else
		{
			n temp;
			temp = head;	
			while(temp->next != NULL)
			{
				temp=temp->next;
				
			}	
			node->next = NULL;
			node->conId = htons(++MaxConId);
			node->fd = htons(fd);	
			temp->next = node;
			cur = node;
		}

		printf("Connection recieved from:%30s%10s\n",node->name,node->port);	
}

// remove clients from the list
void removeClient(int fd)
{
		
		n current;
		n previous;
		current = head;
		while(current!= NULL)
		{
			
			if(ntohs(current->fd) == fd)
			{
				
				close(ntohs(current->fd));
				FD_CLR(ntohs(current->fd),&client_fd);
				FD_CLR(ntohs(current->fd),&master_fd);
				previous->next = current->next;
				send_deleted_info_clients(current->name);
				printf("Connection terminated from:%30s\n",current->name);				
				break;
			}
			previous = current;
			current = current->next;
		}

		
}

//used to add the list sent by the server on client side - client function
void addClientNode(n node)
{
	if(ntohs(node->connected) == 2)
	{
		n current;
		n previous;
		current = head;
		previous = head;
		while(current != NULL)
		{
			if(strcmp(current->name,node->name) == 0)
			{
					
				previous->next = current->next;
								
				break;
			}
			previous = current;
			current = current->next;
		}
	}
	else
	{
		n tempNode;
		tempNode = head;
		int clientFlag = 0;
		while(tempNode!= NULL)
		{
			
			if(strcmp(node->name,tempNode->name) == 0)
			{
				
				clientFlag = 1;
				break;
			}
			tempNode = tempNode->next;
		}
		if(clientFlag == 0)
		{
			tempNode = head;
			if(head->next == NULL)
			{
				node->next = NULL;	
				node->fd = htons(0);
				head->next = node;
				
				cur = node;
		
			}
			else
			{
				while(tempNode->next != NULL)
				{
					tempNode = tempNode->next;
				}
				node->fd = htons(0);
				node->next = NULL;
				tempNode->next = node;
				cur = node;
			}
			
			if(strcmp(node->name,"timberlake.cse.buffalo.edu") != 0)
			{
				
				printf("Peers:%30s%20s%10s\n",node->name,node->ip,node->port);
			}

		}
	}
}

//add the server node to the starting of the list - client function
void addServer(n nd)
{
	nd->next = NULL;
	head = nd;
	cur = nd;
}

//create a node of own details to be sent to the server after registration
void createMyNode()
{
	mynode = (n)malloc(sizeof(struct addr_detail));
	strcpy(mynode->name,hostName);
	strcpy(mynode->port,portNum);
	strcpy(mynode->ip,myIp);
	mynode->connected = htons(0);
	mynode->next = NULL;
}

//function for adding connection in the client list. eg. list contains all the clients whether they are connected or not. This function checks the flag for active connection to be true
void addConnection(char * ipClient,int fd)
{
	n temp;
	temp = head;
	while(temp != NULL)
	{
		if(strcmp(ipClient,temp->ip) == 0 || strcmp(ipClient,temp->name) == 0)
		{
			temp->connected = htons(1);
			temp->fd = htons(fd);
			break;
		}	
		temp = temp->next;
	}
}

//removing a active connection form the list
void closeConnection(int fd)
{
	n temp;
	temp = head;
	while(temp != NULL)
	{
		if(ntohs(temp->fd) == fd)
		{
			close(ntohs(temp->fd));
			FD_CLR(ntohs(temp->fd),&client_fd);
			FD_CLR(ntohs(temp->fd),&master_fd);
			temp->connected = htons(0);
			temp->fd = htons(0);
			printf("Connection closed with:%s\n",temp->name);
			break;
		}
		temp=temp->next;
	
	}


}

//calling function if the server exits. the function would disconnect all other clients
void removeServer(int fd)
{
	FD_CLR(fd,&serv_fd);
	FD_CLR(fd,&master_fd);
	close(fd);
	int i = 0;
	for(i=0;i<fd_max;i++)
	{
		if(FD_ISSET(i,&client_fd) && i != socket_fd)
		{
			close(i);
			FD_CLR(i,&client_fd);
			FD_CLR(i,&master_fd);
		}
	}
	alreadyRegistered = 0;
	head = NULL;
	printf("Connection terminated from server. No client is now accessible.\n");
}

// search if node already present in list
int searchNodePresent(long int pk)
{
	if(isComplete == 1)
	{
		
		return 1;
	}
	else
	{
		if(file_array[pk-1].inserted == 1)
		{
			return 1;
		}
	}
	return 0;
}

//add data recieved during a download command to a data str
void addtoTree(req node)
{
	long int pckNo = atol(node->pckNo) - 1;
	long int offset;
	if(pckNo == 1)
	{
		
		fwrite(node->data,1,pktSize,pfile);
	}		
	else
	{
		if(pckNo * pktSize >= size)
		{ 
	 		offset = pckNo * pktSize;
			fseek(pfile,offset,SEEK_SET);
			fwrite(node->data,1,size-offset,pfile);
		}
		else
		{
			
			offset = pckNo * pktSize;
			fseek(pfile,offset,SEEK_SET);
			fwrite(node->data,1,pktSize,pfile);
		}
	}
	
	file_array[pckNo].inserted = 1;	
	printf("Packet %ld Recieved from:%s\n",pckNo +1, node->ip);
}

//initiate download
void Download(char * fileName)
{
	
	printf("Downloading File...Please Wait...\n");
	int i;
	

	for (i=0;i<fd_max;i++)
	{
		if(i!=socket_fd && i != 0 && i != 1  && i != 2)
		{
			if(FD_ISSET(i,&client_fd))
			{
				req pk;
				pk = (req)malloc(sizeof (struct request));
				strcpy(pk->type,"DOWNLOAD");
				strcpy(pk->filename,fileName);
				pk->pktSize = htons(pktSize);
				++max_pkt;
				
				sprintf(pk->pckNo,"%ld",max_pkt);
				
				if(send(i,pk,sizeof(struct request),0) == -1)
				{
					FD_CLR(i,&client_fd);
					FD_CLR(i,&master_fd);
					printf("Not responding");
					--max_pkt;
					continue;		
				}
				free(pk);
							
			}
		}
	
	}

}

// download specific packet if not available after download completes
void downloadSpecificPacket(long int pckNo,char * fileName)
{
	int i = 0;
	for (i=0;i<fd_max;i++)
	{
		if(i!=socket_fd && i != 0 && i != 1  && i != 2)
		{
			if(FD_ISSET(i,&client_fd))
			{
				req pk;
				pk = (req)malloc(sizeof (struct request));
				strcpy(pk->type,"DOWNLOAD");
				strcpy(pk->filename,fileName);
				pk->pktSize = htons(pktSize);
				
				sprintf(pk->pckNo,"%ld",pckNo);
				
				if(send(i,pk,sizeof(struct request),0) == -1)
				{
					FD_CLR(i,&client_fd);
					FD_CLR(i,&master_fd);
					printf("Not responding");
					--max_pkt;
					free(pk);
					continue;		
				}
				free(pk);
				break;
							
			}
		}
	
	}
}


//download chunks of data from clients
void DownloadAdditionalPackets(char * fileName,int fd)
{
		
	if(FD_ISSET(fd,&client_fd))
	{
		req pk;
		pk = (req)malloc(sizeof (struct request));
		strcpy(pk->type,"DOWNLOAD");
		strcpy(pk->filename,fileName);
		pk->pktSize = htons(pktSize);
		++max_pkt;
		sprintf(pk->pckNo,"%ld",max_pkt);
		if(send(fd,pk,sizeof(struct request),0) == -1)
		{
			FD_CLR(fd,&client_fd);
			FD_CLR(fd,&master_fd);
			printf("Not responding");
			--max_pkt;
			
		}
		free(pk);
							
	}
	
}

//check if file has all chunks downloaded
int isFileComplete()
{
	int flag = 0;
	int i = 0;
	for(i=0;i<totalPkts;i++)
	{
		if(file_array[i].inserted != 1)
		{
			downloadSpecificPacket(i+1,fl);
			flag = 1;		
		}
	}

	return flag;
}

//create a file on the client side executing the download command
void MakeFile()
{
	if(file_array != NULL)
	{
		int i = isFileComplete();
		if(i == 0)
		{
	  		 printf("File Download Complete\n");
	   		 fclose(pfile);
	                 free(file_array);	
			 max_pkt = 0;
			 totalPkts = 0;
			 isComplete = 1;
		}
	}
}


//process request packet form the client. is used to process DOWNLOAD requests, FILESIZE requests etc.
void recieveRequest(req clientRequest,int fd)
{
	if(strcmp(clientRequest->type,"DOWNLOAD") == 0)
	{
		char buff[1400];
		memset(buff,0,1400);
		req newReq;
		newReq = (req)malloc(sizeof(struct request));
		long int pckNo = atol(clientRequest->pckNo)-1;			
		int pckSize = ntohs(clientRequest->pktSize);
		if(pckNo == 0)
		{
			FILE *fpointer;
			fpointer = fopen(clientRequest->filename,"rb");
			fseek(fpointer,0,SEEK_SET);		
			fread(buff,1,pckSize,fpointer);
			strcpy(newReq->type,"ACCEPT");
			strcpy(newReq->data,buff);
			strcpy(newReq->ip,myIp);
			sprintf(newReq->pckNo,"%ld",pckNo+ 1);
			newReq->pktSize = clientRequest->pktSize;	
			strcpy(newReq->filename,clientRequest->filename);	 
			if(send(fd,newReq,sizeof(struct request),0) == -1)
			{
				perror("send request error");
			}
			fclose(fpointer);
		}
		else
		{
			long int fSize;
			long int curPos;			
			FILE *fpointer;
			struct stat stats;
			int fp = open(clientRequest->filename,O_RDONLY);		
			fstat(fp,&stats);
			
			fSize = stats.st_size;
			
			close(fp);			
			fpointer = fopen(clientRequest->filename,"rb");
			fseek(fpointer,0,SEEK_SET);			
			fseek(fpointer,(pckSize*pckNo),SEEK_CUR);
			//curPos = ftell(fpointer);
			
			if(pckSize * pckNo >= fSize)
			{
				
				int offset = pckSize * pckNo;
				fread(buff,1,fSize - offset,fpointer);
				strcpy(newReq->type,"ACCEPT");
				strcpy(newReq->data,buff);	
				sprintf(newReq->pckNo,"%ld",pckNo + 1);
				newReq->pktSize = clientRequest->pktSize;
				strcpy(newReq->filename,clientRequest->filename);
				strcpy(newReq->ip,myIp);
			}
			else
			{		
				fread(buff,1,pckSize,fpointer);
				strcpy(newReq->type,"ACCEPT");
				strcpy(newReq->data,buff);	
				sprintf(newReq->pckNo,"%ld",pckNo+ 1);
				newReq->pktSize = clientRequest->pktSize;
				strcpy(newReq->filename,clientRequest->filename);
				strcpy(newReq->ip,myIp);
			}	 
			if(send(fd,newReq,sizeof(struct request),0) == -1)
			{
				perror("send request error");
			}
			fclose(fpointer);
		}		
		
		free(newReq);				
		
		
	}
	else if(strcmp(clientRequest->type,"ACCEPT") == 0)
	{	
		
		if (searchNodePresent(atol(clientRequest->pckNo)) == 0)
		{
			if(atol(clientRequest->pckNo) == 1)
			{
				pfile = fopen(clientRequest->filename,"wb");
			}
			addtoTree(clientRequest);
		
			if(totalPkts >= max_pkt)
			{
				DownloadAdditionalPackets(clientRequest->filename,fd);
			}
			else
			{
				MakeFile();
			}
		}
			
	}
	else if(strcmp(clientRequest->type,"CONNECT") == 0)
	{
		addConnection(clientRequest->ip,fd);
		printf("Machine connected to:%s\n",clientRequest->ip);
		printf("proj1 >>");
		fflush(stdout);
	}
	else if(strcmp(clientRequest->type,"TERMINATE") == 0)
	{
		closeConnection(fd);
	}
	else if(strcmp(clientRequest->type,"NT_FOUND") == 0)
	{
		printf("File Not found. Unable to Download\nproj1 >>");
		fflush(stdout);
	}
	else if(strcmp(clientRequest->type,"SIZE_ASK")==0)
	{
		int fp;
		struct stat stats;
		if(access(clientRequest->filename,F_OK) == 0)
		{
			fp = open(clientRequest->filename,O_RDONLY);		
			fstat(fp,&stats);
		
			long int sizel;
			sizel = stats.st_size;
			close(fp);
	
			strcpy(clientRequest->type,"SIZE");
			sprintf(clientRequest->ip,"%d",sizel);
		}
		else
		{
			strcpy(clientRequest->type,"NT_FOUND");


		}
		send(fd,clientRequest,sizeof(struct request),0);
	}
	else if(strcmp(clientRequest->type,"SIZE") == 0)
	{
		int f;		
		size = atol(clientRequest->ip);
		totalPkts = atol(clientRequest->ip)/pktSize;
		f = open(clientRequest->filename,O_WRONLY | O_CREAT | O_TRUNC, 0666);
		int offset = lseek(f,atol(clientRequest->ip),SEEK_CUR);
		ftruncate(f,offset);
		close(f);
		//pfile = fopen(node->filename,"wb");
		strcpy(fl,clientRequest->filename);
		file_array = malloc(totalPkts * sizeof(struct file_detail *));
		Download(clientRequest->filename);
		
	}

}

//function to ask filesize of the file to be downloaded
void askSize(char * fileName, char * Size)
{

	int i;
	pktSize = atoi(Size);
	isComplete = 0;
	if(pktSize > 1400)
	{
		printf("Packet size cannot be more that 1400 bytes. Please try a smaller value\nproj1 >>");
		fflush(stdout);
	}
	else
	{
		for (i=0;i<fd_max;i++)
		{
			if(i!=socket_fd && i != 0 && i != 1  && i != 2)
			{
				if(FD_ISSET(i,&client_fd))
				{
					req pk;
					pk = (req)malloc(sizeof (struct request));
					strcpy(pk->type,"SIZE_ASK");
					strcpy(pk->filename,fileName);
	
					if(send(i,pk,sizeof(struct request),0) == -1)
					{
						FD_CLR(i,&client_fd);
						FD_CLR(i,&master_fd);
						printf("Not responding");
						free(pk);					
						continue;
					}
					free(pk);
					break;					
				}
			}
		}
	}	
	

}

//check if client is connected alredy - to refuse multiple connections with a single client
int isClientConnected(char *clientIp)
{
	n temp;
	temp = head;
	while(temp != NULL)
	{
		if(strcmp(clientIp,temp->ip)==0 || strcmp(clientIp,temp->name) == 0)
		{
			if(ntohs(temp->connected) == 1)
			{
				return 1;
			}		
			else
			{
				return 0;
			}
		}

		temp = temp->next;
	}	
	return 0;
}


int isClientRegistered(char *clientIp)
{
	n temp;
	temp = head;
	while(temp != NULL)
	{
		if(strcmp(clientIp,temp->ip)==0 || strcmp(clientIp,temp->name) == 0)
		{
			return 1;
		}
		temp = temp->next;
	}	
	return 0;
}
//Connect to a client
void Connect(char* ipServer, char *portServer)
{
	if(strcmp(mode,"s")== 0)
	{
		printf("Server cannot use CONNECT command.\n");
		printf("proj1 >>");
		fflush(stdout);					

	}
	else if(alreadyRegistered == 0)
	{
		printf("Please register with the server before connecting to peers.\n");
		printf("proj1 >>");
		fflush(stdout);					

	}
	else if(isClientConnected(ipServer) == 1)
	{
		printf("The machine is already connected to the client.\n");	
		printf("proj1 >>");
		fflush(stdout);					

	}
	else if(strcmp(ipServer,"timberlake.cse.buffalo.edu") == 0 || strcmp(ipServer,"128.205.36.8") == 0)
	{
		printf("Cannot call connect command to the server.\n");	
		printf("proj1 >>");
		fflush(stdout);					

	}
	else if(strcmp(ipServer,hostName) ==0 || strcmp(ipServer,myIp) == 0)
	{
		printf("Machine cannot connect with itself.\n");	
		printf("proj1 >>");
		fflush(stdout);					

	}
	else if(isClientRegistered(ipServer) == 0)
	{
		printf("Host currently unavailable.\n");	
		printf("proj1 >>");
		fflush(stdout);					

	}
	else
	{
		struct addrinfo input;
                struct addrinfo *response;
                struct addrinfo *iterator;
                int result;
                int sock;
                void *addr;
                char ipstr[INET6_ADDRSTRLEN];
		int yes = 1;
	

		memset(&input,0,sizeof input);
		input.ai_family = AF_UNSPEC;
		input.ai_socktype = SOCK_STREAM;
		input.ai_flags = AI_PASSIVE;
		if((result =getaddrinfo(ipServer,portServer,&input,&response)) ==0)
		{
			for(iterator=response;iterator != NULL;iterator=iterator->ai_next)
			{
				if((sock=socket(iterator->ai_family,iterator->ai_socktype,iterator->ai_protocol)) == -1)
				{
					perror("error");
					printf("proj1 >>");
					fflush(stdout);					

					break;
				}

				if(setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1)
				{
					perror("setsocketopt");
					printf("proj1 >>");
					fflush(stdout);					

					break;
				}

				if(connect(sock,iterator->ai_addr,iterator->ai_addrlen) == -1)
				{
					close(sock);
					perror("connect");
					printf("proj1 >>");
					fflush(stdout);					
					break;
				}
				
				req connectReq;
				connectReq = (req)malloc(sizeof(struct request));
				strcpy(connectReq->type,"CONNECT");
				strcpy(connectReq->ip,myIp);	

				if(send(sock,connectReq,sizeof(struct request),0) ==-1)
				{
					close(sock);
					perror("send");
					printf("proj1 >>");
					fflush(stdout);

					break;
				}
				addConnection(ipServer,sock);
				set_client_fd(sock);
				printf("Connected to Client: %s\n",ipServer);
				printf("proj1 >>");
				fflush(stdout);
				break;
			}
				
				freeaddrinfo(response);
		}
	}
}	

//Register with the server
void Register(char * ipServer,char * portServer)
{
	
	if(strcmp(mode,"s") == 0)
	{
		printf("Serevr cannot use REGISTER command\n");
		printf("proj1 >>");
		fflush(stdout);

	}
	else if(alreadyRegistered == 1)
	{
		printf("The machine is already registered with the server\n");
		printf("proj1 >>");
		fflush(stdout);
	}
	else if(strcmp(ipServer,hostName) == 0 || strcmp(ipServer,myIp) ==0)
	{
		printf("Machine cannot register to itself.\n");
		printf("proj1 >>");
		fflush(stdout);
	}
	
	else if(strcmp(ipServer,"timberlake.cse.buffalo.edu") == 0 || strcmp(ipServer,"128.205.36.8") == 0)
	{
		
		struct addrinfo input;
                struct addrinfo *response;
                struct addrinfo *iterator;
                int result;
                int sock;
                void *addr;
                char ipstr[INET6_ADDRSTRLEN];
		int yes = 1;
	

		memset(&input,0,sizeof input);
		input.ai_family = AF_UNSPEC;
		input.ai_socktype = SOCK_STREAM;
		input.ai_flags = AI_PASSIVE;
		if((result =getaddrinfo(ipServer,portServer,&input,&response)) ==0)
		{
			for(iterator=response;iterator != NULL;iterator=iterator->ai_next)
			{
				if((sock=socket(iterator->ai_family,iterator->ai_socktype,iterator->ai_protocol)) == -1)
				{
					perror("error");
					printf("proj1 >>");
					fflush(stdout);					
					break;
				}

				if(setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1)
				{
					perror("setsocketopt");
					printf("proj1 >>");
					fflush(stdout);					
					break;
				}

				if(connect(sock,iterator->ai_addr,iterator->ai_addrlen) == -1)
				{
					close(sock);
					perror("connect");
					printf("proj1 >>");
					fflush(stdout);					
					break;
				}

				if((send(sock,mynode,sizeof(struct addr_detail),0)) == -1)
				{
					perror("send");
					close(sock);
					printf("proj1 >>");
					fflush(stdout);					
					break;		
				}
				
				alreadyRegistered = 1;
				set_serv_fd(sock);
				printf("Machine registered with the server\n");
				printf("proj1 >>");
				fflush(stdout);				
				break;
			}
				
				freeaddrinfo(response);
		}
	
		else
		{
			perror("addrinfo");
			printf("proj1 >>");
			fflush(stdout);
		}
	
	}
	else 
	{
		printf("Machine can only register with timberlake.cse.buffalo.edu.\n");	
		printf("proj1 >>");
		fflush(stdout);
	}
}

//create listening socket
void createSocket(char *portSock)
{
	struct addrinfo input;
	struct addrinfo *response;
	struct addrinfo *iterator;
	int result;
	int sock;
	int yes = 1;
	memset(&input,0,sizeof input);
	input.ai_family = AF_UNSPEC;
	input.ai_socktype = SOCK_STREAM;
	input.ai_flags = AI_PASSIVE;
	
	
	if((result = getaddrinfo(NULL,portSock,&input,&response)) == 0)
	{
		for(iterator=response;iterator != NULL;iterator=iterator->ai_next)
		{
			if((sock=socket(iterator->ai_family,iterator->ai_socktype,iterator->ai_protocol)) == -1)
			{
				perror("port not available. Please try running program with another port");
				
				exit(1);
			}

			
			if(setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int))== -1)
			{
				perror("setsockop");
				exit(1);

			}

			if((result =bind(sock,iterator->ai_addr,iterator->ai_addrlen))== -1)
			{
				perror("bind");
				close(sock);
				
				exit(1);
			}		
			if(sock == 0)
			{
				printf("Machine already listening to the port. Please run the program with a different port number\n ");
	exit(0);
			}
			socket_fd = sock;
			set_client_fd(socket_fd);
			printf("Machine waiting for connection:%s\nproj1 >>",portSock);
			fflush(stdout);					
			break;
		}
	}
 			freeaddrinfo(response);
			if(iterator != NULL && socket_fd!=0)
			{
				if(listen(socket_fd,10) == -1)
				{
					perror("listen");
					
					exit(1);
				}
			}
	

			
}

//get the ip of the machine

void get_my_ip()
{
	struct addrinfo input;//input to getaddrinfo
	struct addrinfo *response;	//response from getaddrinfo
	struct addrinfo *iterator;	//iterator for response
	struct sockaddr_in server;	// google addr parameters	
	struct sockaddr name;		  // struct address for system
	socklen_t namelen = sizeof(name); //length of struct address for ip address
	int result;				
	char ipstr[INET6_ADDRSTRLEN];		//string to hold the ip address
	int sock;	
	
		 //socket file 
	
	
	
	const char* serverIpAddress = "8.8.8.8";
	int serverPort = 53;

	socklen_t addr_len;
	memset(&input,0, sizeof input);
	input.ai_family = AF_INET;
	input.ai_socktype = SOCK_DGRAM;

	 result = getaddrinfo("localhost","4567",&input,&response);
	if(result == 0)
	{
		for(iterator = response;iterator!=NULL;iterator = iterator->ai_next)
		{
			
			if((sock = socket(iterator->ai_family,iterator->ai_socktype,iterator->ai_protocol)) == -1)
			{
					perror("socket");	
					continue;		
			}

		    memset(&server, 0, sizeof(server));
		    server.sin_family = AF_INET;
		    server.sin_addr.s_addr = inet_addr(serverIpAddress);
		    server.sin_port = htons(serverPort);

		    int err = connect(sock,(struct sockaddr *)&server,sizeof(server));
		    if(err==-1)
		    {
			perror("error");
		    }
		    
		    err = getsockname(sock, (struct sockaddr*) &name, &namelen);
		    
		    const char *p = inet_ntop(AF_INET, &((struct sockaddr_in *)&name)->sin_addr, ipstr, sizeof ipstr);
		    strcpy(myIp,ipstr);
		    
                    printf("IP Address of this machine is: %s\n",myIp);

		    close(sock);

		    result = gethostname(hostName,sizeof(hostName));

		    freeaddrinfo(response);
		    
		    break;	
		}
		
		
	}
	else
	{
		fprintf(stderr,"%s\n",gai_strerror(result));
		exit(0);
	}

}

//display the list of available connections

void DisplayList()
{
	if (head == NULL) 
	{
		printf("no connections present\n");
		
	}
	else
	{
		n temp;
		temp = head;
		printf("%5s\t%30s\t%15s\t%5s\n","ConId","-----------Hostname-----------","------Ip------","Port");
		do
		{
			printf("%5d\t%30s\t%15s\t%5s\n",ntohs(temp->conId),temp->name,temp->ip,temp->port);
			temp = temp->next;
		}while(temp!= NULL);
		
		
	}
	
}

//utility methods
char* removeLastChar(char *commandText)
{
   	int i;
	int length;
    	char *modifiedCommand;

   	
 	length = strlen(commandText);
	modifiedCommand = (char *)malloc(length-1);
 
    	for(i = 0; i < length-1; i++)
    	{
        	modifiedCommand[i] = commandText[i]; 
    	}

	modifiedCommand[length-1] = '\0';
 

        return modifiedCommand;
}

// terminate connection with a client
void Terminate(char * conId)
{
	int connection = atoi(conId);
	
	int i = 0;
	if(connection == 1)
	{
		for(i=0;i<fd_max;i++)
		{
			if(FD_ISSET(i,&serv_fd) && i!=0)
			{
				close(i);
				FD_CLR(i,&serv_fd);
				FD_CLR(i,&master_fd);
			}
			if(FD_ISSET(i,&client_fd) && i!= socket_fd)
			{
				close(i);
				FD_CLR(i,&client_fd);
				FD_CLR(i,&master_fd);
			}
		}
		head = NULL;
		alreadyRegistered = 0;
		printf("Connection closed with the server. No clients are now accessible\n");
	}
	else
	{	
		n temp;
		temp=head;
		while(temp != NULL)
		{
			if(ntohs(temp->conId) == connection)
			{
				if(ntohs(temp->connected) == 1)
				{
					close(ntohs(temp->fd));			
					FD_CLR(ntohs(temp->fd),&client_fd);
					FD_CLR(ntohs(temp->fd),&master_fd);
					temp->connected = htons(0);
					temp->fd = htons(0);
					printf("Connection terminated with:%s\n", temp->name);
				}
				else
				{
					printf("Client not connected to:%s\n",temp->name);
				}
				break;
			}
			temp=temp->next;
		}	
	}

}

//scanning of command entered by the user
void executecommand(char * commandText)
{
	int inc=0;
	
	char * modifiedCommand = removeLastChar(commandText);	

	char *command[3];
	
	char * str_tok;
	
	str_tok = strtok(modifiedCommand," ");
	
	while(str_tok!=NULL)
	{
		command[inc++] = str_tok;
		
		str_tok = strtok(NULL," ");
		
	}	
		
	if(strcmp("MYIP",command[0]) == 0||strcmp("myip",command[0]) == 0)
	{
		get_my_ip();
		printf("proj1 >>");
		fflush(stdout);	
	}
	else if(strcmp("EXIT",command[0]) == 0||strcmp("exit",command[0]) == 0)
	{
		exit(0);
	}
	else if(strcmp("MYPORT",command[0])==0||strcmp("myport",command[0]) == 0)
	{
		printf("This machine is listening on port number: %s\n",portNum);
		printf("proj1 >>");
		fflush(stdout);
	}
	else if(strcmp("CREATOR",command[0]) == 0 ||strcmp("creator",command[0]) == 0)
	{
		printf("Name:Aditya Singla\nUBITNAME:a28\nUB email address:a28@buffalo.edu\n");
		printf("proj1 >>");
		fflush(stdout);
	}
	else if(strcmp("HELP",command[0]) == 0 ||strcmp("help",command[0]) == 0)
	{
		printf("Commands:\n1)MYIP: Displays the IP Address of the machine.\n2)MYPORT: Dsiplays the port machine is listening to.\n3)REGISTER <server IP/hostname><port number>: Register client with the server.\n4)CONNECT<destination ip/hostname><port number>: Connect to a client in the network.\n5)LIST: List all the machines in the network.\n6)DOWNLOAD <filename><chunk_size>: Download a file from the connected clients\n7)TERMINATE <ConId>: Terminate a connection with a client\n8)CREATER: Displays the ubit and name of the student\n9)HostName: Displays the hostname of the machine.\n10)EXIT: To exit the program.\n");
		printf("proj1 >>");
		fflush(stdout);
	}
	else if(strcmp("HOSTNAME",command[0]) == 0 ||strcmp("hostname",command[0]) == 0)
	{
		printf("Hostname: %s\n",hostName);
		printf("proj1 >>");
		fflush(stdout);
	}
	else if(strcmp("REGISTER",command[0]) == 0 ||strcmp("register",command[0]) == 0)
	{
		
		if(inc == 3)
		{
			
			Register(command[1],command[2]);
		}
		else
		{
			printf("Invalid command format.\n");
			printf("proj1 >>");
			fflush(stdout);
		}
	}
	else if(strcmp("LIST",command[0]) == 0 ||strcmp("list",command[0]) == 0)
	{
		DisplayList();
		printf("proj1 >>");
		fflush(stdout);
	}
	else if(strcmp("CONNECT",command[0])== 0 ||strcmp("connect",command[0]) == 0)
	{
		if(inc == 3)
		{
			Connect(command[1],command[2]);
			
		}
		else
		{
			printf("Invalid command format.\n");
			printf("proj1 >>");
			fflush(stdout);
		}
	}
	else if(strcmp("DOWNLOAD",command[0]) == 0 ||strcmp("download",command[0]) == 0)
	{
		if(inc == 3)
		{
			askSize(command[1],command[2]);
		}
		else
		{
			printf("Invalid command format.\n");
			printf("proj1 >>");
			fflush(stdout);
		}
	}
	else if(strcmp("TERMINATE",command[0]) == 0 ||strcmp("terminate",command[0]) == 0)
	{
		if(inc == 2)
		{
			Terminate(command[1]);
		}
		else
		{
			printf("Invalid command format.\n");
			printf("proj1 >>");
			fflush(stdout);
		}
	}
	else
	{
		printf("Invalid command format.\n");
		printf("proj1 >>");
		fflush(stdout);
	}
	
}

//main function
void main(int args, char * argv[])
{

	char * buf = NULL;
	int i = 0;
	size_t  s= 0;

	//Check whether number of arguments are valid or not
	if(args != 3)
	{
		printf("Please enter arguments:\n1)s for server followed by PORT number\n2) c for client followed by port number");
		exit(0);
	}

	if(strcmp(argv[1],"c") != 0)  
	{
		if(strcmp(argv[1],"s") != 0 )
		{
		printf("Please mention whether to run program as client(c) or server(s)\n");
		exit(0);
		}
	}

	if(strcmp(argv[2],"") == 0)
	{
		printf("Please mention the port number to listen on.\n");
		
		exit(0);
	}


	strcpy(portNum,argv[2]);

	FD_ZERO(&read_file_desc);
	
	FD_ZERO(&write_file_desc);
	
	FD_ZERO(&error_file_desc);

	FD_ZERO(&serv_fd);
	
	set_serv_fd(0);

	set_write_file_desc(1);

	set_error_file_desc(2);
	
	get_my_ip();	
	
	createSocket(argv[2]);
	if(strcmp(argv[1],"s")==0)
	{
		
		createlist();
	}
	
	if(strcmp(argv[1],"c") == 0)
	{
		createMyNode();
		
	}
	

	mode = argv[1];
	
	for(;;)
	{
		read_file_desc = master_fd;
		if(select(fd_max,&read_file_desc,NULL,NULL,NULL) == -1)
		{
			perror("select");
		}
		
		
		for(i=0;i<fd_max;i++)
		{
			
			if(FD_ISSET(i,&read_file_desc))
			{
				if(i==0)
				{
					buf = NULL;
					getline(&buf,&s,stdin);
					
					
					executecommand(buf);
				}
				else if(i == socket_fd)
				{
					
					addrLen = sizeof(remoteaddr);
					new_fd = accept(socket_fd,(struct sockaddr *)&remoteaddr,&addrLen);
					if(new_fd == -1)
					{
						perror("accept");
					}
					else
					{
						
						set_client_fd(new_fd);
						
										
					}
				}
				else
				{
					if(strcmp(mode,"s") == 0)
					{
						char * read_buf;
						size_t size = 0;
						
		
							n node;
							node = (n)malloc(sizeof(struct 	addr_detail));								 
							if((nbytes = recv(i,node,sizeof(struct addr_detail),0)) <= 0)
					
							{
								removeClient(i);
								//perror("recieve");
							
							}
							else
							{
								if(nbytes > 0)
								{
									addNode(node,i);

												send_information_to_clients();	
								}
							}
						
					}
					else
					{
						if(FD_ISSET(i,&serv_fd))
						{
							
							n node;
							node = (n)malloc(sizeof(struct 	addr_detail));								 
							if((nbytes = recv(i,node,sizeof(struct addr_detail),0)) <=0)
					
							{
					
								removeServer(i);
								
							}
							else
							{
								
								if(nbytes > 0)
								{
									
									if(head == NULL)
									{
										
										addServer(node);
									}
									else
									{
										addClientNode(node);
									}
								}
							}
						}
						if(FD_ISSET(i,&client_fd) && i != socket_fd)
						{
							int bytesRecieved;
							
							req recvRequest;
							recvRequest = (req)malloc(sizeof (struct request));		
							if((bytesRecieved = recv(i,recvRequest,sizeof(struct request),0))<=0)							
							{
								closeConnection(i);
								//perror("recieve packet");
											
							}
							else
							{
								if(bytesRecieved > 0)
								{
									recieveRequest(recvRequest,i);

								}
							}
							free(recvRequest);
							
						}
					}
				}


			}
		}
	}
}	


