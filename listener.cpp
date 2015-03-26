#include "Listener.h"

#include <cstdlib>
#include <iostream>

using namespace std;

Listener* l = 0;
int gEnd=0;

void CtrlC(int aSig) {
  if (gEnd==0) {
    cout << endl << "[C-c] Trying a clean exit!" << endl;
    if (l) l->Disconnect();
    exit(0);
  }
  else if (gEnd>=5) {
    cout << endl << "[C-c > 5 times] ... Forcing exit!" << endl;
    exit(0);
  }
  gEnd++;
}

int main(int argc, char* argv[])
{
  if (argc<2) return -1;
  signal(SIGINT, CtrlC);
  
  const int port = atoi(argv[1]);
  cout << "Starting to listen on port " << port << endl;

  l = new Listener(port);
  if (!l->Connect()) {
    cout << "Failed to connect the listener" << endl;
    return -1;
  }

  while (true) {
    //Listener ll;
    try {
      //l.AcceptConnections(ll);
      l->Receive();
    } catch (Exception& e) {
      e.Dump();
    }
  }

  delete l;
  return 0;
}
