#include "../../scripts/ccode/frontend.hpp"
#include <cassert>
#include <iostream>

int main() {
    ccode::EventReader reader;
    auto start = reader.Feed("{\"type\":\"system\",\"subtype\":\"init\",\"session_id\":\"s1\"}\n");
    assert(start.empty());
    assert(reader.session == "s1");
    assert(reader.Feed("{\"type\":\"assistant\",\"message\":{\"content\":[{\"type\":\"text\",\"text\":\"hello").empty());
    auto text = reader.Feed(" world\"}]}}\n");
    assert(text == "hello world\n");
    reader.Feed("{\"type\":\"result\",\"subtype\":\"success\",\"is_error\":false,\"session_id\":\"s1\"}\n");
    reader.Finish();
    assert(reader.complete && !reader.failed);
    for (const auto& invalid : {
        "{bad}\n",
        "{\"type\":\"assistant\",\"message\":{\"content\":[{\"type\":\"tool_use\",\"name\":\"\",\"id\":\"t1\",\"input\":{}}]}}\n",
        "{\"type\":\"assistant\",\"message\":{\"content\":[{\"type\":\"tool_use\",\"name\":\"Read\",\"id\":\"t1\",\"input\":\"bad\"}]}}\n"
    }) {
        bool rejected = false;
        try { ccode::EventReader broken; broken.Feed(invalid); }
        catch (const std::exception&) { rejected = true; }
        assert(rejected);
    }
    assert(ccode::ConsoleText("\x1b[2Jhello\r") == "[2Jhello");
    // A complete event must not be silently accepted after the terminal result.
    bool afterResult = false;
    try { reader.Feed("{\"type\":\"assistant\",\"message\":{\"content\":[]}}\n"); }
    catch (...) { afterResult = true; }
    assert(afterResult);
    std::cout << "frontend event tests passed\n";
}
