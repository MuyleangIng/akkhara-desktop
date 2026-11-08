// Test tool for the C++ engine: reads Sing Khmer lines (UTF-8) from stdin, prints the top-5 per line
// (tab-separated). Used to check that the C++ engine = the Python engine, on the Mac and on Windows.
//
//   akkhara_cli <model_dir> < inputs.txt > outputs.tsv
#include <onnxruntime_c_api.h>

#include <chrono>
#include <cstdio>
#include <iostream>
#include <string>

#include "akkhara_engine.h"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: akkhara_cli <model_dir> < inputs.txt\n");
        return 2;
    }
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);  // raw UTF-8 in and out
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    try {
        const OrtApi* api = OrtGetApiBase()->GetApi(ORT_API_VERSION);
        akkhara::MiniModel model(api, std::filesystem::u8path(argv[1]));
        akkhara::LiveEngine engine(model);
        std::string line;
        double totalMs = 0;
        int n = 0;
        while (std::getline(std::cin, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            auto t0 = std::chrono::steady_clock::now();
            auto top = engine.candidates(akkhara::fromUtf8(line));
            totalMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            ++n;
            std::string out;
            for (size_t i = 0; i < top.size(); ++i) out += (i ? "\t" : "") + akkhara::toUtf8(top[i]);
            std::cout << out << "\n";
        }
        std::fprintf(stderr, "%d lines, %.1f ms per line\n", n, n ? totalMs / n : 0.0);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
    return 0;
}
