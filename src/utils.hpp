#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include "../json.hpp"
using json = nlohmann::json;

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output);
std::string makeAuthenticatedRequest(const std::string& endpoint, const std::string& accessToken);
std::string getEnvValue(const std::string& key);
std::string getAccessToken(const std::string& client_id, const std::string& client_secret);
std::string placeBuyOrder(const std::string& accessToken, const std::string& instrument, double amount, double price, const std::string& orderType);
std::string cancelOrder(const std::string& accessToken, const std::string& orderId);
std::string cancelOrder(const std::string& accessToken, const std::string& orderId);
std::string placeSellOrder(const std::string& accessToken, const std::string& instrument, double amount, double price, const std::string& orderType);
std::string modifyOrder(const std::string& accessToken, const std::string& orderId, double newAmount, double newPrice);
json getMarketData(const std::string& currency, const std::string& kind, const std::string& instrument, int depth);
json getPositions(const std::string& accessToken, const std::string& currency, const std::string& kind);

#endif  // UTILS_HPP
