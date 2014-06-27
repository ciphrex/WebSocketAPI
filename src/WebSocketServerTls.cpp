///////////////////////////////////////////////////////////////////////////////
//
// WebSocketServerTls.cpp
//
// Copyright (c) 2013 Eric Lombrozo
//
// All Rights Reserved.

#include "WebSocketServerTls.h"
#include "JsonRpc.h"

#include <boost/lexical_cast.hpp>

using namespace WebSocket;

bool ServerTls::onValidate(websocketpp::connection_hdl hdl)
{
    std::cout << "ServerTls::onValidate()" << std::endl;
    ws_server_t::connection_ptr con = m_ws_server.get_con_from_hdl(hdl);
    std::string remote_endpoint = boost::lexical_cast<std::string>(con->get_remote_endpoint());
    std::cout << "Remote endpoint: " << remote_endpoint << std::endl;
    if (boost::regex_match(remote_endpoint, m_allow_ips_regex)) {
        std::cout << "Validation successful." << std::endl;
        return true;
    }
    else {
        std::cout << "Validation failed." << std::endl;
        return false;
    }
}

void ServerTls::onOpen(websocketpp::connection_hdl hdl)
{
    std::cout << "ServerTls::onOpen() called with hdl: " << hdl.lock().get() << std::endl;
    {
        boost::unique_lock<boost::mutex> lock(m_connectionMutex);
        m_connections.insert(hdl);
    }
    if (m_openCallback) { m_openCallback(*this, hdl); }
}

void ServerTls::onClose(websocketpp::connection_hdl hdl)
{
    std::cout << "ServerTls::onClose() called with hdl: " << hdl.lock().get() << std::endl;
    {
        boost::unique_lock<boost::mutex> lock(m_connectionMutex);
        m_connections.erase(hdl);
    }
    if (m_closeCallback) { m_closeCallback(*this, hdl); }
}

ServerTls::context_ptr ServerTls::onTlsInit(websocketpp::connection_hdl hdl)
{
    std::cout << "ServerTls::onTlsInit() called with hdl: " << hdl.lock().get() << std::endl;
    if (m_tlsInitCallback) { return m_tlsInitCallback(*this, hdl); }
    return context_ptr();
}

void ServerTls::onMessage(websocketpp::connection_hdl hdl, ws_server_t::message_ptr msg)
{
    std::cout << "ServerTls::onMessage() called with hdl: " << hdl.lock().get()
              << " and message: " << msg->get_payload()
              << std::endl;

    std::stringstream err;

    try {
        JsonRpc::Request request(msg->get_payload());
        boost::unique_lock<boost::mutex> lock(m_requestMutex);
        m_requests.push(std::make_pair(hdl, request));
        lock.unlock();
        m_requestCond.notify_one();
    }
    catch (const stdutils::custom_error& e) {
        JsonRpc::Response response;
        response.setError(e);
        m_ws_server.send(hdl, response.getJson(), msg->get_opcode());
    }
    catch (const std::exception& e) {
        JsonRpc::Response response;
        response.setError(e);
        m_ws_server.send(hdl, response.getJson(), msg->get_opcode());
    }
}

void ServerTls::requestLoop()
{
    while (true) {
        boost::unique_lock<boost::mutex> lock(m_requestMutex);

        while (m_bRunning && m_requests.empty()) {
            m_requestCond.wait(lock);
        }

        if (!m_bRunning) break;

        client_request_t req = m_requests.front();
        m_requests.pop();

        lock.unlock();

        try {
            if (m_requestCallback) {
                m_requestCallback(*this, req);
            }
            else {
                m_ws_server.send(req.first, "Client request callback not set.", websocketpp::frame::opcode::text);
            }
        }
        catch (const std::exception& e) {
            std::cout << "ServerTls::requestLoop() - Error: " << e.what() << std::endl;
        }
    }
}

void ServerTls::init(int port, const std::string& allow_ips)
{
    m_port = port;
    m_bRunning = false;
    m_openCallback = nullptr;
    m_requestCallback = nullptr;
    try {
        m_allow_ips_regex.assign(allow_ips);
    }
    catch (const boost::regex_error& e) {
        std::cout << std::endl << "WARNING: Invalid allowips regex. Allowing localhost only." << std::endl;
        m_allow_ips_regex.assign(DEFAULT_ALLOWED_IPS);
    }

    m_ws_server.set_access_channels(websocketpp::log::alevel::all);
    m_ws_server.clear_access_channels(websocketpp::log::alevel::frame_payload);

    m_ws_server.init_asio();

    m_ws_server.set_validate_handler(websocketpp::lib::bind(&ServerTls::onValidate, this, websocketpp::lib::placeholders::_1));
    m_ws_server.set_open_handler(websocketpp::lib::bind(&ServerTls::onOpen, this, websocketpp::lib::placeholders::_1));
    m_ws_server.set_close_handler(websocketpp::lib::bind(&ServerTls::onClose, this, websocketpp::lib::placeholders::_1));
    m_ws_server.set_tls_init_handler(websocketpp::lib::bind(&ServerTls::onTlsInit, this, websocketpp::lib::placeholders::_1));
    m_ws_server.set_message_handler(websocketpp::lib::bind(&ServerTls::onMessage, this, websocketpp::lib::placeholders::_1, websocketpp::lib::placeholders::_2));
}

void ServerTls::start()
{
    boost::unique_lock<boost::mutex> lock(m_startMutex);
    if (m_bRunning) {
        throw std::runtime_error("Server already started.");
    }

    m_bRunning = true;

    m_ws_server.listen(m_port);
    m_ws_server.start_accept();

    m_request_loop_thread   = boost::thread(websocketpp::lib::bind(&ServerTls::requestLoop, this));
    m_io_service_thread     = boost::thread(websocketpp::lib::bind(&ws_server_t::run, &m_ws_server));
}

void ServerTls::stop()
{
    boost::unique_lock<boost::mutex> lock(m_startMutex);
    m_bRunning = false;
    lock.unlock();

    std::cout << "Websocket server stopping request loop thread..." << std::flush;
    m_requestCond.notify_all();
    m_request_loop_thread.join();
    std::cout << "Done." << std::endl;

    std::cout << "Websocket server stopping io service thread..." << std::flush;
    m_ws_server.stop();
    m_io_service_thread.join();
    std::cout << "Done." << std::endl;
}

std::string ServerTls::getRemoteEndpoint(websocketpp::connection_hdl hdl)
{
    ws_server_t::connection_ptr con = m_ws_server.get_con_from_hdl(hdl);
    return boost::lexical_cast<std::string>(con->get_remote_endpoint());
}

void ServerTls::send(websocketpp::connection_hdl hdl, const JsonRpc::Response& res)
{
    boost::unique_lock<boost::mutex> lock(m_connectionMutex);
    if (m_connections.count(hdl) == 0) return;
    m_ws_server.send(hdl, res.getJson(), websocketpp::frame::opcode::text);
}

void ServerTls::sendAll(const JsonRpc::Response& res)
{
    boost::unique_lock<boost::mutex> lock(m_connectionMutex);
    for (auto& hdl: m_connections)
    {
        m_ws_server.send(hdl, res.getJson(), websocketpp::frame::opcode::text);
    }
}

void ServerTls::send(websocketpp::connection_hdl hdl, const std::string& data)
{
    boost::unique_lock<boost::mutex> lock(m_connectionMutex);
    if (m_connections.count(hdl) == 0) return;
    m_ws_server.send(hdl, data, websocketpp::frame::opcode::text);
}

void ServerTls::sendAll(const std::string& data)
{
    boost::unique_lock<boost::mutex> lock(m_connectionMutex);
    for (auto& hdl: m_connections)
    {
        m_ws_server.send(hdl, data, websocketpp::frame::opcode::text);
    }
}
