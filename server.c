#include <errno.h>
#include <stdio.h>
#include <netinet/in.h> // struct sockaddr_in
#include <signal.h> // sigaction()
#include <stdlib.h> // exit()
#include <unistd.h> // read(), write(), close()

#define BUFLEN 512
#define PORT 8080

// The backlog argument defines the maximum length to which the
// queue of pending connections for sockfd may grow. If a
// connection request arrives when the queue is full, the client may
// receive an error with an indication of ECONNREFUSED or, if the
// underlying protocol supports retransmission, the request may be
// ignored so that a later reattempt at connection succeeds.
#define MAX_BACKLOG 3

void signal_handler(int signo)
{
    // # Signal      Default     Comment                              POSIX
    //   Name        Action
    //
    //  1 SIGHUP     Terminate   Hang up controlling terminal or      Yes
    //                           process
    //  2 SIGINT     Terminate   Interrupt from keyboard, Control-C   Yes
    //  3 SIGQUIT    Dump        Quit from keyboard, Control-\        Yes
    //  4 SIGILL     Dump        Illegal instruction                  Yes
    //  5 SIGTRAP    Dump        Breakpoint for debugging             No
    //  6 SIGABRT    Dump        Abnormal termination                 Yes
    //  6 SIGIOT     Dump        Equivalent to SIGABRT                No
    //  7 SIGBUS     Dump        Bus error                            No
    //  8 SIGFPE     Dump        Floating-point exception             Yes
    //  9 SIGKILL    Terminate   Forced-process termination           Yes
    // 10 SIGUSR1    Terminate   Available to processes               Yes
    // 11 SIGSEGV    Dump        Invalid memory reference             Yes
    // 12 SIGUSR2    Terminate   Available to processes               Yes
    // 13 SIGPIPE    Terminate   Write to pipe with no readers        Yes
    // 14 SIGALRM    Terminate   Real-timer clock                     Yes
    // 15 SIGTERM    Terminate   Process termination                  Yes
    // 16 SIGSTKFLT  Terminate   Coprocessor stack error              No
    // 17 SIGCHLD    Ignore      Child process stopped or terminated  Yes
    //                           or got a signal if traced
    // 18 SIGCONT    Continue    Resume execution, if stopped         Yes
    // 19 SIGSTOP    Stop        Stop process execution, Ctrl-Z       Yes
    // 20 SIGTSTP    Stop        Stop process issued from tty         Yes
    // 21 SIGTTIN    Stop        Background process requires input    Yes
    // 22 SIGTTOU    Stop        Background process requires output   Yes
    // 23 SIGURG     Ignore      Urgent condition on socket           No
    // 24 SIGXCPU    Dump        CPU time limit exceeded              No
    // 25 SIGXFSZ    Dump        File size limit exceeded             No
    // 26 SIGVTALRM  Terminate   Virtual timer clock                  No
    // 27 SIGPROF    Terminate   Profile timer clock                  No
    // 28 SIGWINCH   Ignore      Window resizing                      No
    // 29 SIGIO      Terminate   I/O now possible                     No
    // 29 SIGPOLL    Terminate   Equivalent to SIGIO                  No
    // 30 SIGPWR     Terminate   Power supply failure                 No
    // 31 SIGSYS     Dump        Bad system call                      No
    // 31 SIGUNUSED  Dump        Equivalent to SIGSYS                 No

    fprintf(stderr, "signal received: %d\n", signo);
}

int main()
{
    // custom SIG handler
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);

    int listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if ( -1 == listen_socket )
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

    int reuse = 1;
    if ( -1 == setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) )
    {
        switch ( errno )
        {
            case EBADF:
            case EDOM:
            case EINVAL:
            case EISCONN:
            case ENOPROTOOPT:
            case ENOTSOCK:
            case ENOMEM:
            case ENOBUFS:
            default:
                fprintf(stderr, "socket setsockopt error\n");
                exit(1);
        }
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

            case EADDRINUSE:
                // The given address is already in use.

                // (Internet domain sockets) The port number was specified as
                // zero in the socket address structure, but, upon attempting
                // to bind to an ephemeral port, it was determined that all
                // port numbers in the ephemeral port range are currently in
                // use.  See the discussion of
                // /proc/sys/net/ipv4/ip_local_port_range ip(7).

            case EBADF:
                // sockfd is not a valid file descriptor.

            case EINVAL:
                // The socket is already bound to an address.

                // addrlen is wrong, or addr is not a valid address for this
                // socket's domain.

            case ENOTSOCK:
                // The file descriptor sockfd does not refer to a socket.

            default:
                fprintf(stderr, "socket bind error\n");
                exit(1);
        }

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

            case EBADF:
                // The argument sockfd is not a valid file descriptor.

            case ENOTSOCK:
                // The file descriptor sockfd does not refer to a socket.

            case EOPNOTSUPP:
                // The socket is not of a type that supports the listen()
                // operation.

            default:
                fprintf(stderr, "socket listen error\n");
                exit(1);
        }
    }

    // if we want to prevent select from reacting to SIG,
    // we can use pselect() instead of select() with the following signal mask
    //
    // sigset_t pselect_set;
    // sigemptyset(&pselect_set);
    // sigaddset(&pselect_set, SIGINT);
    // sigaddset(&pselect_set, SIGQUIT);
    // sigaddset(&pselect_set, SIGTERM);

    fd_set current_sockets;
    fd_set ready_sockets;

    FD_ZERO(&current_sockets);
    FD_SET(listen_socket, &current_sockets);

    while ( 1 )
    {
        // select is destructive
        ready_sockets = current_sockets;

        // TODO
        // 1. write_sockets and exception_sockets as well as read_sockets
        // 2. set max sock instead of mindlessly FD_SETSIZE
        if ( -1 == select(FD_SETSIZE, &ready_sockets, NULL, NULL, NULL) )
        {
            switch ( errno )
            {
                case EINTR:
                    // A signal was caught
                    fprintf(stderr, "shutting down...\n");
                    close(listen_socket);
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

        // TODO don't iterate all FD_SETSIZE number of connections
        // keep the active connections elsewhere
        for ( int i = 0; i < FD_SETSIZE; i++ )
        {
            if ( FD_ISSET(i, &ready_sockets) )
            {
                if ( i == listen_socket )
                {
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

                            case EBADF:
                                // sockfd is not an open file descriptor.

                            case ECONNABORTED:
                                // A connection has been aborted.

                            case EFAULT:
                                // The addr argument is not in a writable part of the user
                                // address space.

                            case EINTR:
                                // The system call was interrupted by a signal that was
                                //caught before a valid connection arrived; see signal(7).

                            case EINVAL:
                                // Socket is not listening for connections, or addrlen is
                                // invalid (e.g., is negative).

                                // (accept4()) invalid value in flags.
                                // The per-process limit on the number of open file
                                // descriptors has been reached.

                            case ENFILE:
                                // The system-wide limit on the total number of open files
                                // has been reached.

                            case ENOBUFS:
                            case ENOMEM:
                                // Not enough free memory.  This often means that the memory
                                // allocation is limited by the socket buffer limits, not by
                                // the system memory.

                            case ENOTSOCK:
                                // The file descriptor sockfd does not refer to a socket.

                            case EOPNOTSUPP:
                                // The referenced socket is not of type SOCK_STREAM.

                            case EPERM:
                                // Firewall rules forbid connection.

                            case EPROTO:
                                // Protocol error.

                            default:
                                fprintf(stderr, "socket accept error\n");
                                exit(1);
                        }
                    }

                    FD_SET(client_socket, &current_sockets);
                }
                else
                {
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
                            fprintf(stderr, "socket recv error\n");
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
}
