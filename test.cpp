#undef ASIO_NO_DEPRECATED
#define CROW_MAIN
#include "crow.h"

int main() {
    crow::SimpleApp app;

    CROW_ROUTE(app, "/")
    ([]() {
        return "Hello, world!";
    });

    app.port(18080).run();
}
