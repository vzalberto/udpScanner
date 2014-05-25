#include<stdio.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<string.h>

int main(int argc, char *argv[])
{
	if(argc < 2)
	{
		printf("\nEMPEZAMOS MAL\n");
		return -1;
	}

	int fs = socket(AF_INET, SOCK_DGRAM, 0);
	struct sockaddr_in hostDest;

	hostDest.sin_family = AF_INET;
	hostDest.sin_port = htons(80);

	printf("\n%s\n", argv[1]);

	inet_aton(argv[1], &hostDest.sin_addr);

	char msg[] = "QUE PEDO PUERTO";

	if(sendto(fs, msg, strlen(msg), 0, (struct sockaddr *) &hostDest, sizeof(hostDest)) < 0)
		printf("\nNEL\n");
	else
		printf("\nSI\n");

	return 0;
}