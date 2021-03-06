#include "Messenger.h"

Messenger::Messenger() :
  Socket(-1), fWS(0), fNumAttempts(0)
{}

Messenger::Messenger(int port) :
  Socket(port), fWS(0), fNumAttempts(0)
{
  std::cout << __PRETTY_FUNCTION__ << " new Messenger at port " << GetPort() << std::endl;
  fWS = new WebSocket;
}

Messenger::~Messenger()
{
  Disconnect();
  if (fWS) delete fWS;
}

bool
Messenger::Connect()
{
  try {
    Start();
    Bind();
    Listen(20);
  } catch (Exception& e) {
    e.Dump();
    return false;
  }
  return true;
}

void
Messenger::Disconnect()
{
  if (fPort<0) return; // do not broadcast the death of a secondary messenger!
  try {
    Broadcast(SocketMessage(MASTER_DISCONNECT, ""));
  } catch (Exception& e) {
    e.Dump();
    throw Exception(__PRETTY_FUNCTION__, "Failed to broadcast the server disconnection status!", JustWarning, SOCKET_ERROR(errno));
  }
  Stop();
}

void
Messenger::AddClient()
{
  Socket s;
  try {
    AcceptConnections(s);
    Message message = FetchMessage(s.GetSocketId());
    if (message.IsFromWeb()) {
      message.Dump();
      // Feed the handshake to the WebSocket object
      fWS->parseHandshake((unsigned char*)message.GetString().c_str(), message.GetString().size());
      Send(Message(fWS->answerHandshake()), s.GetSocketId());
      // From now on handle a WebSocket connection
      SwitchClientType(s.GetSocketId(), WEBSOCKET_CLIENT);
    }
    else {
      // if not a WebSocket connection
      SocketMessage m(message);
      if (m.GetKey()==ADD_CLIENT) {
        SocketType type = static_cast<SocketType>(m.GetIntValue());
        if (type!=CLIENT) SwitchClientType(s.GetSocketId(), type);
      }
    }
    // Send the client's unique identifier
    Send(SocketMessage(SET_CLIENT_ID, s.GetSocketId()), s.GetSocketId());
    //SocketMessage(SET_CLIENT_ID, s.GetSocketId()).Dump();
  } catch (Exception& e) {
    e.Dump();
  }
}

void
Messenger::DisconnectClient(int sid, MessageKey key, bool force)
{
  SocketType type;
  try {
    type = GetSocketType(sid);
  } catch (Exception& e) {
    e.Dump();
    return;
  }
  std::ostringstream o; o << "Disconnecting client # " << sid;
  if (type==WEBSOCKET_CLIENT) o << " (web socket)";
  PrintInfo(o.str());
  
  if (type==WEBSOCKET_CLIENT) {
    try {
      Send(SocketMessage(key, sid), sid);
    } catch (Exception& e) {
      e.Dump();
      if (e.ErrorNumber()==10032 or force) {
        fSocketsConnected.erase(std::pair<int,SocketType>(sid, type));
        FD_CLR(sid, &fMaster);
        return;
      }
    }
  }
  else {
    Socket s;
    s.SetSocketId(sid);
    s.Stop();
  }
  fSocketsConnected.erase(std::pair<int,SocketType>(sid, type));
  FD_CLR(sid, &fMaster);
}

void
Messenger::SwitchClientType(int sid, SocketType type)
{
  SocketType oldtype;
  try {
    oldtype = GetSocketType(sid);
    fSocketsConnected.erase (std::pair<int,SocketType>(sid, oldtype));
    fSocketsConnected.insert(std::pair<int,SocketType>(sid, type));    
  } catch (Exception& e) {
    e.Dump();
  }
}

void
Messenger::Send(const Message& m, int sid) const
{
  bool ws = false;
  try {
    ws = IsWebSocket(sid);
    Message tosend = (ws) ? HTTPMessage(fWS, m, EncodeMessage) : m;
    std::cout << "Sending a message to socket # " << sid << " (WS? " << ws << "):" << std::endl;
    tosend.Dump();
    SendMessage(tosend, sid);
  } catch (Exception& e) {
    e.Dump();
  }
}

void
Messenger::Receive()
{
  Message msg;
  SocketMessage m;
  
  // We start by copying the master file descriptors list to the
  // temporary list for readout
  fReadFds = fMaster;
  
  try {
    SelectConnections();
  } catch (Exception& e) {
    e.Dump();
    throw Exception(__PRETTY_FUNCTION__, "Impossible to select the connections!", Fatal);
  }
  
  // Looking for something to read!
  for (SocketCollection::const_iterator s=fSocketsConnected.begin(); s!=fSocketsConnected.end(); s++) {
    if (!FD_ISSET(s->first, &fReadFds)) continue;
    
    // First check if we need to handle new connections
    if (s->first==GetSocketId()) { AddClient(); return; }
    
    // Handle data from a client
    try { msg = FetchMessage(s->first); } catch (Exception& e) {
      if (e.ErrorNumber()==11000) { DisconnectClient(s->first, THIS_CLIENT_DELETED); return; }
    }
    if (s->second==WEBSOCKET_CLIENT) {
      HTTPMessage h_msg(fWS, msg, DecodeMessage);
      try { m = SocketMessage(h_msg); } catch (Exception& e) {;}
    }
    else m = SocketMessage(msg.GetString());
    // Message was successfully decoded
    fNumAttempts = 0;
    
    try {
      ProcessMessage(m, s->first);
    } catch (Exception& e) {
      if (e.ErrorNumber()==11001) break;
    }
  }
}

void
Messenger::ProcessMessage(SocketMessage m, int sid)
{
  if (m.GetKey()==REMOVE_CLIENT) {
    MessageKey key;
    
    if (m.GetIntValue()==GetSocketId()) {
      std::ostringstream o;
      o << "Some client (id=" << sid << ") asked for this master's disconnection!"
        << "\n\tIgnoring this request...";
      throw Exception(__PRETTY_FUNCTION__, o.str(), JustWarning);
      return;
    }
    
    key = (sid==m.GetIntValue()) ? THIS_CLIENT_DELETED : OTHER_CLIENT_DELETED;
    DisconnectClient(m.GetIntValue(), key);
    throw Exception(__PRETTY_FUNCTION__, "Removing socket client", Info, 11001);
  }
  else if (m.GetKey()==PING_CLIENT) {
    int toping = m.GetIntValue();
    Send(SocketMessage(PING_CLIENT), toping);
    SocketMessage msg; int i=0;
    do { msg = FetchMessage(toping); i++; } while (msg.GetKey()!=PING_ANSWER && i<MAX_SOCKET_ATTEMPTS);
    Send(SocketMessage(PING_ANSWER, msg.GetValue()), sid);
  } 
  else if (m.GetKey()==GET_CLIENTS) {
    int i = 0;
    std::ostringstream os;
    for (SocketCollection::const_iterator it=fSocketsConnected.begin(); it!=fSocketsConnected.end(); it++, i++) {
      if (i!=0) os << ";";
      os << it->first << " (type " << static_cast<int>(it->second) << ")";
    }
    Send(SocketMessage(CLIENTS_LIST, os.str()), sid); 
  }
  else if (m.GetKey()==WEB_GET_CLIENTS) {
    int i = 0; SocketType type;
    std::ostringstream os;
    for (SocketCollection::const_iterator it=fSocketsConnected.begin(); it!=fSocketsConnected.end(); it++, i++) {
      type = (it->first==GetSocketId()) ? MASTER : it->second;
      if (i!=0) os << ";";
      os << it->first << ",";
      if (it->first==GetSocketId()) os << "Master,";
      else os << "Client" << it->first << ",";
      os << static_cast<int>(type);
    }
    try {
      Send(SocketMessage(CLIENTS_LIST, os.str()), sid);
    } catch (Exception& e) {
      e.Dump();
    }
  }
  else if (m.GetKey()==GET_RUN_NUMBER) {
    try {
      Send(SocketMessage(RUN_NUMBER, static_cast<int>(time(NULL))), sid);
    } catch (Exception& e) { e.Dump(); }
  }
  else {
    try { Send(SocketMessage(INVALID_KEY), sid); } catch (Exception& e) { e.Dump(); }
    std::ostringstream o;
    o << "Received an invalid message: " << m.GetString();
    throw Exception(__PRETTY_FUNCTION__, o.str(), JustWarning);
  }
}

void
Messenger::Broadcast(const Message& m) const
{
  try {
    for (SocketCollection::const_iterator sid=fSocketsConnected.begin(); sid!=fSocketsConnected.end(); sid++) {
      if (sid->first==GetSocketId()) continue;
      Send(m, sid->first);
    }
  } catch (Exception& e) {
    e.Dump();
  }
}
