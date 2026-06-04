#include "MuPDFEngine.h"
#include <QFileInfo>
#include <QString>

MuPDFEngine::MuPDFEngine() {
    // Initializing fitz context with high-performance resource cache (256MB)
    m_ctx = fz_new_context(NULL, NULL, 256 * 1024 * 1024);
    if (!m_ctx) {
        throw std::runtime_error("Could not create MuPDF context");
    }

    // Register the default set of document handlers
    fz_try(m_ctx) {
        fz_register_document_handlers(m_ctx);
    }
    fz_catch(m_ctx) {
        // Fallback or error
    }
}

MuPDFEngine::~MuPDFEngine() {
    if (m_ctx) {
        fz_drop_context(m_ctx);
    }
}

std::unique_ptr<MuPDFDocument> MuPDFEngine::openDocument(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Fast reject non-PDF/invalid paths to avoid expensive MuPDF exception churn.
    QString qPath = QString::fromStdString(path);
    QFileInfo fi(qPath);
    if (!fi.exists() || !fi.isFile()) {
        return nullptr;
    }
    if (fi.suffix().compare("pdf", Qt::CaseInsensitive) != 0) {
        return nullptr;
    }

    auto doc = std::make_unique<MuPDFDocument>(m_ctx, path);
    if (doc->isValid()) {
        return doc;
    }
    return nullptr;
}
