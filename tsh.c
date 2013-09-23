/*
 * Tiny SHell version 0.6 - client side,
 * by Christophe Devine <devine@cr0.net>;
 * this program is licensed under the GPL.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <netdb.h>

#include "tsh.h"
#include "pel.h"

unsigned char message[BUFSIZE + 1];

/* function declaration */

int tsh_get_file( int server, char *argv3, char *argv4 );
int tsh_put_file( int server, char *argv3, char *argv4 );
int tsh_runshell( int server, char *argv2 );

void pel_error( char *s );

/* program entry point */

int main( int argc, char *argv[] )
{
    int ret, client, server, n;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    struct hostent *server_host;
    char action, *password;

    action = 0;

    password = NULL;

    /* check the arguments */

    if( argc == 5 && ! strcmp( argv[2], "get" ) )
    {
        action = GET_FILE;
    }

    if( argc == 5 && ! strcmp( argv[2], "put" ) )
    {
        action = PUT_FILE;
    }

    if( argc == 2 || argc == 3 )
    {
        action = RUNSHELL;
    }

    if( action == 0 ) return( 1 );

connect:

    if( strcmp( argv[1], "cb" ) != 0 )
    {
        /* create a socket */

        server = socket( AF_INET, SOCK_STREAM, 0 );

        if( server < 0 )
        {
            perror( "socket" );
            return( 2 );
        }

        /* resolve the server hostname */

        server_host = gethostbyname( argv[1] );

        if( server_host == NULL )
        {
            fprintf( stderr, "gethostbyname failed.\n" );
            return( 3 );
        }

        memcpy( (void *) &server_addr.sin_addr,
                (void *) server_host->h_addr,
                server_host->h_length );

        server_addr.sin_family = AF_INET;
        server_addr.sin_port   = htons( SERVER_PORT );

        /* connect to the remote host */

        ret = connect( server, (struct sockaddr *) &server_addr,
                       sizeof( server_addr ) );

        if( ret < 0 )
        {
            perror( "connect" );
            return( 4 );
        }
    }
    else
    {
        /* create a socket */

        client = socket( AF_INET, SOCK_STREAM, 0 );

        if( client < 0 )
        {
            perror( "socket" );
            return( 5 );
        }

        /* bind the client on the port the server will connect to */

        n = 1;

        ret = setsockopt( client, SOL_SOCKET, SO_REUSEADDR,
                          (void *) &n, sizeof( n ) );

        if( ret < 0 )
        {
            perror( "setsockopt" );
            return( 6 );
        }

        client_addr.sin_family      = AF_INET;
        client_addr.sin_port        = htons( SERVER_PORT );
        client_addr.sin_addr.s_addr = INADDR_ANY;

        ret = bind( client, (struct sockaddr *) &client_addr,
                    sizeof( client_addr ) );

        if( ret < 0 )
        {
            perror( "bind" );
            return( 7 );
        }

        if( listen( client, 5 ) < 0 )
        {
            perror( "listen" );
            return( 8 );
        }

        fprintf( stderr, "Waiting for the server to connect..." );
        fflush( stderr );

        n = sizeof( server_addr );

        server = accept( client, (struct sockaddr *)
                         &server_addr, &n );

        if( server < 0 )
        {
            perror( "accept" );
            return( 9 );
        }

        fprintf( stderr, "connected.\n" );

        close( client );
    }

    /* setup the packet encryption layer */

    if( password == NULL )
    {
        /* 1st try, using the built-in secret key */

        ret = pel_client_init( server, secret );

        if( ret != PEL_SUCCESS )
        {
            close( server );

            /* secret key invalid, so ask for a password */

            password = getpass( "Password: " );
            goto connect;
        }
    }
    else
    {
        /* 2nd try, with the user's password */

        ret = pel_client_init( server, password );

        memset( password, 0, strlen( password ) );

        if( ret != PEL_SUCCESS )
        {
            /* password invalid, exit */

            fprintf( stderr, "Authentication failed.\n" );
            shutdown( server, 2 );
            return( 10 );
        }

    }

    /* send the action requested by the user */

    ret = pel_send_msg( server, (unsigned char *) &action, 1 );

    if( ret != PEL_SUCCESS )
    {
        pel_error( "pel_send_msg" );
        shutdown( server, 2 );
        return( 11 );
    }

    /* howdy */

    switch( action )
    {
        case GET_FILE:

            ret = tsh_get_file( server, argv[3], argv[4] );
            break;

        case PUT_FILE:

            ret = tsh_put_file( server, argv[3], argv[4] );
            break;

        case RUNSHELL:

            ret = ( ( argc == 3 )
                ? tsh_runshell( server, argv[2] )
                : tsh_runshell( server, "exec bash --login" ) );
            break;

        default:

            ret = -1;
            break;
    }

    shutdown( server, 2 );

    return( ret );
}

int tsh_get_file( int server, char *argv3, char *argv4 )
{
    char *temp, *pathname;
    int ret, len, fd, total;

    /* send remote filename */

    len = strlen( argv3 );

    ret = pel_send_msg( server, (unsigned char *) argv3, len );

    if( ret != PEL_SUCCESS )
    {
        pel_error( "pel_send_msg" );
        return( 12 );
    }

    /* create local file */

    temp = strrchr( argv3, '/' );

    if( temp != NULL ) temp++;
    if( temp == NULL ) temp = argv3;

    len = strlen( argv4 );

    pathname = (char *) malloc( len + strlen( temp ) + 2 );

    if( pathname == NULL )
    {
        perror( "malloc" );
        return( 13 );
    }

    strcpy( pathname, argv4 );
    strcpy( pathname + len, "/" );
    strcpy( pathname + len + 1, temp );

    fd = creat( pathname, 0644 );

    if( fd < 0 )
    {
        perror( "creat" );
        return( 14 );
    }

    free( pathname );

    /* transfer from server */

    total = 0;

    while( 1 )
    {
        ret = pel_recv_msg( server, message, &len );

        if( ret != PEL_SUCCESS )
        {
            if( pel_errno == PEL_CONN_CLOSED && total > 0 )
            {
                break;
            }

            pel_error( "pel_recv_msg" );
            fprintf( stderr, "Transfer failed.\n" );
            return( 15 );
        }

        if( write( fd, message, len ) != len )
        {
            perror( "write" );
            return( 16 );
        }

        total += len;

        printf( "%d\r", total );
        fflush( stdout );
    }

    printf( "%d done.\n", total );

    return( 0 );
}

int tsh_put_file( int server, char *argv3, char *argv4 )
{
    char *temp, *pathname;
    int ret, len, fd, total;

    /* send remote filename */

    temp = strrchr( argv3, '/' );

    if( temp != NULL ) temp++;
    if( temp == NULL ) temp = argv3;

    len = strlen( argv4 );

    pathname = (char *) malloc( len + strlen( temp ) + 2 );

    if( pathname == NULL )
    {
        perror( "malloc" );
        return( 17 );
    }

    strcpy( pathname, argv4 );
    strcpy( pathname + len, "/" );
    strcpy( pathname + len + 1, temp );

    len = strlen( pathname );

    ret = pel_send_msg( server, (unsigned char *) pathname, len );

    if( ret != PEL_SUCCESS )
    {
        pel_error( "pel_send_msg" );
        return( 18 );
    }

    free( pathname );

    /* open local file */

    fd = open( argv3, O_RDONLY );

    if( fd < 0 )
    {
        perror( "open" );
        return( 19 );
    }

    /* transfer to server */

    total = 0;

    while( 1 )
    {
        len = read( fd, message, BUFSIZE );

        if( len < 0 )
        {
            perror( "read" );
            return( 20 );
        }

        if( len == 0 )
        {
            break;
        }

        ret = pel_send_msg( server, message, len );

        if( ret != PEL_SUCCESS )
        {
            pel_error( "pel_send_msg" );
            fprintf( stderr, "Transfer failed.\n" );
            return( 21 );
        }

        total += len;

        printf( "%d\r", total );
        fflush( stdout );
    }

    printf( "%d done.\n", total );

    return( 0 );
}

int tsh_runshell( int server, char *argv2 )
{
    fd_set rd;
    char *term;
    int ret, len, imf;
    struct winsize ws;
    struct termios tp, tr;

    /* send the TERM environment variable */

    term = getenv( "TERM" );

    if( term == NULL )
    {
        term = "vt100";
    }

    len = strlen( term );

    ret = pel_send_msg( server, (unsigned char *) term, len );

    if( ret != PEL_SUCCESS )
    {
        pel_error( "pel_send_msg" );
        return( 22 );
    }

    /* send the window size */

    imf = 0;

    if( isatty( 0 ) )
    {
        /* set the interactive mode flag */

        imf = 1;

        if( ioctl( 0, TIOCGWINSZ, &ws ) < 0 )
        {
            perror( "ioctl(TIOCGWINSZ)" );
            return( 23 );
        }
    }
    else
    {
        /* fallback on standard settings */

        ws.ws_row = 25;
        ws.ws_col = 80;
    }

    message[0] = ( ws.ws_row >> 8 ) & 0xFF;
    message[1] = ( ws.ws_row      ) & 0xFF;

    message[2] = ( ws.ws_col >> 8 ) & 0xFF;
    message[3] = ( ws.ws_col      ) & 0xFF;

    ret = pel_send_msg( server, message, 4 );

    if( ret != PEL_SUCCESS )
    {
        pel_error( "pel_send_msg" );
        return( 24 );
    }

    /* send the system command */

    len = strlen( argv2 );

    ret = pel_send_msg( server, (unsigned char *) argv2, len );

    if( ret != PEL_SUCCESS )
    {
        pel_error( "pel_send_msg" );
        return( 25 );
    }

    /* set the tty to RAW */

    if( isatty( 1 ) )
    {
        if( tcgetattr( 1, &tp ) < 0 )
        {
            perror( "tcgetattr" );
            return( 26 );
        }

        memcpy( (void *) &tr, (void *) &tp, sizeof( tr ) );

        tr.c_iflag |= IGNPAR;
        tr.c_iflag &= ~(ISTRIP|INLCR|IGNCR|ICRNL|IXON|IXANY|IXOFF);
        tr.c_lflag &= ~(ISIG|ICANON|ECHO|ECHOE|ECHOK|ECHONL|IEXTEN);
        tr.c_oflag &= ~OPOST;

        tr.c_cc[VMIN]  = 1;
        tr.c_cc[VTIME] = 0;

        if( tcsetattr( 1, TCSADRAIN, &tr ) < 0 )
        {
            perror( "tcsetattr" );
            return( 27 );
        }
    }

    /* let's forward the data back and forth */

    while( 1 )
    {
        FD_ZERO( &rd );

        if( imf != 0 )
        {
            FD_SET( 0, &rd );
        }

        FD_SET( server, &rd );

        if( select( server + 1, &rd, NULL, NULL, NULL ) < 0 )
        {
            perror( "select" );
            ret = 28;
            break;
        }

        if( FD_ISSET( server, &rd ) )
        {
            ret = pel_recv_msg( server, message, &len );

            if( ret != PEL_SUCCESS )
            {
                if( pel_errno == PEL_CONN_CLOSED )
                {
                    ret = 0;
                }
                else
                {
                    pel_error( "pel_recv_msg" );
                    ret = 29;
                }
                break;
            }

            if( write( 1, message, len ) != len )
            {
                perror( "write" );
                ret = 30;
                break;
            }
        }

        if( imf != 0 && FD_ISSET( 0, &rd ) )
        {
            len = read( 0, message, BUFSIZE );

            if( len == 0 )
            {
                fprintf( stderr, "stdin: end-of-file\n" );
                ret = 31;
                break;
            }

            if( len < 0 )
            {
                perror( "read" );
                ret = 32;
                break;
            }

            ret = pel_send_msg( server, message, len );

            if( ret != PEL_SUCCESS )
            {
                pel_error( "pel_send_msg" );
                ret = 33;
                break;
            }
        }
    }

    /* restore the terminal attributes */

    if( isatty( 1 ) )
    {
        tcsetattr( 1, TCSADRAIN, &tp );
    }

    return( ret );
}

void pel_error( char *s )
{
    switch( pel_errno )
    {
        case PEL_CONN_CLOSED:

            fprintf( stderr, "%s: Connection closed.\n", s );
            break;

        case PEL_SYSTEM_ERROR:

            perror( s );
            break;

        case PEL_WRONG_CHALLENGE:

            fprintf( stderr, "%s: Wrong challenge.\n", s );
            break;

        case PEL_BAD_MSG_LENGTH:

            fprintf( stderr, "%s: Bad message length.\n", s );
            break;

        case PEL_CORRUPTED_DATA:

            fprintf( stderr, "%s: Corrupted data.\n", s );
            break;

        case PEL_UNDEFINED_ERROR:

            fprintf( stderr, "%s: No error.\n", s );
            break;

        default:

            fprintf( stderr, "%s: Unknown error code.\n", s );
            break;
    }
}
