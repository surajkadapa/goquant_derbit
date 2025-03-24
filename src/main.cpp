#include <iostream>
#include <curl/curl.h>
#include "../json.hpp"
#include "websocket_server.hpp"
#include "utils.hpp"

using json = nlohmann::json;

int main() {
    std::string client_id = getEnvValue("DERIBIT_CLIENT_ID");
    std::string client_secret = getEnvValue("DERIBIT_CLIENT_SECRET");

    if (client_id.empty() || client_secret.empty()) {
        std::cerr << "Error: Missing API credentials!" << std::endl;
        return 1;
    }

    std::string accessToken = getAccessToken(client_id, client_secret);
    if (accessToken.empty()) {
        std::cerr << "Failed to obtain access token!" << std::endl;
        return 1;
    }
    std::cout << "Access Token: " << accessToken << std::endl;

    std::string accountResponse = makeAuthenticatedRequest("/api/v2/private/get_account_summary?currency=BTC", accessToken);

    try {
        json jsonResponse = json::parse(accountResponse);
        if (jsonResponse.contains("result")) {
            std::cout << "Account Balance (BTC): " << jsonResponse["result"]["balance"] << std::endl;
        } else {
            std::cerr << "Error retrieving account info: " << accountResponse << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "JSON Parsing Error: " << e.what() << std::endl;
    }

    std::string orderResponse = placeBuyOrder(accessToken, "ETH-PERPETUAL", 10, 0, "market");

    std::cout << "Order Buy Response: " << orderResponse << std::endl;

std::string sellOrderId = placeSellOrder(accessToken, "ETH-PERPETUAL", 10, 85000, "limit");

    if (!sellOrderId.empty()) {
        std::cout << "Sell Order ID: " << sellOrderId << std::endl;

        std::string modifyResponse = modifyOrder(accessToken, sellOrderId, 10000, 84500);
        std::cout << "Modify Order Response: " << modifyResponse << std::endl;

        std::string cancelResponse = cancelOrder(accessToken, sellOrderId);
        std::cout << "Cancel Order Response: " << cancelResponse << std::endl;
    } else {
        std::cerr << "Sell order failed, skipping modify/cancel." << std::endl;
    }

    std::string currency = "BTC";
    std::string kind = "future";
    std::string instrument = "BTC-PERPETUAL";
    int depth = 10;

    std::cout << "Fetching market data..." << std::endl;
    json marketData = getMarketData(currency, kind, instrument, depth);

    std::cout << "Market Data: " << marketData.dump(4) << std::endl;


    std::cout << "Fetching open positions..." << std::endl;
    json positions = getPositions(accessToken, "BTC", "future");

    std::cout << "Open Positions: " << positions.dump(4) << std::endl;

    WebSocketServer server;
    std::cout << "Starting WebSocket Server on port 9002..." << std::endl;
    server.run(9002);
    return 0;
}

