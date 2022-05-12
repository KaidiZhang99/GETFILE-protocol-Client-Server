//Socket Notes

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

int getaddrinfo(const char *node,     // e.g. "www.example.com" or IP --> The node parameter is the host name to connect to, or an IP address.
                const char *service,  // e.g. "http" or port number
                const struct addrinfo *hints,
                struct addrinfo **servinfo);


// example 1
int status;
struct addrinfo hints;
struct addrinfo *servinfo;  // will point to the results --> used to free this later --> memory leak

memset(&hints, 0, sizeof hints); // make sure the struct is empty
hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
hints.ai_flags = AI_PASSIVE;     // fill in my IP for me --> this tells getaddrinfo() to assign the address of my local host to the socket structures

if ((status = getaddrinfo(NULL, "3490", &hints, &servinfo)) != 0) {  // servinfo now points to a linked list of 1 or more struct addrinfos
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    exit(1);
}

// example 2:
// connecting to example.net server, port 3490, get ready to connect
status = getaddrinfo("www.example.net", "3490", &hints, &servinfo);

// socket
int socket(int domain, int type, int protocol);  // what kind of socket you want (IPv4 or IPv6, stream or datagram, and TCP or UDP).
// domain is PF_INET or PF_INET6, type is SOCK_STREAM or SOCK_DGRAM, and protocol can be set to 0 to choose the proper protocol for the given type. 
// Or you can call getprotobyname() to look up the protocol you want, “tcp” or “udp”.
// This PF_INET thing is a close relative of the AF_INET that you can use when initializing the sin_family field in your struct sockaddr_in.
// In fact, they’re so closely related that they actually have the same value, 
// and many programmers will call socket() and pass AF_INET as the first argument instead of PF_INET.
// use AF_INET in your struct sockaddr_in and PF_INET in your call to socket(). --> address family vs protocal family

// example 3:
sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol); 
// socket() simply returns to you a socket descriptor that you can use in later system calls, or -1 on error.

// bind():
// This is commonly done if you’re going to listen() for incoming connections on a specific port
int bind(int sockfd, struct sockaddr *my_addr, int addrlen);
// my_addr is a pointer to a struct sockaddr that contains information about your address, namely, port and IP address.
// addrlen is the length in bytes of that address.
// bind socket (sockfd) with this address/port (my_Addr)

// example 4:
int status;
struct addrinfo hints; // used to load current IP and port
struct addrinfo *servinfo;  // will point to the results --> used to free this later --> memory leak

memset(&hints, 0, sizeof hints); // make sure the struct is empty
hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
hints.ai_flags = AI_PASSIVE;     // fill in my IP for me --> this tells getaddrinfo() to assign the address of my local host to the socket structures

if ((status = getaddrinfo(NULL, "3490", &hints, &servinfo)) != 0) {  // servinfo now points to a linked list of 1 or more struct addrinfos
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    exit(1);
}

s = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol); 
// bind it to the port we passed in to getaddrinfo():
bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen);
// By using the AI_PASSIVE flag, I’m telling the program to bind to the IP of the host it’s running on
// bind() also returns -1 on error and sets errno to the error’s value. --> errno is predefined in one of the header files above
// All ports below 1024 are RESERVED (unless you’re the superuser)! You can have any port number above that, right up to 65535

// connect()
int connect(int sockfd, struct sockaddr *serv_addr, int addrlen); 
// sockfd is our friendly neighborhood socket file descriptor, as returned by the socket() call
// serv_addr is a struct sockaddr containing the destination port and IP address
// addrlen is the length in bytes of the server address structure

// example 5: where we make a socket connection to “www.example.com”, port 3490
struct addrinfo hints, *res;
int sockfd;

// first, load up address structs with getaddrinfo():
memset(&hints, 0, sizeof hints);
hints.ai_family = AF_UNSPEC;
hints.ai_socktype = SOCK_STREAM;

getaddrinfo("www.example.com", "3490", &hints, &res);

// make a socket:
sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

// connect!
connect(sockfd, res->ai_addr, res->ai_addrlen);
// Also, notice that we didn’t call bind(). Basically, we don’t care about our local port number; 
// we only care where we’re going (the remote port). The kernel will choose a local port for us, 
// and the site we connect to will automatically get this information from us. No worries.

// listen()
int listen(int sockfd, int backlog); 
// backlog is the number of connections allowed on the incoming queue
// as per usual, listen() returns -1 and sets errno on error.

// Well, as you can probably imagine, we need to call bind() before we call listen() 
// so that the server is running on a specific port. (You have to be able to tell your buddies which port to connect to!) 
// So if you’re going to be listening for incoming connections, the sequence of system calls you’ll make is:
getaddrinfo();
socket();
bind();
listen();
/* accept() goes here */ 

// accept()
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen); 
// sockfd is the listen()ing socket descriptor.
// addr will usually be a pointer to a local struct sockaddr_storage --> store info of their socket, 
// and with it you can determine which host is calling you from which port
// addrlen is a local integer variable that should be set to sizeof(struct sockaddr_storage)

// example 6:
#define MYPORT "3490"  // the port users will be connecting to
#define BACKLOG 10     // how many pending connections queue will hold

int main(void)
{
    struct sockaddr_storage their_addr;
    socklen_t addr_size;
    struct addrinfo hints, *res;
    int sockfd, new_fd;

    // !! don't forget your error checking for these calls !!

    // first, load up address structs with getaddrinfo():

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;  // use IPv4 or IPv6, whichever
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me

    getaddrinfo(NULL, MYPORT, &hints, &res);

    // make a socket, bind it, and listen on it:

    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    bind(sockfd, res->ai_addr, res->ai_addrlen);
    listen(sockfd, BACKLOG);

    // now accept an incoming connection:

    addr_size = sizeof their_addr;
    new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);

    // ready to communicate on socket descriptor new_fd!


// send() && recv()
int send(int sockfd, const void *msg, int len, int flags); 
// sockfd is the socket descriptor you want to send data to --> whether it’s the one returned by socket() or the one you got with accept()
// msg is a pointer to the data you want to send
//  len is the length of that data in bytes
// Just set flags to 0

// example 7:
char *msg = "Beej was here!";
int len, bytes_sent;
.
.
.
len = strlen(msg);
bytes_sent = send(sockfd, msg, len, 0);
// send() returns the number of bytes actually sent out

// recv()
int recv(int sockfd, void *buf, int len, int flags);
// sockfd is the socket descriptor to read from
// buf is the buffer to read the information into
// len is the maximum length of the buffer
// flags set to 0
// recv() returns the number of bytes actually read into the buffer, or -1 on error (with errno set, accordingly).
// recv() can return 0. This can mean only one thing: the remote side has closed the connection on you

// close()
close(sockfd); 
// This will prevent any more reads and writes to the socket.



// ... do everything until you don't need servinfo anymore ....

freeaddrinfo(servinfo); // free the linked-list
// freeaddrinfo(res)


