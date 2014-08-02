///////////////////////////////////////////////////////////////////////////////
//
// WebSocketServer.cpp
//
// Copyright (c) 2013-2014 Eric Lombrozo
//
// All Rights Reserved.

#include "WebSocketServer.h"
#include "JsonRpc.h"

#include <boost/lexical_cast.hpp>

using namespace WebSocket;

#if defined(USE_TLS)
bool ServerTls::onValidate(websocketpp::connection_hdl hdl)
#else
bool Server::onValidate(websocketpp::connection_hdl hdl)
#endif
{
    std::cout << "Server::onValidate()" << std::endl;
    ws_server_t::connection_ptr con = m_ws_server.get_con_from_hdl(hdl);
    std::string remote_endpoint = boost::lexical_cast<std::string>(con->get_remote_endpoint());
    std::cout << "Remote endpoint: " << remote_endpoint << std::endl;
    if (boost::regex_match(remote_endpoint, m_allow_ips_regex)) {
        std::cout << "IP validation successful." << std::endl;
        if (m_validateCallback) { return m_validateCallback(*this, hdl); }
        return true;
    }
    else {
        std::cout << "IP Validation failed." << std::endl;
        return false;
    }
}

#if defined(USE_TLS)
void ServerTls::onOpen(websocketpp::connection_hdl hdl)
#else
void Server::onOpen(websocketpp::connection_hdl hdl)
#endif
{
    std::cout << "Server::onOpen() called with hdl: " << hdl.lock().get() << std::endl;
    {
        boost::unique_lock<boost::mutex> lock(m_connectionMutex);
        m_connections.insert(hdl);
    }
    if (m_openCallback) { m_openCallback(*this, hdl); }
}

#if defined(USE_TLS)
void ServerTls::onClose(websocketpp::connection_hdl hdl)
#else
void Server::onClose(websocketpp::connection_hdl hdl)
#endif
{
    std::cout << "Server::onClose() called with hdl: " << hdl.lock().get() << std::endl;
    {
        boost::unique_lock<boost::mutex> lock(m_connectionMutex);
        removeFromAllGroups(hdl);
        m_connections.erase(hdl);
    }
    if (m_closeCallback) { m_closeCallback(*this, hdl); }
}

#if defined(USE_TLS)
void ServerTls::onMessage(websocketpp::connection_hdl hdl, ws_server_t::message_ptr msg)
#else
void Server::onMessage(websocketpp::connection_hdl hdl, ws_server_t::message_ptr msg)
#endif
{
    std::cout << "Server::onMessage() called with hdl: " << hdl.lock().get()
              << " and message: " << msg->get_payload()
              << std::endl;

    std::stringstream err;

    try {
        JsonRpc::Request request;
        request.setJson(msg->get_payload());
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

#if defined(USE_TLS)
ServerTls::context_ptr ServerTls::onTlsInit(websocketpp::connection_hdl hdl)
{
    std::cout << "ServerTls::onTlsInit() called with hdl: " << hdl.lock().get() << std::endl;
    if (m_tlsInitCallback) { return m_tlsInitCallback(*this, hdl); }
    return context_ptr();
}
#endif

#if defined(USE_TLS)
void ServerTls::requestLoop()
#else
void Server::requestLoop()
#endif
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

#if defined(USE_TLS)
void ServerTls::init(int port, const std::string& allow_ips)
#else
void Server::init(int port, const std::string& allow_ips)
#endif
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

    m_ws_server.set_validate_handler(websocketpp::lib::bind(&ServerType::onValidate, this, websocketpp::lib::placeholders::_1));
    m_ws_server.set_open_handler(websocketpp::lib::bind(&ServerType::onOpen, this, websocketpp::lib::placeholders::_1));
    m_ws_server.set_close_handler(websocketpp::lib::bind(&ServerType::onClose, this, websocketpp::lib::placeholders::_1));
    m_ws_server.set_message_handler(websocketpp::lib::bind(&ServerType::onMessage, this, websocketpp::lib::placeholders::_1, websocketpp::lib::placeholders::_2));
#if defined(USE_TLS)
    m_ws_server.set_tls_init_handler(websocketpp::lib::bind(&ServerType::onTlsInit, this, websocketpp::lib::placeholders::_1));
#endif
}

#if defined(USE_TLS)
void ServerTls::start()
#else
void Server::start()
#endif
{
    boost::unique_lock<boost::mutex> lock(m_startMutex);
    if (m_bRunning) {
        throw std::runtime_error("Server already started.");
    }

    m_bRunning = true;

    m_ws_server.listen(m_port);
    m_ws_server.start_accept();

    m_request_loop_thread   = boost::thread(websocketpp::lib::bind(&ServerType::requestLoop, this));
    m_io_service_thread     = boost::thread(websocketpp::lib::bind(&ws_server_t::run, &m_ws_server));
}

#if defined(USE_TLS)
void ServerTls::stop()
#else
void Server::stop()
#endif
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

#if defined(USE_TLS)
std::string ServerTls::getRemoteEndpoint(websocketpp::connection_hdl hdl)
#else
std::string Server::getRemoteEndpoint(websocketpp::connection_hdl hdl)
#endif
{
    ws_server_t::connection_ptr con = m_ws_server.get_con_from_hdl(hdl);
    return boost::lexical_cast<std::string>(con->get_remote_endpoint());
}

#if defined(USE_TLS)
std::string ServerTls::getResource(websocketpp::connection_hdl hdl)
#else
std::string Server::getResource(websocketpp::connection_hdl hdl)
#endif
{
    ws_server_t::connection_ptr con = m_ws_server.get_con_from_hdl(hdl);
    return boost::lexical_cast<std::string>(con->get_resource());
}

#if defined(USE_TLS)
void ServerTls::addToGroup(const std::string& group, websocketpp::connection_hdl hdl)
#else
void Server::addToGroup(const std::string& group, websocketpp::connection_hdl hdl)
#endif
{
    boost::unique_lock<boost::mutex> lock(m_connectionMutex);
    if (m_connections.count(hdl)) { m_groups.insert(std::pair<std::string, websocketpp::connection_hdl>(group, hdl)); }
}

#if defined(USE_TLS)
void ServerTls::removeFromGroup(const std::string& group, websocketpp::connection_hdl hdl)
#else
void Server::removeFromGroup(const std::string& group, websocketpp::connection_hdl hdl)
#endif
{
    // TODO: improve upon this linear search
    std::vector<connection_group_t::iterator> its;

    boost::unique_lock<boost::mutex> lock(m_connectionMutex);
    auto range = m_groups.equal_range(group);
    for (connection_group_t::iterator it = range.first; it != range.second; ++it)
    {
        if (it->second.owner_before(hdl) && !hdl.owner_before(it->second)) { its.push_back(it); }
    }
    for (auto& it: its) { m_groups.erase(it); }
}

#if defined(USE_TLS)
void ServerTls::removeFromAllGroups(websocketpp::connection_hdl hdl)
#else
void Server::removeFromAllGroups(websocketpp::connection_hdl hdl)
#endif
{
    // TODO: improve upon this linear search
    std::vector<connection_group_t::iterator> its;

    boost::unique_lock<boost::mutex> lock(m_connectionMutex);
    for (connection_group_t::iterator it = m_groups.begin(); it != m_groups.end(); ++it)
    {
        if (it->second.owner_before(hdl) && !hdl.owner_before(it->second)) { its.push_back(it); }
    }
    for (auto& it: its) { m_groups.erase(it); }
}

#if defined(USE_TLS)
void ServerTls::removeGroup(const std::string& group)
#else
void Server::removeGroup(const std::string& group)
#endif
{
    boost::unique_lock<boost::mutex> lock(m_connectionMutex);
    m_groups.erase(group);
}

#if defined(USE_TLS)
void ServerTls::send(websocketpp::connection_hdl hdl, const JsonRpc::Response& res)
#else
void Server::send(websocketpp::connection_hdl hdl, const JsonRpc::Response& res)
#endif
{
    if (!m_bRunning) return;
    boost::unique_lock<boost::mutex> lock(m_connectionMutex);
    if (!m_bRunning) return;
    if (m_connections.count(hdl) == 0) return;
    m_ws_server.send(hdl, res.getJson(), websocketpp::frame::opcode::text);
}

#if defined(USE_TLS)
void ServerTls::sendAll(const JsonRpc::Response& res)
#else
void Server::sendAll(const JsonRpc::Response& res)
#endif
{
    if (!m_bRunning) return;
    boost::unique_lock<boost::mutex> lock(m_connectionMutex);
    if (!m_bRunning) return;
    for (auto& hdl: m_connections)
    {
        m_ws_server.send(hdl, res.getJson(), websocketpp::frame::opcode::text);
    }
}

#if defined(USE_TLS)
void ServerTls::sendGroup(const std::string& group, const JsonRpc::Response& res)
#else
void Server::sendGroup(const std::string& group, const JsonRpc::Response& res)
#endif
{
    if (!m_bRunning) return;
    boost::unique_lock<boost::mutex> lock(m_connectionMutex);
    if (!m_bRunning) return;

    auto range = m_groups.equal_range(group);
    for (connection_group_t::iterator it = range.first; it != range.second; ++it)
    {
        m_ws_server.send(it->second, res.getJson(), websocketpp::frame::opcode::text);
    }
}

#if defined(USE_TLS)
void ServerTls::send(websocketpp::connection_hdl hdl, const std::string& data)
#else
void Server::send(websocketpp::connection_hdl hdl, const std::string& data)
#endif
{
    if (!m_bRunning) return;
    boost::unique_lock<boost::mutex> lock(m_connectionMutex);
    if (!m_bRunning) return;
    if (m_connections.count(hdl) == 0) return;
    m_ws_server.send(hdl, data, websocketpp::frame::opcode::text);
}

#if defined(USE_TLS)
void ServerTls::sendAll(const std::string& data)
#else
void Server::sendAll(const std::string& data)
#endif
{
    if (!m_bRunning) return;
    boost::unique_lock<boost::mutex> lock(m_connectionMutex);
    if (!m_bRunning) return;
    for (auto& hdl: m_connections)
    {
        m_ws_server.send(hdl, data, websocketpp::frame::opcode::text);
    }
}

#if defined(USE_TLS)
void ServerTls::sendGroup(const std::string& group, const std::string& data)
#else
void Server::sendGroup(const std::string& group, const std::string& data)
#endif
{
    if (!m_bRunning) return;
    boost::unique_lock<boost::mutex> lock(m_connectionMutex);
    if (!m_bRunning) return;

    auto range = m_groups.equal_range(group);
    for (connection_group_t::iterator it = range.first; it != range.second; ++it)
    {
        m_ws_server.send(it->second, data, websocketpp::frame::opcode::text);
    }
}

