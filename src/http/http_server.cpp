/**
 * HTTP control server implementation.
 */

#include "html2ndi/http/http_server.h"
#include "html2ndi/application.h"
#include "html2ndi/ndi/ndi_sender.h"
#include "html2ndi/utils/logger.h"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <mach-o/dyld.h>

using json = nlohmann::json;

namespace html2ndi {

// Embedded control panel HTML
static const char* CONTROL_PANEL_HTML = R"HTMLEND(
<!DOCTYPE html>
<html>
<head>
    <title>HTML2NDI Control Panel</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        :root{--bg:#0a0a0f;--card:#12121a;--input:#1a1a24;--accent:#6366f1;--glow:rgba(99,102,241,.3);--ok:#22c55e;--err:#ef4444;--text:#e4e4e7;--dim:#71717a;--border:#27272a}
        *{box-sizing:border-box;margin:0;padding:0}
        body{font-family:-apple-system,system-ui,sans-serif;background:var(--bg);color:var(--text);min-height:100vh;background-image:radial-gradient(ellipse at top,rgba(99,102,241,.1)0,transparent 50%)}
        .c{max-width:1200px;margin:0 auto;padding:2rem}
        header{display:flex;align-items:center;justify-content:space-between;margin-bottom:2rem;padding-bottom:1.5rem;border-bottom:1px solid var(--border)}
        .logo{display:flex;align-items:center;gap:.75rem}
        .logo-icon{width:48px;height:48px;background:linear-gradient(135deg,var(--accent),#8b5cf6);border-radius:12px;display:flex;align-items:center;justify-content:center;font-size:1.5rem;font-weight:700;box-shadow:0 4px 20px var(--glow)}
        h1{font-size:1.75rem;font-weight:600;background:linear-gradient(135deg,#fff,#a1a1aa);-webkit-background-clip:text;-webkit-text-fill-color:transparent}
        .badge{display:flex;align-items:center;gap:.5rem;padding:.5rem 1rem;background:var(--card);border:1px solid var(--border);border-radius:9999px;font-size:.875rem}
        .dot{width:8px;height:8px;border-radius:50%;background:var(--ok);animation:pulse 2s infinite}
        @keyframes pulse{0%,100%{opacity:1}50%{opacity:.5}}
        .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(320px,1fr));gap:1.5rem}
        .card{background:var(--card);border:1px solid var(--border);border-radius:16px;padding:1.5rem;transition:transform .2s,box-shadow .2s}
        .card:hover{transform:translateY(-2px);box-shadow:0 8px 30px rgba(0,0,0,.3)}
        .card-h{display:flex;align-items:center;gap:.75rem;margin-bottom:1.25rem}
        .card-i{width:36px;height:36px;background:var(--input);border-radius:10px;display:flex;align-items:center;justify-content:center;font-weight:600}
        .card-t{font-size:1rem;font-weight:600}
        .stats{display:grid;grid-template-columns:repeat(2,1fr);gap:1rem}
        .stat{background:var(--input);padding:1rem;border-radius:10px}
        .stat-l{font-size:.75rem;color:var(--dim);text-transform:uppercase;letter-spacing:.05em;margin-bottom:.25rem}
        .stat-v{font-family:ui-monospace,monospace;font-size:1.25rem;font-weight:600}
        .hl{color:var(--accent)}.ok{color:var(--ok)}
        label{display:block;font-size:.875rem;color:var(--dim);margin-bottom:.5rem}
        input,select{width:100%;padding:.75rem 1rem;background:var(--input);border:1px solid var(--border);border-radius:10px;color:var(--text);font-family:ui-monospace,monospace;font-size:.9rem;transition:border-color .2s,box-shadow .2s}
        input:focus,select:focus{outline:none;border-color:var(--accent);box-shadow:0 0 0 3px var(--glow)}
        .ig{margin-bottom:1rem}
        .btns{display:flex;gap:.75rem;margin-top:1rem}
        button{flex:1;padding:.75rem 1.25rem;border:none;border-radius:10px;font-family:inherit;font-size:.9rem;font-weight:500;cursor:pointer;transition:all .2s}
        .btn-p{background:linear-gradient(135deg,var(--accent),#8b5cf6);color:#fff;box-shadow:0 4px 15px var(--glow)}
        .btn-p:hover{transform:translateY(-1px);box-shadow:0 6px 20px var(--glow)}
        .btn-s{background:var(--input);color:var(--text);border:1px solid var(--border)}
        .btn-s:hover{background:var(--border)}
        .btn-d{background:var(--err);color:#fff}
        .btn-d:hover{background:#dc2626}
        .presets{display:grid;grid-template-columns:repeat(2,1fr);gap:.75rem;margin-bottom:1rem}
        .preset{padding:.75rem;background:var(--input);border:2px solid var(--border);border-radius:10px;color:var(--text);font-size:.85rem;cursor:pointer;transition:all .2s;text-align:center}
        .preset:hover{border-color:var(--accent);background:rgba(99,102,241,.1)}
        .preset.on{border-color:var(--accent);background:rgba(99,102,241,.2)}
        .preset small{color:var(--dim)}
        .adv-t{display:flex;align-items:center;gap:.5rem;color:var(--dim);font-size:.85rem;cursor:pointer;margin-bottom:1rem;user-select:none}
        .adv-t:hover{color:var(--text)}
        .adv{display:none}.adv.show{display:block}
        .toast{position:fixed;bottom:2rem;right:2rem;padding:1rem 1.5rem;background:var(--card);border:1px solid var(--border);border-radius:12px;box-shadow:0 10px 40px rgba(0,0,0,.4);transform:translateY(100px);opacity:0;transition:all .3s;z-index:1000}
        .toast.show{transform:translateY(0);opacity:1}
        .toast.ok{border-color:var(--ok)}.toast.err{border-color:var(--err)}
        @media(max-width:768px){.c{padding:1rem}header{flex-direction:column;gap:1rem;text-align:center}.stats{grid-template-columns:1fr}}
    </style>
</head>
<body>
<div class="c">
    <header>
        <div class="logo"><div class="logo-icon">N</div><div><h1>HTML2NDI</h1><div style="font-size:.85rem;color:var(--dim)">Control Panel</div></div></div>
        <div class="badge"><div class="dot" id="dot"></div><span id="st">Connecting...</span></div>
    </header>
    <div class="grid">
        <div class="card"><div class="card-h"><div class="card-i">i</div><div class="card-t">Status</div></div>
            <div class="stats">
                <div class="stat"><div class="stat-l">NDI Source</div><div class="stat-v hl" id="ndi">-</div></div>
                <div class="stat"><div class="stat-l">Connections</div><div class="stat-v ok" id="conn">0</div></div>
                <div class="stat"><div class="stat-l">Resolution</div><div class="stat-v" id="res">-</div></div>
                <div class="stat"><div class="stat-l">FPS</div><div class="stat-v" id="fps">-</div></div>
            </div>
        </div>
        <div class="card"><div class="card-h"><div class="card-i">W</div><div class="card-t">Source URL</div></div>
            <div class="ig"><label>Current URL</label><input type="url" id="url" placeholder="https://example.com/graphics.html"></div>
            <div class="btns"><button class="btn-p" onclick="setUrl()">Load URL</button><button class="btn-s" onclick="reload()">Reload</button></div>
        </div>
        <div class="card"><div class="card-h"><div class="card-i">C</div><div class="card-t">Color Space</div></div>
            <div class="presets">
                <button class="preset" onclick="setP('rec709')" id="p-rec709"><strong>Rec. 709</strong><br><small>HD Broadcast</small></button>
                <button class="preset" onclick="setP('rec2020')" id="p-rec2020"><strong>Rec. 2020</strong><br><small>UHD / HDR</small></button>
                <button class="preset" onclick="setP('srgb')" id="p-srgb"><strong>sRGB</strong><br><small>Web Standard</small></button>
                <button class="preset" onclick="setP('rec601')" id="p-rec601"><strong>Rec. 601</strong><br><small>SD Legacy</small></button>
            </div>
            <div class="adv-t" onclick="togAdv()"><span id="arr">&#9654;</span> Advanced</div>
            <div class="adv" id="adv">
                <div class="ig"><label>Colorimetry</label><select id="cs"><option value="BT709">BT.709</option><option value="BT2020">BT.2020</option><option value="sRGB">sRGB</option><option value="BT601">BT.601</option></select></div>
                <div class="ig"><label>Gamma</label><select id="gm"><option value="BT709">BT.709</option><option value="BT2020">BT.2020</option><option value="sRGB">sRGB</option><option value="Linear">Linear</option></select></div>
                <div class="ig"><label>Range</label><select id="rg"><option value="full">Full (0-255)</option><option value="limited">Limited (16-235)</option></select></div>
                <button class="btn-p" onclick="applyC()" style="width:100%">Apply</button>
            </div>
        </div>
        <div class="card"><div class="card-h"><div class="card-i">S</div><div class="card-t">System</div></div>
            <div class="stats" style="margin-bottom:1rem">
                <div class="stat"><div class="stat-l">Actual FPS</div><div class="stat-v" id="afps">-</div></div>
                <div class="stat"><div class="stat-l">Color Mode</div><div class="stat-v" id="cm" style="font-size:.9rem">-</div></div>
            </div>
            <button class="btn-d" onclick="shut()" style="width:100%">Shutdown</button>
        </div>
    </div>
</div>
<div class="toast" id="toast"></div>
<script>
async function stat(){try{const r=await fetch('/status'),d=await r.json();$('ndi').textContent=d.ndi_name;$('conn').textContent=d.ndi_connections;$('res').textContent=d.width+'x'+d.height;$('fps').textContent=d.fps;$('afps').textContent=(d.actual_fps||0).toFixed(1);if(document.activeElement!==$('url'))$('url').value=d.url||'';if(d.color){$('cs').value=d.color.colorspace;$('gm').value=d.color.gamma;$('rg').value=d.color.range;$('cm').textContent=d.color.colorspace;updP(d.color)}$('dot').style.background=d.running?'var(--ok)':'var(--err)';$('st').textContent=d.running?'Running':'Stopped'}catch(e){$('dot').style.background='var(--err)';$('st').textContent='Disconnected'}}
function $(id){return document.getElementById(id)}
function updP(c){document.querySelectorAll('.preset').forEach(b=>b.classList.remove('on'));if(c.colorspace==='BT709'&&c.gamma==='BT709'&&c.range==='full')$('p-rec709').classList.add('on');else if(c.colorspace==='BT2020'&&c.gamma==='BT2020')$('p-rec2020').classList.add('on');else if(c.colorspace==='sRGB'&&c.gamma==='sRGB')$('p-srgb').classList.add('on');else if(c.colorspace==='BT601')$('p-rec601').classList.add('on')}
async function setUrl(){const u=$('url').value;if(!u)return toast('Enter a URL','err');try{const r=await fetch('/seturl',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({url:u})});const d=await r.json();toast(d.success?'URL loaded!':d.error,d.success?'ok':'err')}catch(e){toast('Failed','err')}}
async function reload(){try{await fetch('/reload',{method:'POST'});toast('Reloaded!','ok')}catch(e){toast('Failed','err')}}
async function setP(p){try{const r=await fetch('/color',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({preset:p})});const d=await r.json();if(d.success){toast(p.toUpperCase(),'ok');stat()}}catch(e){toast('Failed','err')}}
async function applyC(){try{const r=await fetch('/color',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({colorspace:$('cs').value,gamma:$('gm').value,range:$('rg').value})});const d=await r.json();toast(d.success?'Applied!':d.error,d.success?'ok':'err');stat()}catch(e){toast('Failed','err')}}
async function shut(){if(!confirm('Shutdown HTML2NDI?'))return;try{await fetch('/shutdown',{method:'POST'});toast('Shutting down...','ok')}catch(e){toast('Failed','err')}}
function togAdv(){const e=$('adv'),a=$('arr');e.classList.toggle('show');a.innerHTML=e.classList.contains('show')?'&#9660;':'&#9654;'}
function toast(m,t){const e=$('toast');e.textContent=m;e.className='toast '+t+' show';setTimeout(()=>e.classList.remove('show'),3000)}
stat();setInterval(stat,2000);$('url').addEventListener('keypress',e=>{if(e.key==='Enter')setUrl()});
</script>
</body>
</html>
)HTMLEND";

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
        res.set_header("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
    };
    
    // OPTIONS preflight
    server_->Options(".*", [add_cors](const httplib::Request&, httplib::Response& res) {
        add_cors(res);
        res.status = 204;
    });
    
    // GET /status - Get current status with full statistics
    server_->Get("/status", [this, add_cors](const httplib::Request&, httplib::Response& res) {
        add_cors(res);
        
        auto* ndi = app_->ndi_sender();
        std::string groups = app_->config().ndi_groups;
        if (groups.empty()) groups = "public";
        
        // Get tally state
        auto tally = ndi->get_tally(0);
        
        // Get frame statistics
        auto stats = app_->get_frame_stats();
        
        json status = {
            {"url", app_->current_url()},
            {"width", app_->config().width},
            {"height", app_->config().height},
            {"fps", app_->config().fps},
            {"actual_fps", app_->current_fps()},
            {"ndi_name", app_->config().ndi_name},
            {"ndi_source", ndi->get_source_name()},
            {"ndi_groups", groups},
            {"ndi_connections", app_->ndi_connection_count()},
            {"running", !app_->is_shutting_down()},
            {"tally", {
                {"on_program", tally.on_program},
                {"on_preview", tally.on_preview}
            }},
            {"stats", {
                {"frames_sent", stats.frames_sent},
                {"frames_dropped", stats.frames_dropped},
                {"drop_rate", stats.drop_rate},
                {"uptime_seconds", stats.uptime_seconds},
                {"bandwidth_mbps", stats.bandwidth_bytes_per_sec / 1000000.0}
            }},
            {"color", {
                {"colorspace", ndi->color_space_name()},
                {"gamma", ndi->gamma_mode_name()},
                {"range", ndi->color_range_name()}
            }}
        };
        
        res.set_content(status.dump(2), "application/json");
    });
    
    // GET /metrics - Prometheus-compatible metrics endpoint
    server_->Get("/metrics", [this, add_cors](const httplib::Request&, httplib::Response& res) {
        add_cors(res);
        
        auto* ndi = app_->ndi_sender();
        auto tally = ndi->get_tally(0);
        auto stats = app_->get_frame_stats();
        
        std::string ndi_name = app_->config().ndi_name;
        
        // Format: metric_name{labels} value
        std::ostringstream metrics;
        
        metrics << "# HELP html2ndi_info NDI stream information\n";
        metrics << "# TYPE html2ndi_info gauge\n";
        metrics << "html2ndi_info{ndi_name=\"" << ndi_name << "\",url=\"" << app_->current_url() << "\"} 1\n\n";
        
        metrics << "# HELP html2ndi_running Whether the stream is running\n";
        metrics << "# TYPE html2ndi_running gauge\n";
        metrics << "html2ndi_running{ndi_name=\"" << ndi_name << "\"} " << (app_->is_shutting_down() ? 0 : 1) << "\n\n";
        
        metrics << "# HELP html2ndi_ndi_connections Number of NDI receivers connected\n";
        metrics << "# TYPE html2ndi_ndi_connections gauge\n";
        metrics << "html2ndi_ndi_connections{ndi_name=\"" << ndi_name << "\"} " << app_->ndi_connection_count() << "\n\n";
        
        metrics << "# HELP html2ndi_tally_program Whether stream is on program output\n";
        metrics << "# TYPE html2ndi_tally_program gauge\n";
        metrics << "html2ndi_tally_program{ndi_name=\"" << ndi_name << "\"} " << (tally.on_program ? 1 : 0) << "\n\n";
        
        metrics << "# HELP html2ndi_tally_preview Whether stream is on preview output\n";
        metrics << "# TYPE html2ndi_tally_preview gauge\n";
        metrics << "html2ndi_tally_preview{ndi_name=\"" << ndi_name << "\"} " << (tally.on_preview ? 1 : 0) << "\n\n";
        
        metrics << "# HELP html2ndi_fps_target Target frames per second\n";
        metrics << "# TYPE html2ndi_fps_target gauge\n";
        metrics << "html2ndi_fps_target{ndi_name=\"" << ndi_name << "\"} " << app_->config().fps << "\n\n";
        
        metrics << "# HELP html2ndi_fps_actual Actual frames per second\n";
        metrics << "# TYPE html2ndi_fps_actual gauge\n";
        metrics << "html2ndi_fps_actual{ndi_name=\"" << ndi_name << "\"} " << app_->current_fps() << "\n\n";
        
        metrics << "# HELP html2ndi_resolution_width Video width in pixels\n";
        metrics << "# TYPE html2ndi_resolution_width gauge\n";
        metrics << "html2ndi_resolution_width{ndi_name=\"" << ndi_name << "\"} " << app_->config().width << "\n\n";
        
        metrics << "# HELP html2ndi_resolution_height Video height in pixels\n";
        metrics << "# TYPE html2ndi_resolution_height gauge\n";
        metrics << "html2ndi_resolution_height{ndi_name=\"" << ndi_name << "\"} " << app_->config().height << "\n\n";
        
        metrics << "# HELP html2ndi_frames_sent_total Total frames sent\n";
        metrics << "# TYPE html2ndi_frames_sent_total counter\n";
        metrics << "html2ndi_frames_sent_total{ndi_name=\"" << ndi_name << "\"} " << stats.frames_sent << "\n\n";
        
        metrics << "# HELP html2ndi_frames_dropped_total Total frames dropped\n";
        metrics << "# TYPE html2ndi_frames_dropped_total counter\n";
        metrics << "html2ndi_frames_dropped_total{ndi_name=\"" << ndi_name << "\"} " << stats.frames_dropped << "\n\n";
        
        metrics << "# HELP html2ndi_drop_rate Frame drop rate (0-1)\n";
        metrics << "# TYPE html2ndi_drop_rate gauge\n";
        metrics << "html2ndi_drop_rate{ndi_name=\"" << ndi_name << "\"} " << stats.drop_rate << "\n\n";
        
        metrics << "# HELP html2ndi_uptime_seconds Stream uptime in seconds\n";
        metrics << "# TYPE html2ndi_uptime_seconds counter\n";
        metrics << "html2ndi_uptime_seconds{ndi_name=\"" << ndi_name << "\"} " << stats.uptime_seconds << "\n\n";
        
        metrics << "# HELP html2ndi_bandwidth_bytes_per_second Estimated bandwidth in bytes/sec\n";
        metrics << "# TYPE html2ndi_bandwidth_bytes_per_second gauge\n";
        metrics << "html2ndi_bandwidth_bytes_per_second{ndi_name=\"" << ndi_name << "\"} " << stats.bandwidth_bytes_per_sec << "\n";
        
        res.set_content(metrics.str(), "text/plain; version=0.0.4; charset=utf-8");
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
    
    // GET /thumbnail - Get a JPEG thumbnail of the current frame
    server_->Get("/thumbnail", [this, add_cors](const httplib::Request& req, httplib::Response& res) {
        add_cors(res);
        
        // Parse optional width and quality parameters
        int width = 320;
        int quality = 75;
        
        if (req.has_param("width")) {
            width = std::stoi(req.get_param_value("width"));
            width = std::max(64, std::min(1920, width)); // Clamp to reasonable range
        }
        if (req.has_param("quality")) {
            quality = std::stoi(req.get_param_value("quality"));
            quality = std::max(10, std::min(100, quality));
        }
        
        std::vector<uint8_t> jpeg_data;
        if (app_->get_thumbnail(jpeg_data, width, quality)) {
            res.set_content(reinterpret_cast<const char*>(jpeg_data.data()), 
                           jpeg_data.size(), "image/jpeg");
        } else {
            res.status = 503;
            res.set_content(R"({"error": "No frame available"})", "application/json");
        }
    });
    
    // GET /groups - Get NDI access groups
    server_->Get("/groups", [this, add_cors](const httplib::Request&, httplib::Response& res) {
        add_cors(res);
        
        std::string groups = app_->config().ndi_groups;
        if (groups.empty()) groups = "public";
        
        json response = {
            {"groups", groups},
            {"note", "Use 'public' for all groups. Groups can only be set at startup."}
        };
        res.set_content(response.dump(2), "application/json");
    });
    
    // GET /color - Get current color settings
    server_->Get("/color", [this, add_cors](const httplib::Request&, httplib::Response& res) {
        add_cors(res);
        
        auto* ndi = app_->ndi_sender();
        json response = {
            {"colorspace", ndi->color_space_name()},
            {"gamma", ndi->gamma_mode_name()},
            {"range", ndi->color_range_name()},
            {"presets", {"rec709", "rec2020", "srgb", "rec601"}}
        };
        res.set_content(response.dump(2), "application/json");
    });
    
    // POST /color - Set color space settings
    server_->Post("/color", [this, add_cors](const httplib::Request& req, httplib::Response& res) {
        add_cors(res);
        
        try {
            auto body = json::parse(req.body);
            auto* ndi = app_->ndi_sender();
            
            // Apply preset if specified
            if (body.contains("preset") && body["preset"].is_string()) {
                std::string preset = body["preset"].get<std::string>();
                
                if (preset == "rec709") {
                    ndi->set_color_space(ColorSpace::Rec709);
                    ndi->set_gamma_mode(GammaMode::BT709);
                    ndi->set_color_range(ColorRange::Full);
                } else if (preset == "rec2020") {
                    ndi->set_color_space(ColorSpace::Rec2020);
                    ndi->set_gamma_mode(GammaMode::BT2020);
                    ndi->set_color_range(ColorRange::Full);
                } else if (preset == "srgb") {
                    ndi->set_color_space(ColorSpace::sRGB);
                    ndi->set_gamma_mode(GammaMode::sRGB);
                    ndi->set_color_range(ColorRange::Full);
                } else if (preset == "rec601") {
                    ndi->set_color_space(ColorSpace::Rec601);
                    ndi->set_gamma_mode(GammaMode::BT709);
                    ndi->set_color_range(ColorRange::Limited);
                } else {
                    res.status = 400;
                    res.set_content(R"({"error": "Unknown preset. Use: rec709, rec2020, srgb, rec601"})", "application/json");
                    return;
                }
                
                LOG_INFO("HTTP API: color preset set to %s", preset.c_str());
            }
            
            // Apply individual settings if specified
            if (body.contains("colorspace") && body["colorspace"].is_string()) {
                std::string cs = body["colorspace"].get<std::string>();
                if (cs == "BT709") ndi->set_color_space(ColorSpace::Rec709);
                else if (cs == "BT2020") ndi->set_color_space(ColorSpace::Rec2020);
                else if (cs == "sRGB") ndi->set_color_space(ColorSpace::sRGB);
                else if (cs == "BT601") ndi->set_color_space(ColorSpace::Rec601);
            }
            
            if (body.contains("gamma") && body["gamma"].is_string()) {
                std::string gm = body["gamma"].get<std::string>();
                if (gm == "BT709") ndi->set_gamma_mode(GammaMode::BT709);
                else if (gm == "BT2020") ndi->set_gamma_mode(GammaMode::BT2020);
                else if (gm == "sRGB") ndi->set_gamma_mode(GammaMode::sRGB);
                else if (gm == "Linear") ndi->set_gamma_mode(GammaMode::Linear);
            }
            
            if (body.contains("range") && body["range"].is_string()) {
                std::string rg = body["range"].get<std::string>();
                if (rg == "full") ndi->set_color_range(ColorRange::Full);
                else if (rg == "limited") ndi->set_color_range(ColorRange::Limited);
            }
            
            json response = {
                {"success", true},
                {"colorspace", ndi->color_space_name()},
                {"gamma", ndi->gamma_mode_name()},
                {"range", ndi->color_range_name()}
            };
            res.set_content(response.dump(), "application/json");
            
        } catch (const json::exception& e) {
            res.status = 400;
            json error = {{"error", e.what()}};
            res.set_content(error.dump(), "application/json");
        }
    });
    
    // POST /execute - Execute JavaScript in the browser
    server_->Post("/execute", [this, add_cors](const httplib::Request& req, httplib::Response& res) {
        add_cors(res);
        
        try {
            auto body = json::parse(req.body);
            
            if (!body.contains("code") || !body["code"].is_string()) {
                res.status = 400;
                res.set_content(R"({"error": "Missing 'code' field"})", "application/json");
                return;
            }
            
            std::string code = body["code"].get<std::string>();
            
            LOG_INFO("HTTP API: execute JavaScript (%zu chars)", code.length());
            app_->execute_javascript(code);
            
            json response = {
                {"success", true},
                {"code_length", code.length()}
            };
            res.set_content(response.dump(), "application/json");
            
        } catch (const json::exception& e) {
            res.status = 400;
            json error = {{"error", e.what()}};
            res.set_content(error.dump(), "application/json");
        }
    });
    
    // GET /console - Get console messages
    server_->Get("/console", [this, add_cors](const httplib::Request& req, httplib::Response& res) {
        add_cors(res);
        
        size_t limit = 100;
        bool clear = false;
        
        if (req.has_param("limit")) {
            limit = std::stoul(req.get_param_value("limit"));
        }
        if (req.has_param("clear")) {
            clear = req.get_param_value("clear") == "true" || req.get_param_value("clear") == "1";
        }
        
        auto messages = app_->get_console_messages(limit, clear);
        
        json messages_array = json::array();
        for (const auto& msg : messages) {
            messages_array.push_back({
                {"level", msg.level},
                {"message", msg.message},
                {"source", msg.source},
                {"line", msg.line},
                {"timestamp", msg.timestamp}
            });
        }
        
        json response = {
            {"count", messages.size()},
            {"total", app_->get_console_message_count()},
            {"messages", messages_array}
        };
        
        res.set_content(response.dump(2), "application/json");
    });
    
    // DELETE /console - Clear console messages
    server_->Delete("/console", [this, add_cors](const httplib::Request&, httplib::Response& res) {
        add_cors(res);
        
        app_->clear_console_messages();
        
        json response = {{"success", true}};
        res.set_content(response.dump(), "application/json");
    });
    
    // GET / - Control Panel GUI
    server_->Get("/", [add_cors](const httplib::Request&, httplib::Response& res) {
        add_cors(res);
        res.set_content(CONTROL_PANEL_HTML, "text/html");
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
