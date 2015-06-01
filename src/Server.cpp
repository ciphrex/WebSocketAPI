///////////////////////////////////////////////////////////////////////////////
//
// Server.cpp
//
// Copyright (c) 2013-2014 Eric Lombrozo
//
// All Rights Reserved.

#include "Server.h"
#include "JsonRpc.h"

#include <logger/logger.h>

#include <boost/lexical_cast.hpp>

using namespace WebSocket;
using namespace std;

#if defined(USE_TLS)
bool ServerTls::onValidate(websocketpp::connection_hdl hdl)
#else
bool ServerNoTls::onValidate(websocketpp::connection_hdl hdl)
#endif
{
    LOGGER(trace) << SERVER_CLASS_NAME << "::onValidate() entered." << endl;
    ws_server_t::connection_ptr con = m_ws_server.get_con_from_hdl(hdl);
    std::string remote_endpoint = boost::lexical_cast<std::string>(con->get_remote_endpoint());
    LOGGER(trace) << SERVER_CLASS_NAME << "::onValidate() - Remote endpoint: " << remote_endpoint << endl;
    if (boost::regex_match(remote_endpoint, m_allow_ips_regex)) {
        LOGGER(trace) << SERVER_CLASS_NAME << "::onValidate() - IP validation successful." << endl;
        if (m_validateCallback) { return m_validateCallback(*this, hdl); }
        return true;
    }
    else {
        LOGGER(trace) << "IP Validation failed." << endl;
        return false;
    }
}

#if defined(USE_TLS)
void ServerTls::onOpen(websocketpp::connection_hdl hdl)
#else
void ServerNoTls::onOpen(websocketpp::connection_hdl hdl)
#endif
{
    LOGGER(trace) << SERVER_CLASS_NAME << "::onOpen() called with hdl: " << hdl.lock().get() << endl;
    {
        boost::unique_lock<boost::mutex> lock(m_connectionMutex);
        m_connections.insert(hdl);
    }
    if (m_openCallback) { m_openCallback(*this, hdl); }
}

#if defined(USE_TLS)
void ServerTls::onClose(websocketpp::connection_hdl hdl)
#else
void ServerNoTls::onClose(websocketpp::connection_hdl hdl)
#endif
{
    LOGGER(trace) << SERVER_CLASS_NAME << "::onClose() called with hdl: " << hdl.lock().get() << endl;
    {
        boost::unique_lock<boost::mutex> lock(m_connectionMutex);
        do_removeFromAllChannels(hdl);
        m_connections.erase(hdl);
    }
    if (m_closeCallback) { m_closeCallback(*this, hdl); }
}

#if defined(USE_TLS)
void ServerTls::onFail(websocketpp::connection_hdl hdl)
#else
void ServerNoTls::onFail(websocketpp::connection_hdl hdl)
#endif
{
    ws_server_t::connection_ptr con = m_ws_server.get_con_from_hdl(hdl);
    string error = con->get_ec().message();
    LOGGER(trace) << SERVER_CLASS_NAME << "::onFail() called with hdl: " << hdl.lock().get() << " Error: " << error << " Value: " << con->get_ec().value() << endl;

/*
    {
        boost::unique_lock<boost::mutex> lock(m_connectionMutex);
        do_removeFromAllChannels(hdl);
        m_connections.erase(hdl);
    }
    if (m_closeCallback) { m_closeCallback(*this, hdl); }
*/
}

#if defined(USE_TLS)
void ServerTls::onMessage(websocketpp::connection_hdl hdl, ws_server_t::message_ptr msg)
#else
void ServerNoTls::onMessage(websocketpp::connection_hdl hdl, ws_server_t::message_ptr msg)
#endif
{
    LOGGER(trace) << SERVER_CLASS_NAME << "::onMessage() called with hdl: " << hdl.lock().get()
                  << " and message: " << msg->get_payload()
                  << endl;

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
        string json(response.getJson());
        LOGGER(trace) << SERVER_CLASS_NAME << "::onMessage() sending error to hdl " << hdl.lock().get() << ": " << json << endl;
        m_ws_server.send(hdl, json, msg->get_opcode());
    }
    catch (const std::exception& e) {
        JsonRpc::Response response;
        response.setError(e);
        string json(response.getJson());
        LOGGER(trace) << SERVER_CLASS_NAME << "::onMessage() sending error to hdl " << hdl.lock().get() << ": " << json << endl;
        m_ws_server.send(hdl, json, msg->get_opcode());
    }
}

#if defined(USE_TLS)
ServerTls::context_ptr ServerTls::onTlsInit(websocketpp::connection_hdl hdl)
{
    LOGGER(trace) << SERVER_CLASS_NAME << "::onTlsInit() called with hdl: " << hdl.lock().get() << endl;
    if (m_tlsInitCallback) { return m_tlsInitCallback(*this, hdl); }
    return context_ptr();
}
#endif

#if defined(USE_TLS)
void ServerTls::requestLoop()
#else
void ServerNoTls::requestLoop()
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
            LOGGER(trace) << SERVER_CLASS_NAME << "::requestLoop() - Error: " << e.what() << endl;
        }
    }
}

#if defined(USE_TLS)
void ServerTls::init(int port, const std::string& allow_ips)
#else
void ServerNoTls::init(int port, const std::string& allow_ips)
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
        LOGGER(error) <<  "WARNING: Invalid allowips regex. Allowing localhost only." << endl;
        m_allow_ips_regex.assign(DEFAULT_ALLOWED_IPS);
    }

    m_ws_server.set_access_channels(websocketpp::log::alevel::all);
    m_ws_server.clear_access_channels(websocketpp::log::alevel::frame_payload);

    m_ws_server.init_asio();

    m_ws_server.set_validate_handler(websocketpp::lib::bind(&Server::onValidate, this, websocketpp::lib::placeholders::_1));
    m_ws_server.set_open_handler(websocketpp::lib::bind(&Server::onOpen, this, websocketpp::lib::placeholders::_1));
    m_ws_server.set_close_handler(websocketpp::lib::bind(&Server::onClose, this, websocketpp::lib::placeholders::_1));
    m_ws_server.set_fail_handler(websocketpp::lib::bind(&Server::onFail, this, websocketpp::lib::placeholders::_1));
    m_ws_server.set_message_handler(websocketpp::lib::bind(&Server::onMessage, this, websocketpp::lib::placeholders::_1, websocketpp::lib::placeholders::_2));
#if defined(USE_TLS)
    m_ws_server.set_tls_init_handler(websocketpp::lib::bind(&Server::onTlsInit, this, websocketpp::lib::placeholders::_1));
#endif
}

#if defined(USE_TLS)
void ServerTls::start()
#else
void ServerNoTls::start()
#endif
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

#if defined(USE_TLS)
void ServerTls::stop()
#else
void ServerNoTls::stop()
#endif
{
    boost::unique_lock<boost::mutex> lock(m_startMutex);
    m_bRunning = false;
    lock.unlock();

    LOGGER(trace) << "Websocket server stopping request loop thread..." << endl;
    m_requestCond.notify_all();
    m_request_loop_thread.join();
    LOGGER(trace) << "Done." << endl;

    LOGGER(trace) << "Websocket server stopping io service thread..." << endl;
    m_ws_server.stop();
    m_io_service_thread.join();
    LOGGER(trace) << "Done." << endl;
}

#if defined(USE_TLS)
std::string ServerTls::getRemoteEndpoint(websocketpp::connection_hdl hdl)
#else
std::string ServerNoTls::getRemoteEndpoint(websocketpp::connection_hdl hdl)
#endif
{
    ws_server_t::connection_ptr con = m_ws_server.get_con_from_hdl(hdl);
    return boost::lexical_cast<std::string>(con->get_remote_endpoint());
}

#if defined(USE_TLS)
std::string ServerTls::getResource(websocketpp::connection_hdl hdl)
#else
std::string ServerNoTls::getResource(websocketpp::connection_hdl hdl)
#endif
{
    ws_server_t::connection_ptr con = m_ws_server.get_con_from_hdl(hdl);
    return boost::lexical_cast<std::string>(con->get_resource());
}

#if defined(USE_TLS)
void ServerTls::addToChannel(const std::string& channel, websocketpp::connection_hdl hdl)
#else
void ServerNoTls::addToChannel(const std::string& channel, websocketpp::connection_hdl hdl)
#endif
{
    boost::unique_lock<boost::mutex> lock(m_connectionMutex);
    if (m_connections.count(hdl)) { m_channels.insert(std::pair<std::string, websocketpp::connection_hdl>(channel, hdl.lock())); }
}

#if defined(USE_TLS)
void ServerTls::removeFromChannel(const std::string& channel, websocketpp::connection_hdl hdl)
#else
void ServerNoTls::removeFromChannel(const std::string& channel, websocketpp::connection_hdl hdl)
#endif
{
    // TODO: improve upon this linear search
    std::vector<channels_t::iterator> its;

    boost::unique_lock<boost::mutex> lock(m_connectionMutex);
    auto hdl_ = hdl.lock();
    if (!hdl_) return;

    auto range = m_channels.equal_range(channel);
    for (channels_t::iterator it = range.first; it != range.second; ++it)
    {
        if (hdl_ == it->second.lock()) { its.push_back(it); }
    }
    for (auto& it: its) { m_channels.erase(it); }
}

#if defined(USE_TLS)
void ServerTls::removeFromAllChannels(websocketpp::connection_hdl hdl)
#else
void ServerNoTls::removeFromAllChannels(websocketpp::connection_hdl hdl)
#endif
{
    boost::unique_lock<boost::mutex> lock(m_connectionMutex);
    do_removeFromAllChannels(hdl);
}

#if defined(USE_TLS)
void ServerTls::do_removeFromAllChannels(websocketpp::connection_hdl hdl)
#else
void ServerNoTls::do_removeFromAllChannels(websocketpp::connection_hdl hdl)
#endif
{
    // TODO: improve upon this linear search
    std::vector<channels_t::iterator> its;
    auto hdl_ = hdl.lock();
    if (!hdl_) return;
    for (channels_t::iterator it = m_channels.begin(); it != m_channels.end(); ++it)
    {
        if (hdl_ == it->second.lock()) { its.push_back(it); }
    }
    for (auto& it: its) { m_channels.erase(it); }
}

#if defined(USE_TLS)
void ServerTls::removeChannel(const std::string& channel)
#else
void ServerNoTls::removeChannel(const std::string& channel)
#endif
{
    boost::unique_lock<boost::mutex> lock(m_connectionMutex);
    m_channels.erase(channel);
}

#if defined(USE_TLS)
void ServerTls::send(websocketpp::connection_hdl hdl, const JsonRpc::Response& res)
#else
void ServerNoTls::send(websocketpp::connection_hdl hdl, const JsonRpc::Response& res)
#endif
{
    if (!m_bRunning) return;
    boost::unique_lock<boost::mutex> lock(m_connectionMutex);
    if (!m_bRunning) return;
    if (m_connections.count(hdl) == 0) return;
    string json(res.getJson());
    LOGGER(trace) << SERVER_CLASS_NAME << "::send() sending response to hdl " << hdl.lock().get() << ": " << json << endl;
    m_ws_server.send(hdl, json, websocketpp::frame::opcode::text);
}

#if defined(USE_TLS)
void ServerTls::sendAll(const JsonRpc::Response& res)
#else
void ServerNoTls::sendAll(const JsonRpc::Response& res)
#endif
{
    if (!m_bRunning) return;
    boost::unique_lock<boost::mutex> lock(m_connectionMutex);
    if (!m_bRunning) return;
    for (auto& hdl: m_connections)
    {
        string json(res.getJson());
        LOGGER(trace) << SERVER_CLASS_NAME << "::sendAll() sending response to hdl " << hdl.lock().get() << ": " << json << endl;
        m_ws_server.send(hdl, json, websocketpp::frame::opcode::text);
    }
}

#if defined(USE_TLS)
void ServerTls::sendChannel(const std::string& channel, const JsonRpc::Response& res)
#else
void ServerNoTls::sendChannel(const std::string& channel, const JsonRpc::Response& res)
#endif
{
    if (!m_bRunning) return;
    boost::unique_lock<boost::mutex> lock(m_connectionMutex);
    if (!m_bRunning) return;

    LOGGER(trace) << SERVER_CLASS_NAME << "::sendChannel() response sending to channel " << channel << endl;
    auto range = m_channels.equal_range(channel);
    for (channels_t::iterator it = range.first; it != range.second; ++it)
    {
        string json(res.getJson());
        LOGGER(trace) << SERVER_CLASS_NAME << "::send() sending response to hdl " << it->second.lock().get() << ": " << json << endl;
        m_ws_server.send(it->second, json, websocketpp::frame::opcode::text);
    }
}

#if defined(USE_TLS)
void ServerTls::send(websocketpp::connection_hdl hdl, const std::string& data)
#else
void ServerNoTls::send(websocketpp::connection_hdl hdl, const std::string& data)
#endif
{
    if (!m_bRunning) return;
    boost::unique_lock<boost::mutex> lock(m_connectionMutex);
    if (!m_bRunning) return;
    if (m_connections.count(hdl) == 0) return;
    LOGGER(trace) << SERVER_CLASS_NAME << "::send() sending data to hdl " << hdl.lock().get() << ": " << data << endl;
    m_ws_server.send(hdl, data, websocketpp::frame::opcode::text);
}

#if defined(USE_TLS)
void ServerTls::sendAll(const std::string& data)
#else
void ServerNoTls::sendAll(const std::string& data)
#endif
{
    if (!m_bRunning) return;
    boost::unique_lock<boost::mutex> lock(m_connectionMutex);
    if (!m_bRunning) return;
    for (auto& hdl: m_connections)
    {
        LOGGER(trace) << SERVER_CLASS_NAME << "::sendAll() sending data to hdl " << hdl.lock().get() << ": " << data << endl;
        m_ws_server.send(hdl, data, websocketpp::frame::opcode::text);
    }
}

#if defined(USE_TLS)
void ServerTls::sendChannel(const std::string& channel, const std::string& data)
#else
void ServerNoTls::sendChannel(const std::string& channel, const std::string& data)
#endif
{
    if (!m_bRunning) return;
    boost::unique_lock<boost::mutex> lock(m_connectionMutex);
    if (!m_bRunning) return;

    LOGGER(trace) << SERVER_CLASS_NAME << "::sendChannel() sending data to channel " << channel << endl;
    auto range = m_channels.equal_range(channel);
    for (channels_t::iterator it = range.first; it != range.second; ++it)
    {
        LOGGER(trace) << SERVER_CLASS_NAME << "::sendChannel() sending data to hdl " << it->second.lock().get() << ": " << data << endl;
        m_ws_server.send(it->second, data, websocketpp::frame::opcode::text);
    }
}

