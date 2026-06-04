#include "RenderWorker.h"
#include "MuPDFEngine.h"
#include <QDebug>

RenderWorker::RenderWorker(const std::string& path, bool isMetadataLoader, QObject* parent)
    : QObject(parent), m_path(path), m_isMetadataLoader(isMetadataLoader) {
}

RenderWorker::~RenderWorker() {}

void RenderWorker::init() {
    try {
        m_doc = MuPDFEngine::instance().openDocument(m_path);
        if (m_doc && m_doc->isValid()) {
            int count = m_doc->pageCount();
            QVector<QSizeF> sizes;
            sizes.resize(count);

            if (m_isMetadataLoader) {
                // Async loading of actual sizes for ALL pages (never guesses)
                for (int i = 0; i < count; ++i) {
                    PageMetadata meta = m_doc->pageMetadata(i);
                    sizes[i] = QSizeF(meta.width, meta.height);
                }
                emit documentLoaded(count, sizes);
            }
            else {
                // If false, it only reports page count and sends empty page-size data
                emit documentLoaded(count, sizes);
            }
        }
        else {
            emit error("Failed to open document: " + QString::fromStdString(m_path));
        }
    }
    catch (const std::exception& e) {
        emit error("C++ Exception opening document: " + QString::fromStdString(e.what()));
    }
    catch (...) {
        emit error("Unknown C++ Exception opening document");
    }
}

void RenderWorker::setBatchInfo(int generation, const QSet<int>& activePages, float zoom) {
    m_currentGeneration.store(generation, std::memory_order_relaxed);
    m_activePages = activePages;
    m_activeZoom.store(zoom, std::memory_order_relaxed);
}

void RenderWorker::requestPage(int pageIndex, float zoom, int rotation, bool nightMode, float dpi, int targetWidth, int targetHeight, int generation, bool priority) {
    decrementPendingRenderCount();
    if (!m_doc) return;

    // Instant thread-safe early abort check
    if (generation < m_currentGeneration.load(std::memory_order_relaxed)) return;

    if (!m_activePages.isEmpty() && !m_activePages.contains(pageIndex) && !priority) return;

    // Double-check just before embarking on expensive CPU decoding/rendering
    if (generation < m_currentGeneration.load(std::memory_order_relaxed)) return;

    fz_cookie cookie;
    memset(&cookie, 0, sizeof(cookie));
    m_activeCookie.store(reinterpret_cast<fz_cookie*>(&cookie), std::memory_order_relaxed);

    // Dynamic quality reduction at high zoom during scrolling is informed by m_activeZoom
    
    // Rendering is an expensive operation
    QImage image = m_doc->renderPage(pageIndex, zoom, rotation, dpi, targetWidth, targetHeight, &cookie);

    m_activeCookie.store(nullptr, std::memory_order_relaxed);

    // Triple-check after rendering finishes to avoid emitting a stale page to the GUI thread
    if (generation < m_currentGeneration.load(std::memory_order_relaxed)) return;

    if (!image.isNull()) {
        if (nightMode) image.invertPixels();
        emit pageRendered(pageIndex, image, zoom);
    }
    else {
        emit error("Failed to render page " + QString::number(pageIndex), pageIndex);
    }
}

PageMetadata RenderWorker::getMetadata(int pageIndex) {
    if (!m_doc) return { pageIndex, 0, 0 };
    return m_doc->pageMetadata(pageIndex);
}
