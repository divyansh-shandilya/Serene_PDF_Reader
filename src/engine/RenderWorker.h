#pragma once
#include <QObject>
#include <QImage>
#include <QMutex>
#include <QSet>
#include <string>
#include <atomic>
#include "MuPDFDocument.h"

class RenderWorker : public QObject {
    Q_OBJECT
public:
    explicit RenderWorker(const std::string& path, bool isMetadataLoader = false, QObject* parent = nullptr);
    ~RenderWorker();

    // Instant thread-safe cancellation called from the main GUI thread!
    void cancelOlderThan(int generation) {
        m_currentGeneration.store(generation, std::memory_order_relaxed);
        fz_cookie* cookie = m_activeCookie.load(std::memory_order_relaxed);
        if (cookie) {
            // Asynchronously signal abort to MuPDF's fz_run_page loop
            // Since we know fz_cookie has int abort at the first offset, we can safely set it.
            // But doing it via casting is extremely robust.
            struct fz_cookie_type { int abort; };
            reinterpret_cast<fz_cookie_type*>(cookie)->abort = 1;
        }
    }

    void incrementPendingRenderCount() {
        m_pendingRenderCount.fetch_add(1, std::memory_order_relaxed);
    }

    void decrementPendingRenderCount() {
        m_pendingRenderCount.fetch_sub(1, std::memory_order_relaxed);
    }

    int pendingRenderCount() const {
        return m_pendingRenderCount.load(std::memory_order_relaxed);
    }

public slots:
    void init();
    void requestPage(int pageIndex, float zoom, int rotation = 0, bool nightMode = false, float dpi = 72.0f, int targetWidth = 0, int targetHeight = 0, int generation = 0, bool priority = false);
    void setBatchInfo(int generation, const QSet<int>& activePages, float zoom = 1.0f);
    PageMetadata getMetadata(int pageIndex);

signals:
    void pageRendered(int pageIndex, QImage image, float zoom);
    void error(const QString& message, int pageIndex = -1);
    void documentLoaded(int pageCount, const QVector<QSizeF>& sizes);
    void metadataUpdated(const QVector<QPair<int, QSizeF>>& updates);

private:
    std::string m_path;
    bool m_isMetadataLoader = false;
    std::unique_ptr<MuPDFDocument> m_doc;
    QMutex m_mutex;
    std::atomic<int> m_currentGeneration{ 0 };
    QSet<int> m_activePages;
    std::atomic<int> m_pendingRenderCount{ 0 };
    std::atomic<fz_cookie*> m_activeCookie{ nullptr };
    std::atomic<float> m_activeZoom{ 1.0f };
};
