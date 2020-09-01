/*
▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄
█▄ ▄█ ▄▄▀█▀▄▀█ ██ ██ █ ▄▀█ ▄▄█ ▄▄██
██ ██ ██ █ █▀█ ██ ██ █ █ █ ▄▄█▄▄▀██
█▀ ▀█▄██▄██▄██▄▄██▄▄▄█▄▄██▄▄▄█▄▄▄██
▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netinet/ether.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netpacket/packet.h>

#include <getopt.h>

/*
▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄
██ ▄▄▄█ ██ █ ▄▄▀█▀▄▀█▄ ▄██▄██▀▄▄▀█ ▄▄▀█ ▄▄██
██ ▄▄██ ██ █ ██ █ █▀██ ███ ▄█ ██ █ ██ █▄▄▀██
██ █████▄▄▄█▄██▄██▄███▄██▄▄▄██▄▄██▄██▄█▄▄▄██
▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀
*/
struct in_addr get_my_ip(struct ifreq* if_req)
{
	struct sockaddr_in* ip_addr;
	int sock = socket(AF_INET, SOCK_DGRAM, 0);

	ioctl(sock, SIOCGIFADDR, if_req);
	ip_addr = (struct sockaddr_in*)&(if_req->ifr_addr);

	close(sock);

	return ip_addr->sin_addr;
}

struct ether_addr get_my_mac(struct ifreq* if_req)
{
	struct ether_addr mac_addr;
	int sock = socket(AF_INET, SOCK_DGRAM, 0);

	if(sock < 0)
		printf("socket");

	if(ioctl(sock, SIOCGIFHWADDR, if_req) < 0)
		printf("error");

	memcpy(&(mac_addr.ether_addr_octet), if_req->ifr_hwaddr.sa_data, sizeof(mac_addr.ether_addr_octet));

	close(sock);

	return mac_addr;
}

/*
▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄
████ ▄▀▄ █ ▄▄▀██▄██ ▄▄▀████
████ █ █ █ ▀▀ ██ ▄█ ██ ████
████ ███ █▄██▄█▄▄▄█▄██▄████
▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀
*/
int main(int argc, char* argv[])
{
	int if_index;
	struct ifreq if_req;
	int socket_fd;
	struct sockaddr_ll dest_addr;
	socklen_t len;
	int t = 3;
	struct timeval timeout;
	timeout.tv_sec = 3;
	timeout.tv_usec = 0;
	char* if_str;
	char* dest_ip_str;


	if(argc != 3)
	{
		printf("Expected 2 parameters. Found %i\n", argc);
		exit(1);
	}

	int parsed_option;
	struct option long_options[] = {
		{"interface", required_argument, NULL, 'i'},
		{"timeout", required_argument, NULL, 't'},
	};

	while((parsed_option = getopt_long(argc, argv, "i:t:", long_options, NULL)) != -1){
		switch(parsed_option){
			case 'i': if_str = optarg; break;
			case 't': t = atoi(optarg); break;
		}
	}
	dest_ip_str = argv[argc - 1];

	if(t < 1)
	{
		printf("negative timeout\n");
		exit(1);
	}
	timeout.tv_sec = t;


	if(strlen(if_str) > IFNAMSIZ)
		printf("error\n"), exit(1);

	strcpy(if_req.ifr_name, if_str);

	if((if_index = if_nametoindex(if_str)) == 0)
	{
		perror("Error: could not detect the interface");
		exit(1);
	}



	struct ether_addr dest_mac = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	struct ether_addr source_mac = get_my_mac(&if_req);
	struct in_addr dest_ip;
	struct in_addr source_ip = get_my_ip(&if_req);

	if((inet_aton(dest_ip_str, &dest_ip)) == 0)
	{
		printf("inet_aton");
		exit(1);
	}



	uint8_t buffer[sizeof(struct ether_header) + sizeof(struct ether_arp)];
	struct ether_header* ether = (struct ether_header*)buffer;
	struct ether_arp* arp = (struct ether_arp*)(buffer + sizeof(struct ether_header));

	memcpy(&(ether->ether_dhost), &dest_mac, sizeof(ether->ether_dhost));
	memcpy(&(ether->ether_shost), &source_mac, sizeof(ether->ether_shost));
	ether->ether_type = htons(ETH_P_ARP);

	arp->arp_hrd = htons(ARPHRD_ETHER);
	arp->arp_pro = htons(ETH_P_IP);
	arp->arp_hln = ETH_ALEN;
	arp->arp_pln = sizeof(struct in_addr);
	arp->arp_op = htons(ARPOP_REQUEST);
	memcpy(&(arp->arp_sha), &source_mac, sizeof(arp->arp_sha));
	memcpy(&(arp->arp_spa), &source_ip, sizeof(arp->arp_spa));
	memcpy(&(arp->arp_tha), &dest_mac, sizeof(arp->arp_tha));
	memcpy(&(arp->arp_tpa), &dest_ip, sizeof(arp->arp_tpa));


	memset(&dest_addr, 0x0, sizeof(struct sockaddr_ll));
	dest_addr.sll_family = AF_PACKET;
	dest_addr.sll_ifindex = if_index;
	dest_addr.sll_halen = ETH_ALEN;
	dest_addr.sll_protocol = htons(ETH_P_ARP);
	memcpy(&(dest_addr.sll_addr), &dest_mac, sizeof(dest_addr.sll_addr));



	if((socket_fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ARP))) < 0){
		perror("Error: could not open socket");
		exit(1);
	}

	setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
	

	sendto(socket_fd, &buffer, sizeof(buffer), 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
	recvfrom(socket_fd, &buffer, sizeof(buffer), 0, (struct sockaddr*)&dest_addr, &len);

	if(memcmp(&source_mac, &(arp->arp_sha), sizeof(arp->arp_sha)) != 0)
	{
		memcpy(&source_mac, &(arp->arp_sha), sizeof(arp->arp_sha));

		printf("%s\n", ether_ntoa(&source_mac));
	}

	close(socket_fd);

	return 0;
}
