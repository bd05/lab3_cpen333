/**
 * @file
 * @brief Windows implementation of a socket
 *
 * Uses WinSock
 */
#ifndef CPEN333_PROCESS_WINDOWS_SOCKET_H
#define CPEN333_PROCESS_WINDOWS_SOCKET_H

// allow inet_ntop in mingw
#undef _WIN32_WINNT
/**
 * @brief Allow sockets in MinGW
 */
#define _WIN32_WINNT 0x600
#undef NOMINMAX
 /**
 * @brief Prevent windows from defining min(), max() macros
 */
#define NOMINMAX 1

#include <string>
#include <cstdint>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <mutex>

#include "../../../util.h"

#ifdef _MSC_VER
// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")
#endif

#ifndef CPEN333_SOCKET_DEFAULT_PORT
/**
 * @brief Default port for making connections
 */
#define CPEN333_SOCKET_DEFAULT_PORT 5120
#endif

namespace cpen333 {
namespace process {

namespace windows {

namespace detail {

/**
 * @brief Singleton used for initializing and destroying WSA
 */
class WSASingleton {
 private:
  std::mutex mutex_;
  int count_;

  /**
   * @brief Constructor, starts WSA
   */
  WSASingleton() : mutex_(), count_(0) {
    // Initialize Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
      cpen333::perror(std::string("WSAStartup(...) failed with error: ") + std::to_string(result));
    }
    // std::cout << "WSA startup" << std::endl;
  }

  /**
   * @brief Destructor, cleans up WSA
   */
  ~WSASingleton() {
    WSACleanup();
    // std::cout << "WSA cleanup" << std::endl;
  }

 public:
  /**
   * @brief Increments usage count
   */
  void acquire() {
    std::lock_guard<std::mutex> lock(mutex_);
    ++count_;
  }

  /**
   * @brief Decrements usage count
   */
  void release() {
    std::lock_guard<std::mutex> lock(mutex_);
    --count_;
  }

  /**
   * @brief Retrieves usage count
   * @return current usage count (number of clients/servers)
   */
  int usage_count() {
    std::lock_guard<std::mutex> lock(mutex_);
    return count_;
  }

  /**
   * @Brief Meyers' singleton
   * @return singleton instance reference
   */
  static WSASingleton &instance() {
    static WSASingleton guard;
    return guard;
  }
};

} // detail

// forward declaration so client can friend it
class socket_server;

/**
 * @brief Socket client
 *
 * WinSock implementation of a socket client.  The client is
 * NOT connected automatically.  To start the connection,
 * call the open() function.
 */
class socket {
 private:
  std::string server_;
  int port_;
  SOCKET socket_;

  bool open_;
  bool connected_;
  detail::WSASingleton& wsa_;

  friend class socket_server;

  /**
   * @brief Initialize a socket with provided info
   *
   * For use by the socket server when creating socket
   *
   * @param server server name
   * @param port port number
   * @param socket socket identifier
   * @param open whether the socket is open
   * @param connected whether the socket is connected for sending
   */
  void __initialize(const std::string& server, int port,
                    SOCKET socket, bool open, bool connected) {
    server_ = server;
    port_ = port;
    socket_ = socket;
    open_ = open;
    connected_ = connected;
  }

  /**
   * @brief Disconnect the socket for sending data (can still receive)
   * @return true if successful, false if failed
   */
  bool disconnect() {

    if (!connected_) {
      return false;
    }

    // shutdown the connection since no more data will be sent
    int result = ::shutdown(socket_, SD_SEND);
    if (result == SOCKET_ERROR) {
      cpen333::perror(std::string("shutdown(...) failed with error: ")
                         + std::to_string(WSAGetLastError()));
      return false;
    }
    connected_ = false;
    return true;
  }

 public:
  /**
   * @brief Default constructor, connects to localhost at the default port
   */
  socket() : server_("localhost"), port_(CPEN333_SOCKET_DEFAULT_PORT),
                    socket_(INVALID_SOCKET), open_(false), connected_(false),
                    wsa_(detail::WSASingleton::instance()){
    wsa_.acquire();
  }

  /**
   * @brief Constructor specifying server address and port
   * @param server server address
   * @param port  port number
   */
  socket(const std::string& server, int port) :
      server_(server), port_(port),
      socket_(INVALID_SOCKET), open_(false), connected_(false),
      wsa_(detail::WSASingleton::instance()) {
    wsa_.acquire();
  }

 private:
  socket(const socket &) DELETE_METHOD;
  socket &operator=(const socket &) DELETE_METHOD;

 public:
  /**
   * @brief Move-constructor
   * @param other socket to move to this
   */
  socket(socket&& other) : wsa_(detail::WSASingleton::instance()){
    *this = std::move(other);
  }

  /**
   * @brief Move-assignment
   * @param other socket to move to this
   * @return reference to this
   */
  socket &operator=(socket&& other) {
    __initialize(other.server_, other.port_, other.socket_, other.open_, other.connected_);
    other.server_ = "";
    other.port_ = 0;
    other.socket_ = INVALID_SOCKET;
    other.open_ = false;
    other.connected_ = false;
    return *this;
  }
  
  /**
   * @brief Destructor, closes socket if not already closed
   */
  ~socket() {
    close();
    wsa_.release();
  }

  /**
   * @brief Opens socket if not already open, attempts to connect
   * to server.
   *
   * @return true if connection established, false otherwise
   */
  bool open() {

    // don't open if already opened
    if (open_) {
      return false;
    }

    // don't open if illegal port
    if (port_ <= 0) {
      return false;
    }

    /* Obtain address(es) matching host/port */
    struct addrinfo hints;
    ZeroMemory( &hints, sizeof(hints) );
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    struct addrinfo *addrresult;
    std::string strport = std::to_string(port_);
    int status = getaddrinfo(server_.c_str(), strport.c_str(),
                             &hints, &addrresult);
    if (status != 0) {
      cpen333::perror(std::string("getaddrinfo(...) failed with error: ")
                         + std::to_string(status));
      return false;
    }

    // Attempt to connect to an address until one succeeds
    for (struct addrinfo* ptr = addrresult; ptr != NULL; ptr = ptr->ai_next) {

      // Create a SOCKET for connecting to server
      socket_ = ::socket(ptr->ai_family, ptr->ai_socktype,
                             ptr->ai_protocol);

      if (socket_ == INVALID_SOCKET) {
        continue;
      }

      // Connect to server.
      status = ::connect( socket_, ptr->ai_addr, (int)ptr->ai_addrlen);
      if (status != SOCKET_ERROR) {
        break;
      }

      // failed to connect, close socket
      closesocket(socket_);
      socket_ = INVALID_SOCKET;
    }
    freeaddrinfo(addrresult);

    if (socket_ == INVALID_SOCKET) {
      cpen333::perror(std::string("Unable to connect to server: ")
                         + server_ + std::string(":") + strport);
      return false;
    }

    open_ = true;
    connected_ = true;
    return true;
  }

  /**
   * @brief Sends string through the socket, including the terminating zero
   *
   * This is potentially a blocking operation: if the socket buffer is full, this
   * method will wait until there is room to write the entire contents of the string.
   *
   * @param str string to send, length+1 must fit into a signed integer
   * @return true if send successful, false otherwise
   */
  bool write(const std::string& str) {
    return write(str.c_str(), (int)(str.length()+1));
  }

  /**
   * @brief Sends bytes through the socket
   *
   * This is potentially a blocking operation: if the socket buffer is full, this
   * method will wait until there is room to write the entire contents.
   *
   * @param buff pointer to data buffer to send
   * @param size number of bytes to send
   * @return true if send successful, false otherwise
   */
  bool write(const void* buff, int size) {

    if (!connected_) {
      return false;
    }

    int status = ::send( socket_, (char*)buff, (int)size, 0 );
    if (status == SOCKET_ERROR) {
      cpen333::perror(std::string("send(...) failed with error: ")
                         + std::to_string(WSAGetLastError()));
      return false;
    }
    return true;
  }

  /**
   * @brief Receives bytes of data from a socket
   *
   * This is a blocking operation: if there is no data, this
   * method will wait until some becomes available or until the socket is closed.
   *
   * @param buff pointer to data buffer to populate
   * @param size size of buffer
   * @return number of bytes read, 0 if closed, or -1 if error
   */
  int read(void* buff, int size) {

    if (!open_) {
      return -1;
    }

    int result = recv(socket_, (char*)buff, size, 0);
    if (result == -1) {
      cpen333::perror(std::string("recv(...) failed with error: ")
                         + std::to_string(WSAGetLastError()));
    }
    return result;
  }

  /**
   * @brief Closes the socket
   * @return true if successful, false otherwise
   */
  bool close() {

    if (!open_) {
      return false;
    }
    if (connected_) {
      disconnect();
    }

    // cleanup
    closesocket(socket_);
    socket_ = INVALID_SOCKET;
    open_ = false;
    return true;
  }

};


/**
 * @brief Socket server
 *
 * WinSock implementation of a socket server that listens
 * for connections.  The server is NOT started automatically.
 * To start listening for connections, call the start() function.
 */
class socket_server {
  int port_;
  SOCKET socket_;
  bool open_;
  detail::WSASingleton& wsa_;

 public:
  /**
   * @brief Default constructor, creates a server that listens on the default
   * port.
   */
  socket_server() : port_(CPEN333_SOCKET_DEFAULT_PORT), socket_(INVALID_SOCKET),
                    open_(false), wsa_(detail::WSASingleton::instance()) {
    wsa_.acquire();
  }

 private:
  socket_server(const socket_server &) DELETE_METHOD;
  socket_server(socket_server &&) DELETE_METHOD;
  socket_server &operator=(const socket_server &) DELETE_METHOD;
  socket_server &operator=(socket_server &&) DELETE_METHOD;

 public:

  /**
   * @brief Constructor, creates a server that listens on the provided port
   *
   * If the port is 0, then finds an open port.  The port number can
   * be quieried with get_port()
   *
   * @param port port number to listen for connections
   */
  socket_server(int port) : port_(port), socket_(INVALID_SOCKET),
                            open_(false), wsa_(detail::WSASingleton::instance()) {
    wsa_.acquire();
  }

  /**
   * @brief Destructor, closes the server socket
   */
  ~socket_server() {
    close();
    wsa_.release();
  }

  /**
   * @brief Starts listening for connections.
   * @return true if successful, false otherwise.
   */
  bool start() {

    if (open_){
      return false;
    }

    struct addrinfo hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    struct addrinfo *addrresult = NULL;
    std::string strport = std::to_string(port_);
    int status = getaddrinfo(NULL, strport.c_str(), &hints, &addrresult);
    if ( status != 0 ) {
      cpen333::perror(std::string("getaddrinfo(...) failed with error: ")
                         + std::to_string(status));
      return false;
    }

    // Create a SOCKET for connecting to server
    socket_ = ::socket(addrresult->ai_family, addrresult->ai_socktype,
                     addrresult->ai_protocol);
    if (socket_ == INVALID_SOCKET) {
      cpen333::perror(std::string("socket(...) failed with error: ")
                         + std::to_string(WSAGetLastError()));
      freeaddrinfo(addrresult);
      return false;
    }

    // Setup the TCP listening socket
    status = bind( socket_, addrresult->ai_addr, (int)addrresult->ai_addrlen);
    if (status == SOCKET_ERROR) {
      cpen333::perror(std::string("bind(...) failed with error: ")
                         + std::to_string(WSAGetLastError()));
      freeaddrinfo(addrresult);
      closesocket(socket_);
      socket_ = INVALID_SOCKET;
      return false;
    }
    freeaddrinfo(addrresult);

    if (port_ == 0) {
      struct sockaddr_in sin;
      int addrlen = sizeof(sin);
      status = getsockname(socket_, (struct sockaddr *)&sin, &addrlen);
      if(status == 0 ) {
        port_ = ntohs(sin.sin_port);
      } else {
        cpen333::perror(std::string("getsockname(...) failed with error: ")
                           + std::to_string(status));
      }
    }

    // start listening
    status = listen(socket_, SOMAXCONN);
    if (status == SOCKET_ERROR) {
      cpen333::perror(std::string("listen(...) failed with error: ")
                         + std::to_string(WSAGetLastError()));
      closesocket(socket_);
      socket_ = INVALID_SOCKET;
      return false;
    }

    open_ = true;
    return true;
  }

  /**
   * @brief Accepts a client connection
   *
   * This method will block until a client connects to the server.
   * Upon connection, the provide client is populated and automatically
   * opened.
   *
   * @param client connected socket client
   * @return true if successful, false otherwise
   */
  bool accept(socket& client) {
    if (!open_) {
      return false;
    }

    SOCKET client_socket = INVALID_SOCKET;

    // Accept a client socket
    client_socket = ::accept(socket_, NULL, NULL);
    if (client_socket == INVALID_SOCKET) {
      cpen333::perror(std::string("accept(...) failed with error: ")
                         + std::to_string(WSAGetLastError()));
      return false;
    }

    client.close();
    client.__initialize("", -1, client_socket, true, true);

    return true;
  }

  /**
   * @brief Close the server socket
   * @return true if successful, false otherwise
   */
  bool close() {
    if (!open_) {
      return false;
    }

    closesocket(socket_);
    socket_ = INVALID_SOCKET;
    open_ = false;
    return true;
  }

  /**
   * @brief Retries the server port
   * @return port number that server is listening on for connections
   */
  int port() {
    return port_;
  }

  /**
   * @brief Looks up an address path based on the local host name
   * @return vector of addresses if found
   */
  static std::vector<std::string> address_lookup() {

    detail::WSASingleton& wsa = detail::WSASingleton::instance();
    wsa.acquire();

    std::vector<std::string> out;

    // host name
    char ac[80];
    if (gethostname(ac, sizeof(ac)) == SOCKET_ERROR) {
      wsa.release();
      return out;
    }
    std::string hostname(ac);

	/* Obtain address(es) matching host/port */
	struct addrinfo hints;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	struct addrinfo *addrresult;
	int status = getaddrinfo(hostname.c_str(), NULL,
		&hints, &addrresult);

    if (status != 0) {
      wsa.release();
      return out;
    }

    // loop through addresses
	char ipbuf[INET_ADDRSTRLEN];
	// Attempt to connect to an address until one succeeds
	for (struct addrinfo* ptr = addrresult; ptr != NULL; ptr = ptr->ai_next) {
		out.push_back(inet_ntop(AF_INET, &((struct sockaddr_in *)ptr->ai_addr)->sin_addr, ipbuf, sizeof(ipbuf)));
	}
	freeaddrinfo(addrresult);
	
    wsa.release();
    return out;
  }

};

} // windows

/**
 * @brief Windows implementation of a socket client
 */
typedef windows::socket socket;

/**
 * @brief Windows implementation of a socket server
 */
typedef windows::socket_server socket_server;

} // process
} // cpen333

#endif