#include <errno.h>
#include <stdio.h>
#include <netinet/in.h> // struct sockaddr_in
#include <stdlib.h>
#include <unistd.h> // read(), write(), close()

#define BUFLEN 80
#define PORT 8080

// The backlog argument defines the maximum length to which the
// queue of pending connections for sockfd may grow. If a
// connection request arrives when the queue is full, the client may
// receive an error with an indication of ECONNREFUSED or, if the
// underlying protocol supports retransmission, the request may be
// ignored so that a later reattempt at connection succeeds.
#define MAX_BACKLOG 3

int main()
{
    int listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if ( -1 == listen_socket )
    {
        switch (errno )
        {
            case EACCES:
                // Permission to create a socket of the specified type and/or
                // protocol is denied.
                break;

            case EAFNOSUPPORT:
                // The implementation does not support the specified address
                // family.
                break;

            case EINVAL:
                // Unknown protocol, or protocol family not available.

                // Invalid flags in type.

                // The per-process limit on the number of open file
                // descriptors has been reached.
                break;

            case ENFILE:
                // The system-wide limit on the total number of open files
                // has been reached.
                break;

            case ENOBUFS:
            case ENOMEM:
                // Insufficient memory is available.  The socket cannot be
                // created until sufficient resources are freed.
                break;

            case EPROTONOSUPPORT:
                // The protocol type or the specified protocol is not
                // supported within this domain.
                break;
        }

        fprintf(stderr, "socket creation failed\n");
        exit(1);
    }

    int reuse = 1;
    if ( -1 == setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) )
    {
        fprintf(stderr, "socket setsockopt failed\n");
        exit(1);
    }

    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if ( -1 == bind(listen_socket, (struct sockaddr*) &servaddr, sizeof(servaddr)) )
    {
        switch (errno )
        {
            case EACCES:
                // The address is protected, and the user is not the
                // superuser.
                break;

            case EADDRINUSE:
                // The given address is already in use.

                // (Internet domain sockets) The port number was specified as
                // zero in the socket address structure, but, upon attempting
                // to bind to an ephemeral port, it was determined that all
                // port numbers in the ephemeral port range are currently in
                // use.  See the discussion of
                // /proc/sys/net/ipv4/ip_local_port_range ip(7).
                break;

            case EBADF:
                // sockfd is not a valid file descriptor.
                break;

            case EINVAL:
                // The socket is already bound to an address.

                // addrlen is wrong, or addr is not a valid address for this
                // socket's domain.
                break;

            case ENOTSOCK:
                // The file descriptor sockfd does not refer to a socket.
                break;
        }

        fprintf(stderr, "socket bind failed\n");
        exit(1);
    }

    if ( -1 == listen(listen_socket, MAX_BACKLOG) )
    {
        switch ( errno )
        {
            case EADDRINUSE:
                // Another socket is already listening on the same port.
                // (Internet domain sockets) The socket referred to by sockfd
                // had not previously been bound to an address and, upon
                // attempting to bind it to an ephemeral port, it was
                // determined that all port numbers in the ephemeral port
                // range are currently in use.  See the discussion of
                // /proc/sys/net/ipv4/ip_local_port_range in ip(7).
                break;

            case EBADF:
                // The argument sockfd is not a valid file descriptor.
                break;

            case ENOTSOCK:
                // The file descriptor sockfd does not refer to a socket.
                break;

            case EOPNOTSUPP:
                // The socket is not of a type that supports the listen()
                // operation.
                break;
        }

        fprintf(stderr, "socket listen failed\n");
        exit(1);
    }

    fd_set current_sockets;
    fd_set ready_sockets;

    FD_ZERO(&current_sockets);
    FD_SET(listen_socket, &current_sockets);

    while ( 1 )
    {
        // select is destructive
        ready_sockets = current_sockets;

        // TODO
        // 1. use pselect instead of select
        // 2. write_sockets and exception_sockets as well as read_sockets
        // 3. set max sock instead of mindlessly FD_SETSIZE
        if ( -1 == select(FD_SETSIZE, &ready_sockets, NULL, NULL, NULL) )
        {
            switch ( errno )
            {
                case EBADF:
                    // An invalid file descriptor was given in one of the sets.
                    // (Perhaps a file descriptor that was already closed, or one
                    // on which an error has occurred.)  However, see BUGS.
                    break;

                case EINTR:
                    // A signal was caught; see signal(7).
                    break;

                case EINVAL:
                    // nfds is negative or exceeds the RLIMIT_NOFILE resource
                    // limit (see getrlimit(2)).
                    // The value contained within timeout is invalid.
                    break;

                case ENOMEM:
                    // Unable to allocate memory for internal tables.
                    break;
            }

            fprintf(stderr, "select failed\n");
            exit(1);
        }

        for ( int i = 0; i < FD_SETSIZE; i++ )
        {
            if ( FD_ISSET(i, &ready_sockets) )
            {
                if ( i == listen_socket )
                {
                    // accept connection
                    socklen_t addr_size = sizeof(struct sockaddr_in);
                    struct sockaddr_in client_addr;
                    int client_socket = accept(listen_socket, (struct sockaddr*) &client_addr, &addr_size);
                    if ( -1 == client_socket )
                    {
                        switch ( errno )
                        {
                            case EWOULDBLOCK:
                                // The socket is marked nonblocking and no connections are
                                // present to be accepted.  POSIX.1-2001 and POSIX.1-2008
                                // allow either error to be returned for this case, and do
                                // not require these constants to have the same value, so a
                                // portable application should check for both possibilities.
                                break;

                            case EBADF:
                                // sockfd is not an open file descriptor.
                                break;

                            case ECONNABORTED:
                                // A connection has been aborted.
                                break;

                            case EFAULT:
                                // The addr argument is not in a writable part of the user
                                // address space.
                                break;

                            case EINTR:
                                // The system call was interrupted by a signal that was
                                //caught before a valid connection arrived; see signal(7).
                                break;

                            case EINVAL:
                                // Socket is not listening for connections, or addrlen is
                                // invalid (e.g., is negative).

                                // (accept4()) invalid value in flags.
                                // The per-process limit on the number of open file
                                // descriptors has been reached.
                                break;

                            case ENFILE:
                                // The system-wide limit on the total number of open files
                                // has been reached.
                                break;

                            case ENOBUFS:
                            case ENOMEM:
                                // Not enough free memory.  This often means that the memory
                                // allocation is limited by the socket buffer limits, not by
                                // the system memory.
                                break;

                            case ENOTSOCK:
                                // The file descriptor sockfd does not refer to a socket.
                                break;

                            case EOPNOTSUPP:
                                // The referenced socket is not of type SOCK_STREAM.
                                break;

                            case EPERM:
                                // Firewall rules forbid connection.
                                break;

                            case EPROTO:
                                // Protocol error.
                                break;
                        }

                        fprintf(stderr, "socket accept failed\n");
                        exit(1);
                    }

                    FD_SET(client_socket, &current_sockets);
                }
                else
                {
                    // handle connection
                    char buffer[BUFLEN];
                    size_t msgsize = 0;
                    ssize_t bytes_read;

                    while ( 0 < ( bytes_read = recv(i, buffer, sizeof(buffer) - 1, MSG_DONTWAIT) ) )
                    {
                        *(buffer + bytes_read) = '\0';
                        msgsize += bytes_read;
                        printf("%s", buffer);
                    }

                    if ( -1 == bytes_read )
                    {
                        if ( EAGAIN != errno && EWOULDBLOCK != errno )
                        {
                            fprintf(stderr, "socket recv failed\n");
                            FD_CLR(i, &current_sockets);
                        }
                    }

                    if ( 0 == msgsize )
                    {
                        // When a stream socket peer has performed an orderly shutdown,
                        // the return value will be 0 (the traditional "end-of-file" return).
                        FD_CLR(i, &current_sockets);
                    }
                }
            }
        }
    }

    close(listen_socket);
}
