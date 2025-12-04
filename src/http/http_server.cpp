/**
 * HTTP control server implementation.
 */

#include "html2ndi/http/http_server.h"
#include "html2ndi/application.h"
#include "html2ndi/utils/logger.h"

#include <httplib.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace html2ndi {

HttpServer::HttpServer(Application* app, const std::string& host, uint16_t port)
    : app_(app)
    , host_(host)
    , port_(port)
    , server_(std::make_unique<httplib::Server>()) {
}

HttpServer::~HttpServer() {
    stop();
}

bool HttpServer::start() {
    if (running_) {
        return true;
    }
    
    setup_routes();
    
    // Start server in background thread
    server_thread_ = std::thread([this]() {
        LOG_DEBUG("HTTP server thread starting on %s:%d", host_.c_str(), port_);
        
        if (!server_->listen(host_.c_str(), port_)) {
            LOG_ERROR("Failed to start HTTP server on %s:%d", host_.c_str(), port_);
        }
        
        LOG_DEBUG("HTTP server thread exited");
    });
    
    // Give server time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    running_ = true;
    return true;
}

void HttpServer::stop() {
    if (!running_) {
        return;
    }
    
    LOG_DEBUG("Stopping HTTP server...");
    
    server_->stop();
    
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
    
    running_ = false;
    LOG_DEBUG("HTTP server stopped");
}

std::string HttpServer::listen_url() const {
    return "http://" + host_ + ":" + std::to_string(port_);
}

void HttpServer::setup_routes() {
    // CORS headers
    auto add_cors = [](httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
    };
    
    // OPTIONS preflight
    server_->Options(".*", [add_cors](const httplib::Request&, httplib::Response& res) {
        add_cors(res);
        res.status = 204;
    });
    
    // GET /status - Get current status
    server_->Get("/status", [this, add_cors](const httplib::Request&, httplib::Response& res) {
        add_cors(res);
        
        json status = {
            {"url", app_->current_url()},
            {"width", app_->config().width},
            {"height", app_->config().height},
            {"fps", app_->config().fps},
            {"actual_fps", app_->current_fps()},
            {"ndi_name", app_->config().ndi_name},
            {"ndi_connections", app_->ndi_connection_count()},
            {"running", !app_->is_shutting_down()}
        };
        
        res.set_content(status.dump(2), "application/json");
    });
    
    // POST /seturl - Set URL to load
    server_->Post("/seturl", [this, add_cors](const httplib::Request& req, httplib::Response& res) {
        add_cors(res);
        
        try {
            auto body = json::parse(req.body);
            
            if (!body.contains("url") || !body["url"].is_string()) {
                res.status = 400;
                res.set_content(R"({"error": "Missing 'url' field"})", "application/json");
                return;
            }
            
            std::string url = body["url"].get<std::string>();
            
            LOG_INFO("HTTP API: seturl to %s", url.c_str());
            app_->set_url(url);
            
            json response = {
                {"success", true},
                {"url", url}
            };
            res.set_content(response.dump(), "application/json");
            
        } catch (const json::exception& e) {
            res.status = 400;
            json error = {{"error", e.what()}};
            res.set_content(error.dump(), "application/json");
        }
    });
    
    // POST /reload - Reload current page
    server_->Post("/reload", [this, add_cors](const httplib::Request&, httplib::Response& res) {
        add_cors(res);
        
        LOG_INFO("HTTP API: reload");
        app_->reload();
        
        json response = {
            {"success", true},
            {"url", app_->current_url()}
        };
        res.set_content(response.dump(), "application/json");
    });
    
    // POST /shutdown - Shutdown the application
    server_->Post("/shutdown", [this, add_cors](const httplib::Request&, httplib::Response& res) {
        add_cors(res);
        
        LOG_INFO("HTTP API: shutdown requested");
        
        json response = {{"success", true}};
        res.set_content(response.dump(), "application/json");
        
        // Shutdown after sending response
        std::thread([this]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            app_->shutdown();
        }).detach();
    });
    
    // GET / - Simple info page
    server_->Get("/", [this, add_cors](const httplib::Request&, httplib::Response& res) {
        add_cors(res);
        
        std::string html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>HTML2NDI Control</title>
    <style>
        body { font-family: system-ui; background: #1a1a2e; color: #eee; padding: 2rem; }
        h1 { color: #0f4c75; }
        pre { background: #16213e; padding: 1rem; border-radius: 8px; overflow-x: auto; }
        .endpoint { margin: 1rem 0; padding: 1rem; background: #0f3460; border-radius: 8px; }
        .method { display: inline-block; padding: 0.25rem 0.5rem; border-radius: 4px; font-weight: bold; }
        .get { background: #2e7d32; }
        .post { background: #1565c0; }
        code { background: #0a0a15; padding: 0.25rem 0.5rem; border-radius: 4px; }
    </style>
</head>
<body>
    <h1>HTML2NDI Control API</h1>
    <p>NDI Source: <code>)" + app_->config().ndi_name + R"(</code></p>
    
    <div class="endpoint">
        <span class="method get">GET</span> <code>/status</code>
        <p>Get current status including URL, resolution, FPS, and NDI connections.</p>
    </div>
    
    <div class="endpoint">
        <span class="method post">POST</span> <code>/seturl</code>
        <p>Load a new URL. Body: <code>{"url": "https://example.com"}</code></p>
    </div>
    
    <div class="endpoint">
        <span class="method post">POST</span> <code>/reload</code>
        <p>Reload the current page.</p>
    </div>
    
    <div class="endpoint">
        <span class="method post">POST</span> <code>/shutdown</code>
        <p>Gracefully shutdown the application.</p>
    </div>
</body>
</html>
)";
        
        res.set_content(html, "text/html");
    });
    
    // Error handler
    server_->set_error_handler([add_cors](const httplib::Request&, httplib::Response& res) {
        add_cors(res);
        
        json error = {
            {"error", "Not Found"},
            {"status", res.status}
        };
        res.set_content(error.dump(), "application/json");
    });
}

} // namespace html2ndi

