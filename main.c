#include<stdio.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<net/if.h>
#include<arpa/inet.h>
#include<string.h>
#include<sys/ioctl.h>

int main(int argc, char *argv[])
{
	if(argc < 3)
	{
		printf("\nEMPEZAMOS MAL\n");
		return -1;
	}

	int fs = socket(AF_INET, SOCK_DGRAM, 0);
	unsigned int puerto = strtoumax (argv[2], NULL, 10);
	struct sockaddr_in hostDest;
	struct ifreq ifr;
	unsigned char my_ip[4];
	unsigned char their_ip[4];

	hostDest.sin_family = AF_INET;
	hostDest.sin_port = htons( puerto );

	printf("\nORA\n");

	printf("\n puerto %d\n", puerto);

	inet_aton(argv[1], &hostDest.sin_addr);
	memcpy(&their_ip, &hostDest.sin_addr, 4);

	char msg[] = "QUE PEDO PUERTO";

	if(sendto(fs, msg, strlen(msg), 0, (struct sockaddr *) &hostDest, sizeof(hostDest)) < 0)
		printf("\nNEL\n");
	else
		printf("\nSI\n");

	strcpy(ifr.ifr_name, "wlan1");
	if(ioctl(fs, SIOCGIFADDR, &ifr) < 0)
		{
			perror("ERROR al obtener IP origen");
			return -1;
		}

	memcpy(&my_ip, &ifr.ifr_hwaddr.sa_data[2], 4);
	printf("\n%u.%u.%u.%u\n", my_ip[0], my_ip[1], my_ip[2], my_ip[3]);

	unsigned char buff[512];
	memset(&msg, 0, sizeof(msg));
	memset(&hostDest, 0, sizeof(hostDest));

	do
	{
		if(recvfrom(fs, buff, sizeof(buff), 0, (struct sockaddr *) &hostDest, sizeof(hostDest)) < 0)
		{
			perror("ERROR al recibir");
			return -1;
		}
		if(memcmp(&hostDest.sin_addr, &their_ip, 4) == 0)				//Verificar si hay un sintaxis mas reducida
			printf("\nHay respuesta\n");
	}while(1);	

	close(fs);
	return 0;
}