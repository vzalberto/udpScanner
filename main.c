
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>           // close()
#include <string.h>           // strcpy, memset(), and memcpy()
#include <pthread.h>
#include <time.h>

#include <netdb.h>            // struct addrinfo
#include <sys/types.h>        // needed for socket(), uint8_t, uint16_t, uint32_t
#include <sys/socket.h>       // needed for socket()
#include <netinet/in.h>       // IPPROTO_RAW, IPPROTO_IP, IPPROTO_UDP, INET_ADDRSTRLEN
#include <netinet/ip.h>       // struct ip and IP_MAXPACKET (which is 65535)
#include <netinet/udp.h>      // struct udphdr
#include <arpa/inet.h>        // inet_pton() and inet_ntop()
#include <sys/ioctl.h>        // macro ioctl is defined
#include <bits/ioctls.h>      // defines values for argument "request" of ioctl.
#include <net/if.h>           // struct ifreq


#include <errno.h>            // errno, perror()

#include <mysql/mysql.h>

// Define some constants.
#define IP4_HDRLEN 20         // IPv4 header length
#define UDP_HDRLEN  8         // UDP header length, excludes data

//infinitum
// #define SOURCE_IP "192.168.1.69"
// #define TARGET_IP "192.168.1.254"

//motorola
#define SOURCE_IP "192.168.0.11"
#define TARGET_IP "192.168.0.1"

#define _10MILI "91603005"
#define _5MILI "45801502"

#define ETH "wlan0"

#define PORT_REPLY_UPDATE "update puertosudp set estado = 0 where puerto = "
#define FINAL_QUERY "select * from puertosudp where estado = 0"

#define DELIMITER_PROCEDURE "delimiter $$ drop procedure if exists load_port_table $$ "
#define PROCEDURE_DEF "create procedure load_port_table () begin declare crs int default 1; while crs < 65536 do insert into puertosudp (puerto) values (crs); set crs = crs + 1; end while; end $$ "
#define END_DELIMITER "delimiter ; "
#define PROCEDURE_CALL "call load_port_table()"

#define RESET_PORTS "truncate table puertosudp;"

// Function prototypes
uint16_t checksum (uint16_t *, int);
uint16_t udp4_checksum (struct ip, struct udphdr, uint8_t *, int);
char *allocate_strmem (int);
uint8_t *allocate_ustrmem (int);
int *allocate_intmem (int);

unsigned short respuestas[65535];

pthread_t tid[16];

void* sendPacket(int* args)
{
  
  int status, datalen, sd, *ip_flags;
  const int on = 1;
  char *interface, *target, *src_ip, *dst_ip;
  struct ip iphdr;
  struct udphdr udphdr;
  uint8_t *data, *packet;
  struct addrinfo hints, *res;
  struct sockaddr_in *ipv4, sin;
  struct ifreq ifr;
  void *tmp;
  pthread_t thread_id;

   // Allocate memory for various arrays.
  data = allocate_ustrmem (IP_MAXPACKET);
  packet = allocate_ustrmem (IP_MAXPACKET);
  interface = allocate_strmem (40);
  target = allocate_strmem (40);
  src_ip = allocate_strmem (INET_ADDRSTRLEN);
  dst_ip = allocate_strmem (INET_ADDRSTRLEN);
  ip_flags = allocate_intmem (4);

  // Interface to send packet through.
  strcpy (interface, ETH);

  // Submit request for a socket descriptor to look up interface.
  if ((sd = socket (AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) {
    perror ("socket() failed to get socket descriptor for using ioctl() ");
    exit (EXIT_FAILURE);
  }

  // Use ioctl() to look up interface index which we will use to
  // bind socket descriptor sd to specified interface with setsockopt() since
  // none of the other arguments of sendto() specify which interface to use.
  memset (&ifr, 0, sizeof (ifr));
  snprintf (ifr.ifr_name, sizeof (ifr.ifr_name), "%s", interface);
  if (ioctl (sd, SIOCGIFINDEX, &ifr) < 0) {
    perror ("ioctl() failed to find interface ");
    return;
  }

    // Source IPv4 address: you need to fill this out

  strcpy (src_ip, SOURCE_IP);

  // Destination URL or IPv4 address: you need to fill this out
  //Starbucks: 10.128.128.128
  //Target MOTOROLA: 192.168.0.1

  strcpy (target, TARGET_IP);

  // Fill out hints for getaddrinfo().
  memset (&hints, 0, sizeof (struct addrinfo));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = hints.ai_flags | AI_CANONNAME;

  // Resolve target using getaddrinfo().
  if ((status = getaddrinfo (target, NULL, &hints, &res)) != 0) {
    fprintf (stderr, "getaddrinfo() failed: %s\n", gai_strerror (status));
    exit (EXIT_FAILURE);
  }
  ipv4 = (struct sockaddr_in *) res->ai_addr;
  tmp = &(ipv4->sin_addr);
  if (inet_ntop (AF_INET, tmp, dst_ip, INET_ADDRSTRLEN) == NULL) {
    status = errno;
    fprintf (stderr, "inet_ntop() failed.\nError message: %s", strerror (status));
    exit (EXIT_FAILURE);
  }
  freeaddrinfo (res);

  // UDP data
  datalen = 4;
  data[0] = 'H';
  data[1] = 'O';
  data[2] = 'L';
  data[3] = 'A';

  // IPv4 header

  // IPv4 header length (4 bits): Number of 32-bit words in header = 5
  iphdr.ip_hl = IP4_HDRLEN / sizeof (uint32_t);

  // Internet Protocol version (4 bits): IPv4
  iphdr.ip_v = 4;

  // Type of service (8 bits)
  iphdr.ip_tos = 0;

  // Total length of datagram (16 bits): IP header + UDP header + datalen
  iphdr.ip_len = htons (IP4_HDRLEN + UDP_HDRLEN + datalen);

  // ID sequence number (16 bits): unused, since single datagram
  iphdr.ip_id = htons (0);

  // Flags, and Fragmentation offset (3, 13 bits): 0 since single datagram

  // Zero (1 bit)
  ip_flags[0] = 0;

  // Do not fragment flag (1 bit)
  ip_flags[1] = 0;

  // More fragments following flag (1 bit)
  ip_flags[2] = 0;

  // Fragmentation offset (13 bits)
  ip_flags[3] = 0;

  iphdr.ip_off = htons ((ip_flags[0] << 15)
                      + (ip_flags[1] << 14)
                      + (ip_flags[2] << 13)
                      +  ip_flags[3]);

  // Time-to-Live (8 bits): default to maximum value
  iphdr.ip_ttl = 255;

  // Transport layer protocol (8 bits): 17 for UDP
  iphdr.ip_p = IPPROTO_UDP;

  // Source IPv4 address (32 bits)
  if ((status = inet_pton (AF_INET, src_ip, &(iphdr.ip_src))) != 1) {
    fprintf (stderr, "inet_pton() failed.\nError message: %s", strerror (status));
    exit (EXIT_FAILURE);
  }

  // Destination IPv4 address (32 bits)
  if ((status = inet_pton (AF_INET, dst_ip, &(iphdr.ip_dst))) != 1) {
    fprintf (stderr, "inet_pton() failed.\nError message: %s", strerror (status));
    exit (EXIT_FAILURE);
  }

  // IPv4 header checksum (16 bits): set to 0 when calculating checksum
  iphdr.ip_sum = 0;
  iphdr.ip_sum = checksum ((uint16_t *) &iphdr, IP4_HDRLEN);


  unsigned short packets_low_limit = 1;
  unsigned short packets_top_limit = 65535; 

  int lote = 1;
  struct timespec contador;
  contador.tv_nsec = _5MILI;


  int packets = 1;
  int lotes = 1;
  int limita, limitb;

// while(lote < 100)
// {
while(lotes <= 100)
{
  limita = packets;
  limitb = packets + 655;
  while(packets < limitb)
  {

      // UDP header

     // Source port number (16 bits): pick a number
     udphdr.source = htons (4950);

      // Destination port number (16 bits): pick a number
     udphdr.dest = htons (packets);

      // Length of UDP datagram (16 bits): UDP header + UDP data
      udphdr.len = htons (UDP_HDRLEN + datalen);

     // UDP checksum (16 bits)
     udphdr.check = udp4_checksum (iphdr, udphdr, data, datalen);

      // Prepare packet.

     // First part is an IPv4 header.
      memcpy (packet, &iphdr, IP4_HDRLEN * sizeof (uint8_t));

      // Next part of packet is upper layer protocol header.
     memcpy ((packet + IP4_HDRLEN), &udphdr, UDP_HDRLEN * sizeof (uint8_t));

  // Finally, add the UDP data.
  memcpy (packet + IP4_HDRLEN + UDP_HDRLEN, data, datalen * sizeof (uint8_t));

 // The kernel is going to prepare layer 2 information (ethernet frame header) for us.
  // For that, we need to specify a destination for the kernel in order for it
  // to decide where to send the raw datagram. We fill in a struct in_addr with
  // the desired destination IP address, and pass this structure to the sendto() function.
  memset (&sin, 0, sizeof (struct sockaddr_in));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = iphdr.ip_dst.s_addr;

  // Submit request for a raw socket descriptor.
  if ((sd = socket (AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) {
    perror ("socket() failed ");
    exit (EXIT_FAILURE);
  }

  // Set flag so socket expects us to provide IPv4 header.
  if (setsockopt (sd, IPPROTO_IP, IP_HDRINCL, &on, sizeof (on)) < 0) {
    perror ("setsockopt() failed to set IP_HDRINCL ");
    exit (EXIT_FAILURE);
  }

  // Bind socket to interface index.
  if (setsockopt (sd, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof (ifr)) < 0) {
    perror ("setsockopt() failed to bind to interface ");
    exit (EXIT_FAILURE);
  }

  // Send packet.
  if (sendto (sd, packet, IP4_HDRLEN + UDP_HDRLEN + datalen, 0, (struct sockaddr *) &sin, sizeof (struct sockaddr)) < 0)  {
    perror ("sendto() failed ");
    exit (EXIT_FAILURE);
  }

 // Close socket descriptor.
  close (sd);

  memset (packet, 0x00, IP4_HDRLEN + UDP_HDRLEN + datalen);

//nanosleep(&contador, NULL);
  packets++;
}
printf("\noye que pasa que ocurre\n");
printf("\nlote: %d\n", lotes);
lotes++;
}

//   
//   lote++;
//   printf("\n%d\n", lote);
// }


  //printf("\n%d paquetes UDP enviados\n", packets);
  
  // Free allocated memory.
  free (data);
  free (interface);
  free (target);
  free (src_ip);
  free (dst_ip);
  free (ip_flags);

}


void* recvPacket()
{
    int saddr_size , data_size, packets;
    int sock_raw;
    struct sockaddr_in omg;
    struct sockaddr saddr;
    struct in_addr in;
    struct timeval tv;
     
    unsigned char *buffer = (unsigned char *)malloc(65536); //Its Big!
     
    //Create a raw socket that shall sniff
    sock_raw = socket(AF_INET , SOCK_RAW , 1); //NO FUNCIONA CON HTONS POR QUEEEE
    if(sock_raw < 0)
    {
        printf("Socket Error\n");
        return -1;
    }

    omg.sin_family = AF_INET;
    omg.sin_port = htons(4950);
    omg.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock_raw, (struct sockaddr *)&omg, sizeof(omg)) < 0)
    {
      perror("NEL con bind");
      exit(1);
      }

        int c = 0;

    while(1)
    {
        saddr_size = sizeof (omg);
        //Receive a packet
        data_size = recvfrom(sock_raw , buffer , 65536 , 0 , (struct sockaddr *)&omg , &saddr_size);
        if(data_size <0 )
        {
            printf("Recvfrom error , failed to get packets\n");
            return -1;
        }

        //Now process the packet
        unsigned short puerto;
        puerto = (buffer[50] << 8) + buffer[51];
        printf("\nPuerto: %hu\n", puerto);

        /*if(puerto != 0)
        {
          
        printf("\nPaquete: \n");
        int i = 0;
        for(i; i < 100; i++)
        {
          if(i%8 == 0)
            printf("\n");
          printf("%02X ", buffer[i]);
        }

        printf("\nPuerto: %hu\n", puerto);*/
        printf("\nRespuestas recibidas: %d\n", c);
        actualizaEnTabla(puerto);
        //respuestas[c] = puerto;
        c++;

        

}
    return (void *) packets;    
}



int main(int argc, char **argv)
{
    int i = 0;
    int err;
    char* target;

    int sendPacket_args[2];
    sendPacket_args[0] = 1;
    sendPacket_args[1] = 32767;

  //resetPorts();
  //setPorts();

    while(i < 2)
    {
      
      err = pthread_create(&(tid[i]), NULL, &sendPacket, sendPacket_args);
        if (err != 0)
            printf("\ncan't create thread :[%s]", strerror(err));

          printf("\nTHREAD %d", i + 1);
          printf("\nLow limit main: %d", sendPacket_args[0]);
          printf("\nTop limit main: %d\n", sendPacket_args[1]);

          sendPacket_args[0]+=32767;
          sendPacket_args[1]+=32767;
          i++;

    }
    err = pthread_create(&(tid[0]), NULL, &sendPacket, NULL);
        if (err != 0)
            printf("\ncan't create thread :[%s]", strerror(err));
        else
            printf("\n Sending...\n");

     err = pthread_create(&(tid[1]), NULL, &recvPacket, NULL);
        if (err != 0)
            printf("\ncan't create thread :[%s]", strerror(err));
        else
            printf("\n Receiving...\n");


    sleep(20);
    
    consultapuertos();
    printf("\nBYE\n");


    return 0;
}


// Checksum function
uint16_t
checksum (uint16_t *addr, int len)
{
  int nleft = len;
  int sum = 0;
  uint16_t *w = addr;
  uint16_t answer = 0;

  while (nleft > 1) {
    sum += *w++;
    nleft -= sizeof (uint16_t);
  }

  if (nleft == 1) {
    *(uint8_t *) (&answer) = *(uint8_t *) w;
    sum += answer;
  }

  sum = (sum >> 16) + (sum & 0xFFFF);
  sum += (sum >> 16);
  answer = ~sum;
  return (answer);
}

// Build IPv4 UDP pseudo-header and call checksum function.
uint16_t
udp4_checksum (struct ip iphdr, struct udphdr udphdr, uint8_t *payload, int payloadlen)
{
  char buf[IP_MAXPACKET];
  char *ptr;
  int chksumlen = 0;
  int i;

  ptr = &buf[0];  // ptr points to beginning of buffer buf

  // Copy source IP address into buf (32 bits)
  memcpy (ptr, &iphdr.ip_src.s_addr, sizeof (iphdr.ip_src.s_addr));
  ptr += sizeof (iphdr.ip_src.s_addr);
  chksumlen += sizeof (iphdr.ip_src.s_addr);

  // Copy destination IP address into buf (32 bits)
  memcpy (ptr, &iphdr.ip_dst.s_addr, sizeof (iphdr.ip_dst.s_addr));
  ptr += sizeof (iphdr.ip_dst.s_addr);
  chksumlen += sizeof (iphdr.ip_dst.s_addr);

  // Copy zero field to buf (8 bits)
  *ptr = 0; ptr++;
  chksumlen += 1;

  // Copy transport layer protocol to buf (8 bits)
  memcpy (ptr, &iphdr.ip_p, sizeof (iphdr.ip_p));
  ptr += sizeof (iphdr.ip_p);
  chksumlen += sizeof (iphdr.ip_p);

  // Copy UDP length to buf (16 bits)
  memcpy (ptr, &udphdr.len, sizeof (udphdr.len));
  ptr += sizeof (udphdr.len);
  chksumlen += sizeof (udphdr.len);

  // Copy UDP source port to buf (16 bits)
  memcpy (ptr, &udphdr.source, sizeof (udphdr.source));
  ptr += sizeof (udphdr.source);
  chksumlen += sizeof (udphdr.source);

  // Copy UDP destination port to buf (16 bits)
  memcpy (ptr, &udphdr.dest, sizeof (udphdr.dest));
  ptr += sizeof (udphdr.dest);
  chksumlen += sizeof (udphdr.dest);

  // Copy UDP length again to buf (16 bits)
  memcpy (ptr, &udphdr.len, sizeof (udphdr.len));
  ptr += sizeof (udphdr.len);
  chksumlen += sizeof (udphdr.len);

  // Copy UDP checksum to buf (16 bits)
  // Zero, since we don't know it yet
  *ptr = 0; ptr++;
  *ptr = 0; ptr++;
  chksumlen += 2;

  // Copy payload to buf
  memcpy (ptr, payload, payloadlen);
  ptr += payloadlen;
  chksumlen += payloadlen;

  // Pad to the next 16-bit boundary
  for (i=0; i<payloadlen%2; i++, ptr++) {
    *ptr = 0;
    ptr++;
    chksumlen++;
  }

  return checksum ((uint16_t *) buf, chksumlen);
}

// Allocate memory for an array of chars.
char *
allocate_strmem (int len)
{
  void *tmp;

  if (len <= 0) {
    fprintf (stderr, "ERROR: Cannot allocate memory because len = %i in allocate_strmem().\n", len);
    exit (EXIT_FAILURE);
  }

  tmp = (char *) malloc (len * sizeof (char));
  if (tmp != NULL) {
    memset (tmp, 0, len * sizeof (char));
    return (tmp);
  } else {
    fprintf (stderr, "ERROR: Cannot allocate memory for array allocate_strmem().\n");
    exit (EXIT_FAILURE);
  }
}

// Allocate memory for an array of unsigned chars.
uint8_t *
allocate_ustrmem (int len)
{
  void *tmp;

  if (len <= 0) {
    fprintf (stderr, "ERROR: Cannot allocate memory because len = %i in allocate_ustrmem().\n", len);
    exit (EXIT_FAILURE);
  }

  tmp = (uint8_t *) malloc (len * sizeof (uint8_t));
  if (tmp != NULL) {
    memset (tmp, 0, len * sizeof (uint8_t));
    return (tmp);
  } else {
    fprintf (stderr, "ERROR: Cannot allocate memory for array allocate_ustrmem().\n");
    exit (EXIT_FAILURE);
  }
}

// Allocate memory for an array of ints.
int *
allocate_intmem (int len)
{
  void *tmp;

  if (len <= 0) {
    fprintf (stderr, "ERROR: Cannot allocate memory because len = %i in allocate_intmem().\n", len);
    exit (EXIT_FAILURE);
  }

  tmp = (int *) malloc (len * sizeof (int));
  if (tmp != NULL) {
    memset (tmp, 0, len * sizeof (int));
    return (tmp);
  } else {
    fprintf (stderr, "ERROR: Cannot allocate memory for array allocate_intmem().\n");
    exit (EXIT_FAILURE);
  }
}

void actualizaEnTabla(unsigned short puerto)
{
  MYSQL *conn;
  
  char *server = "localhost";
  char *user = "root";
  char *password = "//lsoazules"; 
  char *database = "redes";

  conn = mysql_init(NULL);
  if(!mysql_real_connect(conn, server, user, password, database, 0, NULL, 0)) 
  { 
    fprintf(stderr, "ERROR para conectarse a mysql", mysql_error(conn));
    return -1;
  }

  char *pseudo_insert_query = malloc(60);
  pseudo_insert_query[0] = 0x00;
  char port_string[5];

  strcat(pseudo_insert_query, PORT_REPLY_UPDATE);

  sprintf(port_string, "%hu", puerto);
  strncat(pseudo_insert_query, port_string, 5);

  if(mysql_query(conn, pseudo_insert_query))
      fprintf(stderr, "\nNEL con la actualizacion del puerto\n", mysql_error(conn));

  mysql_close(conn);
  free(pseudo_insert_query);
}

void printSockaddr(struct sockaddr *in)
{
  int size = sizeof(in);
}

void consultapuertos()
{
  MYSQL *conn;
  MYSQL_RES *res;
  MYSQL_ROW row;
  
  char *server = "localhost";
  char *user = "root";
  char *password = "//lsoazules"; 
  char *database = "redes";

  conn = mysql_init(NULL);
  if(!mysql_real_connect(conn, server, user, password, database, 0, NULL, 0)) 
  { 
    fprintf(stderr, "ERROR para conectarse a mysql", mysql_error(conn));
    return -1;
  }

  if(mysql_query(conn, "select * from puertosudp where estado = 0"))
            fprintf(stderr, "\nNEL con el query, quien sabe si tiene puertos abiertos, te la debo\n", mysql_error(conn));     
  else
    {
            printf("\nPUERTOS QUE ENVIARON RESPUESTA ICMP DE ERROR:\n\n");
            res = mysql_use_result(conn);
            while((row = mysql_fetch_row(res)) != NULL)
            {
              printf("%s  \n", row[0]);
            }
          }

    mysql_free_result(res);
    mysql_close(conn);
    //free(final_query);
}

void setPorts()
{
  MYSQL *conn;
  
  char *server = "localhost";
  char *user = "root";
  char *password = "//lsoazules"; 
  char *database = "redes";

  conn = mysql_init(NULL);
  if(!mysql_real_connect(conn, server, user, password, database, 0, NULL, 0)) 
  { 
    fprintf(stderr, "ERROR para conectarse a mysql", mysql_error(conn));
    return -1;
  }
  else
  {    
    char q1[50], q2[90], q3[20], q4[40];
    strcat(q1, DELIMITER_PROCEDURE);
    strcat(q2, PROCEDURE_DEF);
    strcat(q3, END_DELIMITER);
    strcat(q4, PROCEDURE_CALL);

  if(mysql_query(conn, q1))
  {    
            fprintf(stderr, "\nError q1\n", mysql_error(conn));
            exit(1);     
  }
  else   if(mysql_query(conn, q2))
  {    
            fprintf(stderr, "\nError q2\n", mysql_error(conn));
            exit(1);     
  }
  else   if(mysql_query(conn, q3))
  {    
            fprintf(stderr, "\nError q3\n", mysql_error(conn));
            exit(1);     
  }
  else   if(mysql_query(conn, q4))
  {    
            fprintf(stderr, "\nError q4\n", mysql_error(conn));
            exit(1);     
  }


  
  else
    printf("\nTabla de puertos lista\n");
}
              
    mysql_close(conn);
}

void resetPorts()
{
  MYSQL *conn;
  
  char *server = "localhost";
  char *user = "root";
  char *password = "//lsoazules"; 
  char *database = "redes";

  conn = mysql_init(NULL);

  if(!mysql_real_connect(conn, server, user, password, database, 0, NULL, 0)) 
  { 
    fprintf(stderr, "ERROR para conectarse a mysql", mysql_error(conn));
    return -1;
  }
  else
  {    
    char query[90];
    strcat(query, RESET_PORTS);
    printf("\nTruncando la tabla de puertos: %s\n", query);

  if(mysql_query(conn, query))
  {    
            fprintf(stderr, "\nError al resetear lista de puertos\n", mysql_error(conn));
            exit(1);     
  }
  else
    printf("\nTabla de puertos lista\n");
              }
    mysql_close(conn);
}
