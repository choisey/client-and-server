/*
 * A TCP client that manages multiple connections to a server and handles
 * all read and write operations in a single thread by using socket select.
 */
#include <arpa/inet.h> // inet_addr()
#include <errno.h>
#include <stdio.h>
#include <stdlib.h> // exit()
#include <unistd.h> // read(), write(), close()

#define BUFLEN 64
#define PORT 8080
#define HOST "127.0.0.1"
#define MAX_CONN 20

int main(int argc, char* argv[])
{
    if ( argc <= 1 )
    {
        fprintf(stderr, "Usage: %s [filename]...\n", argv[0]);
        exit(0);
    }

    int conn_cnt = 0;
    int connfds[MAX_CONN];
    FILE *fds[MAX_CONN];

    for ( int i = 1; i < argc; i++ )
    {
        FILE* fp = fopen(argv[i], "r");
        if ( fp )
        {
            int sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if ( -1 == sockfd )
            {
                switch ( errno )
                {
                    case EACCES:
                        // Permission to create a socket of the specified type and/or
                        // protocol is denied.

                    case EAFNOSUPPORT:
                        // The implementation does not support the specified address
                        // family.

                    case EINVAL:
                        // Unknown protocol, or protocol family not available.

                        // Invalid flags in type.

                        // The per-process limit on the number of open file
                        // descriptors has been reached.

                    case ENFILE:
                        // The system-wide limit on the total number of open files
                        // has been reached.

                    case ENOBUFS:
                    case ENOMEM:
                        // Insufficient memory is available.  The socket cannot be
                        // created until sufficient resources are freed.

                    case EPROTONOSUPPORT:
                        // The protocol type or the specified protocol is not
                        // supported within this domain.

                    default:
                        fprintf(stderr, "socket creation error\n");
                        exit(1);
                }
            }

            struct sockaddr_in servaddr;
            servaddr.sin_family = AF_INET;
            servaddr.sin_port = htons(PORT);
            servaddr.sin_addr.s_addr = inet_addr(HOST);

            if ( -1 == connect(sockfd, (struct sockaddr*) &servaddr, sizeof(servaddr) ))
            {
                fprintf(stderr, "socket connec error\n");
                exit(1);
            }

            fds[conn_cnt] = fp;
            connfds[conn_cnt] = sockfd;

            ++conn_cnt;
        }
    }

    int max_socket_fd = 0;

    fd_set current_sockets;
    fd_set read_fds;
    fd_set write_fds;
    fd_set except_fds;

    FD_ZERO(&current_sockets);

    for ( int i = 0; i < conn_cnt; i++ )
    {
        if ( max_socket_fd < connfds[i] ) max_socket_fd = connfds[i];
        FD_SET(connfds[i], &current_sockets);
    }

    int active_sock_cnt = conn_cnt;

    while ( active_sock_cnt )
    {
        // select is destructive
        read_fds = current_sockets;
        write_fds = current_sockets;
        except_fds = current_sockets;

        if ( -1 == select(max_socket_fd + 1, &read_fds, &write_fds, &except_fds, NULL) )
        {
            switch ( errno )
            {
                case EINTR:
                    // A signal was caught
                    fprintf(stderr, "shutting down...\n");
                    //close(listen_socket);
                    exit(0);

                case EBADF:
                    // An invalid file descriptor was given in one of the sets.
                    // (Perhaps a file descriptor that was already closed, or one
                    // on which an error has occurred.)  However, see BUGS.

                case EINVAL:
                    // nfds is negative or exceeds the RLIMIT_NOFILE resource
                    // limit (see getrlimit(2)).
                    // The value contained within timeout is invalid.

                case ENOMEM:
                    // Unable to allocate memory for internal tables.

                default:
                    fprintf(stderr, "select error\n");
                    exit(1);
            }
        }

        // TODO
        // use fcntl(O_NONBLOCK) instead of send(..., MSG_DONTWAIT)
        //int flags = fcntl(fd, F_GETFL, 0);
        //fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        for ( int i = 0; i < conn_cnt; i++ )
        {
            if ( FD_ISSET(connfds[i], &write_fds) )
            {
                char bb[BUFLEN];
                size_t bytes_read;
                bytes_read = fread(bb, sizeof(char), sizeof(bb), fds[i]);
                if ( 0 != bytes_read )
                {
                    /*int bytes_sent =*/ send(connfds[i], bb, bytes_read, MSG_DONTWAIT);
                }
                else
                {
                    FD_CLR(connfds[i], &current_sockets);
                    close(connfds[i]);
                    fclose(fds[i]);
                    active_sock_cnt--;
                }
            }
        }

    }
}
