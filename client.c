/*
 * Copyright (c) Seungyeob Choi
 *
 * A TCP client that manages multiple connections to a server and handles
 * all read and write operations in a single thread using epoll.
 */
#include <arpa/inet.h>  // inet_addr()
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>     // exit()
#include <string.h>     // strncmp()
#include <sys/epoll.h>
#include <unistd.h>     // read(), write(), close()

#define BUFLEN 64
#define PORT 8080
#define HOST "127.0.0.1"

// max number of events that can be returned by epoll at a time
#define MAX_EVENTS 20

struct connection_ctx
{
    int socket_fd;
    FILE* fp;
    char buffer[BUFLEN];
    struct connection_ctx *next;
};

static void clear_connection_ctx_list(struct connection_ctx *head)
{
    while ( NULL != head )
    {
        struct connection_ctx *next = head->next;

        if ( 0 != head->socket_fd )
        {
            if ( -1 == close(head->socket_fd) )
            {
                switch ( errno )
                {
                    case EBADF:
                    case EINTR:
                    case EIO:
                    default:
                        fprintf(stderr, "socket close error (%d)", errno);
                        exit(1);
                }
            }
        }

        if ( NULL != head->fp )
            fclose(head->fp);

        free(head);

        head = next;
    }
}

static int close_connection(int epollfd, int connfd)
{
    if ( -1 == epoll_ctl(epollfd, EPOLL_CTL_DEL, connfd, NULL) )
    {
        switch ( errno )
        {
            case EBADF:
            case EEXIST:
            case EINVAL:
            case ENOENT:
            case ENOMEM:
            case ENOSPC:
            case EPERM:
            default:
                fprintf(stderr, "epoll_ctl error (%d)\n", errno);
                exit(1);
        }
    }

    if ( -1 == close(connfd) )
    {
        switch ( errno )
        {
            case EBADF:
            case EINTR:
            case EIO:
            default:
                fprintf(stderr, "socket close error (%d)\n", errno);
                exit(1);
        }
    }

    return 0;
}

int main(int argc, char* argv[])
{
    if ( argc <= 1 )
    {
        fprintf(stderr, "Usage: %s [filename]...\n", argv[0]);
        exit(0);
    }

    struct connection_ctx *connection_head = NULL;
    struct connection_ctx *connection_tail = NULL;
    int conn_cnt = 0;

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
                    case EAFNOSUPPORT:
                    case EINVAL:
                    case ENFILE:
                    case ENOBUFS:
                    case ENOMEM:
                    case EPROTONOSUPPORT:
                    default:
                        fprintf(stderr, "socket creation error (%d)\n", errno);
                        exit(1);
                }
            }

            // connect to the server

            struct sockaddr_in servaddr;
            servaddr.sin_family = AF_INET;
            servaddr.sin_port = htons(PORT);
            servaddr.sin_addr.s_addr = inet_addr(HOST);

            if ( -1 == connect(sockfd, (struct sockaddr*) &servaddr, sizeof(servaddr) ))
            {
                switch ( errno )
                {
                    case ECONNREFUSED:
                        fprintf(stderr, "connection refused.\n");
                        exit(1);

                    case EADDRNOTAVAIL:
                    case EAFNOSUPPORT:
                    case EALREADY:
                    case EBADF:
                    case EINPROGRESS:
                    case EINTR:
                    case EISCONN:
                    case ENETUNREACH:
                    case ENOTSOCK:
                    case EPROTOTYPE:
                    case ETIMEDOUT:
                    case EIO:
                    case ENOENT:
                    case ENOTDIR:
                    case EACCES:
                    case EADDRINUSE:
                    case ECONNRESET:
                    case EHOSTUNREACH:
                    case EINVAL:
                    case ELOOP:
                    case ENAMETOOLONG:
                    case ENETDOWN:
                    case ENOBUFS:
                    case EOPNOTSUPP:
                    default:
                        fprintf(stderr, "socket connect error (%d)\n", errno);
                        exit(1);
                }
            }

            // set non-blocking

            int flags = fcntl(sockfd, F_GETFL, 0);
            if ( -1 == flags )
            {
                switch ( errno )
                {
                    case EACCES:
                    case EAGAIN:
                    case EBADF:
                    case EINTR:
                    case EINVAL:
                    case EMFILE:
                    case ENOLCK:
                    case EOVERFLOW:
                    default:
                        fprintf(stderr, "select fcntl error (%d)\n", errno);
                        exit(1);
                }
            }

            if ( -1 == fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) )
            {
                switch ( errno )
                {
                    case EACCES:
                    case EAGAIN:
                    case EBADF:
                    case EINTR:
                    case EINVAL:
                    case EMFILE:
                    case ENOLCK:
                    case EOVERFLOW:
                    default:
                        fprintf(stderr, "select fcntl error (%d)\n", errno);
                        exit(1);
                }
            }

            // store the socket in connection_ctx

            struct connection_ctx *new_conn = (struct connection_ctx *) malloc(sizeof(struct connection_ctx));
            if ( NULL != new_conn )
            {
                new_conn->socket_fd = sockfd;
                new_conn->fp = fp;
                new_conn->next = NULL;

                if ( NULL != connection_tail )
                {
                    connection_tail->next = new_conn;
                }
                connection_tail = new_conn;

                if ( NULL == connection_head )
                    connection_head = new_conn;
            }

            ++conn_cnt;
        }
    }

    // epoll

    int epollfd = epoll_create1(0);
    if ( -1 == epollfd )
    {
        switch ( errno )
        {
            case EINVAL:
            case EMFILE:
            case ENFILE:
            case ENOMEM:
            default:
                fprintf(stderr, "epoll create1 error (%d)\n", errno);
                exit(1);
        }
    }

    // register sockets

    struct epoll_event ev;

    for ( struct connection_ctx *conn = connection_head; conn != NULL; conn = conn->next )
    {
        ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
        ev.data.ptr = conn;

        if ( -1 == epoll_ctl(epollfd, EPOLL_CTL_ADD, conn->socket_fd, &ev) )
        {
            switch ( errno )
            {
                case EBADF:
                case EEXIST:
                case EINVAL:
                case ENOENT:
                case ENOMEM:
                case ENOSPC:
                case EPERM:
                default:
                    fprintf(stderr, "epoll_ctl error (%d)\n", errno);
                    exit(1);
            }
        }
    }

    struct epoll_event events[MAX_EVENTS];

    while ( 0 < conn_cnt )
    {
        int nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if ( -1 == nfds )
        {
            switch ( errno )
            {
                case EINTR:
                    // A signal was caught
                    fprintf(stderr, "shutting down...\n");
                    clear_connection_ctx_list(connection_head);
                    exit(0);

                case EBADF:
                case EFAULT:
                case EINVAL:
                default:
                    fprintf(stderr, "epoll_wait error (%d)\n", errno);
                    exit(1);
            }
        }

        for ( int i = 0; i < nfds; i++ )
        {
            struct connection_ctx *conn = (struct connection_ctx *) events[i].data.ptr;

            // This is declared here to pass it from EPOLLIN to EPOLLOUT in this test implementation.
            // In most other cases, it would likely be placed inside EPOLLIN block.
            size_t total_bytes_in = 0;
            int acknowledged = 0;

            if ( events[i].events & EPOLLIN )
            {
                // socket has data to read

                char buffer[BUFLEN];
                ssize_t received;

                while ( 0 < ( received = recv(conn->socket_fd, buffer, sizeof(buffer), 0) ) )
                {
                    printf("sock:%d, %.*s", conn->socket_fd, (int) received, buffer);
                    fflush(stdout);

                    total_bytes_in += received;
                }

                switch ( received )
                {
                    case -1:
                        switch ( errno )
                        {
                            case EAGAIN:
                                // no data available right now, try again later...
                                break;

                            case ECONNRESET:
                                // connection reset by the peer
                                close_connection(epollfd, conn->socket_fd);
                                conn->socket_fd = 0;
                                conn_cnt--;
                                break;

                            case EBADF:
                            case ECONNREFUSED:
                            case EFAULT:
                            case EINTR:
                            case EINVAL:
                            case ENOMEM:
                            case ENOTCONN:
                            case ENOTSOCK:
                            default:
                                fprintf(stderr, "socket recv error (%d)\n", errno);
                                exit(1);
                        }
                        break;

                    case 0:
                        if ( 0 == total_bytes_in )
                        {
                            // The stream socket peer has performed an orderly shutdown.
                            // recv returning 0 is a socket-closed notification.

                            close_connection(epollfd, conn->socket_fd);
                            conn->socket_fd = 0;
                            conn_cnt--;
                        }
                }

                // we arrive at this point
                // if recv() returned -1 with errno == EAGAIN

                if ( 0 == strncmp(buffer, "Ack\n", 4) )
                {
                    acknowledged = 1;

                    // if this acknowledgement is after all data have been sent
                    if ( NULL == conn->fp )
                    {
                        close_connection(epollfd, conn->socket_fd);
                        conn->socket_fd = 0;
                        conn_cnt--;
                    }
                }
            }

            if ( events[i].events & EPOLLOUT )
            {
                if ( NULL != conn->fp && 0 != conn->socket_fd )
                {
                    size_t nbytes;
                    nbytes = fread(conn->buffer, sizeof(char), BUFLEN, conn->fp);
                    if ( 0 != nbytes )
                    {
                        int sent = send(conn->socket_fd, conn->buffer, nbytes, 0);
                        if ( -1 == sent )
                        {
                            switch ( errno )
                            {
                                case EACCES:
                                case EWOULDBLOCK:
                                case EBADF:
                                case ECONNRESET:
                                case EDESTADDRREQ:
                                case EFAULT:
                                case EINTR:
                                case EINVAL:
                                case EISCONN:
                                case EMSGSIZE:
                                case ENOBUFS:
                                case ENOMEM:
                                case ENOTCONN:
                                case ENOTSOCK:
                                case EOPNOTSUPP:
                                case EPIPE:
                                default:
                                    fprintf(stderr, "socket send error (%d)\n", errno);
                                    exit(1);
                            }
                        }

                        // reached to end-of-file
                        // beware: there is corner case that the buffer ends exactly at the end-of-file
                        // in that case, the end-of-file is not detected here, and will be taken care of
                        // in the next EPOLLOUT
                        if ( nbytes < BUFLEN )
                        {
                            fclose(conn->fp);
                            conn->fp = NULL;
                        }

                        fprintf(stderr, "sock:%d, fread:%lu, sent:%d\n", conn->socket_fd, nbytes, sent);
                    }
                    else
                    {
                        // already end-of-file
                        // we reach here in case the send buffer ends exactly at the end-of-file
                        // and the end-of-file was not detected in the previous EPOLLOUT

                        fclose(conn->fp);
                        conn->fp = NULL;

                        if ( 0 != acknowledged )
                        {
                            close_connection(epollfd, conn->socket_fd);
                            conn->socket_fd = 0;
                            conn_cnt--;
                        }
                    }
                }
            }

            if ( events[i].events & EPOLLERR )
            {
                // error condition
                fprintf(stderr, "EPOLLERR\n");
            }
        }
    }

    clear_connection_ctx_list(connection_head);
}
