/************************************************

  socket.c -

  created at: Thu Mar 31 12:21:29 JST 1994

  Copyright (C) 1993-2007 Yukihiro Matsumoto

************************************************/

#include "rubysocket.h"

static void
setup_domain_and_type(VALUE domain, int *dv, VALUE type, int *tv)
{
    *dv = family_arg(domain);
    *tv = socktype_arg(type);
}

/*
 * call-seq:
 *   Socket.new(domain, socktype, protocol) => socket
 *
 * Creates a new socket object.
 *
 * _domain_ should be a communications domain such as: :INET, :INET6, :UNIX, etc.
 *
 * _socktype_ should be a socket type such as: :STREAM, :DGRAM, :RAW, etc.
 *
 * _protocol_ should be a protocol defined in the domain.
 * 0 is default protocol for the domain.
 *
 *   Socket.new(:INET, :STREAM, 0) # TCP socket
 *   Socket.new(:INET, :DGRAM, 0)  # UDP socket
 *   Socket.new(:UNIX, :STREAM, 0) # UNIX stream socket
 *   Socket.new(:UNIX, :DGRAM, 0)  # UNIX datagram socket
 */
static VALUE
sock_initialize(VALUE sock, VALUE domain, VALUE type, VALUE protocol)
{
    int fd;
    int d, t;

    rb_secure(3);
    setup_domain_and_type(domain, &d, type, &t);
    fd = ruby_socket(d, t, NUM2INT(protocol));
    if (fd < 0) rb_sys_fail("socket(2)");

    return init_sock(sock, fd);
}

#if defined HAVE_SOCKETPAIR
static VALUE
io_call_close(VALUE io)
{
    return rb_funcall(io, rb_intern("close"), 0, 0);
}

static VALUE
io_close(VALUE io)
{
    return rb_rescue(io_call_close, io, 0, 0);
}

static VALUE
pair_yield(VALUE pair)
{
    return rb_ensure(rb_yield, pair, io_close, rb_ary_entry(pair, 1));
}
#endif

/*
 * call-seq:
 *   Socket.pair(domain, type, protocol)       => [socket1, socket2]
 *   Socket.socketpair(domain, type, protocol) => [socket1, socket2]
 *
 * Creates a pair of sockets connected each other.
 *
 * _domain_ should be a communications domain such as: :INET, :INET6, :UNIX, etc.
 *
 * _socktype_ should be a socket type such as: :STREAM, :DGRAM, :RAW, etc.
 *
 * _protocol_ should be a protocol defined in the domain.
 * 0 is default protocol for the domain.
 *
 *   s1, s2 = Socket.pair(:UNIX, :DGRAM, 0)
 *   s1.send "a", 0
 *   s1.send "b", 0
 *   p s2.recv(10) #=> "a"
 *   p s2.recv(10) #=> "b"
 *
 */
VALUE
sock_s_socketpair(VALUE klass, VALUE domain, VALUE type, VALUE protocol)
{
#if defined HAVE_SOCKETPAIR
    int d, t, p, sp[2];
    int ret;
    VALUE s1, s2, r;

    setup_domain_and_type(domain, &d, type, &t);
    p = NUM2INT(protocol);
    ret = socketpair(d, t, p, sp);
    if (ret < 0 && (errno == EMFILE || errno == ENFILE)) {
        rb_gc();
        ret = socketpair(d, t, p, sp);
    }
    if (ret < 0) {
	rb_sys_fail("socketpair(2)");
    }

    s1 = init_sock(rb_obj_alloc(klass), sp[0]);
    s2 = init_sock(rb_obj_alloc(klass), sp[1]);
    r = rb_assoc_new(s1, s2);
    if (rb_block_given_p()) {
        return rb_ensure(pair_yield, r, io_close, s1);
    }
    return r;
#else
    rb_notimplement();
#endif
}

/*
 * call-seq:
 * 	socket.connect(server_sockaddr) => 0
 * 
 * Requests a connection to be made on the given +server_sockaddr+. Returns 0 if
 * successful, otherwise an exception is raised.
 *  
 * === Parameter
 * * +server_sockaddr+ - the +struct+ sockaddr contained in a string
 * 
 * === Example:
 * 	# Pull down Google's web page
 * 	require 'socket'
 * 	include Socket::Constants
 * 	socket = Socket.new( AF_INET, SOCK_STREAM, 0 )
 * 	sockaddr = Socket.pack_sockaddr_in( 80, 'www.google.com' )
 * 	socket.connect( sockaddr )
 * 	socket.write( "GET / HTTP/1.0\r\n\r\n" )
 * 	results = socket.read 
 * 
 * === Unix-based Exceptions
 * On unix-based systems the following system exceptions may be raised if 
 * the call to _connect_ fails:
 * * Errno::EACCES - search permission is denied for a component of the prefix
 *   path or write access to the +socket+ is denided
 * * Errno::EADDRINUSE - the _sockaddr_ is already in use
 * * Errno::EADDRNOTAVAIL - the specified _sockaddr_ is not available from the
 *   local machine
 * * Errno::EAFNOSUPPORT - the specified _sockaddr_ is not a valid address for 
 *   the address family of the specified +socket+
 * * Errno::EALREADY - a connection is already in progress for the specified
 *   socket
 * * Errno::EBADF - the +socket+ is not a valid file descriptor
 * * Errno::ECONNREFUSED - the target _sockaddr_ was not listening for connections
 *   refused the connection request
 * * Errno::ECONNRESET - the remote host reset the connection request
 * * Errno::EFAULT - the _sockaddr_ cannot be accessed
 * * Errno::EHOSTUNREACH - the destination host cannot be reached (probably 
 *   because the host is down or a remote router cannot reach it)
 * * Errno::EINPROGRESS - the O_NONBLOCK is set for the +socket+ and the
 *   connection cnanot be immediately established; the connection will be
 *   established asynchronously
 * * Errno::EINTR - the attempt to establish the connection was interrupted by
 *   delivery of a signal that was caught; the connection will be established
 *   asynchronously
 * * Errno::EISCONN - the specified +socket+ is already connected
 * * Errno::EINVAL - the address length used for the _sockaddr_ is not a valid
 *   length for the address family or there is an invalid family in _sockaddr_ 
 * * Errno::ENAMETOOLONG - the pathname resolved had a length which exceeded
 *   PATH_MAX
 * * Errno::ENETDOWN - the local interface used to reach the destination is down
 * * Errno::ENETUNREACH - no route to the network is present
 * * Errno::ENOBUFS - no buffer space is available
 * * Errno::ENOSR - there were insufficient STREAMS resources available to 
 *   complete the operation
 * * Errno::ENOTSOCK - the +socket+ argument does not refer to a socket
 * * Errno::EOPNOTSUPP - the calling +socket+ is listening and cannot be connected
 * * Errno::EPROTOTYPE - the _sockaddr_ has a different type than the socket 
 *   bound to the specified peer address
 * * Errno::ETIMEDOUT - the attempt to connect time out before a connection
 *   was made.
 * 
 * On unix-based systems if the address family of the calling +socket+ is
 * AF_UNIX the follow exceptions may be raised if the call to _connect_
 * fails:
 * * Errno::EIO - an i/o error occured while reading from or writing to the 
 *   file system
 * * Errno::ELOOP - too many symbolic links were encountered in translating
 *   the pathname in _sockaddr_
 * * Errno::ENAMETOOLLONG - a component of a pathname exceeded NAME_MAX 
 *   characters, or an entired pathname exceeded PATH_MAX characters
 * * Errno::ENOENT - a component of the pathname does not name an existing file
 *   or the pathname is an empty string
 * * Errno::ENOTDIR - a component of the path prefix of the pathname in _sockaddr_
 *   is not a directory 
 * 
 * === Windows Exceptions
 * On Windows systems the following system exceptions may be raised if 
 * the call to _connect_ fails:
 * * Errno::ENETDOWN - the network is down
 * * Errno::EADDRINUSE - the socket's local address is already in use
 * * Errno::EINTR - the socket was cancelled
 * * Errno::EINPROGRESS - a blocking socket is in progress or the service provider
 *   is still processing a callback function. Or a nonblocking connect call is 
 *   in progress on the +socket+.
 * * Errno::EALREADY - see Errno::EINVAL
 * * Errno::EADDRNOTAVAIL - the remote address is not a valid address, such as 
 *   ADDR_ANY TODO check ADDRANY TO INADDR_ANY
 * * Errno::EAFNOSUPPORT - addresses in the specified family cannot be used with
 *   with this +socket+
 * * Errno::ECONNREFUSED - the target _sockaddr_ was not listening for connections
 *   refused the connection request
 * * Errno::EFAULT - the socket's internal address or address length parameter
 *   is too small or is not a valid part of the user space address
 * * Errno::EINVAL - the +socket+ is a listening socket
 * * Errno::EISCONN - the +socket+ is already connected
 * * Errno::ENETUNREACH - the network cannot be reached from this host at this time
 * * Errno::EHOSTUNREACH - no route to the network is present
 * * Errno::ENOBUFS - no buffer space is available
 * * Errno::ENOTSOCK - the +socket+ argument does not refer to a socket
 * * Errno::ETIMEDOUT - the attempt to connect time out before a connection
 *   was made.
 * * Errno::EWOULDBLOCK - the socket is marked as nonblocking and the 
 *   connection cannot be completed immediately
 * * Errno::EACCES - the attempt to connect the datagram socket to the 
 *   broadcast address failed
 * 
 * === See
 * * connect manual pages on unix-based systems
 * * connect function in Microsoft's Winsock functions reference
 */
static VALUE
sock_connect(VALUE sock, VALUE addr)
{
    rb_io_t *fptr;
    int fd, n;

    SockAddrStringValue(addr);
    addr = rb_str_new4(addr);
    GetOpenFile(sock, fptr);
    fd = fptr->fd;
    n = ruby_connect(fd, (struct sockaddr*)RSTRING_PTR(addr), RSTRING_LEN(addr), 0);
    if (n < 0) {
	rb_sys_fail("connect(2)");
    }

    return INT2FIX(n);
}

/*
 * call-seq:
 * 	socket.connect_nonblock(server_sockaddr) => 0
 * 
 * Requests a connection to be made on the given +server_sockaddr+ after
 * O_NONBLOCK is set for the underlying file descriptor.
 * Returns 0 if successful, otherwise an exception is raised.
 *  
 * === Parameter
 * * +server_sockaddr+ - the +struct+ sockaddr contained in a string
 * 
 * === Example:
 * 	# Pull down Google's web page
 * 	require 'socket'
 * 	include Socket::Constants
 * 	socket = Socket.new(AF_INET, SOCK_STREAM, 0)
 * 	sockaddr = Socket.sockaddr_in(80, 'www.google.com')
 * 	begin # emulate blocking connect
 * 	  socket.connect_nonblock(sockaddr)
 * 	rescue Errno::EINPROGRESS
 * 	  IO.select(nil, [socket])
 * 	  begin
 * 	    socket.connect_nonblock(sockaddr)
 * 	  rescue Errno::EISCONN
 * 	  end
 * 	end
 * 	socket.write("GET / HTTP/1.0\r\n\r\n")
 * 	results = socket.read 
 * 
 * Refer to Socket#connect for the exceptions that may be thrown if the call
 * to _connect_nonblock_ fails. 
 *
 * Socket#connect_nonblock may raise any error corresponding to connect(2) failure,
 * including Errno::EINPROGRESS.
 *
 * === See
 * * Socket#connect
 */
static VALUE
sock_connect_nonblock(VALUE sock, VALUE addr)
{
    rb_io_t *fptr;
    int n;

    SockAddrStringValue(addr);
    addr = rb_str_new4(addr);
    GetOpenFile(sock, fptr);
    rb_io_set_nonblock(fptr);
    n = connect(fptr->fd, (struct sockaddr*)RSTRING_PTR(addr), RSTRING_LEN(addr));
    if (n < 0) {
	rb_sys_fail("connect(2)");
    }

    return INT2FIX(n);
}

/*
 * call-seq:
 * 	socket.bind(server_sockaddr) => 0
 * 
 * Binds to the given +struct+ sockaddr.
 * 
 * === Parameter
 * * +server_sockaddr+ - the +struct+ sockaddr contained in a string
 *
 * === Example
 * 	require 'socket'
 * 	include Socket::Constants
 * 	socket = Socket.new( AF_INET, SOCK_STREAM, 0 )
 * 	sockaddr = Socket.pack_sockaddr_in( 2200, 'localhost' )
 * 	socket.bind( sockaddr )
 *  
 * === Unix-based Exceptions
 * On unix-based based systems the following system exceptions may be raised if 
 * the call to _bind_ fails:
 * * Errno::EACCES - the specified _sockaddr_ is protected and the current
 *   user does not have permission to bind to it
 * * Errno::EADDRINUSE - the specified _sockaddr_ is already in use
 * * Errno::EADDRNOTAVAIL - the specified _sockaddr_ is not available from the
 *   local machine
 * * Errno::EAFNOSUPPORT - the specified _sockaddr_ isnot a valid address for
 *   the family of the calling +socket+
 * * Errno::EBADF - the _sockaddr_ specified is not a valid file descriptor
 * * Errno::EFAULT - the _sockaddr_ argument cannot be accessed
 * * Errno::EINVAL - the +socket+ is already bound to an address, and the 
 *   protocol does not support binding to the new _sockaddr_ or the +socket+
 *   has been shut down.
 * * Errno::EINVAL - the address length is not a valid length for the address
 *   family
 * * Errno::ENAMETOOLONG - the pathname resolved had a length which exceeded
 *   PATH_MAX
 * * Errno::ENOBUFS - no buffer space is available
 * * Errno::ENOSR - there were insufficient STREAMS resources available to 
 *   complete the operation
 * * Errno::ENOTSOCK - the +socket+ does not refer to a socket
 * * Errno::EOPNOTSUPP - the socket type of the +socket+ does not support 
 *   binding to an address
 * 
 * On unix-based based systems if the address family of the calling +socket+ is
 * Socket::AF_UNIX the follow exceptions may be raised if the call to _bind_
 * fails:
 * * Errno::EACCES - search permission is denied for a component of the prefix
 *   path or write access to the +socket+ is denided
 * * Errno::EDESTADDRREQ - the _sockaddr_ argument is a null pointer
 * * Errno::EISDIR - same as Errno::EDESTADDRREQ
 * * Errno::EIO - an i/o error occurred
 * * Errno::ELOOP - too many symbolic links were encountered in translating
 *   the pathname in _sockaddr_
 * * Errno::ENAMETOOLLONG - a component of a pathname exceeded NAME_MAX 
 *   characters, or an entired pathname exceeded PATH_MAX characters
 * * Errno::ENOENT - a component of the pathname does not name an existing file
 *   or the pathname is an empty string
 * * Errno::ENOTDIR - a component of the path prefix of the pathname in _sockaddr_
 *   is not a directory
 * * Errno::EROFS - the name would reside on a read only filesystem
 * 
 * === Windows Exceptions
 * On Windows systems the following system exceptions may be raised if 
 * the call to _bind_ fails:
 * * Errno::ENETDOWN-- the network is down
 * * Errno::EACCES - the attempt to connect the datagram socket to the 
 *   broadcast address failed
 * * Errno::EADDRINUSE - the socket's local address is already in use
 * * Errno::EADDRNOTAVAIL - the specified address is not a valid address for this
 *   computer
 * * Errno::EFAULT - the socket's internal address or address length parameter
 *   is too small or is not a valid part of the user space addressed
 * * Errno::EINVAL - the +socket+ is already bound to an address
 * * Errno::ENOBUFS - no buffer space is available
 * * Errno::ENOTSOCK - the +socket+ argument does not refer to a socket
 * 
 * === See
 * * bind manual pages on unix-based systems
 * * bind function in Microsoft's Winsock functions reference
 */ 
static VALUE
sock_bind(VALUE sock, VALUE addr)
{
    rb_io_t *fptr;

    SockAddrStringValue(addr);
    GetOpenFile(sock, fptr);
    if (bind(fptr->fd, (struct sockaddr*)RSTRING_PTR(addr), RSTRING_LEN(addr)) < 0)
	rb_sys_fail("bind(2)");

    return INT2FIX(0);
}

/*
 * call-seq:
 * 	socket.listen( int ) => 0
 * 
 * Listens for connections, using the specified +int+ as the backlog. A call
 * to _listen_ only applies if the +socket+ is of type SOCK_STREAM or 
 * SOCK_SEQPACKET.
 * 
 * === Parameter
 * * +backlog+ - the maximum length of the queue for pending connections.
 * 
 * === Example 1
 * 	require 'socket'
 * 	include Socket::Constants
 * 	socket = Socket.new( AF_INET, SOCK_STREAM, 0 )
 * 	sockaddr = Socket.pack_sockaddr_in( 2200, 'localhost' )
 * 	socket.bind( sockaddr )
 * 	socket.listen( 5 )
 * 
 * === Example 2 (listening on an arbitary port, unix-based systems only):
 * 	require 'socket'
 * 	include Socket::Constants
 * 	socket = Socket.new( AF_INET, SOCK_STREAM, 0 )
 * 	socket.listen( 1 )
 * 
 * === Unix-based Exceptions
 * On unix based systems the above will work because a new +sockaddr+ struct
 * is created on the address ADDR_ANY, for an arbitrary port number as handed
 * off by the kernel. It will not work on Windows, because Windows requires that
 * the +socket+ is bound by calling _bind_ before it can _listen_.
 * 
 * If the _backlog_ amount exceeds the implementation-dependent maximum
 * queue length, the implementation's maximum queue length will be used.
 * 
 * On unix-based based systems the following system exceptions may be raised if the
 * call to _listen_ fails:
 * * Errno::EBADF - the _socket_ argument is not a valid file descriptor
 * * Errno::EDESTADDRREQ - the _socket_ is not bound to a local address, and 
 *   the protocol does not support listening on an unbound socket
 * * Errno::EINVAL - the _socket_ is already connected
 * * Errno::ENOTSOCK - the _socket_ argument does not refer to a socket
 * * Errno::EOPNOTSUPP - the _socket_ protocol does not support listen
 * * Errno::EACCES - the calling process does not have approriate privileges
 * * Errno::EINVAL - the _socket_ has been shut down
 * * Errno::ENOBUFS - insufficient resources are available in the system to 
 *   complete the call
 * 
 * === Windows Exceptions
 * On Windows systems the following system exceptions may be raised if 
 * the call to _listen_ fails:
 * * Errno::ENETDOWN - the network is down
 * * Errno::EADDRINUSE - the socket's local address is already in use. This 
 *   usually occurs during the execution of _bind_ but could be delayed
 *   if the call to _bind_ was to a partially wildcard address (involving
 *   ADDR_ANY) and if a specific address needs to be commmitted at the 
 *   time of the call to _listen_
 * * Errno::EINPROGRESS - a Windows Sockets 1.1 call is in progress or the
 *   service provider is still processing a callback function
 * * Errno::EINVAL - the +socket+ has not been bound with a call to _bind_.
 * * Errno::EISCONN - the +socket+ is already connected
 * * Errno::EMFILE - no more socket descriptors are available
 * * Errno::ENOBUFS - no buffer space is available
 * * Errno::ENOTSOC - +socket+ is not a socket
 * * Errno::EOPNOTSUPP - the referenced +socket+ is not a type that supports
 *   the _listen_ method
 * 
 * === See
 * * listen manual pages on unix-based systems
 * * listen function in Microsoft's Winsock functions reference
 */
VALUE
sock_listen(VALUE sock, VALUE log)
{
    rb_io_t *fptr;
    int backlog;

    rb_secure(4);
    backlog = NUM2INT(log);
    GetOpenFile(sock, fptr);
    if (listen(fptr->fd, backlog) < 0)
	rb_sys_fail("listen(2)");

    return INT2FIX(0);
}

/*
 * call-seq:
 * 	socket.recvfrom(maxlen) => [mesg, sender_sockaddr]
 * 	socket.recvfrom(maxlen, flags) => [mesg, sender_sockaddr]
 * 
 * Receives up to _maxlen_ bytes from +socket+. _flags_ is zero or more
 * of the +MSG_+ options. The first element of the results, _mesg_, is the data
 * received. The second element, _sender_sockaddr_, contains protocol-specific information
 * on the sender.
 * 
 * === Parameters
 * * +maxlen+ - the number of bytes to receive from the socket
 * * +flags+ - zero or more of the +MSG_+ options 
 * 
 * === Example
 * 	# In one file, start this first
 * 	require 'socket'
 * 	include Socket::Constants
 * 	socket = Socket.new( AF_INET, SOCK_STREAM, 0 )
 * 	sockaddr = Socket.pack_sockaddr_in( 2200, 'localhost' )
 * 	socket.bind( sockaddr )
 * 	socket.listen( 5 )
 * 	client, client_sockaddr = socket.accept
 * 	data = client.recvfrom( 20 )[0].chomp
 * 	puts "I only received 20 bytes '#{data}'"
 * 	sleep 1
 * 	socket.close
 * 
 * 	# In another file, start this second
 * 	require 'socket'
 * 	include Socket::Constants
 * 	socket = Socket.new( AF_INET, SOCK_STREAM, 0 )
 * 	sockaddr = Socket.pack_sockaddr_in( 2200, 'localhost' )
 * 	socket.connect( sockaddr )
 * 	socket.puts "Watch this get cut short!"
 * 	socket.close 
 * 
 * === Unix-based Exceptions
 * On unix-based based systems the following system exceptions may be raised if the
 * call to _recvfrom_ fails:
 * * Errno::EAGAIN - the +socket+ file descriptor is marked as O_NONBLOCK and no
 *   data is waiting to be received; or MSG_OOB is set and no out-of-band data
 *   is available and either the +socket+ file descriptor is marked as 
 *   O_NONBLOCK or the +socket+ does not support blocking to wait for 
 *   out-of-band-data
 * * Errno::EWOULDBLOCK - see Errno::EAGAIN
 * * Errno::EBADF - the +socket+ is not a valid file descriptor
 * * Errno::ECONNRESET - a connection was forcibly closed by a peer
 * * Errno::EFAULT - the socket's internal buffer, address or address length 
 *   cannot be accessed or written
 * * Errno::EINTR - a signal interupted _recvfrom_ before any data was available
 * * Errno::EINVAL - the MSG_OOB flag is set and no out-of-band data is available
 * * Errno::EIO - an i/o error occurred while reading from or writing to the 
 *   filesystem
 * * Errno::ENOBUFS - insufficient resources were available in the system to 
 *   perform the operation
 * * Errno::ENOMEM - insufficient memory was available to fulfill the request
 * * Errno::ENOSR - there were insufficient STREAMS resources available to 
 *   complete the operation
 * * Errno::ENOTCONN - a receive is attempted on a connection-mode socket that
 *   is not connected
 * * Errno::ENOTSOCK - the +socket+ does not refer to a socket
 * * Errno::EOPNOTSUPP - the specified flags are not supported for this socket type
 * * Errno::ETIMEDOUT - the connection timed out during connection establishment
 *   or due to a transmission timeout on an active connection
 * 
 * === Windows Exceptions
 * On Windows systems the following system exceptions may be raised if 
 * the call to _recvfrom_ fails:
 * * Errno::ENETDOWN - the network is down
 * * Errno::EFAULT - the internal buffer and from parameters on +socket+ are not
 *   part of the user address space, or the internal fromlen parameter is
 *   too small to accomodate the peer address
 * * Errno::EINTR - the (blocking) call was cancelled by an internal call to
 *   the WinSock function WSACancelBlockingCall
 * * Errno::EINPROGRESS - a blocking Windows Sockets 1.1 call is in progress or 
 *   the service provider is still processing a callback function
 * * Errno::EINVAL - +socket+ has not been bound with a call to _bind_, or an
 *   unknown flag was specified, or MSG_OOB was specified for a socket with
 *   SO_OOBINLINE enabled, or (for byte stream-style sockets only) the internal
 *   len parameter on +socket+ was zero or negative
 * * Errno::EISCONN - +socket+ is already connected. The call to _recvfrom_ is
 *   not permitted with a connected socket on a socket that is connetion 
 *   oriented or connectionless.
 * * Errno::ENETRESET - the connection has been broken due to the keep-alive 
 *   activity detecting a failure while the operation was in progress.
 * * Errno::EOPNOTSUPP - MSG_OOB was specified, but +socket+ is not stream-style
 *   such as type SOCK_STREAM. OOB data is not supported in the communication
 *   domain associated with +socket+, or +socket+ is unidirectional and 
 *   supports only send operations
 * * Errno::ESHUTDOWN - +socket+ has been shutdown. It is not possible to 
 *   call _recvfrom_ on a socket after _shutdown_ has been invoked.
 * * Errno::EWOULDBLOCK - +socket+ is marked as nonblocking and a  call to 
 *   _recvfrom_ would block.
 * * Errno::EMSGSIZE - the message was too large to fit into the specified buffer
 *   and was truncated.
 * * Errno::ETIMEDOUT - the connection has been dropped, because of a network
 *   failure or because the system on the other end went down without
 *   notice
 * * Errno::ECONNRESET - the virtual circuit was reset by the remote side 
 *   executing a hard or abortive close. The application should close the
 *   socket; it is no longer usable. On a UDP-datagram socket this error
 *   indicates a previous send operation resulted in an ICMP Port Unreachable
 *   message.
 */
static VALUE
sock_recvfrom(int argc, VALUE *argv, VALUE sock)
{
    return s_recvfrom(sock, argc, argv, RECV_SOCKET);
}

/*
 * call-seq:
 * 	socket.recvfrom_nonblock(maxlen) => [mesg, sender_sockaddr]
 * 	socket.recvfrom_nonblock(maxlen, flags) => [mesg, sender_sockaddr]
 * 
 * Receives up to _maxlen_ bytes from +socket+ using recvfrom(2) after
 * O_NONBLOCK is set for the underlying file descriptor.
 * _flags_ is zero or more of the +MSG_+ options.
 * The first element of the results, _mesg_, is the data received.
 * The second element, _sender_sockaddr_, contains protocol-specific information
 * on the sender.
 *
 * When recvfrom(2) returns 0, Socket#recvfrom_nonblock returns
 * an empty string as data.
 * The meaning depends on the socket: EOF on TCP, empty packet on UDP, etc.
 * 
 * === Parameters
 * * +maxlen+ - the number of bytes to receive from the socket
 * * +flags+ - zero or more of the +MSG_+ options 
 * 
 * === Example
 * 	# In one file, start this first
 * 	require 'socket'
 * 	include Socket::Constants
 * 	socket = Socket.new(AF_INET, SOCK_STREAM, 0)
 * 	sockaddr = Socket.sockaddr_in(2200, 'localhost')
 * 	socket.bind(sockaddr)
 * 	socket.listen(5)
 * 	client, client_sockaddr = socket.accept
 * 	begin # emulate blocking recvfrom
 * 	  pair = client.recvfrom_nonblock(20)
 * 	rescue Errno::EAGAIN, Errno::EWOULDBLOCK
 * 	  IO.select([client])
 * 	  retry
 * 	end
 * 	data = pair[0].chomp
 * 	puts "I only received 20 bytes '#{data}'"
 * 	sleep 1
 * 	socket.close
 * 
 * 	# In another file, start this second
 * 	require 'socket'
 * 	include Socket::Constants
 * 	socket = Socket.new(AF_INET, SOCK_STREAM, 0)
 * 	sockaddr = Socket.sockaddr_in(2200, 'localhost')
 * 	socket.connect(sockaddr)
 * 	socket.puts "Watch this get cut short!"
 * 	socket.close 
 * 
 * Refer to Socket#recvfrom for the exceptions that may be thrown if the call
 * to _recvfrom_nonblock_ fails. 
 *
 * Socket#recvfrom_nonblock may raise any error corresponding to recvfrom(2) failure,
 * including Errno::EWOULDBLOCK.
 *
 * === See
 * * Socket#recvfrom
 */
static VALUE
sock_recvfrom_nonblock(int argc, VALUE *argv, VALUE sock)
{
    return s_recvfrom_nonblock(sock, argc, argv, RECV_SOCKET);
}

/*
 * call-seq:
 *   socket.accept => [client_socket, client_addrinfo]
 *
 * Accepts a next connection.
 * Returns a new Socket object and AddrInfo object.
 *
 *   serv = Socket.new(:INET, :STREAM, 0)
 *   serv.listen(5)
 *   c = Socket.new(:INET, :STREAM, 0)
 *   c.connect(serv.local_address)
 *   p serv.accept #=> [#<Socket:fd 6>, #<AddrInfo: 127.0.0.1:48555 TCP>]
 *
 */
static VALUE
sock_accept(VALUE sock)
{
    rb_io_t *fptr;
    VALUE sock2;
    char buf[1024];
    socklen_t len = sizeof buf;

    GetOpenFile(sock, fptr);
    sock2 = s_accept(rb_cSocket,fptr->fd,(struct sockaddr*)buf,&len);

    return rb_assoc_new(sock2, io_socket_addrinfo(sock2, (struct sockaddr*)buf, len));
}

/*
 * call-seq:
 * 	socket.accept_nonblock => [client_socket, client_sockaddr]
 * 
 * Accepts an incoming connection using accept(2) after
 * O_NONBLOCK is set for the underlying file descriptor.
 * It returns an array containg the accpeted socket
 * for the incoming connection, _client_socket_,
 * and a string that contains the +struct+ sockaddr information
 * about the caller, _client_sockaddr_.
 * 
 * === Example
 * 	# In one script, start this first
 * 	require 'socket'
 * 	include Socket::Constants
 * 	socket = Socket.new(AF_INET, SOCK_STREAM, 0)
 * 	sockaddr = Socket.sockaddr_in(2200, 'localhost')
 * 	socket.bind(sockaddr)
 * 	socket.listen(5)
 * 	begin # emulate blocking accept
 * 	  client_socket, client_sockaddr = socket.accept_nonblock
 * 	rescue Errno::EAGAIN, Errno::EWOULDBLOCK, Errno::ECONNABORTED, Errno::EPROTO, Errno::EINTR
 * 	  IO.select([socket])
 * 	  retry
 * 	end
 * 	puts "The client said, '#{client_socket.readline.chomp}'"
 * 	client_socket.puts "Hello from script one!"
 * 	socket.close
 * 
 * 	# In another script, start this second
 * 	require 'socket'
 * 	include Socket::Constants
 * 	socket = Socket.new(AF_INET, SOCK_STREAM, 0)
 * 	sockaddr = Socket.sockaddr_in(2200, 'localhost')
 * 	socket.connect(sockaddr)
 * 	socket.puts "Hello from script 2." 
 * 	puts "The server said, '#{socket.readline.chomp}'"
 * 	socket.close
 * 
 * Refer to Socket#accept for the exceptions that may be thrown if the call
 * to _accept_nonblock_ fails. 
 *
 * Socket#accept_nonblock may raise any error corresponding to accept(2) failure,
 * including Errno::EWOULDBLOCK.
 * 
 * === See
 * * Socket#accept
 */
static VALUE
sock_accept_nonblock(VALUE sock)
{
    rb_io_t *fptr;
    VALUE sock2;
    char buf[1024];
    socklen_t len = sizeof buf;

    GetOpenFile(sock, fptr);
    sock2 = s_accept_nonblock(rb_cSocket, fptr, (struct sockaddr *)buf, &len);
    return rb_assoc_new(sock2, io_socket_addrinfo(sock2, (struct sockaddr*)buf, len));
}

/*
 * call-seq:
 * 	socket.sysaccept => [client_socket_fd, client_sockaddr]
 * 
 * Accepts an incoming connection returnings an array containg the (integer)
 * file descriptor for the incoming connection, _client_socket_fd_,
 * and a string that contains the +struct+ sockaddr information
 * about the caller, _client_sockaddr_.
 * 
 * === Example
 * 	# In one script, start this first
 * 	require 'socket'
 * 	include Socket::Constants
 * 	socket = Socket.new( AF_INET, SOCK_STREAM, 0 )
 * 	sockaddr = Socket.pack_sockaddr_in( 2200, 'localhost' )
 * 	socket.bind( sockaddr )
 * 	socket.listen( 5 )
 * 	client_fd, client_sockaddr = socket.sysaccept
 * 	client_socket = Socket.for_fd( client_fd )
 * 	puts "The client said, '#{client_socket.readline.chomp}'"
 * 	client_socket.puts "Hello from script one!"
 * 	socket.close
 * 
 * 	# In another script, start this second
 * 	require 'socket'
 * 	include Socket::Constants
 * 	socket = Socket.new( AF_INET, SOCK_STREAM, 0 )
 * 	sockaddr = Socket.pack_sockaddr_in( 2200, 'localhost' )
 * 	socket.connect( sockaddr )
 * 	socket.puts "Hello from script 2." 
 * 	puts "The server said, '#{socket.readline.chomp}'"
 * 	socket.close
 * 
 * Refer to Socket#accept for the exceptions that may be thrown if the call
 * to _sysaccept_ fails. 
 * 
 * === See
 * * Socket#accept
 */
static VALUE
sock_sysaccept(VALUE sock)
{
    rb_io_t *fptr;
    VALUE sock2;
    char buf[1024];
    socklen_t len = sizeof buf;

    GetOpenFile(sock, fptr);
    sock2 = s_accept(0,fptr->fd,(struct sockaddr*)buf,&len);

    return rb_assoc_new(sock2, io_socket_addrinfo(sock2, (struct sockaddr*)buf, len));
}

#ifdef HAVE_GETHOSTNAME
/*
 * call-seq:
 *   Socket.gethostname => hostname
 *
 * Returns the hostname.
 *
 * Note that it is not guaranteed to be able to convert to IP address using gethostbyname, getaddrinfo, etc.
 *
 *   p Socket.gethostname #=> "hal"
 *
 */
static VALUE
sock_gethostname(VALUE obj)
{
    char buf[1024];

    rb_secure(3);
    if (gethostname(buf, (int)sizeof buf - 1) < 0)
	rb_sys_fail("gethostname");

    buf[sizeof buf - 1] = '\0';
    return rb_str_new2(buf);
}
#else
#ifdef HAVE_UNAME

#include <sys/utsname.h>

static VALUE
sock_gethostname(VALUE obj)
{
    struct utsname un;

    rb_secure(3);
    uname(&un);
    return rb_str_new2(un.nodename);
}
#else
static VALUE
sock_gethostname(VALUE obj)
{
    rb_notimplement();
}
#endif
#endif

static VALUE
make_addrinfo(struct addrinfo *res0)
{
    VALUE base, ary;
    struct addrinfo *res;

    if (res0 == NULL) {
	rb_raise(rb_eSocket, "host not found");
    }
    base = rb_ary_new();
    for (res = res0; res; res = res->ai_next) {
	ary = ipaddr(res->ai_addr, do_not_reverse_lookup);
	if (res->ai_canonname) {
	    RARRAY_PTR(ary)[2] = rb_str_new2(res->ai_canonname);
	}
	rb_ary_push(ary, INT2FIX(res->ai_family));
	rb_ary_push(ary, INT2FIX(res->ai_socktype));
	rb_ary_push(ary, INT2FIX(res->ai_protocol));
	rb_ary_push(base, ary);
    }
    return base;
}

static VALUE
sock_sockaddr(struct sockaddr *addr, size_t len)
{
    char *ptr;

    switch (addr->sa_family) {
      case AF_INET:
	ptr = (char*)&((struct sockaddr_in*)addr)->sin_addr.s_addr;
	len = sizeof(((struct sockaddr_in*)addr)->sin_addr.s_addr);
	break;
#ifdef INET6
      case AF_INET6:
	ptr = (char*)&((struct sockaddr_in6*)addr)->sin6_addr.s6_addr;
	len = sizeof(((struct sockaddr_in6*)addr)->sin6_addr.s6_addr);
	break;
#endif
      default:
        rb_raise(rb_eSocket, "unknown socket family:%d", addr->sa_family);
	break;
    }
    return rb_str_new(ptr, len);
}

/*
 * call-seq:
 *   Socket.gethostbyname(hostname) => [official_hostname, alias_hostnames, address_family, *address_list]
 *
 * Obtains the host information for _hostname_.
 *
 *   p Socket.gethostbyname("hal") #=> ["localhost", ["hal"], 2, "\x7F\x00\x00\x01"]
 *
 */
static VALUE
sock_s_gethostbyname(VALUE obj, VALUE host)
{
    rb_secure(3);
    return make_hostent(host, sock_addrinfo(host, Qnil, SOCK_STREAM, AI_CANONNAME), sock_sockaddr);
}

/*
 * call-seq:
 *   Socket.gethostbyaddr(address_string [, address_family]) => hostent
 *
 * Obtains the host information for _address_.
 *
 *   p Socket.gethostbyaddr([221,186,184,68].pack("CCCC"))              
 *   #=> ["carbon.ruby-lang.org", [], 2, "\xDD\xBA\xB8D"]
 */
static VALUE
sock_s_gethostbyaddr(int argc, VALUE *argv)
{
    VALUE addr, family;
    struct hostent *h;
    struct sockaddr *sa;
    char **pch;
    VALUE ary, names;
    int t = AF_INET;

    rb_scan_args(argc, argv, "11", &addr, &family);
    sa = (struct sockaddr*)StringValuePtr(addr);
    if (!NIL_P(family)) {
	t = family_arg(family);
    }
#ifdef INET6
    else if (RSTRING_LEN(addr) == 16) {
	t = AF_INET6;
    }
#endif
    h = gethostbyaddr(RSTRING_PTR(addr), RSTRING_LEN(addr), t);
    if (h == NULL) {
#ifdef HAVE_HSTRERROR
	extern int h_errno;
	rb_raise(rb_eSocket, "%s", (char*)hstrerror(h_errno));
#else
	rb_raise(rb_eSocket, "host not found");
#endif
    }
    ary = rb_ary_new();
    rb_ary_push(ary, rb_str_new2(h->h_name));
    names = rb_ary_new();
    rb_ary_push(ary, names);
    if (h->h_aliases != NULL) {
	for (pch = h->h_aliases; *pch; pch++) {
	    rb_ary_push(names, rb_str_new2(*pch));
	}
    }
    rb_ary_push(ary, INT2NUM(h->h_addrtype));
#ifdef h_addr
    for (pch = h->h_addr_list; *pch; pch++) {
	rb_ary_push(ary, rb_str_new(*pch, h->h_length));
    }
#else
    rb_ary_push(ary, rb_str_new(h->h_addr, h->h_length));
#endif

    return ary;
}

/*
 * call-seq:
 *   Socket.getservbyname(service_name)                => port_number
 *   Socket.getservbyname(service_name, protocol_name) => port_number
 *
 * Obtains the port number for _service_name_.
 *
 * If _protocol_name_ is not given, "tcp" is assumed.
 *
 *   Socket.getservbyname("smtp")          #=> 25
 *   Socket.getservbyname("shell")         #=> 514
 *   Socket.getservbyname("syslog", "udp") #=> 514
 */   
static VALUE
sock_s_getservbyname(int argc, VALUE *argv)
{
    VALUE service, proto;
    struct servent *sp;
    int port;
    const char *servicename, *protoname = "tcp";

    rb_scan_args(argc, argv, "11", &service, &proto);
    StringValue(service);
    if (!NIL_P(proto)) StringValue(proto);
    servicename = StringValueCStr(service);
    if (!NIL_P(proto)) protoname = StringValueCStr(proto);
    sp = getservbyname(servicename, protoname);
    if (sp) {
	port = ntohs(sp->s_port);
    }
    else {
	char *end;

	port = STRTOUL(servicename, &end, 0);
	if (*end != '\0') {
	    rb_raise(rb_eSocket, "no such service %s/%s", servicename, protoname);
	}
    }
    return INT2FIX(port);
}

/*
 * call-seq:
 *   Socket.getservbyport(port [, protocol_name]) => service
 *
 * Obtains the port number for _port_.
 *
 * If _protocol_name_ is not given, "tcp" is assumed.
 *
 *   Socket.getservbyport(80)         #=> "www"
 *   Socket.getservbyport(514, "tcp") #=> "shell"
 *   Socket.getservbyport(514, "udp") #=> "syslog"
 *
 */
static VALUE
sock_s_getservbyport(int argc, VALUE *argv)
{
    VALUE port, proto;
    struct servent *sp;
    long portnum;
    const char *protoname = "tcp";

    rb_scan_args(argc, argv, "11", &port, &proto);
    portnum = NUM2LONG(port);
    if (portnum != (uint16_t)portnum) {
	const char *s = portnum > 0 ? "big" : "small";
	rb_raise(rb_eRangeError, "integer %ld too %s to convert into `int16_t'", portnum, s);
    }
    if (!NIL_P(proto)) protoname = StringValueCStr(proto);

    sp = getservbyport((int)htons((uint16_t)portnum), protoname);
    if (!sp) {
	rb_raise(rb_eSocket, "no such service for port %d/%s", (int)portnum, protoname);
    }
    return rb_tainted_str_new2(sp->s_name);
}

/*
 * call-seq:
 *   Socket.getaddrinfo(nodename, servname[, family[, socktype[, protocol[, flags]]]]) => array
 *
 * Obtains address information for _nodename_:_servname_.
 *
 * _family_ should be an address family such as: :INET, :INET6, :UNIX, etc.
 *
 * _socktype_ should be a socket type such as: :STREAM, :DGRAM, :RAW, etc.
 *
 * _protocol_ should be a protocol defined in the family.
 * 0 is default protocol for the family.
 *
 * _flags_ should be bitwise OR of Socket::AI_* constants.
 *
 *   Socket.getaddrinfo("www.ruby-lang.org", "http", nil, :STREAM)
 *   #=> [["AF_INET", 80, "carbon.ruby-lang.org", "221.186.184.68", 2, 1, 6]] # PF_INET/SOCK_STREAM/IPPROTO_TCP
 *
 *   Socket.getaddrinfo("localhost", nil)
 *   #=> [["AF_INET", 0, "localhost", "127.0.0.1", 2, 1, 6],  # PF_INET/SOCK_STREAM/IPPROTO_TCP
 *   #    ["AF_INET", 0, "localhost", "127.0.0.1", 2, 2, 17], # PF_INET/SOCK_DGRAM/IPPROTO_UDP
 *   #    ["AF_INET", 0, "localhost", "127.0.0.1", 2, 3, 0]]  # PF_INET/SOCK_RAW/IPPROTO_IP
 *
 */
static VALUE
sock_s_getaddrinfo(int argc, VALUE *argv)
{
    VALUE host, port, family, socktype, protocol, flags, ret;
    struct addrinfo hints, *res;

    rb_scan_args(argc, argv, "24", &host, &port, &family, &socktype, &protocol, &flags);

    MEMZERO(&hints, struct addrinfo, 1);
    hints.ai_family = NIL_P(family) ? PF_UNSPEC : family_arg(family);

    if (!NIL_P(socktype)) {
	hints.ai_socktype = socktype_arg(socktype);
    }
    if (!NIL_P(protocol)) {
	hints.ai_protocol = NUM2INT(protocol);
    }
    if (!NIL_P(flags)) {
	hints.ai_flags = NUM2INT(flags);
    }
    res = sock_getaddrinfo(host, port, &hints, 0);

    ret = make_addrinfo(res);
    freeaddrinfo(res);
    return ret;
}

/*
 * call-seq:
 *   Socket.getnameinfo(sockaddr [, flags]) => [hostname, servicename]
 *
 * Obtains name information for _sockaddr_.
 *
 * _sockaddr_ should be one of follows.
 * - packed sockddr string such as Socket.sockaddr_in(80, "127.0.0.1")
 * - 3-elements array such as ["AF_INET", 80, "127.0.0.1"]
 * - 4-elements array such as ["AF_INET", 80, ignored, "127.0.0.1"]
 *
 * _flags_ should be bitwise OR of Socket::NI_* constants.
 *
 * Note that the last form is compatible with IPSocket#{addr,peeraddr}.
 *
 *   Socket.getnameinfo(Socket.sockaddr_in(80, "127.0.0.1"))       #=> ["localhost", "www"]
 *   Socket.getnameinfo(["AF_INET", 80, "127.0.0.1"])              #=> ["localhost", "www"]
 *   Socket.getnameinfo(["AF_INET", 80, "localhost", "127.0.0.1"]) #=> ["localhost", "www"]
 */
static VALUE
sock_s_getnameinfo(int argc, VALUE *argv)
{
    VALUE sa, af = Qnil, host = Qnil, port = Qnil, flags, tmp;
    char *hptr, *pptr;
    char hbuf[1024], pbuf[1024];
    int fl;
    struct addrinfo hints, *res = NULL, *r;
    int error;
    struct sockaddr_storage ss;
    struct sockaddr *sap;

    sa = flags = Qnil;
    rb_scan_args(argc, argv, "11", &sa, &flags);

    fl = 0;
    if (!NIL_P(flags)) {
	fl = NUM2INT(flags);
    }
    tmp = rb_check_string_type(sa);
    if (!NIL_P(tmp)) {
	sa = tmp;
	if (sizeof(ss) < RSTRING_LEN(sa)) {
	    rb_raise(rb_eTypeError, "sockaddr length too big");
	}
	memcpy(&ss, RSTRING_PTR(sa), RSTRING_LEN(sa));
	if (RSTRING_LEN(sa) != SA_LEN((struct sockaddr*)&ss)) {
	    rb_raise(rb_eTypeError, "sockaddr size differs - should not happen");
	}
	sap = (struct sockaddr*)&ss;
	goto call_nameinfo;
    }
    tmp = rb_check_array_type(sa);
    if (!NIL_P(tmp)) {
	sa = tmp;
	MEMZERO(&hints, struct addrinfo, 1);
	if (RARRAY_LEN(sa) == 3) {
	    af = RARRAY_PTR(sa)[0];
	    port = RARRAY_PTR(sa)[1];
	    host = RARRAY_PTR(sa)[2];
	}
	else if (RARRAY_LEN(sa) >= 4) {
	    af = RARRAY_PTR(sa)[0];
	    port = RARRAY_PTR(sa)[1];
	    host = RARRAY_PTR(sa)[3];
	    if (NIL_P(host)) {
		host = RARRAY_PTR(sa)[2];
	    }
	    else {
		/*
		 * 4th element holds numeric form, don't resolve.
		 * see ipaddr().
		 */
#ifdef AI_NUMERICHOST /* AIX 4.3.3 doesn't have AI_NUMERICHOST. */
		hints.ai_flags |= AI_NUMERICHOST;
#endif
	    }
	}
	else {
	    rb_raise(rb_eArgError, "array size should be 3 or 4, %ld given",
		     RARRAY_LEN(sa));
	}
	/* host */
	if (NIL_P(host)) {
	    hptr = NULL;
	}
	else {
	    strncpy(hbuf, StringValuePtr(host), sizeof(hbuf));
	    hbuf[sizeof(hbuf) - 1] = '\0';
	    hptr = hbuf;
	}
	/* port */
	if (NIL_P(port)) {
	    strcpy(pbuf, "0");
	    pptr = NULL;
	}
	else if (FIXNUM_P(port)) {
	    snprintf(pbuf, sizeof(pbuf), "%ld", NUM2LONG(port));
	    pptr = pbuf;
	}
	else {
	    strncpy(pbuf, StringValuePtr(port), sizeof(pbuf));
	    pbuf[sizeof(pbuf) - 1] = '\0';
	    pptr = pbuf;
	}
	hints.ai_socktype = (fl & NI_DGRAM) ? SOCK_DGRAM : SOCK_STREAM;
	/* af */
        hints.ai_family = NIL_P(af) ? PF_UNSPEC : family_arg(af);
	error = rb_getaddrinfo(hptr, pptr, &hints, &res);
	if (error) goto error_exit_addr;
	sap = res->ai_addr;
    }
    else {
	rb_raise(rb_eTypeError, "expecting String or Array");
    }

  call_nameinfo:
    error = rb_getnameinfo(sap, SA_LEN(sap), hbuf, sizeof(hbuf),
			   pbuf, sizeof(pbuf), fl);
    if (error) goto error_exit_name;
    if (res) {
	for (r = res->ai_next; r; r = r->ai_next) {
	    char hbuf2[1024], pbuf2[1024];

	    sap = r->ai_addr;
	    error = rb_getnameinfo(sap, SA_LEN(sap), hbuf2, sizeof(hbuf2),
				   pbuf2, sizeof(pbuf2), fl);
	    if (error) goto error_exit_name;
	    if (strcmp(hbuf, hbuf2) != 0|| strcmp(pbuf, pbuf2) != 0) {
		freeaddrinfo(res);
		rb_raise(rb_eSocket, "sockaddr resolved to multiple nodename");
	    }
	}
	freeaddrinfo(res);
    }
    return rb_assoc_new(rb_str_new2(hbuf), rb_str_new2(pbuf));

  error_exit_addr:
    if (res) freeaddrinfo(res);
    raise_socket_error("getaddrinfo", error);

  error_exit_name:
    if (res) freeaddrinfo(res);
    raise_socket_error("getnameinfo", error);
}

/*
 * call-seq:
 *   Socket.sockaddr_in(port, host)      => sockaddr
 *   Socket.pack_sockaddr_in(port, host) => sockaddr
 *
 * Packs _port_ and _host_ as an AF_INET/AF_INET6 sockaddr string.
 *
 *   Socket.sockaddr_in(80, "127.0.0.1")
 *   #=> "\x02\x00\x00P\x7F\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00"
 *
 *   Socket.sockaddr_in(80, "::1")
 *   #=> "\n\x00\x00P\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00"
 *
 */
static VALUE
sock_s_pack_sockaddr_in(VALUE self, VALUE port, VALUE host)
{
    struct addrinfo *res = sock_addrinfo(host, port, 0, 0);
    VALUE addr = rb_str_new((char*)res->ai_addr, res->ai_addrlen);

    freeaddrinfo(res);
    OBJ_INFECT(addr, port);
    OBJ_INFECT(addr, host);

    return addr;
}

/*
 * call-seq:
 *   Socket.unpack_sockaddr_in(sockaddr) => [port, ip_address]
 *
 * Unpacks _sockaddr_ into port and ip_address.
 *
 * _sockaddr_ should be a string or an addrinfo for AF_INET/AF_INET6.
 *
 *   sockaddr = Socket.sockaddr_in(80, "127.0.0.1")
 *   p sockaddr #=> "\x02\x00\x00P\x7F\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00"
 *   p Socket.unpack_sockaddr_in(sockaddr) #=> [80, "127.0.0.1"]
 *
 */
static VALUE
sock_s_unpack_sockaddr_in(VALUE self, VALUE addr)
{
    struct sockaddr_in * sockaddr;
    VALUE host;

    sockaddr = (struct sockaddr_in*)SockAddrStringValuePtr(addr);
    if (RSTRING_LEN(addr) <
        (char*)&((struct sockaddr *)sockaddr)->sa_family +
        sizeof(((struct sockaddr *)sockaddr)->sa_family) -
        (char*)sockaddr)
        rb_raise(rb_eArgError, "too short sockaddr");
    if (((struct sockaddr *)sockaddr)->sa_family != AF_INET
#ifdef INET6
        && ((struct sockaddr *)sockaddr)->sa_family != AF_INET6
#endif
        ) {
#ifdef INET6
        rb_raise(rb_eArgError, "not an AF_INET/AF_INET6 sockaddr");
#else
        rb_raise(rb_eArgError, "not an AF_INET sockaddr");
#endif
    }
    host = make_ipaddr((struct sockaddr*)sockaddr);
    OBJ_INFECT(host, addr);
    return rb_assoc_new(INT2NUM(ntohs(sockaddr->sin_port)), host);
}

#ifdef HAVE_SYS_UN_H

/*
 * call-seq:
 *   Socket.sockaddr_un(path)      => sockaddr
 *   Socket.pack_sockaddr_un(path) => sockaddr
 *
 * Packs _path_ as an AF_UNIX sockaddr string.
 *
 *   Socket.sockaddr_un("/tmp/sock") #=> "\x01\x00/tmp/sock\x00\x00..."
 *
 */
static VALUE
sock_s_pack_sockaddr_un(VALUE self, VALUE path)
{
    struct sockaddr_un sockaddr;
    char *sun_path;
    VALUE addr;

    MEMZERO(&sockaddr, struct sockaddr_un, 1);
    sockaddr.sun_family = AF_UNIX;
    sun_path = StringValueCStr(path);
    if (sizeof(sockaddr.sun_path) <= strlen(sun_path)) {
        rb_raise(rb_eArgError, "too long unix socket path (max: %dbytes)",
            (int)sizeof(sockaddr.sun_path)-1);
    }
    strncpy(sockaddr.sun_path, sun_path, sizeof(sockaddr.sun_path)-1);
    addr = rb_str_new((char*)&sockaddr, sizeof(sockaddr));
    OBJ_INFECT(addr, path);

    return addr;
}

/*
 * call-seq:
 *   Socket.unpack_sockaddr_un(sockaddr) => path
 *
 * Unpacks _sockaddr_ into path.
 *
 * _sockaddr_ should be a string or an addrinfo for AF_UNIX.
 *
 *   sockaddr = Socket.sockaddr_un("/tmp/sock") 
 *   p Socket.unpack_sockaddr_un(sockaddr) #=> "/tmp/sock"
 *
 */
static VALUE
sock_s_unpack_sockaddr_un(VALUE self, VALUE addr)
{
    struct sockaddr_un * sockaddr;
    const char *sun_path;
    VALUE path;

    sockaddr = (struct sockaddr_un*)SockAddrStringValuePtr(addr);
    if (RSTRING_LEN(addr) <
        (char*)&((struct sockaddr *)sockaddr)->sa_family +
        sizeof(((struct sockaddr *)sockaddr)->sa_family) -
        (char*)sockaddr)
        rb_raise(rb_eArgError, "too short sockaddr");
    if (((struct sockaddr *)sockaddr)->sa_family != AF_UNIX) {
        rb_raise(rb_eArgError, "not an AF_UNIX sockaddr");
    }
    if (sizeof(struct sockaddr_un) < RSTRING_LEN(addr)) {
	rb_raise(rb_eTypeError, "too long sockaddr_un - %ld longer than %d",
		 RSTRING_LEN(addr), (int)sizeof(struct sockaddr_un));
    }
    sun_path = unixpath(sockaddr, RSTRING_LEN(addr));
    if (sizeof(struct sockaddr_un) == RSTRING_LEN(addr) &&
        sun_path == sockaddr->sun_path &&
        sun_path + strlen(sun_path) == RSTRING_PTR(addr) + RSTRING_LEN(addr)) {
        rb_raise(rb_eArgError, "sockaddr_un.sun_path not NUL terminated");
    }
    path = rb_str_new2(sun_path);
    OBJ_INFECT(path, addr);
    return path;
}
#endif

/*
 * Class +Socket+ provides access to the underlying operating system
 * socket implementations. It can be used to provide more operating system
 * specific functionality than the protocol-specific socket classes but at the
 * expense of greater complexity. In particular, the class handles addresses
 * using +struct+ sockaddr structures packed into Ruby strings, which can be
 * a joy to manipulate.
 * 
 * === Exception Handling
 * Ruby's implementation of +Socket+ causes an exception to be raised
 * based on the error generated by the system dependent implementation.
 * This is why the methods are documented in a way that isolate
 * Unix-based system exceptions from Windows based exceptions. If more
 * information on particular exception is needed please refer to the 
 * Unix manual pages or the Windows WinSock reference.
 * 
 * 
 * === Documentation by
 * * Zach Dennis
 * * Sam Roberts
 * * <em>Programming Ruby</em> from The Pragmatic Bookshelf.  
 * 
 * Much material in this documentation is taken with permission from  
 * <em>Programming Ruby</em> from The Pragmatic Bookshelf.  
 */
void
Init_socket()
{
    Init_basicsocket();

    rb_cSocket = rb_define_class("Socket", rb_cBasicSocket);

    Init_socket_init();

    rb_define_method(rb_cSocket, "initialize", sock_initialize, 3);
    rb_define_method(rb_cSocket, "connect", sock_connect, 1);
    rb_define_method(rb_cSocket, "connect_nonblock", sock_connect_nonblock, 1);
    rb_define_method(rb_cSocket, "bind", sock_bind, 1);
    rb_define_method(rb_cSocket, "listen", sock_listen, 1);
    rb_define_method(rb_cSocket, "accept", sock_accept, 0);
    rb_define_method(rb_cSocket, "accept_nonblock", sock_accept_nonblock, 0);
    rb_define_method(rb_cSocket, "sysaccept", sock_sysaccept, 0);

    rb_define_method(rb_cSocket, "recvfrom", sock_recvfrom, -1);
    rb_define_method(rb_cSocket, "recvfrom_nonblock", sock_recvfrom_nonblock, -1);

    rb_define_singleton_method(rb_cSocket, "socketpair", sock_s_socketpair, 3);
    rb_define_singleton_method(rb_cSocket, "pair", sock_s_socketpair, 3);
    rb_define_singleton_method(rb_cSocket, "gethostname", sock_gethostname, 0);
    rb_define_singleton_method(rb_cSocket, "gethostbyname", sock_s_gethostbyname, 1);
    rb_define_singleton_method(rb_cSocket, "gethostbyaddr", sock_s_gethostbyaddr, -1);
    rb_define_singleton_method(rb_cSocket, "getservbyname", sock_s_getservbyname, -1);
    rb_define_singleton_method(rb_cSocket, "getservbyport", sock_s_getservbyport, -1);
    rb_define_singleton_method(rb_cSocket, "getaddrinfo", sock_s_getaddrinfo, -1);
    rb_define_singleton_method(rb_cSocket, "getnameinfo", sock_s_getnameinfo, -1);
    rb_define_singleton_method(rb_cSocket, "sockaddr_in", sock_s_pack_sockaddr_in, 2);
    rb_define_singleton_method(rb_cSocket, "pack_sockaddr_in", sock_s_pack_sockaddr_in, 2);
    rb_define_singleton_method(rb_cSocket, "unpack_sockaddr_in", sock_s_unpack_sockaddr_in, 1);
#ifdef HAVE_SYS_UN_H
    rb_define_singleton_method(rb_cSocket, "sockaddr_un", sock_s_pack_sockaddr_un, 1);
    rb_define_singleton_method(rb_cSocket, "pack_sockaddr_un", sock_s_pack_sockaddr_un, 1);
    rb_define_singleton_method(rb_cSocket, "unpack_sockaddr_un", sock_s_unpack_sockaddr_un, 1);
#endif
}
