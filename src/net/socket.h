#pragma once

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>

#include <string>
#include <boost/algorithm/string.hpp>

namespace bblocks {

//............................................................................... SocketOptions ....

/**
 * @class SocketOptions
 *
 * Abstraction to manipulate socket options.
 *
 */
class SocketOptions
{
public:

	static bool SetTcpNoDelay(const int fd, const bool enable)
	{
		const int flag = enable;
		int status = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
		return status != -1;
	}

	static bool SetTcpWindow(const int fd, const int size)
	{
		// Set out buffer size
		int status = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));
		if (status == -1) return false;

		// Set in buffer size
		status = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
		return status != -1;
	}
};

// .............................................................................. SocketAddress ....

/**
 * @class SocketAddress
 *
 * Socket address abstraction.
 *
 */
class SocketAddress
{
public:

	/*.... Static Function ....*/

	/**
	 * Create socket for accepting connection
	 */
	 static SocketAddress ServerSocketAddr(const std::string & hostname, const short port)
	 {
		SocketAddress addr;
		addr.laddr_ =  GetAddr(hostname, port);
		return addr;
	 }

	 static SocketAddress ServerSocketAddr(const sockaddr_in & laddr)
	 {
		SocketAddress addr;
		addr.laddr_ =  laddr;
		return addr;
	 }


	/**
	 * Convert hostname:port to sockaddr_in
	 */
	static sockaddr_in GetAddr(const std::string & hostname, const short port)
	{
		sockaddr_in addr;

		addrinfo hints;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = 0;
		hints.ai_protocol = 0;

		addrinfo * result = NULL;
		int status = getaddrinfo(hostname.c_str(), NULL, &hints, &result);
		INVARIANT(status != -1);
		ASSERT(result);
		ASSERT(result->ai_addrlen == sizeof(sockaddr_in));

		memset(&addr, 0, sizeof(sockaddr_in)); 
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		addr.sin_addr.s_addr = ((sockaddr_in *)result->ai_addr)->sin_addr.s_addr;

		freeaddrinfo(result);

		return addr;
	}

	/**
	 * Convert IP:port to sockaddr_in
	 */
	static sockaddr_in GetAddr(const in_addr_t & addr, const short port)
	{
		sockaddr_in sockaddr;
		memset(&sockaddr, /*ch=*/ 0, sizeof(sockaddr_in));
		sockaddr.sin_family = AF_INET;
		sockaddr.sin_port = htons(port);
		sockaddr.sin_addr.s_addr = addr;

		return sockaddr;
	}

	/**
	 * Convert hostname:port string to sockaddr_in
	 */
	static sockaddr_in GetAddr(const std::string & saddr)
	{
		std::vector<std::string> tokens;
		boost::split(tokens, saddr, boost::is_any_of(":"));

		ASSERT(tokens.size() == 2);

		const std::string host = tokens[0];
		const short port = atoi(tokens[1].c_str());

		return GetAddr(host, port);
	}

	/**
	 * Construct a connection (local binding-remote binding) form laddr and
	 * raddr strings of format host:port
	 */
	static SocketAddress GetAddr(const std::string & laddr,
				     const std::string & raddr)
	{
		return SocketAddress(GetAddr(laddr), GetAddr(raddr));
	}

	/*.... ctor/dtor ....*/

	explicit SocketAddress(const sockaddr_in & raddr)
	    : laddr_(GetAddr(INADDR_ANY, /*port=*/ 0)), raddr_(raddr)
	{}

	explicit SocketAddress(const sockaddr_in & laddr, const sockaddr_in & raddr)
	    : laddr_(laddr), raddr_(raddr)
	{}

	/*.... get/set ....*/

	/**
	 * Get local binding socket address
	 */
	const sockaddr_in & LocalAddr() const { return laddr_; }

	/**
	 * Get remote socket address
	 */
	const sockaddr_in & RemoteAddr() const { return raddr_; }

private:

	SocketAddress() {}

	struct sockaddr_in laddr_;
	struct sockaddr_in raddr_;
};

}
