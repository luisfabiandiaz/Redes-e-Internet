  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>
  #include <unistd.h>

  int main(void)
  {
    struct sockaddr_in stSockAddr;
    int Res;
    char buffer[256];
    int SocketFD = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    int n;

    if (-1 == SocketFD)
    {
      perror("cannot create socket");
      exit(EXIT_FAILURE);
    }

    memset(&stSockAddr, 0, sizeof(struct sockaddr_in));

    stSockAddr.sin_family = AF_INET;
    stSockAddr.sin_port = htons(1100);
    Res = inet_pton(AF_INET, "10.0.2.15", &stSockAddr.sin_addr);

    if (0 > Res)
    {
      perror("error: first parameter is not a valid address family");
      close(SocketFD);
      exit(EXIT_FAILURE);
    }
    else if (0 == Res)
    {
      perror("char string (second parameter does not contain valid ipaddress");
      close(SocketFD);
      exit(EXIT_FAILURE);
    }

    if (-1 == connect(SocketFD, (const struct sockaddr *)&stSockAddr, sizeof(struct sockaddr_in)))
    {
      perror("connect failed");
      close(SocketFD);
      exit(EXIT_FAILURE);
    }

    while(1) {
        bzero(buffer,256);
        fgets(buffer, 255, stdin);
        if (strcmp("adios\n", buffer) == 0) {
            n = write(SocketFD,buffer,strlen(buffer));
            break;
        }
        n = write(SocketFD,buffer,strlen(buffer));

        if (n < 0) perror("ERROR writing to socket");

        bzero(buffer,256);
        n = read(SocketFD,buffer,255);
        if (n < 0) perror("ERROR reading from socket");
        printf(buffer);

        if (strcmp("adios\n", buffer) == 0) {
            break;
        }
    }
    shutdown(SocketFD, SHUT_RDWR);
    close(SocketFD);
    return 0;
}
