#include "socket.h"
#include "stdio.h"
#include "stdint.h"

static signed short fd;

void udp_init(void)
{
	if (fd == 0)
		fd = sl_Socket(SL_AF_INET,SL_SOCK_DGRAM, 0);
}

int udp_sendmsg(unsigned int ulStaIp, unsigned short usPort, unsigned char *buf, int len)
{
	SlSockAddrIn_t  sAddr;
	int iStatus;
	int iAddrSize = sizeof(SlSockAddrIn_t);

    sAddr.sin_family = SL_AF_INET;
    sAddr.sin_port = sl_Htons((unsigned short)usPort);
    sAddr.sin_addr.s_addr = sl_Htonl((unsigned int)ulStaIp);
    iAddrSize = sizeof(SlSockAddrIn_t);

    iStatus = sl_SendTo(fd, buf, len, 0,
    									(SlSockAddr_t *)&sAddr, iAddrSize);

    if (iStatus < 0) {
    	printf("send message failed\n");
    }

    return iStatus;
}

void udp_close(void)
{
	sl_Close(fd);
}
