#include "mcp/PmxMcpServer.h"
#include "models/PmxDocumentController.h"

#include <QCoreApplication>
#include <QStringList>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("pmx-mcp"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    bool isHeadless = false;
    QString targetFile;
    QString socketPath;

    const QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        const QString& arg = args[i];
        if (arg == QStringLiteral("--headless")) {
            isHeadless = true;
            if (i + 1 < args.size() && !args[i + 1].startsWith(QLatin1Char('-'))) {
                targetFile = args[++i];
            }
        } else if (arg == QStringLiteral("--socket") && i + 1 < args.size()) {
            socketPath = args[++i];
        } else if (arg == QStringLiteral("--help") || arg == QStringLiteral("-h")) {
            std::cout << "Usage: pmx-mcp [options] [model.pmx]\n"
                      << "PMX Editor Model Context Protocol (MCP) Server\n\n"
                      << "Options:\n"
                      << "  --headless [file]   Run in headless mode without connecting to GUI socket\n"
                      << "  --socket <path>     Specify custom GUI Unix domain socket path\n"
                      << "  -h, --help          Display this help message\n";
            return 0;
        } else if (!arg.startsWith(QLatin1Char('-')) && targetFile.isEmpty()) {
            targetFile = arg;
        }
    }

    pmx::PmxDocumentController controller;
    if (!targetFile.isEmpty()) {
        QString err;
        if (!controller.loadFromFile(targetFile, &err)) {
            std::cerr << "Warning: Failed to load file '" << targetFile.toStdString() << "': " << err.toStdString() << std::endl;
        }
    }

    pmx::PmxMcpServer server(&controller);

    if (!isHeadless) {
        // Attempt to connect to running GUI
        bool connected = server.connectToGui(socketPath, 500);
        if (connected) {
            std::cerr << "Connected to live PMX Editor GUI socket." << std::endl;
        } else {
            std::cerr << "No live GUI editor found on socket. Running in headless mode." << std::endl;
        }
    }

    // stdio loop supporting both newline-delimited JSON and Content-Length framing
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }

        // Trim trailing '\r' if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }

        // Check for Content-Length header
        if (line.rfind("Content-Length:", 0) == 0) {
            size_t colonPos = line.find(':');
            int length = -1;
            try {
                length = std::stoi(line.substr(colonPos + 1));
            } catch (const std::exception&) {
                std::cerr << "Error: invalid Content-Length header" << std::endl;
                continue;
            }

            if (length < 0 || length > 64 * 1024 * 1024) {
                std::cerr << "Error: Content-Length out of range [0, 64MB]: " << length << std::endl;
                continue;
            }

            // Read headers until blank line
            while (std::getline(std::cin, line)) {
                if (line.empty() || line == "\r") {
                    break;
                }
            }

            std::vector<char> buf(static_cast<size_t>(length));
            std::cin.read(buf.data(), length);
            if (std::cin.gcount() != length) {
                std::cerr << "Error: incomplete payload read (expected " << length << ", got " << std::cin.gcount() << ")" << std::endl;
                continue;
            }

            QByteArray reqBytes(buf.data(), static_cast<qsizetype>(length));
            QByteArray respBytes = server.processMessage(reqBytes);
            if (!respBytes.isEmpty()) {
                std::cout << "Content-Length: " << respBytes.size() << "\r\n\r\n"
                          << respBytes.constData() << std::flush;
            }
        } else {
            // Newline-delimited JSON
            QByteArray reqBytes = QByteArray::fromStdString(line);
            QByteArray respBytes = server.processMessage(reqBytes);
            if (!respBytes.isEmpty()) {
                std::cout << respBytes.constData() << "\n" << std::flush;
            }
        }
    }

    return 0;
}
