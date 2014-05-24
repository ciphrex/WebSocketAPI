///////////////////////////////////////////////////////////////////////////////
//
// WebSocketServer.h 
//
// Copyright (c) 2013-2014 Eric Lombrozo
//
// All Rights Reserved.

#pragma once

#include "JsonRpc.h"

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <boost/thread.hpp>
#include <boost/regex.hpp>

#include <memory>
#include <queue>
#include <set>

namespace WebSocket
{

const std::string DEFAULT_ALLOWED_IPS = "^\\[(::1|::ffff:127\\.0\\.0\\.1)\\].*";

class Server
{
public:
    typedef std::pair<websocketpp::connection_hdl, JsonRpc::Request> client_request_t;

    typedef std::function<void(Server&, websocketpp::connection_hdl)> open_callback_t;
    typedef std::function<void(Server&, websocketpp::connection_hdl)> close_callback_t;
    typedef std::function<void(Server&, const client_request_t&)> request_callback_t;

    Server(int port, const std::string& allow_ips = DEFAULT_ALLOWED_IPS) { init(port, allow_ips); }
    Server(const std::string& port, const std::string& allow_ips = DEFAULT_ALLOWED_IPS) { init(strtoul(port.c_str(), NULL, 10), allow_ips); }

    void start();
    void stop();
    void send(websocketpp::connection_hdl hdl, const JsonRpc::Response& res);
    void sendAll(const JsonRpc::Response& res);

    void setOpenCallback(open_callback_t callback) { m_openCallback = callback; }
    void setCloseCallback(close_callback_t callback) { m_closeCallback = callback; }
    void setRequestCallback(request_callback_t callback) { m_requestCallback = callback; }

private:
    typedef websocketpp::server<websocketpp::config::asio> ws_server_t;
    ws_server_t m_ws_server;

    typedef std::set<websocketpp::connection_hdl> connection_set_t;
    connection_set_t m_connections;

    int m_port;
    boost::regex m_allow_ips_regex;

    typedef std::queue<client_request_t> request_queue_t;
    request_queue_t m_requests;

    open_callback_t m_openCallback;
    close_callback_t m_closeCallback;
    request_callback_t m_requestCallback;

    boost::mutex m_requestMutex;
    boost::condition_variable m_requestCond;

    boost::mutex m_connectionMutex;

    bool onValidate(websocketpp::connection_hdl hdl);
    void onOpen(websocketpp::connection_hdl hdl);
    void onClose(websocketpp::connection_hdl hdl);
    void onMessage(websocketpp::connection_hdl hdl, ws_server_t::message_ptr msg);

    void requestLoop();

    bool m_bRunning;

    boost::mutex m_startMutex;

    boost::thread m_request_loop_thread;
    boost::thread m_io_service_thread;

    void init(int port, const std::string& allow_ips);
};

}

