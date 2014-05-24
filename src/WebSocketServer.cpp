///////////////////////////////////////////////////////////////////////////////
//
// WebSocketServer.cpp
//
// Copyright (c) 2013 Eric Lombrozo
//
// All Rights Reserved.

#include "WebSocketServer.h"
#include "JsonRpc.h"

#include <boost/lexical_cast.hpp>

using namespace WebSocket;

bool Server::onValidate(websocketpp::connection_hdl hdl)
{
    std::cout << "Server::onValidate()" << std::endl;
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

void Server::onOpen(websocketpp::connection_hdl hdl)
{
    std::cout << "Server::onOpen() called with hdl: " << hdl.lock().get() << std::endl;
    {
        boost::unique_lock<boost::mutex> lock(m_connectionMutex);
        m_connections.insert(hdl);
    }
    if (m_openCallback) { m_openCallback(*this, hdl); }
}

void Server::onClose(websocketpp::connection_hdl hdl)
{
    std::cout << "Server::onClose() called with hdl: " << hdl.lock().get() << std::endl;
    {
        boost::unique_lock<boost::mutex> lock(m_connectionMutex);
        m_connections.erase(hdl);
    }
    if (m_closeCallback) { m_closeCallback(*this, hdl); }
}

void Server::onMessage(websocketpp::connection_hdl hdl, ws_server_t::message_ptr msg)
{
    std::cout << "Server::onMessage() called with hdl: " << hdl.lock().get()
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
    catch (const std::exception& e) {
        JsonRpc::Response response;
        response.setError(e.what());
        m_ws_server.send(hdl, response.getJson(), msg->get_opcode());
    }
}

void Server::requestLoop()
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
            std::cout << "Server::requestLoop() - Error: " << e.what() << std::endl;
        }
    }
}

void Server::init(int port, const std::string& allow_ips)
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

    m_ws_server.set_validate_handler(websocketpp::lib::bind(&Server::onValidate, this, websocketpp::lib::placeholders::_1));
    m_ws_server.set_open_handler(websocketpp::lib::bind(&Server::onOpen, this, websocketpp::lib::placeholders::_1));
    m_ws_server.set_close_handler(websocketpp::lib::bind(&Server::onClose, this, websocketpp::lib::placeholders::_1));
    m_ws_server.set_message_handler(websocketpp::lib::bind(&Server::onMessage, this, websocketpp::lib::placeholders::_1, websocketpp::lib::placeholders::_2));
}

void Server::start()
{
    boost::unique_lock<boost::mutex> lock(m_startMutex);
    if (m_bRunning) {
        throw std::runtime_error("Server already started.");
    }

    m_bRunning = true;

    m_ws_server.listen(m_port);
    m_ws_server.start_accept();

    m_request_loop_thread   = boost::thread(websocketpp::lib::bind(&Server::requestLoop, this));
    m_io_service_thread     = boost::thread(websocketpp::lib::bind(&ws_server_t::run, &m_ws_server));
}

void Server::stop()
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

void Server::send(websocketpp::connection_hdl hdl, const JsonRpc::Response& res)
{
    boost::unique_lock<boost::mutex> lock(m_connectionMutex);
    if (m_connections.count(hdl) == 0) return;
    m_ws_server.send(hdl, res.getJson(), websocketpp::frame::opcode::text);
}

void Server::sendAll(const JsonRpc::Response& res)
{
    boost::unique_lock<boost::mutex> lock(m_connectionMutex);
    for (auto& hdl: m_connections)
    {
        m_ws_server.send(hdl, res.getJson(), websocketpp::frame::opcode::text);
    }
}

void Server::send(websocketpp::connection_hdl hdl, const std::string& data)
{
    boost::unique_lock<boost::mutex> lock(m_connectionMutex);
    if (m_connections.count(hdl) == 0) return;
    m_ws_server.send(hdl, data, websocketpp::frame::opcode::text);
}

void Server::sendAll(const std::string& data)
{
    boost::unique_lock<boost::mutex> lock(m_connectionMutex);
    for (auto& hdl: m_connections)
    {
        m_ws_server.send(hdl, data, websocketpp::frame::opcode::text);
    }
}
