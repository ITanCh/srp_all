#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <linux/if_ether.h>
#include <linux/in.h>
#include <linux/sockios.h> 
#include <linux/if.h>
#include <netpacket/packet.h>
#include <net/if_arp.h>
#include <string.h>
#include <errno.h>

#define BUFFER_MAX 2048
#define MAX_ROUTE_INFO 10	
#define MAX_ARP_SIZE 10			
#define MAX_DEVICE 10


//the information of the static_routing_table
struct route_item{
	char destination[16];		
	char gateway[16];		
	char netmask[16];		
	int interface;
}route_info[MAX_ROUTE_INFO];	
//the number of the items in the routing table
int route_item_index=0;			

//the information of the arp_cache
struct arp_table_item{
	char ip_addr[16];		
	char mac_addr[18];		
}arp_table[MAX_ARP_SIZE];	
//the number of the items in the arp cache	
int arp_item_index=0;			

//the storage of the device, got information from device_information
struct device_info{
	char mac[18];		
	int interface;			
}device[MAX_DEVICE];	
//the sum of the interface	
int device_index=0;			

//initial static routing table
int read_static_route_table(){
	FILE *fp=fopen("static_routing_table", "r");
	if(fp == NULL){
		printf("can't open the static_routing_table\n");
		return -1;
	}
	char buf[100];
	memset(buf,0,100);
	char *p;
	fgets(buf,100,fp);
	while(!feof(fp)){
		 printf("route");
		if(route_item_index < MAX_ROUTE_INFO){
			p=strtok(buf," ");
			printf("%s\n",p);
			strcpy(route_info[route_item_index].destination, p);
			p=strtok(NULL," ");
			printf("%s\n",p);
			strcpy(route_info[route_item_index].gateway, p);
			p=strtok(NULL," ");
			printf("%s\n",p);
			strcpy(route_info[route_item_index].netmask, p);
			p=strtok(NULL," ");
			printf("%s\n",p);
			route_info[route_item_index].interface = atoi(p);
			route_item_index++;
		}
		memset(buf,0,100);
		fgets(buf,100,fp);
	}
	return 1;
}

//initial my arp cache
int read_arp_cache(){
	FILE *fp=fopen("arp_cache", "r");
	if(fp == NULL){
		printf("can't open the arp_cache!\n");
		return -1;
	}
	char buf[50];
	memset(buf,0,50);
	char *p;
	fgets(buf,50,fp);
	while(!feof(fp)){
		printf("arp");
		if(arp_item_index < MAX_ARP_SIZE){
			p=strtok(buf," ");
			  printf("%s\n",p);
			strcpy(arp_table[arp_item_index].ip_addr, p);
			p=strtok(NULL," ");
			  printf("%s\n",p);
			strcpy(arp_table[arp_item_index].mac_addr, p);
			arp_item_index++;
		}
		memset(buf,0,50);
		fgets(buf,50,fp);
	}
	return 1;
}

//the storage of the device, got information from configuration file: device_information
int read_device_information(){
	FILE *fp=fopen("device_information", "r");	
	if(fp == NULL){
		printf("can't open the device_information\n");
		return -1;
	}
	char buf[100];
	char *p;
	memset(buf,0,100);
	fgets(buf,100,fp);
	while(!feof(fp)){
		printf("device");
		if(device_index < MAX_DEVICE){
			p=strtok(buf," ");
			  printf("%s\n",p);
			strcpy(device[device_index].mac, p);
			p=strtok(NULL," ");
			  printf("%s\n",p);
			device[device_index].interface = atoi(p);
			device_index++;
		}
		memset(buf,0,100);
		fgets(buf,100,fp);
	}
	return 1;
}

//check des_mac is local or not
int is_to_transmit(char des_mac[18]){
	int i;
	for(i=0; i < device_index; i++)
		if(strcmp(device[i].mac, des_mac) == 0)
			return i;
	return -1;
}

//check static routing table to get gateway index
int check_route_table(char des_ip[16]){
	int i;
	for(i=0; i<route_item_index; i++)
		if(strcmp(route_info[i].destination, des_ip) == 0)
			return i;
	return -1;
}

//check table by gateway
int check_route_gateway(char des_ip[16]){
	int i;
	for(i=0; i<route_item_index; i++)
		if(strcmp(route_info[i].gateway, des_ip) == 0)
			return i;
	return -1;
}

//check arp cache to get next mac address index
int check_arp_cache(char gateway[16]){
	int i;
	for(i=0; i<arp_item_index; i++)
		if(strcmp(arp_table[i].ip_addr, gateway) == 0)
			return i;
	
	return -1;
}


int main(int argc, char* argv[]){
	int sock_fd;
	int n_read;
	char *p;
	char type[4];
	char buf[100];
	char buffer[2048];
	char *eth_head;
	char *ip_head;
	int gateway_index=-1;
	int arp_index=-1;
	int dec_eth=-1;
	int i;
	char src_ip[16];	
	char des_ip[16];
	char des_mac[18];
	char src_mac[18];

	unsigned char des_mac6[6];
	unsigned char src_mac6[6];
	unsigned int tempMac[6];
	memset(des_ip,0,16);
	memset(des_mac,0,18);
	printf("%s\n",des_mac);
	memset(src_mac,0,18);
	printf("%s\n",src_mac);
	struct sockaddr_ll addr0;

	if(read_static_route_table()==-1 || read_arp_cache()==-1 || read_device_information()==-1)
		return -1;

	if ((sock_fd = socket(PF_PACKET,SOCK_RAW,htons(ETH_P_IP)))<0){
		printf("create raw socket error!\n");
		return -1;
	}

	while (1){
		n_read = recvfrom(sock_fd,buffer,BUFFER_MAX, 0, NULL, NULL);
		if (n_read < 42){
			printf("error when recv msg\n");
			return -1;
		}
		
		eth_head = buffer;
		p = eth_head;
		int n = 0xff;
		sprintf(des_mac,"%02x:%02x:%02x:%02x:%02x:%02x",
			p[0]&n,p[1]&n,p[2]&n,p[3]&n,p[4]&n,p[5]&n);
		sprintf(src_mac,"%02x:%02x:%02x:%02x:%02x:%02x",
			p[0]&n,p[1]&n,p[2]&n,p[3]&n,p[4]&n,p[5]&n);
		//check whether to transmit
		if(is_to_transmit(des_mac) != -1){	
			printf("check the des_mac,meet the condition to transmit\n ");
			printf("%s\n",des_mac);
			sprintf(type,"%02x%02x",p[12]&n,p[13]&n);

			if(strncmp(type,"0800",4) != 0){
				printf("this is not an ip datagram!\n");
				goto final;
			}
			printf("this is an ip datagram!\n");
				
			ip_head = eth_head+14;
			p = ip_head+12;
			//get src IP!!!
			sprintf(src_ip,"%d.%d.%d.%d",
				(256+p[0])%256,(256+p[1])%256,(256+p[2])%256,(256+p[3])%256);
			printf("The src_ip is: %s \n",src_ip);
			//get  des IP!!!
			sprintf(des_ip,"%d.%d.%d.%d",
				(256+p[4])%256,(256+p[5])%256,(256+p[6])%256,(256+p[7])%256);

			printf("The des_ip is: %s \n",des_ip);
			//check and get the des_ip's gateway index
			gateway_index = check_route_table(des_ip);
			if (gateway_index == -1){
				printf("the des_ip to transmit can't be found in static routing table!so we broadcast\n");
				//broadcask
				//use gateway_index to find eth???
				for(i=0; i < arp_item_index; i++){
					printf("i: %d\n",i);//don't back
					if(strcmp(arp_table[i].ip_addr, src_ip) == 0)
					{
						continue;
					}	
					else{
					int inter_index=check_route_gateway(arp_table[i].ip_addr);
					printf("inter_index: %d\n",inter_index);
					if(inter_index==-1)continue;	
					dec_eth=route_info[inter_index].interface;
					
					printf("dec_eth: %d\n",dec_eth);
				printf("check and find that next mac address is in the arp cache.\n");
				//modify the header
				memcpy(des_mac,arp_table[i].mac_addr,18);
				//check if the des_mac is hemself
				if (is_to_transmit(des_mac)!=-1){
			        	printf("the new destination mac is the router.so don't need to transmit.\n");
					continue;
				}

				//printf("%s\n",des_mac);
				sscanf(src_mac,"%02x:%02x:%02x:%02x:%02x:%02x",
				&tempMac[0],&tempMac[1],&tempMac[2],&tempMac[3],&tempMac[4],&tempMac[5]);
				src_mac6[0]=(unsigned char)tempMac[0];
				src_mac6[1]=(unsigned char)tempMac[1];
				src_mac6[2]=(unsigned char)tempMac[2];
				src_mac6[3]=(unsigned char)tempMac[3];
				src_mac6[4]=(unsigned char)tempMac[4];
				src_mac6[5]=(unsigned char)tempMac[5];
	
				sscanf(des_mac,"%02x:%02x:%02x:%02x:%02x:%02x",
				&tempMac[0],&tempMac[1],&tempMac[2],&tempMac[3],&tempMac[4],&tempMac[5]);
				des_mac6[0]=(unsigned char)tempMac[0];
				des_mac6[1]=(unsigned char)tempMac[1];
				des_mac6[2]=(unsigned char)tempMac[2];
				des_mac6[3]=(unsigned char)tempMac[3];
				des_mac6[4]=(unsigned char)tempMac[4];
				des_mac6[5]=(unsigned char)tempMac[5];
	
				memcpy(&(eth_head[6]),src_mac6,6);
				printf("new src_mac: %02x:%02x:%02x:%02x:%02x:%02x\n",
				eth_head[6]&n,eth_head[7]&n,eth_head[8]&n,eth_head[9]&n,eth_head[10]&n,eth_head[11]&n);
						
				memcpy(eth_head,des_mac6,6);
				printf("new des_mac: %02x:%02x:%02x:%02x:%02x:%02x\n",
				eth_head[0]&n,eth_head[1]&n,eth_head[2]&n,eth_head[3]&n,eth_head[4]&n,eth_head[5]&n);	

				//send
				memset(&addr0,0,sizeof(addr0));
				char eth[10]="eth";
				int length = strlen(eth);  
				//get eth?
				sprintf(eth+length, "%d", dec_eth); 
				printf("eth: %s\n",eth);
				struct ifreq ifrq;
				strcpy(ifrq.ifr_name,eth);
				ioctl(sock_fd,SIOCGIFINDEX,&ifrq);
				addr0.sll_ifindex=ifrq.ifr_ifindex;
				addr0.sll_family=PF_PACKET;
						
				if((sendto(sock_fd,buffer,n_read,0,(struct sockaddr*)&addr0,sizeof(addr0)))<0){
					printf("sendto() error.\n");
					goto final;
				}
				else{
					printf("It is successful to broadcask to eth: %s!\n",eth);
				}
				
			   }

			}
                               
				goto final;	
			}
			printf("check and find the des_ip in the static routing table.\n");
			
			//use gateway_index to find eth???
			dec_eth=route_info[gateway_index].interface;	
			
			//check the arp cache to get the next mac address index
			//use gateway to find MAC
			arp_index = check_arp_cache(route_info[gateway_index].gateway);		
			if (arp_index == -1){
				printf("the next mac address can't be found in the arp cache!\n");
				goto final;
			}
			printf("check and find that next mac address is in the arp cache.\n");
			//modify the header
			memcpy(des_mac,arp_table[arp_index].mac_addr,18);
			//check if the des_mac is hemself
			if (is_to_transmit(des_mac)!=-1){
			        printf("the new destination mac is the router.so don't need to transmit.\n");
				goto final;
			}

			//printf("%s\n",des_mac);
			sscanf(src_mac,"%02x:%02x:%02x:%02x:%02x:%02x",
				&tempMac[0],&tempMac[1],&tempMac[2],&tempMac[3],&tempMac[4],&tempMac[5]);
			src_mac6[0]=(unsigned char)tempMac[0];
			src_mac6[1]=(unsigned char)tempMac[1];
			src_mac6[2]=(unsigned char)tempMac[2];
			src_mac6[3]=(unsigned char)tempMac[3];
			src_mac6[4]=(unsigned char)tempMac[4];
			src_mac6[5]=(unsigned char)tempMac[5];
	
			sscanf(des_mac,"%02x:%02x:%02x:%02x:%02x:%02x",
				&tempMac[0],&tempMac[1],&tempMac[2],&tempMac[3],&tempMac[4],&tempMac[5]);
			des_mac6[0]=(unsigned char)tempMac[0];
			des_mac6[1]=(unsigned char)tempMac[1];
			des_mac6[2]=(unsigned char)tempMac[2];
			des_mac6[3]=(unsigned char)tempMac[3];
			des_mac6[4]=(unsigned char)tempMac[4];
			des_mac6[5]=(unsigned char)tempMac[5];
	
			memcpy(&(eth_head[6]),src_mac6,6);
			printf("new src_mac: %02x:%02x:%02x:%02x:%02x:%02x\n",
				eth_head[6]&n,eth_head[7]&n,eth_head[8]&n,eth_head[9]&n,eth_head[10]&n,eth_head[11]&n);
						
			memcpy(eth_head,des_mac6,6);
			printf("new des_mac: %02x:%02x:%02x:%02x:%02x:%02x\n",
				eth_head[0]&n,eth_head[1]&n,eth_head[2]&n,eth_head[3]&n,eth_head[4]&n,eth_head[5]&n);	

			//send
			memset(&addr0,0,sizeof(addr0));
			char eth[10]="eth";
			int length = strlen(eth);  
			//get eth?
			sprintf(eth+length, "%d", dec_eth); 
			printf("eth: %s\n",eth);
			struct ifreq ifrq;
			strcpy(ifrq.ifr_name,eth);
			ioctl(sock_fd,SIOCGIFINDEX,&ifrq);
			addr0.sll_ifindex=ifrq.ifr_ifindex;
			addr0.sll_family=PF_PACKET;
						
				if((sendto(sock_fd,buffer,n_read,0,(struct sockaddr*)&addr0,sizeof(addr0)))<0){
					printf("sendto() error.\n");
					goto final;
				}
				else
					printf("It is successful to transmit the datagram!\n");
		}
		else
			printf("don't need to transmit.\n");
final:		printf("then next......\n");
		printf("\n-----------------------\n");
	}	
				
	return 0;
}
