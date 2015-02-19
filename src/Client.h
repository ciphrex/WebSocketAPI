////////////////////////////////////////////////////////////////////////////////
//
// Client.h
//
// Copyright (c) 2014 Eric Lombrozo, all rights reserved
//

#pragma once

#include "JsonRpc.h"

#if defined(USE_TLS)
    #include <websocketpp/config/asio_client.hpp>
#else
    #include <websocketpp/config/asio_no_tls_client.hpp>
#endif

#include <websocketpp/client.hpp>

#include <iostream>
#include <sstream>
#include <map>

namespace WebSocket
{

#if defined(USE_TLS)
    class ClientTls;
    typedef ClientTls Client;
    typedef websocketpp::client<websocketpp::config::asio_tls_client>   client_t;
    typedef websocketpp::config::asio_tls_client::message_type::ptr     message_ptr_t;
    typedef websocketpp::lib::shared_ptr<boost::asio::ssl::context>     context_ptr;

    typedef std::function<context_ptr(websocketpp::connection_hdl)>     tls_init_callback_t;
#else
    class ClientNoTls;
    typedef ClientNoTls Client;
    typedef websocketpp::client<websocketpp::config::asio_client>       client_t;
    typedef websocketpp::config::asio_client::message_type::ptr         message_ptr_t;
#endif

typedef client_t::connection_ptr                                    connection_ptr_t;
typedef websocketpp::connection_hdl                                 connection_hdl_t;
typedef websocketpp::lib::error_code                                error_code_t;

typedef std::function<void(const json_spirit::Value&)> ResultCallback;
typedef std::function<void(const json_spirit::Value&)> ErrorCallback;
typedef std::pair<ResultCallback, ErrorCallback> CallbackPair;
typedef std::map<uint64_t, CallbackPair> CallbackMap;

typedef std::function<void(const json_spirit::Value&)> EventHandler;
typedef std::map<std::string, EventHandler> EventHandlerMap;
 
typedef std::function<void()> OpenHandler;
typedef std::function<void()> CloseHandler;
typedef std::function<void(const std::string&)> LogHandler;
typedef std::function<void(const std::string&)> ErrorHandler;

#if defined(USE_TLS)
class ClientTls
#else
class ClientNoTls
#endif
{
public:
    // Constructor / Destructor
#if defined(USE_TLS)
    ClientTls(const std::string& event_field, const std::string& data_field = "");
    ~ClientTls();
#else
    ClientNoTls(const std::string& event_field, const std::string& data_field = "");
    ~ClientNoTls();
#endif

    void setResultField(const std::string& result_field) { this->result_field = result_field; }
    void setErrorField(const std::string& error_field) { this->error_field = error_field; }
    void setIdField(const std::string& id_field) { this->id_field = id_field; }

    void returnFullResponse(bool bReturnFullResponse) { this->bReturnFullResponse = bReturnFullResponse; }

    // start() blocks until disconnection occurs.
    void start(const std::string& serverUrl, OpenHandler on_open = nullptr, CloseHandler on_close = nullptr, LogHandler on_log = nullptr, ErrorHandler on_error = nullptr);
    void stop();

    // Send formatted commands
    void send(const json_spirit::Object& cmd, ResultCallback resultCallback = nullptr, ErrorCallback errorCallback = nullptr);
    void send(const JsonRpc::Request& request, ResultCallback resultCallback = nullptr, ErrorCallback errorCallback = nullptr);

    // Subscribe to events
    Client& on(const std::string& eventType, EventHandler handler);

#if defined(USE_TLS)
    void setTlsInitCallback(tls_init_callback_t callback) { on_tls_init = callback; }
#endif

protected:
    // Connection handlers
    void onOpen(connection_hdl_t hdl);
    void onClose(connection_hdl_t hdl);
    void onFail(connection_hdl_t hdl);
    void onMessage(connection_hdl_t, message_ptr_t msg);
#if defined(USE_TLS)
    context_ptr onTlsInit(connection_hdl_t hdl);
#endif

    // Results and errors from commands
    void onResult(const json_spirit::Value& result, uint64_t id);
    void onError(const json_spirit::Value& error, uint64_t id); 

private:
    // WebSocket connection to CoinSocket server
    client_t            client;
    std::string         serverUrl;
    connection_ptr_t    pConnection;
    bool                bConnected;

    OpenHandler         on_open;
    CloseHandler        on_close;
    LogHandler          on_log;
    ErrorHandler        on_error;

#if defined(USE_TLS)
    tls_init_callback_t on_tls_init;
#endif

    std::string         event_field;
    std::string         data_field;
    EventHandlerMap     event_handler_map;

    std::string         result_field;           // default: "result"
    std::string         error_field;            // default: "error"
    std::string         id_field;               // default: "id"

    bool                bReturnFullResponse;    // default: false
    uint64_t            sequence;
    CallbackMap         callback_map;
};

} 
