#ifndef MessageKeys_h
#define MessageKeys_h

#include "messages.h"

/// This is where we define the list of available socket messages to be
/// sent/received by any actor.

MESSAGES_ENUM(\
  // generic messages
  INVALID_KEY, WEBSOCKET_KEY,\
  
  // client messages
  ADD_CLIENT, REMOVE_CLIENT, GET_CLIENTS, CLIENT_TYPE, PING_CLIENT,\
  GET_RUN_NUMBER,\
  
  // master messages
  MASTER_BROADCAST, MASTER_DISCONNECT,\
  SET_CLIENT_ID,\
  THIS_CLIENT_DELETED, OTHER_CLIENT_DELETED,\
  CLIENTS_LIST, GET_CLIENT_TYPE, PING_ANSWER,\
  RUN_NUMBER,\
  
  // web socket messages
  WEB_GET_CLIENTS,\
);

#endif

