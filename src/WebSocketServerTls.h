///////////////////////////////////////////////////////////////////////////////
//
// WebSocketServerTls.h 
//
// Copyright (c) 2013-2014 Eric Lombrozo
//
// All Rights Reserved.

#pragma once

#include "JsonRpc.h"

#include <websocketpp/config/asio.hpp>
#include <websocketpp/server.hpp>

#include <boost/thread.hpp>
#include <boost/regex.hpp>

#include <memory>
#include <queue>
#include <set>

namespace WebSocket
{

const std::string DEFAULT_ALLOWED_IPS = "^\\[(::1|::ffff:127\\.0\\.0\\.1)\\].*";

class ServerTls
{
public:
    typedef websocketpp::lib::shared_ptr<boost::asio::ssl::context> context_ptr;
    typedef std::pair<websocketpp::connection_hdl, JsonRpc::Request> client_request_t;

    typedef std::function<void(ServerTls&, websocketpp::connection_hdl)> open_callback_t;
    typedef std::function<void(ServerTls&, websocketpp::connection_hdl)> close_callback_t;
    typedef std::function<context_ptr(ServerTls&, websocketpp::connection_hdl)> tls_init_callback_t;
    typedef std::function<void(ServerTls&, const client_request_t&)> request_callback_t;

    ServerTls(int port, const std::string& allow_ips = DEFAULT_ALLOWED_IPS) { init(port, allow_ips); }
    ServerTls(const std::string& port, const std::string& allow_ips = DEFAULT_ALLOWED_IPS) { init(strtoul(port.c_str(), NULL, 10), allow_ips); }

    void start();
    void stop();

    std::string getRemoteEndpoint(websocketpp::connection_hdl hdl);

    // Send formatted JSON
    void send(websocketpp::connection_hdl hdl, const JsonRpc::Response& res);
    void sendAll(const JsonRpc::Response& res);

    // Send raw text
    void send(websocketpp::connection_hdl hdl, const std::string& data);
    void sendAll(const std::string& data);

    void setOpenCallback(open_callback_t callback) { m_openCallback = callback; }
    void setCloseCallback(close_callback_t callback) { m_closeCallback = callback; }
    void setTlsInitCallback(tls_init_callback_t callback) { m_tlsInitCallback = callback; }
    void setRequestCallback(request_callback_t callback) { m_requestCallback = callback; }

private:
    typedef websocketpp::server<websocketpp::config::asio_tls> ws_server_t;
    ws_server_t m_ws_server;

    typedef std::set<websocketpp::connection_hdl> connection_set_t;
    connection_set_t m_connections;

    int m_port;
    boost::regex m_allow_ips_regex;

    typedef std::queue<client_request_t> request_queue_t;
    request_queue_t m_requests;

    open_callback_t m_openCallback;
    close_callback_t m_closeCallback;
    tls_init_callback_t m_tlsInitCallback;
    request_callback_t m_requestCallback;

    boost::mutex m_requestMutex;
    boost::condition_variable m_requestCond;

    boost::mutex m_connectionMutex;

    bool onValidate(websocketpp::connection_hdl hdl);
    void onOpen(websocketpp::connection_hdl hdl);
    void onClose(websocketpp::connection_hdl hdl);
    context_ptr onTlsInit(websocketpp::connection_hdl hdl);
    void onMessage(websocketpp::connection_hdl hdl, ws_server_t::message_ptr msg);

    void requestLoop();

    bool m_bRunning;

    boost::mutex m_startMutex;

    boost::thread m_request_loop_thread;
    boost::thread m_io_service_thread;

    void init(int port, const std::string& allow_ips);
};

}

