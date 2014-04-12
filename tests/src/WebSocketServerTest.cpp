#include <WebSocketServer.h>

#include <iostream>

#include <signal.h>

using namespace WebSocket;
using namespace std;

const string WS_PORT = "12345";

bool g_bShutdown = false;

void finish(int sig)
{
    cout << "Stopping..." << endl;
    g_bShutdown = true;
}

void openCallback(Server& server, websocketpp::connection_hdl hdl)
{
    cout << "Client " << hdl.lock().get() << " connected." << endl;

    JsonRpc::Response res;
    res.setResult("connected");
    server.send(hdl, res);
}

void closeCallback(Server& server, websocketpp::connection_hdl hdl)
{
    cout << "Client " << hdl.lock().get() << " disconnected." << endl;    
}

void requestCallback(Server& server, const Server::client_request_t& req)
{
    const std::string& method = req.second.getMethod();

    JsonRpc::Response res;
    res.setResult(method);
    server.send(req.first, res);
}

int main()
{
    signal(SIGINT, &finish);

    Server wsServer(WS_PORT);
    wsServer.setOpenCallback(&openCallback);
    wsServer.setCloseCallback(&closeCallback);
    wsServer.setRequestCallback(&requestCallback);

    try
    {
        cout << "Starting websocket server on port " << WS_PORT << "..." << flush;
        wsServer.start();
        cout << "done." << endl;
    }
    catch (const exception& e)
    {
        cout << endl;
        cerr << "Error starting websocket server: " << e.what() << endl;
        return 1;
    }

    while (!g_bShutdown) { usleep(200); }

    cout << "Stopping websocket server...";
    wsServer.stop();
    cout << "done." << endl;

    return 0;
}
