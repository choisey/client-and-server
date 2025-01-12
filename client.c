/*
 * A TCP client that manages multiple connections to a server and handles
 * all read and write operations in a single thread by using socket select.
 */
#include <arpa/inet.h> // inet_addr()
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h> // exit()
#include <unistd.h> // read(), write(), close()

#define BUFLEN 64
#define PORT 8080
#define HOST "127.0.0.1"
#define MAX_CONN 20

struct connection_ctx
{
    int socket_fd;
    FILE* fp;
    char buffer[BUFLEN];
    struct connection_ctx *next;
};

void clear_connection_ctx_list(struct connection_ctx *head)
{
    while ( NULL != head )
    {
        struct connection_ctx *next = head->next;

        if ( 0 != head->socket_fd )
            close(head->socket_fd);

        if ( NULL != head->fp )
            fclose(head->fp);

        free(head);
        head = next;
    }
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
                switch ( errno )
                {

                    case EADDRNOTAVAIL:
                    case EAFNOSUPPORT:
                    case EALREADY:
                    case EBADF:
                    case ECONNREFUSED:
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
                    case EOPNOTSUPP :
                    default:
                        fprintf(stderr, "socket connect error\n");
                        exit(1);
                }
            }

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
                        fprintf(stderr, "select fcntl error\n");
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
                        fprintf(stderr, "select fcntl error\n");
                        exit(1);
                }
            }

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

    int max_socket_fd = 0;

    fd_set current_sockets;
    fd_set read_fds;
    fd_set write_fds;
    fd_set except_fds;

    FD_ZERO(&current_sockets);

    for ( struct connection_ctx *conn = connection_head; conn != NULL; conn = conn->next )
    {
        if ( max_socket_fd < conn->socket_fd ) max_socket_fd = conn->socket_fd;
        FD_SET(conn->socket_fd, &current_sockets);
    }

    while ( 0 < conn_cnt )
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
                    clear_connection_ctx_list(connection_head);
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
                    fprintf(stderr, "socket select error\n");
                    exit(1);
            }
        }

        for ( struct connection_ctx *conn = connection_head; conn != NULL; conn = conn->next )
        {
            if ( 0 != conn->socket_fd && FD_ISSET(conn->socket_fd, &write_fds) )
            {
                size_t bytes_read;
                bytes_read = fread(conn->buffer, sizeof(char), sizeof(conn->buffer), conn->fp);
                if ( 0 != bytes_read )
                {
                    // TODO
                    // if bytes_sent is smaller than the bytes_read, the rest should be sent in the next try.
                    int bytes_sent = send(conn->socket_fd, conn->buffer, bytes_read, 0);
                    if ( -1 == bytes_sent )
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
                                fprintf(stderr, "socket send error\n");
                                exit(1);
                        }
                    }
                }
                else
                {
                    FD_CLR(conn->socket_fd, &current_sockets);
                    close(conn->socket_fd);
                    fclose(conn->fp);

                    conn->socket_fd = 0;
                    conn->fp = 0;

                    conn_cnt--;
                }
            }
        }
    }

    clear_connection_ctx_list(connection_head);
}
