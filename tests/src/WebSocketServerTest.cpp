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
    JsonRpc::Response response;

    const string& method = req.second.getMethod();
    const json_spirit::Array& params = req.second.getParams();

    try
    {
        if (params.size() != 2)
        {
            throw std::runtime_error("Invalid parameters.");
        }

        uint64_t x = params[0].get_uint64();
        uint64_t y = params[1].get_uint64();
        uint64_t z;

        if (method == "add")
        {
            z = x + y;
        }
        else if (method == "subtract")
        {
            z = x - y;
        }
        else if (method == "multiply")
        {
            z = x * y;
        }
        else if (method == "divide")
        {
            z = x / y;
        }
        else
        {
            throw std::runtime_error("Invalid method.");
        }

        response.setResult(z, req.second.getId());
    }
    catch (const exception& e)
    {
        response.setError(e.what(), req.second.getId());
    }

    server.send(req.first, response);
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
