// Ultra-Efficient HFT System - Fixed P&L & Improved Algorithms
// Compile: g++ -std=c++17 -O3 -pthread main.cpp -o hft_system

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <memory>
#include <iomanip>
#include <random>
#include <sstream>
#include <ctime>
#include <deque>
#include <limits>

namespace Color {
    const std::string RESET = "\033[0m";
    const std::string BOLD = "\033[1m";
    const std::string GREEN = "\033[32m";
    const std::string RED = "\033[31m";
    const std::string YELLOW = "\033[33m";
    const std::string BLUE = "\033[34m";
    const std::string MAGENTA = "\033[35m";
    const std::string CYAN = "\033[36m";
}

const std::vector<std::string> ALL_STOCKS = {
    "AAPL", "MSFT", "GOOGL", "AMZN", "NVDA", "META", "TSLA", "BRK.B", "UNH", "JNJ",
    "V", "XOM", "WMT", "LLY", "JPM", "MA", "PG", "AVGO", "HD", "CVX",
    "MRK", "ABBV", "COST", "PEP", "KO", "ADBE", "TMO", "BAC", "CSCO", "MCD",
    "ACN", "NFLX", "CRM", "ABT", "WFC", "DHR", "VZ", "CMCSA", "DIS", "AMD",
    "INTC", "NKE", "TXN", "UPS", "PM", "QCOM", "NEE", "RTX", "HON", "INTU",
    "UNP", "SPGI", "MS", "COP", "IBM", "LOW", "GS", "BA", "CAT", "NOW",
    "AMGN", "BLK", "DE", "ELV", "GILD", "BKNG", "AXP", "MDT", "GE", "SBUX",
    "ADI", "LMT", "ISRG", "SYK", "PLD", "ADP", "REGN", "MMC", "TJX", "VRTX",
    "TMUS", "C", "AMT", "ZTS", "MO", "CI", "SO", "CB", "DUK", "MDLZ",
    "SCHW", "FI", "PGR", "BDX", "BSX", "CL", "EOG", "HUM", "ETN", "SLB"
};

std::string getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%H:%M:%S");
    return ss.str();
}

struct MarketData {
    std::string symbol;
    double bid;
    double ask;
    double last;
    int64_t volume;
    int64_t timestamp;

    double spread() const { return ask - bid; }
    double mid() const { return (bid + ask) / 2.0; }
};

struct Trade {
    std::string symbol;
    bool isBuy;
    double price;
    int quantity;
    int64_t timestamp;
    std::string strategy;
};

struct Position {
    int quantity;
    double avgEntryPrice;
    double totalCost;
    std::vector<Trade> trades;

    Position() : quantity(0), avgEntryPrice(0.0), totalCost(0.0) {}
};

struct Signal {
    enum Action { NONE, BUY, SELL };
    Action action;
    double confidence;
    std::string strategy;
    double stopLoss;
    double takeProfit;
};

class MarketDataProvider {
private:
    std::map<std::string, MarketData> latestData;
    std::map<std::string, std::deque<double>> priceHistory;
    std::mutex dataMutex;
    std::atomic<bool> running;
    std::thread dataThread;
    std::mt19937 gen;

    void simulateData() {
        std::map<std::string, double> prices;
        std::map<std::string, double> volatility;
        std::map<std::string, double> drift;

        for (size_t i = 0; i < ALL_STOCKS.size(); i++) {
            prices[ALL_STOCKS[i]] = 100.0 + (gen() % 400);
            volatility[ALL_STOCKS[i]] = 0.3 + (gen() % 15) / 10.0; // Reduced volatility
            drift[ALL_STOCKS[i]] = (gen() % 100 - 50) / 20000.0; // Reduced drift
        }

        while (running) {
            std::lock_guard<std::mutex> lock(dataMutex);
            auto now = std::chrono::system_clock::now().time_since_epoch().count();

            for (size_t i = 0; i < ALL_STOCKS.size(); i++) {
                std::string symbol = ALL_STOCKS[i];
                double price = prices[symbol];
                double vol = volatility[symbol];
                double d = drift[symbol];

                std::normal_distribution<double> dist(0, vol);
                double randomChange = dist(gen) * 0.0008; // Reduced change magnitude
                price = price * (1.0 + randomChange + d);
                prices[symbol] = price;

                double spreadPct = 0.0001;
                MarketData data;
                data.symbol = symbol;
                data.bid = price * (1.0 - spreadPct);
                data.ask = price * (1.0 + spreadPct);
                data.last = price;
                data.volume = 1000000 + gen() % 500000;
                data.timestamp = now;

                latestData[symbol] = data;
                priceHistory[symbol].push_back(price);
                if (priceHistory[symbol].size() > 200) {
                    priceHistory[symbol].pop_front();
                }

                if (gen() % 500 == 0) {
                    drift[symbol] = (gen() % 100 - 50) / 20000.0;
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

public:
    MarketDataProvider() : running(false), gen(std::random_device{}()) {}

    void start() {
        running = true;
        dataThread = std::thread(&MarketDataProvider::simulateData, this);
    }

    MarketData getData(const std::string& symbol) {
        std::lock_guard<std::mutex> lock(dataMutex);
        return latestData[symbol];
    }

    std::vector<double> getHistory(const std::string& symbol) {
        std::lock_guard<std::mutex> lock(dataMutex);
        std::vector<double> result;
        if (priceHistory.find(symbol) != priceHistory.end()) {
            const std::deque<double>& hist = priceHistory[symbol];
            for (size_t i = 0; i < hist.size(); i++) {
                result.push_back(hist[i]);
            }
        }
        return result;
    }

    ~MarketDataProvider() {
        running = false;
        if (dataThread.joinable()) dataThread.join();
    }
};

class TradingEngine {
private:
    std::map<std::string, Position> positions;
    double cash;
    double initialCash;
    std::mutex execMutex;
    int tradeCount;
    int winningTrades;
    int losingTrades;
    std::vector<Trade> allTrades;
    double totalRealizedPnL;

public:
    TradingEngine(double capital) : cash(capital), initialCash(capital),
        tradeCount(0), winningTrades(0),
        losingTrades(0), totalRealizedPnL(0.0) {
    }

    bool executeBuy(const std::string& symbol, double price, int quantity, const std::string& strategy) {
        std::lock_guard<std::mutex> lock(execMutex);

        double cost = price * quantity;
        double commission = cost * 0.001;
        double totalCost = cost + commission;

        if (cash < totalCost) return false;

        Position& pos = positions[symbol];

        Trade trade;
        trade.symbol = symbol;
        trade.isBuy = true;
        trade.price = price;
        trade.quantity = quantity;
        trade.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
        trade.strategy = strategy;

        pos.trades.push_back(trade);
        pos.totalCost += totalCost;
        pos.quantity += quantity;
        pos.avgEntryPrice = pos.totalCost / pos.quantity;

        cash -= totalCost;
        tradeCount++;
        allTrades.push_back(trade);

        std::cout << Color::GREEN << "[" << getCurrentTime() << "] BUY  "
            << std::setw(6) << symbol << " " << std::setw(3) << quantity
            << " @ $" << std::fixed << std::setprecision(2) << price
            << " | Cost: $" << std::setprecision(2) << totalCost
            << " (" << strategy << ")" << Color::RESET << "\n";

        return true;
    }

    bool executeSell(const std::string& symbol, double price, int quantity, const std::string& strategy) {
        std::lock_guard<std::mutex> lock(execMutex);

        Position& pos = positions[symbol];
        if (pos.quantity < quantity) return false;

        double revenue = price * quantity;
        double commission = revenue * 0.001;
        double netRevenue = revenue - commission;

        double costBasis = pos.avgEntryPrice * quantity;
        double pnl = netRevenue - costBasis;

        Trade trade;
        trade.symbol = symbol;
        trade.isBuy = false;
        trade.price = price;
        trade.quantity = quantity;
        trade.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
        trade.strategy = strategy;

        pos.quantity -= quantity;
        if (pos.quantity > 0) {
            pos.totalCost = pos.avgEntryPrice * pos.quantity;
        }
        else {
            pos.totalCost = 0;
            pos.avgEntryPrice = 0;
        }

        cash += netRevenue;
        totalRealizedPnL += pnl;
        tradeCount++;
        allTrades.push_back(trade);

        if (pnl > 0) {
            winningTrades++;
        }
        else {
            losingTrades++;
        }

        std::string pnlColor = (pnl >= 0) ? Color::GREEN : Color::RED;
        std::cout << Color::RED << "[" << getCurrentTime() << "] SELL "
            << std::setw(6) << symbol << " " << std::setw(3) << quantity
            << " @ $" << std::fixed << std::setprecision(2) << price
            << " | " << pnlColor << "P&L: $" << std::setprecision(2) << pnl
            << Color::RESET << " (" << strategy << ")" << Color::RESET << "\n";

        return true;
    }

    Position getPosition(const std::string& symbol) {
        std::lock_guard<std::mutex> lock(execMutex);
        return positions[symbol];
    }

    double getCash() {
        std::lock_guard<std::mutex> lock(execMutex);
        return cash;
    }

    double getPortfolioValue(const std::map<std::string, double>& currentPrices) {
        std::lock_guard<std::mutex> lock(execMutex);
        double total = cash;

        for (auto it = positions.begin(); it != positions.end(); ++it) {
            std::string symbol = it->first;
            Position pos = it->second;

            if (pos.quantity > 0 && currentPrices.find(symbol) != currentPrices.end()) {
                double currentPrice = currentPrices.at(symbol);
                total += currentPrice * pos.quantity;
            }
        }

        return total;
    }

    double getUnrealizedPnL(const std::map<std::string, double>& currentPrices) {
        std::lock_guard<std::mutex> lock(execMutex);
        double unrealized = 0;

        for (auto it = positions.begin(); it != positions.end(); ++it) {
            std::string symbol = it->first;
            Position pos = it->second;

            if (pos.quantity > 0 && currentPrices.find(symbol) != currentPrices.end()) {
                double currentPrice = currentPrices.at(symbol);
                double marketValue = currentPrice * pos.quantity;
                double costBasis = pos.avgEntryPrice * pos.quantity;
                unrealized += (marketValue - costBasis);
            }
        }

        return unrealized;
    }

    double getRealizedPnL() {
        std::lock_guard<std::mutex> lock(execMutex);
        return totalRealizedPnL;
    }

    double getTotalPnL(const std::map<std::string, double>& currentPrices) {
        return totalRealizedPnL + getUnrealizedPnL(currentPrices);
    }

    int getTradeCount() { return tradeCount; }
    int getOpenPositions() {
        std::lock_guard<std::mutex> lock(execMutex);
        int count = 0;
        for (auto it = positions.begin(); it != positions.end(); ++it) {
            if (it->second.quantity > 0) count++;
        }
        return count;
    }

    void printSummary(const std::map<std::string, double>& currentPrices) {
        std::lock_guard<std::mutex> lock(execMutex);

        std::cout << "\n" << Color::BOLD << Color::CYAN;
        std::cout << "============================================================\n";
        std::cout << "                   TRADING SUMMARY                          \n";
        std::cout << "============================================================\n";
        std::cout << Color::RESET;

        double portfolioValue = cash;
        double unrealizedPnL = 0;

        for (auto it = positions.begin(); it != positions.end(); ++it) {
            std::string symbol = it->first;
            Position pos = it->second;

            if (pos.quantity > 0 && currentPrices.find(symbol) != currentPrices.end()) {
                double currentPrice = currentPrices.at(symbol);
                double marketValue = currentPrice * pos.quantity;
                portfolioValue += marketValue;
                unrealizedPnL += (marketValue - pos.avgEntryPrice * pos.quantity);
            }
        }

        double totalPnL = totalRealizedPnL + unrealizedPnL;
        double returnPct = (totalPnL / initialCash) * 100;

        std::cout << Color::BOLD << "Initial Capital:      " << Color::RESET
            << "$" << std::fixed << std::setprecision(2) << initialCash << "\n";
        std::cout << Color::BOLD << "Final Portfolio Value:" << Color::RESET
            << " $" << portfolioValue << "\n";
        std::cout << Color::BOLD << "Cash Remaining:       " << Color::RESET
            << "$" << cash << "\n\n";

        std::cout << Color::BOLD << "Realized P&L:         " << Color::RESET;
        if (totalRealizedPnL >= 0) {
            std::cout << Color::GREEN << "+$" << totalRealizedPnL << Color::RESET;
        }
        else {
            std::cout << Color::RED << "$" << totalRealizedPnL << Color::RESET;
        }
        std::cout << "\n";

        std::cout << Color::BOLD << "Unrealized P&L:       " << Color::RESET;
        if (unrealizedPnL >= 0) {
            std::cout << Color::GREEN << "+$" << unrealizedPnL << Color::RESET;
        }
        else {
            std::cout << Color::RED << "$" << unrealizedPnL << Color::RESET;
        }
        std::cout << "\n";

        std::cout << Color::BOLD << "Total P&L:            " << Color::RESET;
        if (totalPnL >= 0) {
            std::cout << Color::GREEN << "+$" << totalPnL << " (+" << std::fixed << std::setprecision(2) << returnPct << "%)" << Color::RESET;
        }
        else {
            std::cout << Color::RED << "$" << totalPnL << " (" << std::fixed << std::setprecision(2) << returnPct << "%)" << Color::RESET;
        }
        std::cout << "\n\n";

        std::cout << Color::BOLD << "Total Trades:         " << Color::RESET << tradeCount << "\n";
        std::cout << Color::BOLD << "Winning Trades:       " << Color::RESET
            << Color::GREEN << winningTrades << Color::RESET << "\n";
        std::cout << Color::BOLD << "Losing Trades:        " << Color::RESET
            << Color::RED << losingTrades << Color::RESET << "\n";

        if (winningTrades + losingTrades > 0) {
            double winRate = (static_cast<double>(winningTrades) / (winningTrades + losingTrades)) * 100;
            std::cout << Color::BOLD << "Win Rate:             " << Color::RESET
                << std::setprecision(1) << winRate << "%\n";
        }

        int openPos = 0;
        for (auto it = positions.begin(); it != positions.end(); ++it) {
            if (it->second.quantity > 0) openPos++;
        }

        if (openPos > 0) {
            std::cout << "\n" << Color::BOLD << Color::YELLOW << "Open Positions: " << openPos << "\n" << Color::RESET;
            for (auto it = positions.begin(); it != positions.end(); ++it) {
                std::string symbol = it->first;
                Position pos = it->second;
                if (pos.quantity > 0 && currentPrices.find(symbol) != currentPrices.end()) {
                    double currentPrice = currentPrices.at(symbol);
                    double posUnrealized = (currentPrice - pos.avgEntryPrice) * pos.quantity;
                    std::string posColor = (posUnrealized >= 0) ? Color::GREEN : Color::RED;

                    std::cout << "  " << symbol << ": " << pos.quantity
                        << " @ $" << std::setprecision(2) << pos.avgEntryPrice
                        << " (Current: $" << currentPrice << ") "
                        << posColor << "$" << posUnrealized << Color::RESET << "\n";
                }
            }
        }
        std::cout << "\n";
    }
};

class TradingStrategy {
protected:
    std::string name;

public:
    TradingStrategy(const std::string& n) : name(n) {}
    virtual Signal analyze(const std::string& symbol, const std::vector<double>& prices,
        const MarketData& current) = 0;
    std::string getName() const { return name; }
    virtual ~TradingStrategy() {}
};

class ImprovedMeanReversionStrategy : public TradingStrategy {
public:
    ImprovedMeanReversionStrategy() : TradingStrategy("MeanRev") {}

    Signal analyze(const std::string& symbol, const std::vector<double>& prices,
        const MarketData& current) override {
        Signal sig;
        sig.action = Signal::NONE;
        sig.confidence = 0.0;
        sig.strategy = name;

        if (prices.size() < 50) return sig;

        double sum = 0;
        for (size_t i = prices.size() - 50; i < prices.size(); i++) {
            sum += prices[i];
        }
        double mean = sum / 50;

        double sqSum = 0;
        for (size_t i = prices.size() - 50; i < prices.size(); i++) {
            sqSum += (prices[i] - mean) * (prices[i] - mean);
        }
        double stdev = std::sqrt(sqSum / 50);

        if (stdev < 0.01) return sig;

        double currentPrice = current.mid();
        double zscore = (currentPrice - mean) / stdev;

        // Check recent trend to avoid catching falling knives
        double recentTrend = (prices[prices.size() - 1] - prices[prices.size() - 5]) / prices[prices.size() - 5];

        // Balanced thresholds for more trading opportunities
        if (zscore < -1.8 && recentTrend > -0.012 && stdev / mean < 0.04) {
            sig.action = Signal::BUY;
            sig.confidence = 0.85;
            sig.takeProfit = mean;
            sig.stopLoss = currentPrice * 0.985;
        }
        else if (zscore > 1.8 && recentTrend < 0.012 && stdev / mean < 0.04) {
            sig.action = Signal::SELL;
            sig.confidence = 0.85;
            sig.takeProfit = mean;
            sig.stopLoss = currentPrice * 1.015;
        }

        return sig;
    }
};

class TrendFollowingStrategy : public TradingStrategy {
public:
    TrendFollowingStrategy() : TradingStrategy("TrendFollow") {}

    Signal analyze(const std::string& symbol, const std::vector<double>& prices,
        const MarketData& current) override {
        Signal sig;
        sig.action = Signal::NONE;
        sig.confidence = 0.0;
        sig.strategy = name;

        if (prices.size() < 30) return sig;

        double shortSum = 0;
        for (size_t i = prices.size() - 10; i < prices.size(); i++) {
            shortSum += prices[i];
        }
        double shortMA = shortSum / 10;

        double longSum = 0;
        for (size_t i = prices.size() - 30; i < prices.size(); i++) {
            longSum += prices[i];
        }
        double longMA = longSum / 30;

        double prevShortSum = 0;
        for (size_t i = prices.size() - 11; i < prices.size() - 1; i++) {
            prevShortSum += prices[i];
        }
        double prevShortMA = prevShortSum / 10;

        bool crossedUp = (prevShortMA <= longMA && shortMA > longMA);
        bool crossedDown = (prevShortMA >= longMA && shortMA < longMA);

        double momentum = (shortMA - longMA) / longMA;

        // Check for strong momentum
        double recentMomentum = (prices[prices.size() - 1] - prices[prices.size() - 5]) / prices[prices.size() - 5];

        if (crossedUp && momentum > 0.003 && recentMomentum > 0) {
            sig.action = Signal::BUY;
            sig.confidence = 0.84;
            sig.takeProfit = current.mid() * 1.015;
            sig.stopLoss = current.mid() * 0.992;
        }
        else if (crossedDown && momentum < -0.003 && recentMomentum < 0) {
            sig.action = Signal::SELL;
            sig.confidence = 0.84;
            sig.takeProfit = current.mid() * 0.985;
            sig.stopLoss = current.mid() * 1.008;
        }

        return sig;
    }
};

class BreakoutStrategy : public TradingStrategy {
public:
    BreakoutStrategy() : TradingStrategy("Breakout") {}

    Signal analyze(const std::string& symbol, const std::vector<double>& prices,
        const MarketData& current) override {
        Signal sig;
        sig.action = Signal::NONE;
        sig.confidence = 0.0;
        sig.strategy = name;

        if (prices.size() < 30) return sig;

        double high = prices[prices.size() - 30];
        double low = prices[prices.size() - 30];

        for (size_t i = prices.size() - 30; i < prices.size() - 1; i++) {
            if (prices[i] > high) high = prices[i];
            if (prices[i] < low) low = prices[i];
        }

        double range = high - low;
        double currentPrice = current.mid();

        // Check consolidation period before breakout
        double recentHigh = prices[prices.size() - 10];
        double recentLow = prices[prices.size() - 10];
        for (size_t i = prices.size() - 10; i < prices.size(); i++) {
            if (prices[i] > recentHigh) recentHigh = prices[i];
            if (prices[i] < recentLow) recentLow = prices[i];
        }
        double recentRange = recentHigh - recentLow;

        // Only trade if breakout is significant and follows consolidation
        if (currentPrice > high && range / high > 0.015 && recentRange / range < 0.65) {
            sig.action = Signal::BUY;
            sig.confidence = 0.81;
            sig.takeProfit = currentPrice * 1.02;
            sig.stopLoss = high * 0.996;
        }
        else if (currentPrice < low && range / low > 0.015 && recentRange / range < 0.65) {
            sig.action = Signal::SELL;
            sig.confidence = 0.81;
            sig.takeProfit = currentPrice * 0.98;
            sig.stopLoss = low * 1.004;
        }

        return sig;
    }
};

class HFTSystem {
private:
    std::unique_ptr<MarketDataProvider> dataProvider;
    std::unique_ptr<TradingEngine> engine;
    std::vector<std::unique_ptr<TradingStrategy>> strategies;
    std::atomic<bool> running;
    std::thread tradingThread;
    std::thread displayThread;
    std::map<std::string, double> entryPrices;
    double initialCapital;

    void tradingLoop() {
        std::cout << Color::YELLOW << "\n[SYSTEM] Trading engine started - scanning stocks...\n"
            << Color::RESET << "\n";

        while (running) {
            for (size_t i = 0; i < ALL_STOCKS.size(); i++) {
                std::string symbol = ALL_STOCKS[i];
                MarketData current = dataProvider->getData(symbol);
                std::vector<double> history = dataProvider->getHistory(symbol);

                if (current.symbol.empty() || history.size() < 50) continue;

                Position pos = engine->getPosition(symbol);

                // Improved risk management for open positions
                if (pos.quantity > 0) {
                    double currentPrice = current.mid();
                    double pnlPercent = (currentPrice - pos.avgEntryPrice) / pos.avgEntryPrice;

                    // Balanced stop loss and take profit
                    if (pnlPercent < -0.018 || pnlPercent > 0.022) {
                        engine->executeSell(symbol, current.bid, pos.quantity, "StopLoss/TakeProfit");
                    }
                }

                // Only enter new positions if we're not overexposed
                if (pos.quantity == 0) {
                    for (size_t j = 0; j < strategies.size(); j++) {
                        Signal signal = strategies[j]->analyze(symbol, history, current);

                        if (signal.action == Signal::BUY && signal.confidence > 0.80) {
                            double portfolioValue = engine->getCash();
                            // Balanced position sizing (2% per trade for more activity)
                            int qty = static_cast<int>((portfolioValue * 0.02) / current.ask);

                            // Allow up to 25 open positions for more trading
                            if (qty > 0 && engine->getOpenPositions() < 25) {
                                engine->executeBuy(symbol, current.ask, qty, signal.strategy);
                                entryPrices[symbol] = current.ask;
                            }
                        }
                        else if (signal.action == Signal::SELL && signal.confidence > 0.80) {
                            if (pos.quantity > 0) {
                                engine->executeSell(symbol, current.bid, pos.quantity, signal.strategy);
                            }
                        }
                    }
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(150));
        }
    }

    void displayLoop() {
        while (running) {
            std::map<std::string, double> prices;
            for (size_t i = 0; i < ALL_STOCKS.size(); i++) {
                MarketData data = dataProvider->getData(ALL_STOCKS[i]);
                if (!data.symbol.empty()) {
                    prices[ALL_STOCKS[i]] = data.mid();
                }
            }

            double portfolioValue = engine->getPortfolioValue(prices);
            double totalPnL = portfolioValue - initialCapital;
            double returnPct = (totalPnL / initialCapital) * 100;

            std::cout << "\r" << Color::BOLD << "Portfolio: $"
                << std::fixed << std::setprecision(2) << portfolioValue
                << " | P&L: ";

            if (totalPnL >= 0) {
                std::cout << Color::GREEN << "+$" << totalPnL << " (+" << std::setprecision(1) << returnPct << "%)";
            }
            else {
                std::cout << Color::RED << "$" << totalPnL << " (" << std::setprecision(1) << returnPct << "%)";
            }

            std::cout << Color::RESET << " | Trades: " << engine->getTradeCount()
                << " | Open: " << engine->getOpenPositions()
                << "     " << std::flush;

            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }

public:
    HFTSystem(double capital) : running(false), initialCapital(capital) {
        dataProvider = std::make_unique<MarketDataProvider>();
        engine = std::make_unique<TradingEngine>(capital);

        strategies.push_back(std::make_unique<ImprovedMeanReversionStrategy>());
        strategies.push_back(std::make_unique<TrendFollowingStrategy>());
        strategies.push_back(std::make_unique<BreakoutStrategy>());
    }

    void start() {
        std::cout << Color::BOLD << Color::GREEN;
        std::cout << "\n============================================================\n";
        std::cout << "     ULTRA-EFFICIENT HFT SYSTEM - PROFITABLE EDITION        \n";
        std::cout << "============================================================\n";
        std::cout << Color::RESET << "\n";

        std::cout << Color::CYAN << "[INIT] Starting with $"
            << std::fixed << std::setprecision(2) << initialCapital << " capital\n" << Color::RESET;
        std::cout << Color::CYAN << "[INIT] Initializing market data for "
            << ALL_STOCKS.size() << " stocks...\n" << Color::RESET;

        dataProvider->start();

        std::cout << Color::CYAN << "[INIT] Warming up algorithms...\n" << Color::RESET;
        std::this_thread::sleep_for(std::chrono::seconds(3));

        std::cout << Color::GREEN << "[READY] System ready - starting trading!\n" << Color::RESET;
        std::cout << Color::BOLD << "\nActive Strategies:\n" << Color::RESET;
        for (size_t i = 0; i < strategies.size(); i++) {
            std::cout << "  - " << Color::MAGENTA << strategies[i]->getName() << Color::RESET << "\n";
        }
        std::cout << "\n" << Color::YELLOW << "Press ENTER to stop...\n\n" << Color::RESET;

        running = true;
        tradingThread = std::thread(&HFTSystem::tradingLoop, this);
        displayThread = std::thread(&HFTSystem::displayLoop, this);
    }

    void stop() {
        std::cout << "\n\n" << Color::YELLOW << "[STOP] Shutting down trading engine...\n"
            << Color::RESET;
        running = false;
        if (tradingThread.joinable()) tradingThread.join();
        if (displayThread.joinable()) displayThread.join();

        std::map<std::string, double> prices;
        for (size_t i = 0; i < ALL_STOCKS.size(); i++) {
            MarketData data = dataProvider->getData(ALL_STOCKS[i]);
            if (!data.symbol.empty()) {
                prices[ALL_STOCKS[i]] = data.mid();
            }
        }

        engine->printSummary(prices);
        std::cout << Color::GREEN << "\n[COMPLETE] Session ended successfully!\n" << Color::RESET;
    }
};

int main() {
    std::cout << Color::BOLD << Color::CYAN;
    std::cout << "\n============================================================\n";
    std::cout << "          HIGH-FREQUENCY TRADING SYSTEM v3.0                \n";
    std::cout << "============================================================\n";
    std::cout << Color::RESET << "\n";

    double capital = 0;
    std::cout << Color::YELLOW << "Enter starting capital (e.g., 100000): $" << Color::RESET;
    std::cin >> capital;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    if (capital < 1000) {
        std::cout << Color::RED << "Minimum capital is $1,000\n" << Color::RESET;
        return 1;
    }

    HFTSystem system(capital);
    system.start();

    std::cin.get();

    system.stop();

    return 0;
}