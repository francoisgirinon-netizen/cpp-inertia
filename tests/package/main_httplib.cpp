// Consumer of the installed package, component httplib: serves one page on an ephemeral port and
// fetches it with the adapter's own HTTP client.
#include <inertia/httplib/httplib.hpp>
#include <inertia/inertia.hpp>

#include <chrono>
#include <cstdlib>
#include <future>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

int main() {
    httplib::Server server;
    inertia::httplib::install(server, std::make_shared<const inertia::Inertia>(inertia::Config{}));
    server.Get("/", [](const httplib::Request &req, httplib::Response &res) {
        inertia::httplib::render(req, res, "Home", {{"a", 1}});
    });

    const int port = server.bind_to_any_port("127.0.0.1");
    if (port <= 0) {
        std::cerr << "cannot bind a port\n";
        return EXIT_FAILURE;
    }
    std::thread serving([&server] { server.listen_after_bind(); });
    server.wait_until_ready();

    std::promise<inertia::HttpResult> promise;
    inertia::httplib::HttplibHttpClient client;
    client.get("http://127.0.0.1:" + std::to_string(port) + "/", std::chrono::seconds(5),
               [&promise](const inertia::HttpResult &result) { promise.set_value(result); });
    const auto result = promise.get_future().get();
    server.stop();
    serving.join();

    if (result.status != 200 || result.body.find("data-page") == std::string::npos) {
        std::cerr << "unexpected reply: " << result.status << ' ' << result.body << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
