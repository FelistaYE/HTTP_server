#include "server.h"
#include "logger.h"

#include <csignal>
#include <iostream>

static Server* g_server = nullptr;

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        LOG_INFO("Received shutdown signal");
        if (g_server) g_server->stop();
    }
}

int main(int argc, char* argv[]) {
    uint16_t port = 8080;
    std::string static_dir = "./static";

    // Simple command-line argument parsing
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if ((arg == "-s" || arg == "--static") && i + 1 < argc) {
            static_dir = argv[++i];
        } else if (arg == "-d" || arg == "--debug") {
            Logger::instance().set_level(LogLevel::DEBUG);
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "  -p, --port PORT     Listen port (default: 8080)\n"
                      << "  -s, --static DIR    Static files directory (default: ./static)\n"
                      << "  -d, --debug         Enable debug logging\n"
                      << "  -h, --help          Show this help\n";
            return 0;
        }
    }

    // Initialize logging
    Logger::instance().set_file("server.log");

    try {
        Server server(port, static_dir);
        g_server = &server;

        // Register signal handlers
        ::signal(SIGINT, signal_handler);
        ::signal(SIGTERM, signal_handler);

        // ═══════════════════════════════════════════════════════════
        //  Register your API routes here
        // ═══════════════════════════════════════════════════════════

        // GET /api/hello — simple JSON response
        server.router().get("/api/hello", [](const HttpRequest& req) {
            (void)req;
            return HttpResponse::ok().json(R"({"message": "Hello, World!"})");
        });

        // GET /api/status — server status
        server.router().get("/api/status", [](const HttpRequest& req) {
            (void)req;
            return HttpResponse::ok().json(
                R"({"status": "running", "version": "1.0.0"})"
            );
        });

        // POST /api/echo — echo request body
        server.router().post("/api/echo", [](const HttpRequest& req) {
            return HttpResponse::ok()
                .json(R"({"echo": ")" + req.body + R"("})");
        });

        // ═══════════════════════════════════════════════════════════

        LOG_INFO("=== HighPerfServer v1.0 ===");
        LOG_INFO("Static dir: " + static_dir);
        LOG_INFO("API routes: GET /api/hello, GET /api/status, POST /api/echo");
        LOG_INFO("Open http://localhost:" + std::to_string(port) + " in your browser");

        server.run();

    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Fatal: ") + e.what());
        return 1;
    }

    return 0;
}
