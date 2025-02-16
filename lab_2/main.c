#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>

void print_usage(const char* program_name) {
    printf("Usage: %s [options] <address> [port]\n"
           "Options:\n"
           "  -4               Use IPv4 (default)\n"
           "  -6               Use IPv6\n"
           "  -n               Show numeric addresses only\n"
           "  -s               Show service names for ports\n"
           "  -h               Show this help message\n", program_name);
}

int main(int argc, char* argv[]) {
    int use_ipv4 = 1;
    int numeric_only = 0;
    int show_service_names = 0;
    int opt;

    while ((opt = getopt(argc, argv, "46nsh")) != -1) {
        switch (opt) {
            case '4': use_ipv4 = 1; break;
            case '6': use_ipv4 = 0; break;
            case 'n': numeric_only = 1; break;
            case 's': show_service_names = 1; break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                fprintf(stderr, "Unknown option: %c\n", opt);
                print_usage(argv[0]);
                return 1;
        }
    }

    struct addrinfo *res;
    {
        const char* address = argv[optind];
        const char* port = (optind + 1 < argc) ? argv[optind + 1] : NULL;
        struct addrinfo hints = {
            .ai_family = use_ipv4 ? AF_INET : AF_INET6,
            .ai_socktype = SOCK_STREAM
        };
        const int status = getaddrinfo(address, port, &hints, &res);
        if (status != 0) {
            fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
            return 1;
        }
    }
    
    for (struct addrinfo *p = res; p != NULL; p = p->ai_next) {
        void* addr;
        char ipstr[INET6_ADDRSTRLEN];
        uint16_t port;

        if (p->ai_family == AF_INET) {
            struct sockaddr_in* ipv4 = (struct sockaddr_in*)p->ai_addr;
            addr = &(ipv4->sin_addr);
            port = ntohs(ipv4->sin_port);
        } else {
            struct sockaddr_in6* ipv6 = (struct sockaddr_in6*)p->ai_addr;
            addr = &(ipv6->sin6_addr);
            port = ntohs(ipv6->sin6_port);
        }

        if(inet_ntop(p->ai_family, addr, ipstr, sizeof(ipstr)) == 0) {
            fprintf(stderr, "inet_ntop error: %s\n", strerror(errno));
            return 1;
        }

        printf("Address: %s", ipstr);
        if (!numeric_only) {
            char hostname[NI_MAXHOST];
            if (getnameinfo(p->ai_addr, p->ai_addrlen, hostname, NI_MAXHOST, NULL, 0, NI_NAMEREQD) == 0) {
                printf(" (%s)", hostname);
            }
        }
        printf("\n");

        printf("Port: %d", port);
        if (show_service_names) {
            char servname[NI_MAXSERV];
            if (getnameinfo(p->ai_addr, p->ai_addrlen, NULL, 0, servname, NI_MAXSERV, NI_NAMEREQD) == 0) {
                printf(" (%s)", servname);
            }
        }
        printf("\n");

        printf("Protocol: %d (", p->ai_protocol);
        switch (p->ai_protocol) {
            case IPPROTO_TCP: printf("TCP"); break;
            case IPPROTO_UDP: printf("UDP"); break;
            default: printf("Unknown"); break;
        }
        printf(")\n\n");
    }

    freeaddrinfo(res);
    return 0;
}