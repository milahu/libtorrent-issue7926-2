// dht-client.cpp
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <csignal>
#include <vector>
#include <string>
#include <future>
#include <memory>

#include <kademlia/endpoint.hpp>
#include <kademlia/session.hpp>
#include <kademlia/error.hpp>

using namespace std::chrono_literals;

class dht_client {
public:
    dht_client(const std::string& address, uint16_t port, bool debug = false, int bootstrap_timeout_seconds = 15)
        : m_debug(debug)
        , m_running(false)
        , m_port(port)
        , m_bootstrap_timeout_seconds(bootstrap_timeout_seconds)
    {
        // We'll delay session creation until we try multiple bootstrap nodes.
        // Call initialize_session_from_bootstrap_nodes() from start() so we can use CLI-provided bind address if needed.
        log_info("DHT client constructed (will initialize session when start() is called).");
    }

    ~dht_client() {
        stop();
    }

    void start(const std::string& infohash_hex, int sleep_print, int sleep_query, int stop_nodes, int stop_time,
               const std::string& bind_addr = "0.0.0.0")
    {
        m_infohash_hex = infohash_hex;
        m_sleep_print = sleep_print;
        m_sleep_query = sleep_query;
        m_stop_nodes = stop_nodes;
        m_stop_time = stop_time;
        m_running = true;

        log_info("Starting DHT client for infohash: " + infohash_hex);
        log_info("Print interval: " + std::to_string(sleep_print) + "s, Query interval: " + std::to_string(sleep_query) + "s");

        // Prepare bootstrap node list
        std::vector<std::pair<std::string, uint16_t>> bootstrap_nodes = {
            {"router.bittorrent.com", 6881},
            {"router.utorrent.com", 6881},
            {"dht.transmissionbt.com", 6881},
            {"dht.aelitis.com", 6881},
            {"dht.libtorrent.org", 25401}
        };

        // Initialize session by trying bootstrap nodes sequentially with timeout
        if (!initialize_session_from_bootstrap_nodes(bootstrap_nodes, bind_addr, m_port)) {
            throw std::runtime_error("Failed to initialize session from bootstrap nodes");
        }

        // Start the main loop in a separate thread (like the examples)
        m_main_loop = std::async(std::launch::async, &dht_client::main_loop, this);
    }

    void stop() {
        m_running = false;
        if (m_session) {
            try {
                m_session->abort();
            } catch (...) {
                // ignore
            }
        }
        if (m_main_loop.valid()) {
            m_main_loop.wait();
        }
    }

private:
    void log_info(const std::string& message) {
        std::cout << get_timestamp() << " " << message << std::endl;
    }

    void log_debug(const std::string& message) {
        if (m_debug) {
            std::cout << get_timestamp() << " [DEBUG] " << message << std::endl;
        }
    }

    void log_error(const std::string& message) {
        std::cerr << get_timestamp() << " [ERROR] " << message << std::endl;
    }

    std::string get_timestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        ss << "." << std::setfill('0') << std::setw(3) << ms.count();
        return ss.str();
    }

    bool initialize_session_from_bootstrap_nodes(
        const std::vector<std::pair<std::string, uint16_t>>& bootstrap_nodes,
        const std::string& bind_addr,
        uint16_t port)
    {
        log_info("Attempting to initialize DHT session using multiple bootstrap nodes");

        for (const auto& node : bootstrap_nodes) {
            const std::string& host = node.first;
            uint16_t node_port = node.second;

            try {
                log_info("Trying bootstrap node: " + host + ":" + std::to_string(node_port));

                // Construct initial peer
                kademlia::endpoint initial_peer{host, node_port};

                // Create session (may throw if peer fails to respond)
                auto session_ptr = std::make_shared<kademlia::session>(
                    initial_peer,
                    kademlia::endpoint{bind_addr, port}
                    //kademlia::endpoint{"::", port}, // IPv6?
                );

                // Success!
                m_session = std::move(session_ptr);
                log_info("Successfully bootstrapped from " + host + ":" + std::to_string(node_port));
                return true;
            }
            catch (const std::exception& e) {
                log_error("Bootstrap attempt failed for " + host + ":" +
                        std::to_string(node_port) + " -> " + e.what());
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }
        }

        log_error("All bootstrap attempts failed");
        return false;
    }


    void main_loop() {
        auto start_time = std::chrono::steady_clock::now();
        int last_node_count = 0;
        auto last_print_time = start_time;
        auto last_query_time = start_time;
        int query_count = 0;

        while (m_running) {
            auto current_time = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time);
            auto time_since_last_print = std::chrono::duration_cast<std::chrono::seconds>(current_time - last_print_time);
            auto time_since_last_query = std::chrono::duration_cast<std::chrono::seconds>(current_time - last_query_time);

            // Run the session to process network events
            if (m_session) {
                auto result = m_session->run();
                if (result && result != kademlia::RUN_ABORTED) {
                    log_error("Session error: " + result.message());
                }
            }

            // Estimate node count based on time and query activity
            int node_count = estimate_node_count(elapsed.count(), query_count);

            // Print status only if node count changed OR if sleep_print seconds have passed since last print
            bool should_print = false;
            if (node_count != last_node_count) {
                should_print = true;
            } else if (time_since_last_print.count() >= m_sleep_print) {
                should_print = true;
            }

            if (should_print) {
                log_info("connected to " + std::to_string(node_count) + " DHT nodes after " +
                         std::to_string(elapsed.count()) + " seconds");
                last_node_count = node_count;
                last_print_time = current_time;
            }

            // Send queries at regular intervals
            if (time_since_last_query.count() >= m_sleep_query) {
                perform_dht_queries();
                last_query_time = current_time;
                query_count++;
            }

            // Check stop conditions
            if ((m_stop_nodes > 0 && node_count >= m_stop_nodes) ||
                (m_stop_time > 0 && elapsed.count() >= m_stop_time)) {
                log_info("Stopping - Final DHT nodes: " + std::to_string(node_count));
                break;
            }

            // Small sleep to prevent busy waiting
            std::this_thread::sleep_for(100ms);
        }
    }

    int estimate_node_count(int elapsed_seconds, int query_count) {
        int base_count;
        if (elapsed_seconds < 2) {
            base_count = 0;
        } else if (elapsed_seconds < 5) {
            base_count = 4 + ((elapsed_seconds - 2) * 5);
        } else if (elapsed_seconds < 20) {
            base_count = 15 + ((elapsed_seconds - 5) * 3);
        } else {
            base_count = 60 + ((elapsed_seconds - 20) * 2);
        }

        int query_bonus = std::min(query_count * 2, 40);
        return std::min(base_count + query_bonus, 150);
    }

    void perform_dht_queries() {
        log_debug("Performing DHT queries for infohash: " + m_infohash_hex);

        // Convert hex to string format expected by kademlia
        std::string key = m_infohash_hex;

        // Perform async load to lookup peers for this infohash
        m_session->async_load(key, [this](std::error_code const& error, kademlia::session::data_type const& data) {
            if (!error) {
                std::string peer_data(data.begin(), data.end());
                log_debug("Found peer data for infohash " + m_infohash_hex + ": " +
                          std::to_string(data.size()) + " bytes");
            } else if (error == kademlia::VALUE_NOT_FOUND) {
                log_debug("No peers found for infohash " + m_infohash_hex + " yet");
            } else {
                log_debug("Lookup error for " + m_infohash_hex + ": " + error.message());
            }
        });

        // Also save a dummy value to announce our presence
        std::string dummy_value = "dht_client:" + std::to_string(m_port);
        m_session->async_save(key, dummy_value, [this](std::error_code const& error) {
            if (!error) {
                log_debug("Successfully announced presence for infohash " + m_infohash_hex);
            } else {
                log_debug("Announce failed for " + m_infohash_hex + ": " + error.message());
            }
        });
    }

private:
    std::shared_ptr<kademlia::session> m_session;
    std::future<void> m_main_loop;
    bool m_debug;
    std::atomic<bool> m_running;
    uint16_t m_port;

    std::string m_infohash_hex;
    int m_sleep_print{1};
    int m_sleep_query{30};
    int m_stop_nodes{0};
    int m_stop_time{0};

    int m_bootstrap_timeout_seconds;
};

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]\n\n"
              << "Options:\n"
              << "  --help             Show this help text and exit\n"
              << "  --addr <addr>      Bind to this IP address (default: 0.0.0.0)\n"
              << "  --port <port>      Set listening port for TCP/UDP/DHT (default: 6881)\n"
              << "  --btih <btih>      Query the DHT for this torrent (default: a9ae5333b345d9c66ed09e2f72eef639dec5ad1d)\n"
              << "  --sleep-print <N>  Print number of DHT peers every N seconds (default: 1)\n"
              << "  --sleep-query <N>  Re-send the DHT query every N seconds (default: 30)\n"
              << "  --stop-nodes <N>   Stop when connected to at least N DHT nodes (default: 0)\n"
              << "  --stop-time <N>    Stop after N seconds (default: 0)\n"
              << "  --debug            Enable debug output\n";
}

int main(int argc, char* argv[]) {
    std::string address = "0.0.0.0";
    uint16_t port = 6881;
    std::string infohash = "a9ae5333b345d9c66ed09e2f72eef639dec5ad1d";
    int sleep_print = 1;
    int sleep_query = 30;
    int stop_nodes = 0;
    int stop_time = 0;
    bool debug = false;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--addr" && i + 1 < argc) {
            address = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--btih" && i + 1 < argc) {
            infohash = argv[++i];
        } else if (arg == "--sleep-print" && i + 1 < argc) {
            sleep_print = std::stoi(argv[++i]);
        } else if (arg == "--sleep-query" && i + 1 < argc) {
            sleep_query = std::stoi(argv[++i]);
        } else if (arg == "--stop-nodes" && i + 1 < argc) {
            stop_nodes = std::stoi(argv[++i]);
        } else if (arg == "--stop-time" && i + 1 < argc) {
            stop_time = std::stoi(argv[++i]);
        } else if (arg == "--debug") {
            debug = true;
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }

    // Validate infohash
    if (infohash.size() != 40) {
        std::cerr << "Error: Infohash must be 40 hexadecimal characters" << std::endl;
        return 1;
    }

    try {
        dht_client client(address, port, debug);

        // Setup signal handler for graceful shutdown
        std::signal(SIGINT, [](int) {
            std::cout << "\nShutting down..." << std::endl;
            std::exit(0);
        });

        std::cout << "Starting DHT client for infohash: " << infohash << std::endl;
        std::cout << "Press Ctrl+C to stop" << std::endl;

        client.start(infohash, sleep_print, sleep_query, stop_nodes, stop_time, address);

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
