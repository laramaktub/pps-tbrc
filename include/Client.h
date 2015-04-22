#ifndef Client_h
#define Client_h

#include <string>

#include "Socket.h"

/**
 * Client object used by the server to send/receive commands from
 * the messenger/broadcaster.
 * \brief Base client object for the socket
 *
 * \author Laurent Forthomme <laurent.forthomme@cern.ch>
 * \date 24 Mar 2015
 */
class Client : public Socket
{
  public:
    /// General void client constructor
    inline Client() {;}
    /// Bind a socket client to a given port
    Client(int port);
    virtual ~Client();

    /// Bind this client to the socket
    bool Connect();
    /// Unbind this client from the socket
    void Disconnect();
  
    /// Send a message to the master through the socket
    inline void Send(const Message& m) const { SendMessage(m); }
    /// Receive a socket message from the master
    void Receive();
    
    /// Parse a SocketMessage received from the master
    virtual void ParseMessage(const SocketMessage& m) {;}
    /// Socket actor type retrieval method
    virtual SocketType GetType() const { return CLIENT; }
  
  private:
    /// Announce our entry on the socket to its master
    void Announce();
  
    int fClientId;
    bool fIsConnected;
};

#endif