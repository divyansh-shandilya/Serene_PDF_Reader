#include <QCoreApplication>
#include <QCommandLineParser>
#include <QElapsedTimer>
#include <QDebug>
#include <algorithm>
#include <numeric>
#include <iostream>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef _WIN32
#include <windows.h>
#endif
#include "../engine/MuPDFDocument.h"

static double percentileMs(const std::vector<qint64>& vals, double p) {
    if (vals.empty()) return 0.0;
    std::vector<qint64> sorted = vals;
    std::sort(sorted.begin(), sorted.end());
    const double idx = p * (sorted.size() - 1);
    const size_t lo = static_cast<size_t>(idx);
    const size_t hi = std::min(lo + 1, sorted.size() - 1);
    const double frac = idx - lo;
    return static_cast<double>(sorted[lo]) * (1.0 - frac) + static_cast<double>(sorted[hi]) * frac;
}

static void debugLog(const std::string& msg, bool isError = false) {
    if (isError) {
        std::cerr << msg << "\n";
    } else {
        std::cout << msg << "\n";
    }
#ifdef _WIN32
    OutputDebugStringA((msg + "\n").c_str());
#endif
}

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addPositionalArgument("pdf", "Path to PDF file.");
    parser.addOption({ "passes", "Number of benchmark passes.", "passes", "2" });
    parser.addOption({ "dpi", "Target DPI.", "dpi", "96" });
    parser.process(app);

    const QStringList args = parser.positionalArguments();
    if (args.isEmpty()) {
        // Silent no-op when launched without arguments from IDE.
        return 0;
    }
    const QString pdfPath = args[0];

    const std::string path = pdfPath.toStdString();
    const int passes = std::max(1, parser.value("passes").toInt());
    const float dpi = static_cast<float>(std::max(36, parser.value("dpi").toInt()));

    MuPDFDocument doc(nullptr, path);
    if (!doc.isValid()) {
        debugLog("Failed to open PDF: " + pdfPath.toStdString(), true);
        return 2;
    }

    const int pageCount = doc.pageCount();
    if (pageCount <= 0) {
        debugLog("No pages to benchmark.", true);
        return 3;
    }

    std::vector<qint64> timings;
    timings.reserve(static_cast<size_t>(pageCount * passes));

    for (int pass = 0; pass < passes; ++pass) {
        for (int i = 0; i < pageCount; ++i) {
            QElapsedTimer t;
            t.start();
            QImage img = doc.renderPage(i, 1.0f, 0, dpi, 0, 0);
            const qint64 elapsed = t.elapsed();
            if (img.isNull()) {
                debugLog("Render failed for page " + std::to_string(i), true);
                return 4;
            }
            timings.push_back(elapsed);
        }
    }

    const qint64 total = std::accumulate(timings.begin(), timings.end(), static_cast<qint64>(0));
    const double avg = static_cast<double>(total) / static_cast<double>(timings.size());
    const double p50 = percentileMs(timings, 0.50);
    const double p95 = percentileMs(timings, 0.95);
    const double p99 = percentileMs(timings, 0.99);

    debugLog(
        "RenderBench pages=" + std::to_string(pageCount) +
        " passes=" + std::to_string(passes) +
        " dpi=" + std::to_string(static_cast<int>(dpi)) +
        " samples=" + std::to_string(timings.size()) +
        " avg_ms=" + std::to_string(avg) +
        " p50_ms=" + std::to_string(p50) +
        " p95_ms=" + std::to_string(p95) +
        " p99_ms=" + std::to_string(p99)
    );
    return 0;
}
