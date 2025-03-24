#include "websocket_server.hpp"
#include "../json.hpp"

using json = nlohmann::json;

WebSocketServer::WebSocketServer() {
    wsServer_.init_asio();

    wsServer_.set_open_handler(std::bind(&WebSocketServer::onOpen, this, std::placeholders::_1));
    wsServer_.set_close_handler(std::bind(&WebSocketServer::onClose, this, std::placeholders::_1));
    wsServer_.set_message_handler(std::bind(&WebSocketServer::onMessage, this, std::placeholders::_1, std::placeholders::_2));
}

WebSocketServer::~WebSocketServer() {
    stop();
}

void WebSocketServer::run(uint16_t port) {
    try {
        wsServer_.set_reuse_addr(true);
        wsServer_.listen(port);
        wsServer_.start_accept();
        std::cout << "WebSocket Server Running on Port " << port << std::endl;
        
        connectToDeribit();
        
        wsServer_.run();
    } catch (const std::exception& e) {
        std::cerr << "Error starting WebSocket Server: " << e.what() << std::endl;
    }
}

void WebSocketServer::stop() {
    stopHeartbeat();
    
    if (deribitClient_.get_io_service().stopped()) {
        deribitClient_.get_io_service().restart();
    }
    
    websocketpp::lib::error_code ec;
    if (auto conn = deribitConn_.lock()) {
        deribitClient_.close(conn, websocketpp::close::status::normal, "Shutting down", ec);
    }
    
    deribitClient_.stop();
    wsServer_.stop();
    
    if (deribitThread_.joinable()) {
        deribitThread_.join();
    }
}

void WebSocketServer::onOpen(websocketpp::connection_hdl hdl) {
    std::cout << "Client Connected!" << std::endl;
    clients_.insert(hdl);
}

void WebSocketServer::onClose(websocketpp::connection_hdl hdl) {
    std::cout << "Client Disconnected!" << std::endl;
    clients_.erase(hdl);
}

void WebSocketServer::onMessage(websocketpp::connection_hdl, WebsocketServerType::message_ptr msg) {
    std::cout << "Received Message from Client: " << msg->get_payload() << std::endl;

    if (msg->get_payload().find("subscribe") != std::string::npos) {
        try {
            json request = json::parse(msg->get_payload());
            if (request.contains("instrument")) {
                std::string instrument = request["instrument"];
                subscribeToOrderbook(instrument);
            } else {
                subscribeToOrderbook("BTC-PERPETUAL");
            }
        } catch (const json::exception& e) {
            std::cerr << "Failed to parse client message: " << e.what() << std::endl;
            subscribeToOrderbook("BTC-PERPETUAL");
        }
    }
}

void WebSocketServer::connectToDeribit() {
    try {
        std::cout << "[INIT] Connecting to Deribit WebSocket API..." << std::endl;
        
        
        deribitClient_.clear_access_channels(websocketpp::log::alevel::all);
        deribitClient_.set_access_channels(websocketpp::log::alevel::connect);
        deribitClient_.set_access_channels(websocketpp::log::alevel::disconnect);
        deribitClient_.set_access_channels(websocketpp::log::alevel::app);
        
        deribitClient_.init_asio();
        
        deribitClient_.set_tls_init_handler([](websocketpp::connection_hdl) {
            auto ctx = std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tlsv12);
            ctx->set_options(boost::asio::ssl::context::default_workarounds |
                             boost::asio::ssl::context::no_sslv2 |
                             boost::asio::ssl::context::no_sslv3 |
                             boost::asio::ssl::context::single_dh_use);
            return ctx;
        });
        
        deribitClient_.set_message_handler(std::bind(
            &WebSocketServer::handleDeribitMessage, this, 
            std::placeholders::_1, std::placeholders::_2
        ));
        
        websocketpp::lib::error_code ec;
        auto con = deribitClient_.get_connection("wss://test.deribit.com/ws/api/v2", ec);
        
        if (ec) {
            std::cerr << "[ERROR] Could not create Deribit connection: " << ec.message() << std::endl;
            return;
        }
        
        con->set_open_handler([this](websocketpp::connection_hdl hdl) {
            std::cout << "[MSG] Deribit WebSocket connection established!" << std::endl;
            deribitConn_ = hdl;
            startHeartbeat();
            
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            subscribeToOrderbook("BTC-PERPETUAL");
        });
        
        con->set_close_handler([this](websocketpp::connection_hdl) {
            std::cerr << "[STOP] Deribit WebSocket connection closed!" << std::endl;
            stopHeartbeat();
            
            std::cout << "Attempting to reconnect in 3 seconds..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(3));
            connectToDeribit();
        });
        
        con->set_fail_handler([this](websocketpp::connection_hdl) {
            std::cerr << "⚠️ Deribit WebSocket connection failed!" << std::endl;
            stopHeartbeat();
            
            std::cout << "Attempting to reconnect in 5 seconds..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5));
            connectToDeribit();
        });
        
        deribitClient_.connect(con);
        
        // Start the Deribit client thread
        if (deribitThread_.joinable()) {
            deribitThread_.join();
        }
        
        deribitThread_ = std::thread([this]() {
            try {
                std::cout << "[MSG] Starting Deribit WebSocket client thread..." << std::endl;
                deribitClient_.run();
                std::cout << "Deribit WebSocket client thread ended" << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "[ERROR] Error in Deribit WebSocket client thread: " << e.what() << std::endl;
            }
        });
        
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Exception in connectToDeribit: " << e.what() << std::endl;
    }
}

void WebSocketServer::startHeartbeat() {
    try {
        heartbeatTimer_ = std::make_shared<boost::asio::steady_timer>(
            deribitClient_.get_io_service()
        );
        
        std::cout << "[IMP] Starting heartbeat service..." << std::endl;
        
        scheduleNextHeartbeat();
        
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Error starting heartbeat: " << e.what() << std::endl;
    }
}

void WebSocketServer::scheduleNextHeartbeat() {
    if (!heartbeatTimer_) return;
    
    heartbeatTimer_->expires_from_now(std::chrono::seconds(20));
    heartbeatTimer_->async_wait([this](const boost::system::error_code& ec) {
        if (ec) {
            return;
        }
        
        sendHeartbeat();
        scheduleNextHeartbeat(); 
    });
}

void WebSocketServer::sendHeartbeat() {
    auto conn = deribitConn_.lock();
    if (!conn) {
        std::cerr << "[ERROR] No active connection for heartbeat" << std::endl;
        return;
    }
    
    try {
        json heartbeat_json = {
            {"jsonrpc", "2.0"},
            {"id", 9999},
            {"method", "public/test"}
        };
        
        std::string message = heartbeat_json.dump();
        
        websocketpp::lib::error_code ec;
        deribitClient_.send(conn, message, websocketpp::frame::opcode::text, ec);
        
        if (ec) {
            std::cerr << "[ERROR] Failed to send heartbeat: " << ec.message() << std::endl;
        } else {
            std::cout << "[HB] Heartbeat sent" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Exception in sendHeartbeat: " << e.what() << std::endl;
    }
}

void WebSocketServer::stopHeartbeat() {
    try {
        if (heartbeatTimer_) {
            boost::system::error_code ec;
            heartbeatTimer_->cancel(ec);
            
            if (ec) {
                std::cerr << "[ERROR] Error cancelling heartbeat timer: " << ec.message() << std::endl;
            } else {
                std::cout << "[STOP] Heartbeat stopped" << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Exception in stopHeartbeat: " << e.what() << std::endl;
    }
}

void WebSocketServer::subscribeToOrderbook(const std::string& symbol) {
    try {
        auto conn = deribitConn_.lock();
        if (!conn) {
            std::cerr << "[ERROR] No active connection to Deribit." << std::endl;
            return;
        }

        auto connection = deribitClient_.get_con_from_hdl(conn);
        if (!connection || connection->get_state() != websocketpp::session::state::open) {
            std::cerr << "[ERROR] WebSocket is not open. Cannot subscribe yet. State: " 
                      << (connection ? std::to_string(static_cast<int>(connection->get_state())) : "null") 
                      << std::endl;
            return;
        }

        json message_json = {
            {"jsonrpc", "2.0"},
            {"id", 42},
            {"method", "public/subscribe"},
            {"params", {
                {"channels", {std::string("book.") + symbol + ".100ms"}}
            }}
        };

        std::string message = message_json.dump();
        
        std::cout << "🔍 Sending Subscription Message: " << message << std::endl;

        websocketpp::lib::error_code ec;
        deribitClient_.send(conn, message, websocketpp::frame::opcode::text, ec);

        if (ec) {
            std::cerr << "[ERROR] Failed to subscribe to orderbook: " << ec.message() << std::endl;
        } else {
            std::cout << "[MSG] Subscription request sent for: " << symbol << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Exception in subscribeToOrderbook: " << e.what() << std::endl;
    }
}

void WebSocketServer::handleDeribitMessage(websocketpp::connection_hdl, WebsocketClientType::message_ptr msg) {
    std::string payload = msg->get_payload();
    

    try {
        json parsed_json = json::parse(payload);
        
        if (parsed_json.contains("id") && parsed_json.contains("result")) {
            std::cout << "[MSG] Received response for request ID " << parsed_json["id"] << std::endl;
             
            if (parsed_json["id"] == 9999) {
                std::cout << "[HB] Heartbeat acknowledged by server" << std::endl;
                return;
            }
            
            if (parsed_json["id"] == 42) {
                std::cout << "[MSG] Subscription confirmed: " << parsed_json["result"].dump(2) << std::endl;
            }
        }
        
        if (parsed_json.contains("error")) {
            std::cerr << "[ERROR] Deribit API Error: " << parsed_json["error"]["message"] << " (Code: " 
                      << parsed_json["error"]["code"] << ")" << std::endl;
            return;
        }
        
        if (parsed_json.contains("method") && parsed_json["method"] == "subscription") {
            if (parsed_json.contains("params") && parsed_json["params"].contains("channel")) {
                std::string channel = parsed_json["params"]["channel"];
                
        
                if (channel.find("book.") == 0) {
                    std::cout << "[WEBSOCKET] Received orderbook update for channel: " << channel << std::endl;
                    
        
                    json data = parsed_json["params"]["data"];
                    std::cout << "[WEBSOCKET] Book state: " << (data.contains("type") ? data["type"].get<std::string>() : "unknown") << std::endl;
                    
                    if (data.contains("bids") && data.contains("asks")) {
                        std::cout << "   Top bid: " << (data["bids"].size() > 0 ? data["bids"][0].dump() : "none") << std::endl;
                        std::cout << "   Top ask: " << (data["asks"].size() > 0 ? data["asks"][0].dump() : "none") << std::endl;
                    }
                    
        
                    for (const auto& client : clients_) {
                        websocketpp::lib::error_code ec;
                        wsServer_.send(client, payload, websocketpp::frame::opcode::text, ec);
                        
                        if (ec) {
                            std::cerr << "[ERROR] Error sending to client: " << ec.message() << std::endl;
                        }
                    }
                } else {
                    std::cout << "[RECEIVED] Received update for channel: " << channel << std::endl;
                }
            }
        }
    } catch (const json::exception& e) {
        std::cerr << "[ERROR] Failed to parse JSON: " << e.what() << std::endl;
        std::cerr << "   Raw payload: " << payload << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Exception processing message: " << e.what() << std::endl;
    }
}