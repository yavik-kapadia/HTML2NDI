#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>

// Forward declare httplib
namespace httplib {
class Server;
}

namespace html2ndi {

// Forward declarations
class Application;

/**
 * HTTP control server.
 * Provides REST API for controlling the application.
 */
class HttpServer {
public:
    /**
     * Create an HTTP server.
     * @param app Application instance for callbacks
     * @param host Host to bind to
     * @param port Port to listen on
     */
    HttpServer(Application* app, const std::string& host, uint16_t port);
    ~HttpServer();
    
    // Non-copyable, non-movable
    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;
    
    /**
     * Start the HTTP server.
     * @return true if server started successfully
     */
    bool start();
    
    /**
     * Stop the HTTP server.
     */
    void stop();
    
    /**
     * Check if server is running.
     */
    bool is_running() const { return running_; }
    
    /**
     * Get the listen URL.
     */
    std::string listen_url() const;

private:
    void setup_routes();
    
    Application* app_;
    std::string host_;
    uint16_t port_;
    
    std::unique_ptr<httplib::Server> server_;
    std::thread server_thread_;
    std::atomic<bool> running_{false};
};

} // namespace html2ndi

