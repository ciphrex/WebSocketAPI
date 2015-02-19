////////////////////////////////////////////////////////////////////////////////
//
// CoinSocketClientTest.cpp
//
// Copyright (c) 2014 Eric Lombrozo, all rights reserved
//

#include <Client.h>

#include <iostream>

using namespace std;
using namespace json_spirit;

#if defined(USE_TLS)
WebSocket::context_ptr newTlsContext(WebSocket::connection_hdl_t hdl)
{
    WebSocket::context_ptr ctx(new boost::asio::ssl::context(boost::asio::ssl::context::tlsv1));

    try
    {
        ctx->set_options(boost::asio::ssl::context::default_workarounds |
                         boost::asio::ssl::context::no_sslv2 |
                         boost::asio::ssl::context::no_sslv3 |
                         boost::asio::ssl::context::single_dh_use);
    }
    catch (const std::exception& e)
    {
        cout << "TLS initialization failed - " << e.what() << endl;
    }

    return ctx;
}
#endif

int main(int argc, char** argv)
{
    if (argc != 2) {
        cout << "Usage: " << argv[0] << " <server url>" << endl;
        return 1;
    }

    try
    {
        WebSocket::Client socket("event", "data");

#if defined(USE_TLS)
        socket.setTlsInitCallback(&newTlsContext);
#endif

        socket.on("statuschanged", [](const Value& data) {
            cout << "statuschanged: " << write_string<Value>(data, true) << endl << endl;
        }).on("txinserted", [](const Value& data) {
            cout << "txinserted: " << write_string<Value>(data, true) << endl << endl;
        }).on("txstatuschanged", [](const Value& data) {
            cout << "txstatuschanged: " << write_string<Value>(data, true) << endl << endl;
        });

        socket.start(argv[1], [&]() {
            JsonRpc::Request getvaultinfo("getvaultinfo");
            socket.send(getvaultinfo, [](const Value& result) {
                cout << "Command result: " << write_string<Value>(result, true) << endl << endl;
            }, [](const Value& error) {
                cout << "Command error: " << write_string<Value>(error, true) << endl << endl; 
            });

            Array params;
            params.push_back("all");
            JsonRpc::Request subscribe("subscribe", params); 
            socket.send(subscribe, [](const Value& result) {
                cout << "Command result: " << write_string<Value>(result, true) << endl << endl;
            }, [](const Value& error) {
                cout << "Command error: " << write_string<Value>(error, true) << endl << endl; 
            });
        }, []() {
            cout << "Connection closed." << endl << endl;
        }, [](const string& msg) {
            // cout << "Log: " << msg << endl << endl;
        }, [](const string& msg) {
            cout << "Error: " << msg << endl << endl;
        });

        return 0;
    }
    catch (const std::exception& e)
    {
        cout << "Exception: " << e.what() << endl;
        return 2;
    }
}
