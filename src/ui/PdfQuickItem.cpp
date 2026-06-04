#include "PdfQuickItem.h"
#include <cmath>
#include <QPainter>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QCursor>
#include <QTimer>
#include <QQuickWindow>
#include <QTouchEvent>
#include <QDateTime>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#ifdef __linux__
#include <unistd.h>
#include <sys/sysinfo.h>
#endif

#ifdef __APPLE__
#include <unistd.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

static quint64 totalPhysicalMemoryBytes() {
#ifdef _WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        return static_cast<quint64>(status.ullTotalPhys);
    }
#endif
#ifdef __linux__
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        return static_cast<quint64>(info.totalram) * info.mem_unit;
    }
#endif
#ifdef __APPLE__
    int mib[2] = { CTL_HW, HW_MEMSIZE };
    int64_t physical_memory;
    size_t length = sizeof(physical_memory);
    if (sysctl(mib, 2, &physical_memory, &length, NULL, 0) == 0) {
        return static_cast<quint64>(physical_memory);
    }
#endif
    return 8ull * 1024ull * 1024ull * 1024ull; // safe fallback
}

PdfQuickItem::PdfQuickItem(QQuickItem* parent)
    : QQuickPaintedItem(parent) {
    // GPU-backed FBO target keeps pan/zoom compositing smooth at 60Hz.
    setRenderTarget(QQuickPaintedItem::FramebufferObject);

    // High-performance defaults; adjusted safely in performRendering().
    m_pageCache.setMaxCost(256 * 1024 * 1024);
    m_thumbCache.setMaxCost(32 * 1024 * 1024);

    qRegisterMetaType<QSet<int>>("QSet<int>");

    m_zoomTimer = new QTimer(this);
    m_zoomTimer->setSingleShot(true);
    m_zoomTimer->setInterval(24); // Quicker sharpen pass after zoom interaction
    connect(m_zoomTimer, &QTimer::timeout, this, [this]() {
        m_isZooming = false;
        if (!m_isPinching) {
            if (m_readerMode == "continuous") {
                updatePagination();
            }
            finalizeScrollChange();
        }
        emit isZoomingChanged();
        emit scrollRatioChanged();
        updateRendering();
        });

    m_renderTimer = new QTimer(this);
    m_renderTimer->setSingleShot(true);
    m_renderTimer->setInterval(16); // ~60fps debounce
    connect(m_renderTimer, &QTimer::timeout, this, [this]() { performRendering(); });

    m_momentumTimer = new QTimer(this);
    m_momentumTimer->setInterval(16); // ~60fps
    connect(m_momentumTimer, &QTimer::timeout, this, [this]() {
        if (m_isDragging || m_isZooming || m_isPinching) {
            m_momentumTimer->stop();
            return;
        }

        m_scrollX += m_momentumVelocity.x();
        m_scrollY += m_momentumVelocity.y();

        // Apply friction
        float friction = 0.94f; // Faster deceleration for snappier feel
        m_momentumVelocity *= friction;

        if (m_momentumVelocity.manhattanLength() < 0.2f) {
            m_momentumVelocity = QPointF(0, 0);
            m_momentumTimer->stop();
        }

        updatePagination();
        finalizeScrollChange();
        updateRendering();
        update();
        });

    m_autoscrollTimer = new QTimer(this);
    m_autoscrollTimer->setInterval(16);
    connect(m_autoscrollTimer, &QTimer::timeout, this, [this]() {
        if (!m_isAutoscrolling) return;

        QPointF delta = m_autoscrollCurrentPos - m_autoscrollStartPos;

        // Deadzone
        if (delta.manhattanLength() < 10) return;

        // Faster response, smoother acceleration, better control
        auto calcSpeed = [](float d) {
            float absD = std::abs(d);
            float deadzone = 12.0f;
            if (absD < deadzone) return 0.0f;

            // Smoother entry into movement
            float x = (absD - deadzone) / 10.0f;
            // Quadratic for low speeds, transitioning to linear for high speeds to avoid "buggy" jumps
            float s = 0.0f;
            if (x < 5.0f) {
                s = x * x * 2.5f;
            }
            else {
                s = 50.0f + (x - 5.0f) * 45.0f;
            }

            float finalSpeed = (d > 0 ? 1 : -1) * s;
            return qBound(-500.0f, finalSpeed, 500.0f);
            };

        float vx = -calcSpeed(delta.x());
        float vy = -calcSpeed(delta.y());
        m_scrollX += vx;
        m_scrollY += vy;

        // Track velocity for rendering heuristics
        m_momentumVelocity = QPointF(vx, vy);

        updatePagination();
        finalizeScrollChange();
        updateRendering();
        update();
        });

    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton | Qt::MiddleButton);
    setAcceptHoverEvents(true);
    setAcceptTouchEvents(false);
    setFlag(QQuickItem::ItemIsFocusScope, true);
    setFocus(true);
    setAntialiasing(false);

    connect(this, &QQuickItem::widthChanged, this, [this]() {
        finalizeScrollChange();
        updateRendering();
        });
    connect(this, &QQuickItem::heightChanged, this, [this]() {
        finalizeScrollChange();
        updateRendering();
        });
}

PdfQuickItem::~PdfQuickItem() {
    auto cleanupWorker = [](WorkerInfo& info) {
        if (info.thread) {
            if (info.worker) {
                info.worker->disconnect();
                QObject::connect(info.thread, &QThread::finished, info.worker, &QObject::deleteLater);
            }
            QObject::connect(info.thread, &QThread::finished, info.thread, &QObject::deleteLater);
            info.thread->quit();
            info.thread->wait(3000);
            info.thread = nullptr;
            info.worker = nullptr;
        }
        };

    cleanupWorker(m_thumbWorker);
    for (auto& w : m_mainWorkers) {
        cleanupWorker(w);
    }
}

void PdfQuickItem::setIsPinching(bool p) {
    if (m_isPinching == p) return;
    m_isPinching = p;
    if (m_isPinching) {
        if (m_momentumTimer) m_momentumTimer->stop();
        m_momentumVelocity = QPointF(0, 0);
    }
    else {
        finalizeScrollChange();
        updatePagination();
    }
    emit isPinchingChanged();
}

void PdfQuickItem::setSource(const QString& source) {
    if (m_source == source) return;
    QString localPath = source;
    if (localPath.startsWith("file:///")) {
        localPath = QUrl(localPath).toLocalFile();
    }
    m_source = source;

    auto cleanupWorker = [](WorkerInfo& info) {
        if (info.thread) {
            if (info.worker) {
                info.worker->disconnect();
                QObject::connect(info.thread, &QThread::finished, info.worker, &QObject::deleteLater);
            }
            QObject::connect(info.thread, &QThread::finished, info.thread, &QObject::deleteLater);
            info.thread->quit();
            info.thread->wait(3000);
            info.thread = nullptr;
            info.worker = nullptr;
        }
        };

    cleanupWorker(m_thumbWorker);
    for (auto& w : m_mainWorkers) {
        cleanupWorker(w);
    }
    m_mainWorkers.clear();

    m_pageCache.clear();
    m_thumbCache.clear();
    m_requestedPages.clear();
    m_requestedThumbs.clear();
    m_requestedPageGeneration.clear();
    m_requestedThumbGeneration.clear();
    m_nextMainWorkerIdx = 0;
    m_currentPage = 0;
    m_pageCount = 0;
    m_scrollY = 0;
    m_scrollX = 0;
    m_zoom = 1.0f;
    m_pageRotation = 0;
    m_pageSizes.clear();
    m_pageOffsets.clear();
    m_totalContentHeight = 0.0f;
    m_cachedMaxRotatedWidth = -1.0f;
    m_lastRotationForCache = -1;
    m_lastZoomForCache = -1.0f;
    m_isLoading = true;
    emit loadingChanged();

    std::string pathStr = localPath.toStdString();

    // 1. Create Thumbnail Worker (isMetadataLoader = false) with NormalPriority to run concurrently
    m_thumbWorker.thread = new QThread();
    m_thumbWorker.worker = new RenderWorker(pathStr, false);
    m_thumbWorker.worker->moveToThread(m_thumbWorker.thread);
    connect(m_thumbWorker.worker, &RenderWorker::pageRendered, this, &PdfQuickItem::onPageRendered);
    connect(m_thumbWorker.worker, &RenderWorker::error, this, [this](const QString& msg, int idx) {
        qWarning() << "Thumb Worker Error:" << msg << "at page" << idx;
        if (idx != -1) {
            m_requestedThumbs.remove(idx);
            m_requestedThumbGeneration.remove(idx);
        }
        });
    m_thumbWorker.thread->start(QThread::NormalPriority);
    QMetaObject::invokeMethod(m_thumbWorker.worker, "init", Qt::QueuedConnection);

    // 2. Create Main Worker Pool (adaptive worker count for better CPU utilization)
    const int ideal = qMax(1, QThread::idealThreadCount());
    const quint64 memBytes = totalPhysicalMemoryBytes();
    const bool veryLowRam = memBytes <= (4ull * 1024ull * 1024ull * 1024ull);
    const bool lowRam = memBytes <= (8ull * 1024ull * 1024ull * 1024ull);
    const int mainWorkerCount = veryLowRam ? 1 : (lowRam ? qBound(1, ideal - 3, 2) : qBound(1, ideal - 2, 3));
    for (int i = 0; i < mainWorkerCount; ++i) {
        WorkerInfo info;
        info.thread = new QThread();
        // Only the first main worker handles metadata loading
        info.worker = new RenderWorker(pathStr, (i == 0));
        info.worker->moveToThread(info.thread);

        connect(info.worker, &RenderWorker::pageRendered, this, &PdfQuickItem::onPageRendered);
        connect(info.worker, &RenderWorker::error, this, [this](const QString& msg, int idx) {
            qWarning() << "Main Worker Error:" << msg << "at page" << idx;
            if (idx != -1) {
                m_requestedPages.remove(idx);
                m_requestedPageGeneration.remove(idx);
                m_requestedThumbs.remove(idx);
                m_requestedThumbGeneration.remove(idx);
            }
            m_isLoading = false;
            emit loadingChanged();
            });

        if (i == 0) {
            connect(info.worker, &RenderWorker::documentLoaded, this, &PdfQuickItem::onDocumentLoaded);
            connect(info.worker, &RenderWorker::metadataUpdated, this, &PdfQuickItem::onMetadataUpdated);

        }

        info.thread->start(QThread::HighPriority);
        QMetaObject::invokeMethod(info.worker, "init", Qt::QueuedConnection);
        m_mainWorkers.append(info);
    }

    emit sourceChanged();
    emit zoomChanged();
    updateRendering();
}

void PdfQuickItem::setCurrentPage(int index) {
    if (index < 0 || (m_pageCount > 0 && index >= m_pageCount)) return;
    if (m_currentPage == index) return;
    m_currentPage = index;
    emit currentPageChanged();
    emit currentPageReadyChanged();
    emit scrollRatioChanged();
    updateRendering();
}

void PdfQuickItem::setZoom(float zoom) {
    float limitedZoom = qMax(0.1f, qMin(zoom, 4.0f));
    if (qAbs(m_zoom - limitedZoom) < 0.0001f) return;
    m_momentumTimer->stop();
    m_momentumVelocity = QPointF(0, 0);

    if (!m_isZooming) {
        CachedPage* current = m_pageCache.object(m_currentPage);
        if (current) {
            m_lastImage = current->image;
            m_lastImageZoom = current->zoom;
            m_lastImagePage = m_currentPage;
        }
        m_isZooming = true;
        m_lastZoomRenderTime.start();
        emit isZoomingChanged();
    }

    m_zoom = limitedZoom;
    m_requestedPages.clear();
    m_requestedPageGeneration.clear();

    // If it's been more than 200ms since last high-res render during active zooming,
    // force one now to avoid staying blurry for too long during a long slide.
    if (m_lastZoomRenderTime.isValid() && m_lastZoomRenderTime.elapsed() > 200) {
        m_lastZoomRenderTime.restart();
        updateRendering();
    }

    m_zoomTimer->start();

    emit zoomChanged();
    emit scrollRatioChanged();
    update();
}

void PdfQuickItem::setZoomAndScroll(float zoom, float x, float y) {
    float limitedZoom = qMax(0.1f, qMin(zoom, 4.0f));

    bool zoomChanged = qAbs(m_zoom - limitedZoom) > 0.0001f;
    bool scrollXChanged = qAbs(m_scrollX - x) > 0.0001f;
    bool scrollYChanged = qAbs(m_scrollY - y) > 0.0001f;

    if (!zoomChanged && !scrollXChanged && !scrollYChanged) return;

    if (!m_isZooming && zoomChanged) {
        CachedPage* current = m_pageCache.object(m_currentPage);
        if (current) {
            m_lastImage = current->image;
            m_lastImageZoom = current->zoom;
            m_lastImagePage = m_currentPage;
        }
        m_isZooming = true;
        emit isZoomingChanged();
    }

    if (zoomChanged) {
        m_zoom = limitedZoom;
        m_requestedPages.clear();
        m_zoomTimer->start();
        emit this->zoomChanged();
    }

    if (scrollXChanged || zoomChanged) {
        float maxW = getMaxRotatedWidth() * m_zoom;
        if (maxW <= width()) {
            m_scrollX = 0;
        }
        else {
            float limitX = (maxW - width()) / 2.0f;
            m_scrollX = qBound(-limitX, x, limitX);
        }
        emit this->scrollXChanged();
    }

    if (scrollYChanged || zoomChanged) {
        m_scrollY = y;
        // Basic clamp for non-continuous mode
        if (m_readerMode != "continuous") {
            float sh = getPageHeight(m_currentPage) * m_zoom;
            float limitY = qMax(0.0f, (sh - height()) / 2.0f);
            m_scrollY = qBound(-limitY, m_scrollY, limitY);
        }
        emit this->scrollYChanged();
    }

    // Keep pagination updated during pinch/hand navigation too.
    if (m_readerMode == "continuous") {
        updatePagination();
    }

    emit scrollRatioChanged();
    // Request decode/render updates while pinch-pan is in progress.
    updateRendering();
    update();
}

void PdfQuickItem::setPageRotation(int degrees) {
    m_justRotated = true;

    if (m_pageRotation == degrees) return;
    m_pageRotation = degrees;
    m_cachedMaxRotatedWidth = -1.0f;
    m_pageCache.clear();
    m_requestedPages.clear();
    m_requestedPageGeneration.clear();
    emit pageRotationChanged();
    emit scrollRatioChanged();
    updateRendering();
}

void PdfQuickItem::setNightMode(bool enabled) {
    if (m_nightMode != enabled) {
        m_thumbCache.clear();
    }
    if (m_nightMode == enabled) return;
    m_nightMode = enabled;
    m_pageCache.clear();
    m_requestedPages.clear();
    m_requestedPageGeneration.clear();
    emit nightModeChanged();
    updateRendering();
}

void PdfQuickItem::setReaderMode(const QString& mode) {
    if (m_readerMode == mode) return;
    m_readerMode = mode;
    m_scrollY = 0;
    m_scrollX = 0;
    emit readerModeChanged();
    update();
}

void PdfQuickItem::setFreePan(bool enabled) {
    if (m_freePan == enabled) return;
    m_freePan = enabled;
    if (!enabled) {
        m_scrollX = 0;
        m_scrollY = 0;
    }
    emit freePanChanged();
    update();
}

void PdfQuickItem::wheelEvent(QWheelEvent* event) {
    if (m_isAutoscrolling) {
        m_isAutoscrolling = false;
        m_autoscrollTimer->stop();
        unsetCursor();
        m_requestedPages.clear();
        updateRendering();
    }

    QPoint delta = event->angleDelta();
    if (delta.isNull()) return;

    if (event->modifiers() & Qt::ControlModifier) {
        m_momentumTimer->stop();
        float zoomFactor = 1.1f;
        float newZoom = m_zoom;
        if (delta.y() > 0) newZoom *= zoomFactor;
        else newZoom /= zoomFactor;

        newZoom = qBound(0.1f, newZoom, 4.0f);

        if (!qFuzzyCompare(m_zoom, newZoom)) {
            QPointF pos = event->position();
            float mouseRelX = pos.x() - (width() / 2.0f);
            float mouseRelY = pos.y() - (height() / 2.0f);

            CachedPage* current = m_pageCache.object(m_currentPage);
            if (current) {
                m_lastImage = current->image;
                m_lastImageZoom = current->zoom;
                m_lastImagePage = m_currentPage;
            }

            float oldZoom = m_zoom;
            float preZoomX = m_scrollX;
            float preZoomY = m_scrollY;

            if (!m_isZooming) {
                m_isZooming = true;
                emit isZoomingChanged();
            }
            m_zoom = newZoom;
            m_requestedPages.clear();
            m_zoomTimer->start();
            emit zoomChanged();
            emit scrollRatioChanged();

            if (oldZoom > 0.001f) {
                float ratio = m_zoom / oldZoom;

                // Horizontal adjustment: follow mouse ONLY if doc is or will be wider than viewport
                float maxW = getMaxRotatedWidth();
                if (maxW * m_zoom > width() || maxW * oldZoom > width()) {
                    m_scrollX = mouseRelX - (mouseRelX - preZoomX) * ratio;
                }
                else {
                    // Gradual centering instead of hard snap if we were scrolled
                    m_scrollX = preZoomX * 0.8f;
                }

                // Vertical adjustment: anchor to mouse
                m_scrollY = mouseRelY - (mouseRelY - preZoomY) * ratio;
            }
            update();
        }
        event->accept();
        return;
    }

    if (delta.y() != 0) {
        m_momentumTimer->stop();
        if (m_readerMode == "continuous") {
            float currentSh = getPageHeight(m_currentPage) * m_zoom;
            bool fitsVertically = (currentSh <= height() + 40.0f);

            if (fitsVertically) {
                // Paginated scrolling: when zoomed out, snap-scroll the entire page at once
                if (delta.y() < 0) {
                    if (m_currentPage < m_pageCount - 1) {
                        setCurrentPage(m_currentPage + 1);
                        m_scrollY = 0;
                    }
                }
                else if (delta.y() > 0) {
                    if (m_currentPage > 0) {
                        setCurrentPage(m_currentPage - 1);
                        m_scrollY = 0;
                    }
                }
                updatePagination();
                updateRendering();
                emit scrollRatioChanged();
                update();
            }
            else {
                // Fixed scroll speed
                m_scrollY += delta.y() * 1.2f;

                // Fast clamp for continuous mode edges to prevent over-scrolling during scroll
                if (m_currentPage == 0 && m_scrollY > 0) {
                    float sh = getPageHeight(0) * m_zoom;
                    float ly = (sh > height()) ? (sh - height()) / 2.0f : 0;
                    if (m_scrollY > ly) m_scrollY = ly;
                }
                else if (m_currentPage == m_pageCount - 1 && m_scrollY < 0) {
                    float sh = getPageHeight(m_pageCount - 1) * m_zoom;
                    float ly = (sh > height()) ? (sh - height()) / 2.0f : 0;
                    if (m_scrollY < -ly) m_scrollY = -ly;
                }

                updatePagination();
                updateRendering();
                emit scrollRatioChanged();
                update();
            }
        }
        else {
            float currentSh = getPageHeight(m_currentPage) * m_zoom;
            float viewportH = height();
            float limitY = (currentSh > viewportH) ? (currentSh - viewportH) / 2.0f : 0;
            m_scrollY += delta.y() * 0.8f;
            if (!m_freePan) {
                m_scrollY = qBound(-limitY, m_scrollY, limitY);
            }

            if (delta.y() < -40 && m_scrollY <= -limitY + 1) {
                if (m_currentPage < m_pageCount - 1) {
                    setCurrentPage(m_currentPage + 1);
                    float nowSh = getPageHeight(m_currentPage) * m_zoom;
                    m_scrollY = (nowSh > height()) ? (nowSh - height()) / 2.0f : 0;
                }
            }
            else if (delta.y() > 40 && m_scrollY >= limitY - 1) {
                if (m_currentPage > 0) {
                    setCurrentPage(m_currentPage - 1);
                    float nowSh = getPageHeight(m_currentPage) * m_zoom;
                    m_scrollY = (nowSh > height()) ? -(nowSh - height()) / 2.0f : 0;
                }
            }
            update();
        }
        event->accept();
        return;
    }

    if (delta.x() != 0) {
        m_momentumTimer->stop();
        float maxW = getMaxRotatedWidth() * m_zoom;
        float viewportW = width();
        m_scrollX += delta.x();
        if (!m_freePan && maxW > viewportW) {
            float limitX = (maxW - viewportW) / 2.0f;
            m_scrollX = qBound(-limitX, m_scrollX, limitX);
        }
        else if (!m_freePan) {
            m_scrollX = 0; // Hard snap to center if it fits
        }
        update();
    }
    event->accept();
}

void PdfQuickItem::keyPressEvent(QKeyEvent* event) {
    if (m_isAutoscrolling && event->key() == Qt::Key_Escape) {
        m_isAutoscrolling = false;
        m_autoscrollTimer->stop();
        unsetCursor();
        m_requestedPages.clear();
        updateRendering();
        update();
        event->accept();
        return;
    }
    QQuickPaintedItem::keyPressEvent(event);
}

void PdfQuickItem::mousePressEvent(QMouseEvent* event) {
    if (m_isAutoscrolling && event->button() != Qt::MiddleButton) {
        // Match browser behavior: any click exits autoscroll mode.
        m_isAutoscrolling = false;
        m_autoscrollTimer->stop();
        m_momentumVelocity = QPointF(0, 0);
        unsetCursor();
        m_requestedPages.clear();
        updateRendering();
        update();
        event->accept();
        return;
    }

    if (event->button() == Qt::MiddleButton) {
        // Press-and-hold middle button autoscroll.
        m_isAutoscrolling = true;
        m_autoscrollStartPos = event->position();
        m_autoscrollCurrentPos = m_autoscrollStartPos;
        m_autoscrollPressTime = QDateTime::currentMSecsSinceEpoch();
        m_autoscrollTimer->start();
        setCursor(Qt::SizeAllCursor);

        m_momentumTimer->stop();
        m_momentumVelocity = QPointF(0, 0);
        update();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        m_momentumTimer->stop();
        m_momentumVelocity = QPointF(0, 0);

        m_lastMousePos = event->position();
        m_isDragging = true;
        m_momentumElapsedTimer.start();
        m_lastMoveTime = m_momentumElapsedTimer.elapsed();

        emit isDraggingChanged();
        setCursor(Qt::ClosedHandCursor);

        finalizeScrollChange();
        event->accept();
    }
}

void PdfQuickItem::mouseMoveEvent(QMouseEvent* event) {
    if (m_isAutoscrolling) {
        m_autoscrollCurrentPos = event->position();

        QPointF delta = m_autoscrollCurrentPos - m_autoscrollStartPos;
        float dx = delta.x();
        float dy = delta.y();
        float threshold = 15.0f;

        if (std::abs(dx) < threshold && std::abs(dy) < threshold) {
            setCursor(Qt::SizeAllCursor);
        }
        else if (std::abs(dx) >= threshold && std::abs(dy) < threshold) {
            setCursor(Qt::SizeHorCursor);
        }
        else if (std::abs(dx) < threshold && std::abs(dy) >= threshold) {
            setCursor(Qt::SizeVerCursor);
        }
        else {
            setCursor(Qt::SizeAllCursor);
        }

        event->accept();
        return;
    }

    if (m_isDragging && !m_isZooming) {
        QPointF pos = event->position();
        QPointF diff = pos - m_lastMousePos;
        m_lastMousePos = pos;

        qint64 currentTime = m_momentumElapsedTimer.elapsed();
        qint64 dt = currentTime - m_lastMoveTime;
        if (dt > 0 && dt < 100) { // Valid move interval
            // Tighter velocity tracking for better flicking
            float alpha = 0.7f;
            QPointF instantVelocity = (diff * 16.0f) / (float)dt;
            m_momentumVelocity = m_momentumVelocity * (1.0f - alpha) + instantVelocity * alpha;
        }
        m_lastMoveTime = currentTime;

        m_scrollX += diff.x();
        m_scrollY += diff.y();

        updatePagination();
        finalizeScrollChange();
        updateRendering();

        update();
        event->accept();
    }
}

void PdfQuickItem::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton) {
        // Stop immediately on release with no extra momentum carry.
        m_isAutoscrolling = false;
        m_autoscrollTimer->stop();
        m_momentumVelocity = QPointF(0, 0);
        unsetCursor();
        m_requestedPages.clear();
        updateRendering();
        update();
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
        emit isDraggingChanged();
        unsetCursor();

        // Start momentum if speed is significant and we didn't hold still before releasing
        qint64 currentTime = m_momentumElapsedTimer.elapsed();
        if (currentTime - m_lastMoveTime > 80) {
            m_momentumVelocity = QPointF(0, 0);
        }

        float speed = m_momentumVelocity.manhattanLength();
        if (speed > 3.0f) {
            m_momentumTimer->start();
        }
        else {
            m_momentumVelocity = QPointF(0, 0);
            updateRendering();
        }

        event->accept();
    }
}

void PdfQuickItem::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (qAbs(m_zoom - 1.0f) < 0.1f) {
            fitToWidth();
        }
        else {
            setZoom(1.0f);
            m_scrollX = 0;
            m_scrollY = 0;
            emit scrollXChanged();
            emit scrollYChanged();
        }
        event->accept();
    }
}

void PdfQuickItem::fitToWidth() {
    if (m_pageCount <= 0) return;

    float w = getRotatedWidth(m_currentPage);
    if (w > 0) {
        float availableW = width() - 50;
        if (availableW <= 0) {
            setZoom(1.0f);
        }
        else {
            setZoom(availableW / w);
        }
        m_scrollX = 0;
        finalizeScrollChange();
    }
}

void PdfQuickItem::fitToPage() {
    if (m_pageCount <= 0) return;

    float w = getRotatedWidth(m_currentPage);
    float h = getPageHeight(m_currentPage);
    if (w > 0 && h > 0) {
        float availableW = width() - 50;
        float availableH = height() - 50;
        if (availableW <= 0 || availableH <= 0) {
            setZoom(1.0f);
        }
        else {
            float zW = availableW / w;
            float zH = availableH / h;
            setZoom(qMin(zW, zH));
        }
        m_scrollX = 0;
        m_scrollY = 0;
        finalizeScrollChange();
    }
}

void PdfQuickItem::updatePageOffsets() const {
    if (m_pageCount <= 0) {
        m_pageOffsets.clear();
        m_totalContentHeight = 0;
        return;
    }

    if (m_pageOffsets.size() == m_pageCount &&
        qAbs(m_lastZoomForCache - m_zoom) < 0.0001f &&
        m_lastRotationForCache == m_pageRotation) {
        return;
    }

    m_pageOffsets.resize(m_pageCount);
    float currentY = 0;
    float spacing = 30.0f;

    for (int i = 0; i < m_pageCount; ++i) {
        m_pageOffsets[i] = currentY;
        currentY += getPageHeight(i) * m_zoom;
        if (i < m_pageCount - 1) currentY += spacing;
    }

    m_totalContentHeight = currentY;
    m_lastZoomForCache = m_zoom;
    m_lastRotationForCache = m_pageRotation;
}

float PdfQuickItem::verticalScrollRatio() const {
    if (m_pageCount <= 0 || m_isLoading) return 0.0f;

    // Self-healing against NaN scroll state
    if (std::isnan(m_scrollY) || std::isinf(m_scrollY)) {
        const_cast<float&>(m_scrollY) = 0.0f;
    }
    if (std::isnan(m_scrollX) || std::isinf(m_scrollX)) {
        const_cast<float&>(m_scrollX) = 0.0f;
    }
    if (std::isnan(m_zoom) || std::isinf(m_zoom) || m_zoom <= 0.0f) {
        const_cast<float&>(m_zoom) = 1.0f;
    }

    if (m_readerMode == "continuous") {
        updatePageOffsets();
        if (m_totalContentHeight <= height()) return 0.5f;

        float sh0 = getPageHeight(0) * m_zoom;
        float minCenter = (sh0 > height()) ? height() / 2.0f : sh0 / 2.0f;

        float shLast = getPageHeight(m_pageCount - 1) * m_zoom;
        float maxCenter = m_totalContentHeight - ((shLast > height()) ? height() / 2.0f : shLast / 2.0f);

        float range = maxCenter - minCenter;
        if (range <= 0.0f) return 0.5f;

        float sh = getPageHeight(m_currentPage) * m_zoom;
        float viewportCenterY = (m_pageOffsets[m_currentPage] + sh / 2.0f) - m_scrollY;

        float ratio = (viewportCenterY - minCenter) / range;
        if (std::isnan(ratio) || std::isinf(ratio)) return 0.0f;
        return qBound(0.0f, ratio, 1.0f);
    }

    // Single page mode logic
    int safePage = qBound(0, m_currentPage, m_pageCount - 1);
    float sh = getPageHeight(safePage) * m_zoom;
    float limitY = qMax(1.0f, sh - (float)height()) / 2.0f;
    float pageProgress = (limitY - m_scrollY) / (2.0f * limitY);
    if (std::isnan(pageProgress) || std::isinf(pageProgress)) pageProgress = 0.5f;
    if (sh <= height()) pageProgress = 0.5f;
    pageProgress = qBound(0.0f, pageProgress, 1.0f);

    float ratio = (safePage + pageProgress) / (float)m_pageCount;
    if (std::isnan(ratio) || std::isinf(ratio)) return 0.0f;
    return ratio;
}

void PdfQuickItem::scrollToRatio(float ratio) {
    if (m_pageCount <= 0 || m_isLoading) return;
    if (std::isnan(ratio) || std::isinf(ratio)) return;

    // Self-healing check
    if (std::isnan(m_scrollY) || std::isinf(m_scrollY)) m_scrollY = 0.0f;
    if (std::isnan(m_scrollX) || std::isinf(m_scrollX)) m_scrollX = 0.0f;
    if (std::isnan(m_zoom) || std::isinf(m_zoom) || m_zoom <= 0.0f) m_zoom = 1.0f;

    float r = qBound(0.0f, ratio, 1.0f);

    if (m_readerMode == "continuous") {
        updatePageOffsets();

        float sh0 = getPageHeight(0) * m_zoom;
        float minCenter = (sh0 > height()) ? height() / 2.0f : sh0 / 2.0f;

        float shLast = getPageHeight(m_pageCount - 1) * m_zoom;
        float maxCenter = m_totalContentHeight - ((shLast > height()) ? height() / 2.0f : shLast / 2.0f);

        float range = maxCenter - minCenter;
        if (range <= 0.0f || std::isnan(range) || std::isinf(range)) return;

        float targetCenter = minCenter + r * range;
        if (std::isnan(targetCenter) || std::isinf(targetCenter)) return;

        // Binary search for target page
        auto it = std::upper_bound(m_pageOffsets.begin(), m_pageOffsets.end(), targetCenter);
        int targetPage = std::distance(m_pageOffsets.begin(), it) - 1;
        if (targetPage < 0) targetPage = 0;
        if (targetPage >= m_pageCount) targetPage = m_pageCount - 1;

        float ph = getPageHeight(targetPage) * m_zoom;
        float nextScrollY = (ph / 2.0f) - (targetCenter - m_pageOffsets[targetPage]);
        if (!std::isnan(nextScrollY) && !std::isinf(nextScrollY)) {
            m_scrollY = nextScrollY;
        }
        else {
            m_scrollY = 0.0f;
        }

        if (m_currentPage != targetPage) {
            m_currentPage = targetPage;
            emit currentPageChanged();
        }
        emit scrollYChanged();
        emit scrollRatioChanged();
        updateRendering();
        update();
        return;
    }

    // Single page mode
    float globalPage = r * (float)m_pageCount;
    if (std::isnan(globalPage) || std::isinf(globalPage)) globalPage = 0.0f;
    int pageIdx = qBound(0, (int)std::floor(globalPage), m_pageCount - 1);
    float pageProgress = globalPage - (float)pageIdx;
    if (std::isnan(pageProgress) || std::isinf(pageProgress)) pageProgress = 0.0f;

    if (pageIdx == m_pageCount) {
        pageIdx = m_pageCount - 1;
        pageProgress = 1.0f;
    }

    m_currentPage = pageIdx;

    float sh = getPageHeight(m_currentPage) * m_zoom;
    if (sh > height()) {
        float limitY = (sh - (float)height()) / 2.0f;
        float nextScrollY = limitY * (1.0f - 2.0f * pageProgress);
        if (!std::isnan(nextScrollY) && !std::isinf(nextScrollY)) {
            m_scrollY = nextScrollY;
        }
        else {
            m_scrollY = 0.0f;
        }
    }
    else {
        m_scrollY = 0;
    }

    emit currentPageChanged();
    emit scrollYChanged();
    emit scrollRatioChanged();
    updateRendering();
    update();
}

void PdfQuickItem::onPageRendered(int pageIndex, QImage image, float zoom) {
    if (image.isNull()) {
        m_requestedPages.remove(pageIndex);
        m_requestedThumbs.remove(pageIndex);
        m_requestedPageGeneration.remove(pageIndex);
        m_requestedThumbGeneration.remove(pageIndex);
        return;
    }

    // Set device pixel ratio so QPainter knows how to map pixels to points perfectly
    float dpr = window() ? (float)window()->devicePixelRatio() : 1.0f;
    image.setDevicePixelRatio(dpr);

    // Drop far-off pages to avoid cache pollution during fast paging.
    if (qAbs(pageIndex - m_currentPage) > 4) {
        m_requestedPages.remove(pageIndex);
        m_requestedThumbs.remove(pageIndex);
        m_requestedPageGeneration.remove(pageIndex);
        m_requestedThumbGeneration.remove(pageIndex);
        return;
    }

    // Zoom of 0.2 is used for thumbnails in performRendering
    if (zoom <= 0.21f) {
        m_requestedThumbs.remove(pageIndex);
        m_requestedThumbGeneration.remove(pageIndex);
        int sizeBytes = (int)image.sizeInBytes();
        m_thumbCache.insert(pageIndex, new CachedPage{ image, zoom, image.width(), image.height() }, sizeBytes);
    }
    else {
        m_requestedPages.remove(pageIndex);
        m_requestedPageGeneration.remove(pageIndex);
        int sizeBytes = (int)image.sizeInBytes();
        CachedPage* existing = m_pageCache.object(pageIndex);
        if (!existing || zoom >= existing->zoom - 0.001f) {
            m_pageCache.insert(pageIndex, new CachedPage{ image, zoom, image.width(), image.height() }, sizeBytes);
            if (pageIndex == m_currentPage) {
                m_lastImage = image;
                m_lastImageZoom = zoom;
                m_lastImagePage = pageIndex;
            }
        }

        // Update the zoom-fallback image if this is the current page (turned off to protect cache quality)
        if (false && pageIndex == m_currentPage) {
            m_lastImage = image;
            m_lastImageZoom = zoom;
            m_lastImagePage = pageIndex;
        }
    }

    bool shouldBeDecoding = !m_requestedPages.isEmpty() || !m_requestedThumbs.isEmpty();
    if (m_isDecoding != shouldBeDecoding) {
        m_isDecoding = shouldBeDecoding;
        emit decodingChanged();
    } // Immediately update if this page is in viewport
    if (pageIndex == m_currentPage) {
        emit currentPageReadyChanged();
    }
    update();
}

void PdfQuickItem::onMetadataUpdated(const QVector<QPair<int, QSizeF>>& updates) {
    if (m_pageCount <= 0) return;

    bool sizeChanged = false;
    for (const auto& update : updates) {
        int idx = update.first;
        if (idx >= 0 && idx < m_pageSizes.size()) {
            if (m_pageSizes[idx] != update.second) {
                m_pageSizes[idx] = update.second;
                sizeChanged = true;
            }
        }
    }

    if (sizeChanged) {
        m_cachedMaxRotatedWidth = -1.0f;
        // Optimization: don't call updatePageOffsets here, it will be called 
        // when needed in verticalScrollRatio or updatePagination.
        // But we MUST invalidate the flag that says it's current.
        m_lastZoomForCache = -1.0f;

        emit scrollRatioChanged();
        update();
    }
}

void PdfQuickItem::onDocumentLoaded(int count, const QVector<QSizeF>& sizes) {
    m_pageCount = count;
    m_pageSizes = sizes;

    m_cachedMaxRotatedWidth = -1.0f;
    m_lastZoomForCache = -1.0f;
    m_isLoading = false;

    // Clamp m_currentPage to valid range
    if (m_pageCount > 0) {
        m_currentPage = qBound(0, m_currentPage, m_pageCount - 1);

        if (m_readerMode == "continuous") {
            updatePageOffsets();
        }

        // AUTO-FIT: For any PDF, open at the correct zoom percentage to see full page
        fitToPage();
    }

    emit loadingChanged();
    emit pageCountChanged();
    emit scrollRatioChanged();
    emit currentPageChanged();
    emit currentPageReadyChanged();
    updateRendering();
}

float PdfQuickItem::getPageHeight(int index) const {
    if (index < 0 || index >= m_pageSizes.size()) return 0;
    QSizeF s = m_pageSizes[index];
    if (s.isEmpty()) {
        return 842.0f; // Default A4 if not loaded yet
    }

    // If rotated 90 or 270, height and width are swapped
    if (m_pageRotation == 90 || m_pageRotation == 270) {
        return s.width();
    }
    return s.height();
}

void PdfQuickItem::finalizeScrollChange() {
    // Self-healing against NaN scroll state
    if (std::isnan(m_scrollY) || std::isinf(m_scrollY)) m_scrollY = 0.0f;
    if (std::isnan(m_scrollX) || std::isinf(m_scrollX)) m_scrollX = 0.0f;
    if (std::isnan(m_zoom) || std::isinf(m_zoom) || m_zoom <= 0.0f) m_zoom = 1.0f;

    if (m_freePan) {
        emit scrollYChanged();
        emit scrollXChanged();
        return;
    }

    float sh = getPageHeight(m_currentPage) * m_zoom;
    float maxW = getRotatedWidth(m_currentPage) * m_zoom;

    float limitX = qMax(0.0f, (maxW - width()) / 2.0f);
    m_scrollX = qBound(-limitX, m_scrollX, limitX);

    if (m_readerMode == "continuous") {
        // Only clamp at the absolute start and end of the document
        if (m_currentPage == 0) {
            float ly = (sh > height()) ? (sh - height()) / 2.0f : 0;
            if (m_scrollY > ly) m_scrollY = ly;
        }
        if (m_currentPage == m_pageCount - 1) {
            float ly = (sh > height()) ? (sh - height()) / 2.0f : 0;
            if (m_scrollY < -ly) m_scrollY = -ly;
        }
    }
    else {
        float limitY = qMax(0.0f, (sh - height()) / 2.0f);
        m_scrollY = qBound(-limitY, m_scrollY, limitY);
    }

    emit scrollYChanged();
    emit scrollXChanged();
    emit scrollRatioChanged();
}

void PdfQuickItem::updatePagination() {
    if (m_readerMode != "continuous" || m_pageCount <= 0) return;

    float spacing = 30.0f;
    bool pageChanged = false;

    // Scrolling down (content moves up, m_scrollY becomes more negative)
    while (m_currentPage < m_pageCount - 1) {
        float sh = getPageHeight(m_currentPage) * m_zoom;

        if (m_scrollY + sh / 2.0f + spacing / 2.0f < 0) {
            int nextIdx = m_currentPage + 1;
            float nextSh = getPageHeight(nextIdx) * m_zoom;

            m_scrollY = m_scrollY + sh / 2.0f + spacing + nextSh / 2.0f;
            m_currentPage = nextIdx;
            pageChanged = true;
        }
        else {
            break;
        }
    }

    // Scrolling up (content moves down, m_scrollY becomes more positive)
    while (m_currentPage > 0) {
        float sh = getPageHeight(m_currentPage) * m_zoom;

        if (m_scrollY - sh / 2.0f - spacing / 2.0f > 0) {
            int prevIdx = m_currentPage - 1;
            float prevSh = getPageHeight(prevIdx) * m_zoom;

            m_scrollY = m_scrollY - sh / 2.0f - spacing - prevSh / 2.0f;
            m_currentPage = prevIdx;
            pageChanged = true;
        }
        else {
            break;
        }
    }

    if (pageChanged) {
        emit currentPageChanged();
        emit currentPageReadyChanged();
        emit scrollYChanged();
        emit scrollRatioChanged();
        updateRendering();
    }
}

float PdfQuickItem::getRotatedWidth(int index) const {
    if (index < 0 || index >= m_pageSizes.size()) return 0;
    QSizeF s = m_pageSizes[index];
    if (s.isEmpty()) {
        getPageHeight(index); // This will populate m_pageSizes
        s = m_pageSizes[index];
    }
    if (m_pageRotation == 90 || m_pageRotation == 270) return s.height();
    return s.width();
}

float PdfQuickItem::getMaxRotatedWidth() const {
    if (m_cachedMaxRotatedWidth >= 0) return m_cachedMaxRotatedWidth;

    float maxW = 0;
    for (int i = 0; i < m_pageCount; ++i) {
        maxW = qMax(maxW, getRotatedWidth(i));
    }
    m_cachedMaxRotatedWidth = maxW;
    return maxW;
}

void PdfQuickItem::updateRendering() {
    if (!m_renderTimer) return;

    // Keep render scheduling on a strict 16ms (~60Hz) cadence.
    int interval = 16;

    if (m_renderTimer->interval() != interval) {
        m_renderTimer->setInterval(interval);
    }

    if (!m_renderTimer->isActive()) {
        m_renderTimer->start();
    }
}

void PdfQuickItem::performRendering() {
    if (m_mainWorkers.isEmpty() || m_source.isEmpty()) return;
    if (width() <= 0 || height() <= 0) return;
    qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    // During active pinch and zoom, skip background rendering queue dispatch entirely. This keeps CPU/threads totally free
    // for seamless 60+ FPS UI scaling on the main thread. High-res redraft is triggered as soon as zoom settling finishes.
    if (m_isZooming || m_isPinching) {
        return;
    }

    bool active = m_isDragging || m_isZooming || m_isPinching || m_isAutoscrolling;

    // During active hand interaction, throttle decode dispatch to prevent queue thrash.
    // Painting still runs every frame from cached/thumbnail content.
    static qint64 lastDecodeDispatchMs = 0;
    int throttleMs = 16;  // Default 60Hz
    if (m_zoom >= 3.0f) {
        throttleMs = 48;  // 20Hz at 3x+ zoom (48ms)
    }
    else if (m_zoom >= 2.0f) {
        throttleMs = 32;  // 30Hz at 2x+ zoom (32ms)
    }

    if (active && (nowMs - lastDecodeDispatchMs) < throttleMs) {
        return;
    }
    lastDecodeDispatchMs = nowMs;

    // Decide DPR once, use everywhere!
    m_dpr = window() ? (float)window()->devicePixelRatio() : 1.0f;

    // Memory pressure check: clamp total system RAM to a safe 4GB maximum for sandboxed container environments.
    quint64 totalRam = qMin(totalPhysicalMemoryBytes(), 4ull * 1024ull * 1024ull * 1024ull);
    quint64 currentCacheBytes = (quint64)m_pageCache.totalCost() + (quint64)m_thumbCache.totalCost();
    if (currentCacheBytes > (totalRam / 3)) { // 33% maximum of assumed RAM
        qWarning() << "Memory pressure detected: cache size" << currentCacheBytes << "bytes exceeds limit";
        return;
    }

    // Dynamic memory budget by RAM tier.
    const bool veryLowRam = totalRam <= (2ull * 1024ull * 1024ull * 1024ull);
    const bool lowRam = totalRam <= (4ull * 1024ull * 1024ull * 1024ull);

    float velocity = m_momentumVelocity.manhattanLength();
    bool isMovingFast = (velocity > 40.0f);
    const bool isHighZoom = (m_zoom > 1.6f);

    // Momentum velocity check: if |v| > 150px/s, skip pre-render entirely to preserve UI resources
    bool isMomentumScrolling = m_momentumTimer->isActive();
    float pY_vel = m_momentumVelocity.y();
    float pX_vel = m_momentumVelocity.x();
    float velocityLength = std::sqrt(pX_vel * pX_vel + pY_vel * pY_vel);
    if (isMomentumScrolling && velocityLength > 150.0f) {
        return;
    }

    // Visible pages set
    QSet<int> activePages;
    activePages.insert(m_currentPage);

    // Direct, stable cache budgets to completely prevent cache eviction thrashing on zoom.
    int pageCapMb = 256;
    int thumbCapMb = 32;
    if (veryLowRam) {
        pageCapMb = 128;
        thumbCapMb = 16;
    }
    else if (lowRam) {
        pageCapMb = 192;
        thumbCapMb = 24;
    }

    m_pageCache.setMaxCost(pageCapMb * 1024 * 1024);
    m_thumbCache.setMaxCost(thumbCapMb * 1024 * 1024);

    // Trim cache footprint periodically (every 1000ms or on 2-page center shifts) to avoid allocator stutter.
    static qint64 lastPruneMs = 0;
    static int lastPruneCenter = -99999;
    const bool centerShifted = qAbs(m_currentPage - lastPruneCenter) >= 2;
    if ((nowMs - lastPruneMs > 1000) || centerShifted) {
        const int keepPageRadius = isHighZoom ? 3 : 5;
        const int keepThumbRadius = isHighZoom ? 5 : 8;
        for (int i = 0; i < m_pageCount; ++i) {
            if (activePages.contains(i)) continue;

            if (qAbs(i - m_currentPage) > keepPageRadius) {
                m_pageCache.remove(i);
            }
            if (qAbs(i - m_currentPage) > keepThumbRadius) {
                m_thumbCache.remove(i);
            }
        }
        lastPruneMs = nowMs;
        lastPruneCenter = m_currentPage;
    }

    // Aggressively reduce pre-render range during active drag/scroll interaction to 0.85x viewport height,
    // which keeps input latency phenomenally low. Use 1.5x height when stationary or idle.
    float bufferMultiplier = active ? 0.85f : 1.50f;

    // Scale pre-render appropriately based on zoom to save memory on heavy zooms.
    if (m_zoom > 1.6f) {
        bufferMultiplier *= 0.70f;
    }

    float upBuffer = height() * bufferMultiplier;
    float downBuffer = height() * bufferMultiplier;
    // Predictive buffering: pre-render slightly farther in the direction of travel.
    if (m_momentumVelocity.y() < -5.0f) downBuffer *= 1.4f;
    else if (m_momentumVelocity.y() > 5.0f) upBuffer *= 1.4f;

    static int lastRenderPage = -1;
    static qint64 lastRenderMs = 0;
    int jumpedPages = (lastRenderPage >= 0) ? qAbs(m_currentPage - lastRenderPage) : 0;
    int scrollDirection = (lastRenderPage >= 0) ? (m_currentPage - lastRenderPage) : 0;
    qint64 dtMs = (lastRenderMs > 0) ? (nowMs - lastRenderMs) : 1000;
    bool rapidPaging = jumpedPages >= 1 && dtMs < 180;
    bool veryRapidPaging = jumpedPages >= 2 && dtMs < 140;
    lastRenderPage = m_currentPage;
    lastRenderMs = nowMs;

    // Use low quality pass during drag, active flicks, rapid paging, or any active momentum scrolling!
    const bool lowQualityPass = active || isMovingFast || rapidPaging || isMomentumScrolling;

    if (m_readerMode == "continuous") {
        if (isMomentumScrolling) {
            // On momentum scroll: don't request pages beyond current ±1 (no speculative decode)
            activePages.clear();
            activePages.insert(m_currentPage);
            if (m_currentPage - 1 >= 0) activePages.insert(m_currentPage - 1);
            if (m_currentPage + 1 < m_pageCount) activePages.insert(m_currentPage + 1);
        }
        else {
            float sh = getPageHeight(m_currentPage) * m_zoom;
            float spacing = 30.0f;
            float pageY = (height() / 2.0f) - (sh / 2.0f) + m_scrollY;
            float upY = pageY - spacing;
            int upIdx = m_currentPage - 1;
            while (upY > -upBuffer && upIdx >= 0) {
                activePages.insert(upIdx);
                upY -= (getPageHeight(upIdx) * m_zoom + spacing);
                upIdx--;
            }
            float downY = pageY + sh + spacing;
            int downIdx = m_currentPage + 1;
            while (downY < height() + downBuffer && downIdx < m_pageCount) {
                activePages.insert(downIdx);
                downY += (getPageHeight(downIdx) * m_zoom + spacing);
                downIdx++;
            }
        }
    }

    // Hard-cancel mode for big jumps: focus only destination page and immediate neighbors.
    // This avoids spending CPU on stale in-between work during scrollbar jumps/flicks.
    const bool hardCancelMode = jumpedPages >= 3 && dtMs < 220;

    // Detect zoom/rotation/night mode/jump changes to selectively increment generation
    bool shouldInvalidate = false;
    if (m_lastRenderZoom < 0.0f) {
        shouldInvalidate = true;
    }
    else if (qAbs(m_zoom - m_lastRenderZoom) > 0.01f) {
        shouldInvalidate = true;
    }
    else if (m_pageRotation != m_lastRenderRotation) {
        shouldInvalidate = true;
    }
    else if (m_nightMode != m_lastRenderNightMode) {
        shouldInvalidate = true;
    }
    else if (hardCancelMode) {
        shouldInvalidate = true;
    }

    if (shouldInvalidate) {
        m_renderGeneration++;
        m_lastRenderZoom = m_zoom;
        m_lastRenderRotation = m_pageRotation;
        m_lastRenderNightMode = m_nightMode;

        // Lock-free instant cancellation of older generation rendering requests on all workers
        for (auto& w : m_mainWorkers) {
            if (w.worker) {
                w.worker->cancelOlderThan(m_renderGeneration);
            }
        }
        if (m_thumbWorker.worker) {
            m_thumbWorker.worker->cancelOlderThan(m_renderGeneration);
        }
    }

    // Inform all workers about currently active pages and zoom level
    for (auto& w : m_mainWorkers) {
        if (w.worker) {
            QMetaObject::invokeMethod(w.worker, "setBatchInfo", Qt::QueuedConnection,
                Q_ARG(int, m_renderGeneration), Q_ARG(QSet<int>, activePages), Q_ARG(float, m_zoom));
        }
    }

    auto requestPage = [this, lowQualityPass, isHighZoom, active](int idx, float z, bool isThumb, bool priority = false) {
        if (idx < 0 || idx >= m_pageCount) return;
        float pageW = getRotatedWidth(idx);

        if (isThumb) {
            if (m_thumbCache.contains(idx)) return;
            const int reqGen = m_requestedThumbGeneration.value(idx, -1);
            if (reqGen == m_renderGeneration && m_requestedThumbs.contains(idx)) return;
        }
        else {
            CachedPage* cached = m_pageCache.object(idx);
            bool needsRefresh = !cached;
            if (cached) {
                // If we already have the page rendered at the full target zoom (or higher), we never need to refresh or downgrade it during interactions.
                if (cached->zoom >= m_zoom - 0.001f) {
                    needsRefresh = false;
                }
                // If the cached zoom is less than the requested zoom, we need a higher resolution refresh.
                else if (cached->zoom < z - 0.001f) {
                    needsRefresh = true;
                }
                // Otherwise, the cached zoom is acceptable, no refresh needed.
                else {
                    needsRefresh = false;
                }
            }

            // If current cache is under-resolved for this target, force high-resolution refresh.
            if (!needsRefresh && cached) {
                bool useLowQualityMod = lowQualityPass;
                int baseMaxDim = isThumb ? 900 : (useLowQualityMod ? 2200 : (priority ? 5200 : 4200));
                int viewportDim = qMax((int)std::round(width() * m_dpr), (int)std::round(height() * m_dpr));
                int maxDim = qMax(baseMaxDim, viewportDim + (isThumb ? 0 : 512));

                if (isHighZoom && !isThumb) {
                    int cap = priority ? (viewportDim * 5 / 4) : viewportDim;
                    if (useLowQualityMod) {
                        cap = cap * 3 / 4;
                    }
                    int minCap = useLowQualityMod ? 1400 : 1800;
                    cap = qBound(minCap, cap, 3200);
                    maxDim = qMin(maxDim, cap);
                }

                int tw = qMin(maxDim, (int)std::round(pageW * z * m_dpr));
                int th = qMin(maxDim, (int)std::round(getPageHeight(idx) * z * m_dpr));

                if (!isThumb && idx == m_currentPage && active && m_zoom > 1.6f) {
                    int vw = qMax(1, (int)std::round(width() * m_dpr * 1.35f));
                    int vh = qMax(1, (int)std::round(height() * m_dpr * 1.35f));
                    tw = qMin(tw, vw);
                    th = qMin(th, vh);
                }

                const bool lowResCached = (cached->pixelWidth < (int)(tw * 0.92f)) ||
                    (cached->pixelHeight < (int)(th * 0.92f));
                if (lowResCached) {
                    needsRefresh = true;
                }
            }
            if (!needsRefresh) return;
            const int reqGen = m_requestedPageGeneration.value(idx, -1);
            if (reqGen == m_renderGeneration && m_requestedPages.contains(idx)) return;
        }

        RenderWorker* target = nullptr;
        if (isThumb) {
            target = m_thumbWorker.worker;
        }
        else {
            if (m_mainWorkers.isEmpty()) return;
            // Dedicated lane: worker 0 is reserved for current-page priority work.
            if (priority) {
                target = m_mainWorkers[0].worker;
            }
            else if (m_mainWorkers.size() > 1) {
                int activeWorkerLimit = m_mainWorkers.size();
                if (m_zoom >= 3.0f) {
                    activeWorkerLimit = 1; // Only 1 worker under 3x+ zoom
                }
                else if (m_zoom >= 2.0f) {
                    activeWorkerLimit = qMin(2, (int)m_mainWorkers.size()); // Max 2 workers under 2x+ zoom
                }

                if (activeWorkerLimit <= 1) {
                    target = m_mainWorkers[0].worker;
                }
                else {
                    if (m_nextMainWorkerIdx <= 0 || m_nextMainWorkerIdx >= activeWorkerLimit) {
                        m_nextMainWorkerIdx = 1;
                    }
                    target = m_mainWorkers[m_nextMainWorkerIdx].worker;
                    m_nextMainWorkerIdx++;
                    if (m_nextMainWorkerIdx >= activeWorkerLimit) {
                        m_nextMainWorkerIdx = 1;
                    }
                }
            }
            else {
                target = m_mainWorkers[0].worker;
            }
        }

        if (!target) return;

        int maxPending = 2;
        if (m_zoom >= 3.0f) {
            maxPending = 1;
        }
        if (target->pendingRenderCount() >= maxPending) return;

        if (isThumb) {
            m_requestedThumbs.insert(idx);
            m_requestedThumbGeneration.insert(idx, m_renderGeneration);
        }
        else {
            m_requestedPages.insert(idx);
            m_requestedPageGeneration.insert(idx, m_renderGeneration);
        }

        bool useLowQualityMod = lowQualityPass;
        float baseDpi = isThumb ? 24.0f : (useLowQualityMod ? 72.0f : (priority ? 132.0f : 108.0f));
        float targetDpi = baseDpi * m_dpr;

        int baseMaxDim = isThumb ? 900 : (useLowQualityMod ? 2200 : (priority ? 5200 : 4200));
        int viewportDim = qMax((int)std::round(width() * m_dpr), (int)std::round(height() * m_dpr));
        int maxDim = qMax(baseMaxDim, viewportDim + (isThumb ? 0 : 512));

        if (isHighZoom && !isThumb) {
            // Under high zoom, we cap the max dimension adaptively to save massive CPU and RAM.
            int cap = priority ? (viewportDim * 5 / 4) : viewportDim;
            if (useLowQualityMod) {
                cap = cap * 3 / 4; // Render even smaller during active motion/scrolling
            }
            // Ensure a reasonable range (e.g. between 1400 and 3200 pixels)
            int minCap = useLowQualityMod ? 1400 : 1800;
            cap = qBound(minCap, cap, 3200);
            maxDim = qMin(maxDim, cap);
        }

        int tw = qMin(maxDim, (int)std::round(pageW * z * m_dpr));
        int th = qMin(maxDim, (int)std::round(getPageHeight(idx) * z * m_dpr));

        // Current-page fast path: during interaction, prioritize viewport detail over full-page rerender.
        if (!isThumb && idx == m_currentPage && active && m_zoom > 1.6f) {
            if (priority && active) {
                targetDpi = qMin(targetDpi, 84.0f * m_dpr);
            }
            int vw = qMax(1, (int)std::round(width() * m_dpr * 1.35f));
            int vh = qMax(1, (int)std::round(height() * m_dpr * 1.35f));
            tw = qMin(tw, vw);
            th = qMin(th, vh);
        }

        target->incrementPendingRenderCount();
        QMetaObject::invokeMethod(target, "requestPage", Qt::QueuedConnection,
            Q_ARG(int, idx), Q_ARG(float, z), Q_ARG(int, m_pageRotation),
            Q_ARG(bool, m_nightMode), Q_ARG(float, targetDpi),
            Q_ARG(int, tw), Q_ARG(int, th),
            Q_ARG(int, m_renderGeneration), Q_ARG(bool, priority));
        };

    if (m_justRotated) {
        m_justRotated = false;
        // Pre-warm thumbnails only (fast, lightweight)
        int thumbRadius = qMin(3, m_pageCount);
        for (int i = m_currentPage - thumbRadius; i <= m_currentPage + thumbRadius; ++i) {
            requestPage(i, 0.2f, true, true);
        }
        // Skip full-res pre-render; user can request on scroll
        update();
        return;
    }

    // TIER 1: Always prioritize current page. Scale down slightly during interaction to save bytes and render instantly.
    float primaryZoom = lowQualityPass ? qMax(0.35f, m_zoom * (isHighZoom ? 0.75f : 0.85f)) : m_zoom;
    requestPage(m_currentPage, primaryZoom, false, true);

    // TIER 2: Render immediate neighbors early so split-view transitions stay crisp.
    // We only prioritize current +/- 1 and never expand to far pages to preserve performance.
    if (!veryRapidPaging) {
        const int prevIdx = m_currentPage - 1;
        const int nextIdx = m_currentPage + 1;
        const float neighborZoom = lowQualityPass ? qMax(0.35f, m_zoom * (isHighZoom ? 0.70f : 0.80f)) : m_zoom;

        // Directional prefetch: page in movement direction gets rendered first.
        if (scrollDirection > 0) {
            requestPage(nextIdx, neighborZoom, false, false);
            requestPage(prevIdx, neighborZoom, false, false);
        }
        else {
            requestPage(prevIdx, neighborZoom, false, false);
            requestPage(nextIdx, neighborZoom, false, false);
        }

        // Only promote neighbors to full quality when not in extreme zoom.
        if (!isHighZoom && !lowQualityPass && qAbs(neighborZoom - m_zoom) > 0.001f) {
            if (scrollDirection > 0) {
                requestPage(nextIdx, m_zoom, false, false);
                requestPage(prevIdx, m_zoom, false, false);
            }
            else {
                requestPage(prevIdx, m_zoom, false, false);
                requestPage(nextIdx, m_zoom, false, false);
            }
        }
    }

    // TIER 3: Lightweight safety-net previews around current page.
    // When paging very rapidly, only keep the current page preview hot.
    int thumbRadius = (veryRapidPaging || hardCancelMode || active) ? 0 : 1;
    for (int i = m_currentPage - thumbRadius; i <= m_currentPage + thumbRadius; ++i) {
        bool isImmediate = (qAbs(i - m_currentPage) <= 1);
        requestPage(i, 0.2f, true, isImmediate);
    }

    // TIER 4: Once stable, force full-quality render only for the current page.
    if (!lowQualityPass && qAbs(primaryZoom - m_zoom) > 0.001f) {
        requestPage(m_currentPage, m_zoom, false, true);
    }

    // 1. Prune requested pages and thumbnails that are no longer active/near the viewport during continuous scrolling
    QList<int> inactivePages;
    for (int idx : m_requestedPages) {
        if (!activePages.contains(idx)) {
            inactivePages.append(idx);
        }
    }
    for (int idx : inactivePages) {
        m_requestedPages.remove(idx);
        m_requestedPageGeneration.remove(idx);
    }

    QList<int> inactiveThumbs;
    for (int idx : m_requestedThumbs) {
        // Keep a small buffer of 2 pages around current page for thumb safety
        if (qAbs(idx - m_currentPage) > 2) {
            inactiveThumbs.append(idx);
        }
    }
    for (int idx : inactiveThumbs) {
        m_requestedThumbs.remove(idx);
        m_requestedThumbGeneration.remove(idx);
    }

    // 2. Prune obsolete generation tracking entries when invalidating queue
    if (shouldInvalidate) {
        QList<int> obsoletePages;
        for (int idx : m_requestedPages) {
            if (m_requestedPageGeneration.value(idx, -1) < m_renderGeneration) {
                obsoletePages.append(idx);
            }
        }
        for (int idx : obsoletePages) {
            m_requestedPages.remove(idx);
            m_requestedPageGeneration.remove(idx);
        }

        QList<int> obsoleteThumbs;
        for (int idx : m_requestedThumbs) {
            if (m_requestedThumbGeneration.value(idx, -1) < m_renderGeneration) {
                obsoleteThumbs.append(idx);
            }
        }
        for (int idx : obsoleteThumbs) {
            m_requestedThumbs.remove(idx);
            m_requestedThumbGeneration.remove(idx);
        }
    }

    bool shouldBeDecoding = !m_requestedPages.isEmpty() || !m_requestedThumbs.isEmpty();
    if (m_isDecoding != shouldBeDecoding) {
        m_isDecoding = shouldBeDecoding;
        emit decodingChanged();
    }

    update();
}

QSizeF PdfQuickItem::drawCachedPage(QPainter* painter, int idx, float x, float y) {
    float pageW = getRotatedWidth(idx);
    float pageH = getPageHeight(idx);

    float sw = pageW * m_zoom;
    float sh = pageH * m_zoom;
    // Round to avoid subpixel rendering and texture-sampling blurriness
    QRectF targetRect(std::round(x), std::round(y), std::round(sw), std::round(sh));

    CachedPage* cached = m_pageCache.object(idx);
    CachedPage* thumb = m_thumbCache.object(idx);
    bool foundImage = false;
    const bool activeMotion = m_isDragging || m_isZooming || m_isPinching || m_isAutoscrolling ||
        (m_momentumVelocity.manhattanLength() > 8.0f);

    painter->save();

    if (cached) {
        foundImage = true;
        bool isExactScale = qAbs(cached->zoom - m_zoom) < 0.001f;
        bool upscale = cached->image.width() < (int)std::round(targetRect.width()) ||
            cached->image.height() < (int)std::round(targetRect.height());
        if (!isExactScale || upscale) {
            painter->setRenderHint(QPainter::SmoothPixmapTransform);
        }
        painter->drawImage(targetRect, cached->image);
    }
    else if (thumb) {
        foundImage = true;
        painter->setRenderHint(QPainter::SmoothPixmapTransform);
        painter->drawImage(targetRect, thumb->image);
    }
    else if (idx == m_currentPage && !m_lastImage.isNull() && m_lastImagePage == m_currentPage) {
        foundImage = true;
        painter->setRenderHint(QPainter::SmoothPixmapTransform);
        painter->drawImage(targetRect, m_lastImage);
    }
    else {
        // Placeholder background - matches page color
        painter->fillRect(targetRect, m_nightMode ? QColor("#1A1A1A") : Qt::white);

        // Draw page index as placeholder text
        painter->setPen(m_nightMode ? QColor("#444444") : QColor("#DDDDDD"));
        QFont font = painter->font();
        font.setPointSize(24);
        font.setBold(true);
        painter->setFont(font);
        painter->drawText(targetRect, Qt::AlignCenter, QString::number(idx + 1));
    }

    // Page border - always draw for visual stability, but lighter while scaling
    bool isExactScale = cached && qAbs(cached->zoom - m_zoom) < 0.001f;
    QColor borderColor = m_nightMode ? QColor("#333333") : QColor("#E8E7E0");
    if (!isExactScale && foundImage) {
        borderColor.setAlpha(120); // Fainter during zoom/scaling
    }
    painter->setPen(borderColor);
    painter->drawRect(targetRect);
    painter->restore();

    return foundImage ? QSizeF(sw, sh) : QSizeF(0, 0); // Return invalid size if only placeholder was drawn
}



void PdfQuickItem::paint(QPainter* painter) {
    if (m_pageCount <= 0) return;
    painter->setRenderHint(QPainter::LosslessImageRendering, true);

    float centerX = width() / 2.0f;
    float centerY = height() / 2.0f;

    float pW = getRotatedWidth(m_currentPage);
    float pH = getPageHeight(m_currentPage);

    float sw = pW * m_zoom;
    float sh = pH * m_zoom;

    float pageX = centerX - sw / 2.0f + m_scrollX;
    float pageY = centerY - sh / 2.0f + m_scrollY;

    // Draw main page (handled by drawCachedPage with fallbacks)
    drawCachedPage(painter, m_currentPage, pageX, pageY);

    if (m_readerMode == "continuous") {
        float spacing = 30.0f;
        const bool highZoom = m_zoom > 1.6f;
        const bool activeMotion = m_isDragging || m_isZooming || m_isPinching || m_isAutoscrolling ||
            (m_momentumVelocity.manhattanLength() > 8.0f);
        // Generous vertical paint buffer so pages don't abruptly clip at viewport edges during flicks.
        float paintBuffer = height() * (highZoom ? (activeMotion ? 0.70f : 1.20f) : (activeMotion ? 1.50f : 2.00f));
        int maxNeighborPaint = highZoom ? (activeMotion ? 3 : 4) : (activeMotion ? 5 : 8);

        // Next pages
        float currentNextY = pageY + sh + spacing;
        int nextIdx = m_currentPage + 1;
        int paintedNext = 0;
        while (currentNextY < height() + paintBuffer && nextIdx < m_pageCount && paintedNext < maxNeighborPaint) {
            float psw = getRotatedWidth(nextIdx) * m_zoom;
            float psh = getPageHeight(nextIdx) * m_zoom;

            QSizeF drawnSize = drawCachedPage(painter, nextIdx, centerX - psw / 2.0f + m_scrollX, currentNextY);
            float nextHeight = drawnSize.isValid() ? drawnSize.height() : psh;
            currentNextY += nextHeight + spacing;
            nextIdx++;
            paintedNext++;
        }

        // Previous pages
        float currentPrevY = pageY - spacing;
        int prevIdx = m_currentPage - 1;
        int paintedPrev = 0;
        while (currentPrevY > -paintBuffer && prevIdx >= 0 && paintedPrev < maxNeighborPaint) {
            float psh = getPageHeight(prevIdx) * m_zoom;
            float psw = getRotatedWidth(prevIdx) * m_zoom;
            float prevY = currentPrevY - psh;
            if (prevY < height() + paintBuffer) {
                drawCachedPage(painter, prevIdx, centerX - psw / 2.0f + m_scrollX, prevY);
            }
            currentPrevY = prevY - spacing;
            prevIdx--;
            paintedPrev++;
        }
    }
}







