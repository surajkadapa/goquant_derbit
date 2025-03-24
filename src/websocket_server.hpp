#pragma once

#include <iostream>
#include <string>
#include <set>
#include <thread>
#include <memory>
#include <functional>

// WebSocket++ includes
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/client.hpp>
#include <websocketpp/server.hpp>
#include <websocketpp/common/thread.hpp>
#include <websocketpp/common/memory.hpp>

// Boost includes for timer
#include <boost/asio/steady_timer.hpp>

// WebSocket type definitions
typedef websocketpp::client<websocketpp::config::asio_tls_client> WebsocketClientType;
typedef websocketpp::server<websocketpp::config::asio> WebsocketServerType;

class WebSocketServer {
public:
    WebSocketServer();
    ~WebSocketServer();

    void run(uint16_t port);
    void stop();

private:
    // Server event handlers
    void onOpen(websocketpp::connection_hdl hdl);
    void onClose(websocketpp::connection_hdl hdl);
    void onMessage(websocketpp::connection_hdl hdl, WebsocketServerType::message_ptr msg);

    // Deribit connection and management
    void connectToDeribit();
    void subscribeToOrderbook(const std::string& symbol);
    void handleDeribitMessage(websocketpp::connection_hdl hdl, WebsocketClientType::message_ptr msg);
    
    // Heartbeat management
    void startHeartbeat();
    void scheduleNextHeartbeat();
    void sendHeartbeat();
    void stopHeartbeat();

    // WebSocket server instance
    WebsocketServerType wsServer_;
    std::set<websocketpp::connection_hdl, std::owner_less<websocketpp::connection_hdl>> clients_;

    // Deribit WebSocket client
    WebsocketClientType deribitClient_;
    std::weak_ptr<void> deribitConn_;  // Using weak_ptr to handle connection lifetime
    std::thread deribitThread_;
    
    // Heartbeat timer
    std::shared_ptr<boost::asio::steady_timer> heartbeatTimer_;
};