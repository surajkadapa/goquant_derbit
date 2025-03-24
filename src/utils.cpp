#include "utils.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include "utils.hpp"
#include <curl/curl.h>
#include "../json.hpp"
#include <fstream>
#include <chrono>


using json = nlohmann::json;


void logBenchmark(const std::string& message) {
    std::ofstream logFile("benchmark.log", std::ios_base::app);
    if (logFile.is_open()) {
        logFile << message << std::endl;
        logFile.close();
    }
}

std::string makeAuthenticatedRequest(const std::string& endpoint, const std::string& accessToken) {
    CURL* curl = curl_easy_init();
    if (!curl) return "";

    std::string response;
    std::string url = "https://test.deribit.com" + endpoint;
    std::cout << "[KEY] Access Token: " << accessToken << std::endl;
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + accessToken).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    auto start = std::chrono::high_resolution_clock::now();

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        std::cerr << "[ERROR] Curl request failed: " << curl_easy_strerror(res) << std::endl;
        return "";
    }

    std::string logMsg = "[TIME] API Call (" + endpoint + ") took " + std::to_string(duration.count()) + " ms";
    logBenchmark(logMsg);
    std::cout << logMsg << std::endl;

    return response;
}
std::string getEnvValue(const std::string& key) {
    std::vector<std::filesystem::path> possiblePaths = {
        ".env",                           
        "../.env",                        
        "../../.env",                     
    };
    
    for (const auto& path : possiblePaths) {
        std::ifstream file(path);
        if (file) {
            std::string line;
            while (std::getline(file, line)) {
                // Skip empty lines and comments
                if (line.empty() || line[0] == '#') continue;
                
                std::istringstream iss(line);
                std::string var, value;
                if (std::getline(iss, var, '=') && std::getline(iss, value)) {
                    // Trim whitespace from variable name
                    var.erase(0, var.find_first_not_of(" \t"));
                    var.erase(var.find_last_not_of(" \t") + 1);
                    
                    if (var == key) {
                        // Trim whitespace from value
                        value.erase(0, value.find_first_not_of(" \t"));
                        value.erase(value.find_last_not_of(" \t") + 1);
                        return value;
                    }
                }
            }
        }
    }
    
    std::cerr << "Error: Key '" << key << "' not found in any .env file" << std::endl;
    return "";
}
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t totalSize = size * nmemb;
    output->append((char*)contents, totalSize);
    return totalSize;
}

std::string getAccessToken(const std::string& client_id, const std::string& client_secret) {
    CURL* curl = curl_easy_init();
    if (!curl) return "";

    std::string response;
    std::string url = "https://test.deribit.com/api/v2/public/auth?client_id=" + client_id + 
                      "&client_secret=" + client_secret + "&grant_type=client_credentials";

    auto start = std::chrono::high_resolution_clock::now();

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;

    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::cerr << "[ERROR] Curl request failed: " << curl_easy_strerror(res) << std::endl;
        return "";
    }

    std::string logMsg = "[TIME] Access Token Request took " + std::to_string(duration.count()) + " ms";
    logBenchmark(logMsg);
    std::cout << logMsg << std::endl;

    try {
        json jsonResponse = json::parse(response);
        if (jsonResponse.contains("result") && jsonResponse["result"].contains("access_token")) {
            std::string token = jsonResponse["result"]["access_token"];
            std::cout << "[MSG] Extracted Access Token: " << token << std::endl;
            return token;
        } else {
            std::cerr << "[ERROR] Failed to retrieve access token from response: " << response << std::endl;
            return "";
        }
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] JSON Parsing Error: " << e.what() << std::endl;
        return "";
    }
}



std::string placeBuyOrder(const std::string& accessToken, const std::string& instrument, double amount, double price, const std::string& orderType) {
    CURL* curl = curl_easy_init();
    if (!curl) return "";

    std::string response;
    std::string url = "https://test.deribit.com/api/v2/private/buy?"
                      "instrument_name=" + instrument +
                      "&amount=" + std::to_string(amount) +
                      "&type=" + orderType;

    if (orderType == "limit") {
        url += "&price=" + std::to_string(price);
    }

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + accessToken).c_str());

    auto start = std::chrono::high_resolution_clock::now();

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        std::cerr << "[ERROR] Curl request failed: " << curl_easy_strerror(res) << std::endl;
        return "";
    }

    std::string logMsg = "[TIME] Buy Order took " + std::to_string(duration.count()) + " ms";
    logBenchmark(logMsg);
    std::cout << logMsg << std::endl;

    return response;
}

std::string cancelOrder(const std::string& accessToken, const std::string& orderId) {
    CURL* curl = curl_easy_init();
    if (!curl) return "";

    std::string response;
    std::string url = "https://test.deribit.com/api/v2/private/cancel?order_id=" + orderId;

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + accessToken).c_str());

    auto start = std::chrono::high_resolution_clock::now();

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        std::cerr << "[ERROR] Curl request failed: " << curl_easy_strerror(res) << std::endl;
        return "";
    }

    std::string logMsg = "[TIME] Cancel Order took " + std::to_string(duration.count()) + " ms";
    logBenchmark(logMsg);
    std::cout << logMsg << std::endl;

    return response;
}
std::string modifyOrder(const std::string& accessToken, const std::string& orderId, double newAmount, double newPrice) {
    CURL* curl = curl_easy_init();
    if (!curl) return "";

    auto start = std::chrono::high_resolution_clock::now();

    std::string response;
    std::string url = "https://test.deribit.com/api/v2/private/edit?"
                      "order_id=" + orderId +
                      "&amount=" + std::to_string(newAmount) +
                      "&price=" + std::to_string(newPrice);

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + accessToken).c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;

    if (res != CURLE_OK) {
        std::cerr << "Curl request failed: " << curl_easy_strerror(res) << std::endl;
        return "";
    }

    std::string logMsg = "[TIME] Modify Order took " + std::to_string(duration.count()) + " ms";
    logBenchmark(logMsg);
    std::cout << logMsg << std::endl;
    return response;
}

std::string placeSellOrder(const std::string& accessToken, const std::string& instrument, double amount, double price, const std::string& orderType) {
    CURL* curl = curl_easy_init();
    if (!curl) return "";
    auto start = std::chrono::high_resolution_clock::now();

    std::string response;
    std::string url = "https://test.deribit.com/api/v2/private/sell?"
                      "instrument_name=" + instrument +
                      "&amount=" + std::to_string(amount) +
                      "&type=" + orderType;

    if (orderType == "limit") {
        url += "&price=" + std::to_string(price);
    }

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + accessToken).c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;

    std::string logMsg = "[TIME] Place Sell Order took " + std::to_string(duration.count()) + " ms";
    logBenchmark(logMsg);
    std::cout << logMsg << std::endl;
    if (res != CURLE_OK) {
        std::cerr << "Curl request failed: " << curl_easy_strerror(res) << std::endl;
        return "";
    }


    try {
        json jsonResponse = json::parse(response);
        if (jsonResponse.contains("result") && jsonResponse["result"].contains("order") && jsonResponse["result"]["order"].contains("order_id")) {
            return jsonResponse["result"]["order"]["order_id"];
        } else {
            std::cerr << "Error: Could not extract order_id from response: " << response << std::endl;
            return "";
        }
    } catch (const std::exception& e) {
        std::cerr << "JSON Parsing Error: " << e.what() << std::endl;
        return "";
    }
}

json getMarketData(const std::string& currency, const std::string& kind, const std::string& instrument, int depth) {
    CURL* curl = curl_easy_init();
    if (!curl) return json();

    json finalResponse;
    std::string response;
    CURLcode res;

    auto start = std::chrono::high_resolution_clock::now();

    std::string orderBookUrl = "https://test.deribit.com/api/v2/public/get_order_book?"
                               "instrument_name=" + instrument + "&depth=" + std::to_string(depth);

    curl_easy_setopt(curl, CURLOPT_URL, orderBookUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    res = curl_easy_perform(curl);

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;

    if (res == CURLE_OK) {
        finalResponse["orderbook"] = json::parse(response);
    } else {
        std::cerr << "[ERROR] Error fetching order book: " << curl_easy_strerror(res) << std::endl;
    }

    curl_easy_cleanup(curl);

    std::string logMsg = "[TIME] Market Data Fetch took " + std::to_string(duration.count()) + " ms";
    logBenchmark(logMsg);
    std::cout << logMsg << std::endl;

    return finalResponse;
}

json getPositions(const std::string& accessToken, const std::string& currency, const std::string& kind) {
    CURL* curl = curl_easy_init();
    if (!curl) return json();

    auto start = std::chrono::high_resolution_clock::now();

    std::string response;
    std::string url = "https://test.deribit.com/api/v2/private/get_positions?"
                      "currency=" + currency +
                      "&kind=" + kind;

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + accessToken).c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> duration = end - start;

    std::string logMsg = "[TIME] Get Positions took " + std::to_string(duration.count()) + " ms";
    logBenchmark(logMsg);
    std::cout << logMsg << std::endl;


    if (res != CURLE_OK) {
        std::cerr << "Curl request failed: " << curl_easy_strerror(res) << std::endl;
        return json();
    }

    try {
        return json::parse(response);
    } catch (const std::exception& e) {
        std::cerr << "JSON Parsing Error: " << e.what() << std::endl;
        return json();
    }
}