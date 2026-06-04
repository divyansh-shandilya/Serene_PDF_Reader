#pragma once
#include <QQuickPaintedItem>
#include <QImage>
#include <QCache>
#include <QThread>
#include <QSet>
#include <QHash>
#include <QElapsedTimer>
#include <QTimer>
#include "../engine/RenderWorker.h"

struct CachedPage {
    QImage image;
    float zoom;
    int pixelWidth = 0;
    int pixelHeight = 0;
};

class PdfQuickItem : public QQuickPaintedItem {
    Q_OBJECT
        Q_PROPERTY(QString source READ source WRITE setSource NOTIFY sourceChanged)
        Q_PROPERTY(int currentPage READ currentPage WRITE setCurrentPage NOTIFY currentPageChanged)
        Q_PROPERTY(float zoom READ zoom WRITE setZoom NOTIFY zoomChanged)
        Q_PROPERTY(int pageCount READ pageCount NOTIFY pageCountChanged)
        Q_PROPERTY(QString readerMode READ readerMode WRITE setReaderMode NOTIFY readerModeChanged)
        Q_PROPERTY(bool freePan READ freePan WRITE setFreePan NOTIFY freePanChanged)
        Q_PROPERTY(bool isDragging READ isDragging WRITE setIsDragging NOTIFY isDraggingChanged)
        Q_PROPERTY(float verticalScrollRatio READ verticalScrollRatio NOTIFY scrollRatioChanged)
        Q_PROPERTY(float scrollX READ scrollX WRITE setScrollX NOTIFY scrollXChanged)
        Q_PROPERTY(float scrollY READ scrollY WRITE setScrollY NOTIFY scrollYChanged)
        Q_PROPERTY(int pageRotation READ pageRotation WRITE setPageRotation NOTIFY pageRotationChanged)
        Q_PROPERTY(bool nightMode READ nightMode WRITE setNightMode NOTIFY nightModeChanged)
        Q_PROPERTY(bool loading READ isLoading NOTIFY loadingChanged)
        Q_PROPERTY(bool decoding READ isDecoding NOTIFY decodingChanged)
        Q_PROPERTY(bool isZooming READ isZooming NOTIFY isZoomingChanged)
        Q_PROPERTY(bool isPinching READ isPinching WRITE setIsPinching NOTIFY isPinchingChanged)
        Q_PROPERTY(bool currentPageReady READ currentPageReady NOTIFY currentPageReadyChanged)

public:
    explicit PdfQuickItem(QQuickItem* parent = nullptr);
    ~PdfQuickItem();

    bool currentPageReady() const {
        return m_pageCache.contains(m_currentPage) || m_thumbCache.contains(m_currentPage) || (!m_lastImage.isNull() && m_lastImagePage == m_currentPage);
    }

    QString source() const { return m_source; }
    Q_INVOKABLE void setSource(const QString& source);

    int currentPage() const { return m_currentPage; }
    Q_INVOKABLE void setCurrentPage(int index);

    float zoom() const { return m_zoom; }
    Q_INVOKABLE void setZoom(float zoom);

    int pageCount() const { return m_pageCount; }

    QString readerMode() const { return m_readerMode; }
    void setReaderMode(const QString& mode);

    bool freePan() const { return m_freePan; }
    void setFreePan(bool enabled);

    bool isDragging() const { return m_isDragging; }
    void setIsDragging(bool dragging) { if (m_isDragging != dragging) { m_isDragging = dragging; emit isDraggingChanged(); } }
    bool isZooming() const { return m_isZooming; }
    bool isPinching() const { return m_isPinching; }
    void setIsPinching(bool p);
    bool isLoading() const { return m_isLoading; }
    bool isDecoding() const { return m_isDecoding; }

    float verticalScrollRatio() const;
    Q_INVOKABLE void scrollToRatio(float ratio);
    Q_INVOKABLE void fitToWidth();
    Q_INVOKABLE void fitToPage();
    Q_INVOKABLE void setZoomAndScroll(float zoom, float x, float y);

    float scrollX() const { return m_scrollX; }
    void setScrollX(float x) { if (!qFuzzyCompare(m_scrollX, x)) { m_scrollX = x; emit scrollXChanged(); update(); } }

    float scrollY() const { return m_scrollY; }
    void setScrollY(float y) { if (!qFuzzyCompare(m_scrollY, y)) { m_scrollY = y; emit scrollYChanged(); update(); } }

    int pageRotation() const { return m_pageRotation; }
    Q_INVOKABLE void setPageRotation(int degrees);

    bool nightMode() const { return m_nightMode; }
    Q_INVOKABLE void setNightMode(bool enabled);

    void paint(QPainter* painter) override;

protected:
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

signals:
    void sourceChanged();
    void currentPageChanged();
    void zoomChanged();
    void pageCountChanged();
    void readerModeChanged();
    void freePanChanged();
    void isDraggingChanged();
    void scrollRatioChanged();
    void scrollXChanged();
    void scrollYChanged();
    void pageRotationChanged();
    void nightModeChanged();
    void loadingChanged();
    void decodingChanged();
    void isZoomingChanged();
    void isPinchingChanged();
    void currentPageReadyChanged();

private slots:
    void onPageRendered(int pageIndex, QImage image, float zoom);
    void onDocumentLoaded(int count, const QVector<QSizeF>& sizes);
    void onMetadataUpdated(const QVector<QPair<int, QSizeF>>& updates);

private:
    QString m_source;
    int m_currentPage = 0;
    float m_zoom = 1.0f;
    int m_pageCount = 0;
    QString m_readerMode = "continuous";
    bool m_freePan = false;
    int m_pageRotation = 0;
    bool m_nightMode = false;

    float m_scrollX = 0;
    float m_scrollY = 0;

    QPointF m_lastMousePos;
    bool m_isDragging = false;
    bool m_isZooming = false;
    bool m_isPinching = false;
    bool m_isLoading = false;

    QTimer* m_zoomTimer = nullptr;
    QTimer* m_renderTimer = nullptr;
    QTimer* m_momentumTimer = nullptr;

    QImage m_lastImage;
    float m_lastImageZoom = 1.0f;
    int m_lastImagePage = -1;

    struct WorkerInfo {
        RenderWorker* worker = nullptr;
        QThread* thread = nullptr;
    };
    QVector<WorkerInfo> m_mainWorkers;
    WorkerInfo m_thumbWorker;
    int m_nextMainWorkerIdx = 0;

    QPointF m_momentumVelocity;
    QElapsedTimer m_momentumElapsedTimer;
    qint64 m_lastMoveTime = 0;

    bool m_isAutoscrolling = false;
    QPointF m_autoscrollStartPos;
    QPointF m_autoscrollCurrentPos;
    QTimer* m_autoscrollTimer = nullptr;
    qint64 m_autoscrollPressTime = 0;
    QElapsedTimer m_lastZoomRenderTime;

    QCache<int, CachedPage> m_pageCache;
    QCache<int, CachedPage> m_thumbCache;
    QSet<int> m_requestedPages;
    QSet<int> m_requestedThumbs;
    QHash<int, int> m_requestedPageGeneration;
    QHash<int, int> m_requestedThumbGeneration;
    int m_renderGeneration = 0;
    float m_lastRenderZoom = -1.0f;
    int m_lastRenderRotation = -1;
    bool m_lastRenderNightMode = false;
    mutable QVector<QSizeF> m_pageSizes;
    mutable float m_cachedMaxRotatedWidth = -1.0f;
    mutable QVector<float> m_pageOffsets; // Cumulative offsets for continuous mode
    mutable float m_totalContentHeight = 0.0f;
    mutable int m_lastRotationForCache = -1;
    mutable float m_lastZoomForCache = -1.0f;

    bool m_isDecoding = false;
    bool m_justRotated = false;
    float m_dpr = 1.0f;

    void updatePageOffsets() const;
    void updateRendering();
    void performRendering();
    void updatePagination();
    float getPageHeight(int index) const;
    float getRotatedWidth(int index) const;
    float getMaxRotatedWidth() const;
    void finalizeScrollChange();
    QSizeF drawCachedPage(QPainter* painter, int index, float x, float y);
};
