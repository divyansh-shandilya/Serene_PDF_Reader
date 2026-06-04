#include "MuPDFDocument.h"
#include <stdexcept>
#include <QDebug>

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

static quint64 getSystemRamBytes() {
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
    return 8ull * 1024ull * 1024ull * 1024ull; // fallback to 8GB
}

extern "C" {
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
}

MuPDFDocument::MuPDFDocument(fz_context* /*base_ctx*/, const std::string& path) {
    // Create its own context for each document to avoid threading issues
    // Keep per-worker MuPDF store bounded; each worker has its own context.
    // Large per-context stores multiply RAM usage and cause long-session lag.
    size_t storeSize = 48 * 1024 * 1024; // 48 MB default
    quint64 memBytes = getSystemRamBytes();
    if (memBytes <= 2ull * 1024ull * 1024ull * 1024ull) {
        storeSize = 16 * 1024 * 1024; // 16 MB limit for 1-2GB RAM
    } else if (memBytes <= 4ull * 1024ull * 1024ull * 1024ull) {
        storeSize = 24 * 1024 * 1024; // 24 MB limit for 3-4GB RAM
    }
    m_ctx = fz_new_context(NULL, NULL, storeSize);
    if (m_ctx) {
        // Improve glyph edge quality, especially visible on large displays.
        fz_set_aa_level(m_ctx, 8);
        fz_register_document_handlers(m_ctx);
        try {
            fz_try(m_ctx) {
                m_doc = fz_open_document(m_ctx, path.c_str());
                if (!m_doc) {
                    qWarning() << "MuPDF failed to open document:" << QString::fromStdString(path);
                }
            }
            fz_catch(m_ctx) {
                qWarning() << "MuPDF catch: failed to open" << QString::fromStdString(path);
                m_doc = nullptr;
            }
        }
        catch (const std::exception& e) {
            qWarning() << "MuPDF C++ Exception:" << e.what() << "for file" << QString::fromStdString(path);
            m_doc = nullptr;
        }
        catch (...) {
            qWarning() << "MuPDF Unknown C++ Exception for file" << QString::fromStdString(path);
            m_doc = nullptr;
        }
    }
}

MuPDFDocument::~MuPDFDocument() {
    if (m_doc) {
        fz_drop_document(m_ctx, m_doc);
    }
    if (m_ctx) {
        fz_drop_context(m_ctx);
    }
}

int MuPDFDocument::pageCount() const {
    if (m_pageCount >= 0) return m_pageCount;
    if (!m_doc) return 0;

    fz_try(m_ctx) {
        m_pageCount = fz_count_pages(m_ctx, m_doc);
    }
    fz_catch(m_ctx) {
        m_pageCount = 0;
    }
    return m_pageCount;
}

PageMetadata MuPDFDocument::pageMetadata(int pageIndex) const {
    PageMetadata meta{ pageIndex, 0, 0 };
    if (!m_doc) return meta;

    fz_try(m_ctx) {
        fz_page* page = fz_load_page(m_ctx, m_doc, pageIndex);
        fz_rect rect = fz_bound_page(m_ctx, page);

        meta.width = qAbs(rect.x1 - rect.x0);
        meta.height = qAbs(rect.y1 - rect.y0);
        fz_drop_page(m_ctx, page);
    }
    fz_catch(m_ctx) {
        // Fallback to standard A4 if metadata fetch fails
        meta.width = 595.0;
        meta.height = 842.0;
    }
    return meta;
}

QImage MuPDFDocument::renderPage(int pageIndex, float zoom, int rotation, float dpi, int targetWidth, int targetHeight, fz_cookie* cookie) {
    if (!m_doc || !m_ctx) return QImage();

    QImage result;
    fz_try(m_ctx) {
        // Ensure AA level is consistently set to maximum quality (8)
        fz_set_aa_level(m_ctx, 8);

        fz_page* page = fz_load_page(m_ctx, m_doc, pageIndex);
        if (page) {
            fz_rect rect = fz_bound_page(m_ctx, page);
            float pw = qAbs(rect.x1 - rect.x0);
            float ph = qAbs(rect.y1 - rect.y0);

            fz_matrix user_rot_mat = fz_rotate((float)rotation);
            fz_rect rot_rect = fz_transform_rect(fz_make_rect(0, 0, pw, ph), user_rot_mat);

            float rpw = qAbs(rot_rect.x1 - rot_rect.x0);
            float rph = qAbs(rot_rect.y1 - rot_rect.y0);

            const float POINTS_PER_INCH = 72.0f;
            float scale;
            if (targetWidth > 0 && targetHeight > 0) {
                scale = std::min((float)targetWidth / rpw, (float)targetHeight / rph);
            }
            else {
                scale = (dpi / POINTS_PER_INCH) * zoom;
            }

            // Limit pixel area to bound memory while still supporting large displays
            // (e.g. classroom smartboards) with crisp text.
            const float maxArea = 6144.0f * 6144.0f;
            float currentArea = (rpw * scale) * (rph * scale);
            if (currentArea > maxArea) {
                scale *= std::sqrt(maxArea / currentArea);
            }
            scale = std::min(scale, 10.0f); // Maximum 10x zoom even on tiny docs

            // Construct total matrix with perfect transformation order:
            // 1. Translate original page bounds to origin [0, 0]
            fz_matrix ctm = fz_translate(-rect.x0, -rect.y0);
            // 2. Scale
            ctm = fz_concat(ctm, fz_scale(scale, scale));
            // 3. Rotate
            ctm = fz_concat(ctm, user_rot_mat);

            // Bounding box of the page after scaling and rotation (positioned at origin [0,0] initially)
            fz_rect rot_rect_scaled = fz_transform_rect(fz_make_rect(0, 0, pw * scale, ph * scale), user_rot_mat);
            // 4. Translate back to positive coordinates [0, float_max] to prevent cut-off edges
            ctm = fz_concat(ctm, fz_translate(-rot_rect_scaled.x0, -rot_rect_scaled.y0));

            // Validate post-rotation coordinate space to ensure x0 and y0 are not negative due to precision
            fz_irect irect = fz_round_rect(fz_transform_rect(rect, ctm));
            if (irect.x0 < 0 || irect.y0 < 0) {
                float adjX = irect.x0 < 0 ? -(float)irect.x0 : 0.0f;
                float adjY = irect.y0 < 0 ? -(float)irect.y0 : 0.0f;
                ctm = fz_concat(ctm, fz_translate(adjX, adjY));
                irect = fz_round_rect(fz_transform_rect(rect, ctm));
            }

            int pix_w = qMax(1, irect.x1 - irect.x0);
            int pix_h = qMax(1, irect.y1 - irect.y0);

            fz_pixmap* pix = fz_new_pixmap(m_ctx, fz_device_rgb(m_ctx), pix_w, pix_h, nullptr, 0);
            if (pix) {
                fz_clear_pixmap_with_value(m_ctx, pix, 0xff);
                fz_matrix render_ctm = fz_concat(ctm, fz_translate(-(float)irect.x0, -(float)irect.y0));

                fz_device* dev = fz_new_draw_device(m_ctx, render_ctm, pix);
                if (dev) {
                    fz_run_page(m_ctx, page, dev, fz_identity, cookie);
                    fz_close_device(m_ctx, dev);
                    fz_drop_device(m_ctx, dev);

                    int width = fz_pixmap_width(m_ctx, pix);
                    int height = fz_pixmap_height(m_ctx, pix);
                    unsigned char* samples = fz_pixmap_samples(m_ctx, pix);

                    if (samples && width > 0 && height > 0) {
                        int comps = fz_pixmap_components(m_ctx, pix);
                        int stride = fz_pixmap_stride(m_ctx, pix);
                        if (comps == 4) {
                            // Fast path: keep RGBA layout and just deep-copy while pixmap is still alive.
                            result = QImage(samples, width, height, stride, QImage::Format_RGBA8888).copy();
                        }
                        else {
                            // RGB pixmaps still need conversion to a painter-friendly format.
                            result = QImage(samples, width, height, stride, QImage::Format_RGB888)
                                .convertToFormat(QImage::Format_RGB32);
                        }
                    }
                }
                fz_drop_pixmap(m_ctx, pix);
            }
            fz_drop_page(m_ctx, page);
        }
    }
    fz_catch(m_ctx) {
        qWarning() << "MuPDF render error for page" << pageIndex;
    }

    return result;
}

std::string MuPDFDocument::title() const {
    if (!m_ctx || !m_doc) return "";
    char buf[256];
    if (fz_lookup_metadata(m_ctx, m_doc, FZ_META_INFO_TITLE, buf, sizeof(buf)) > 0) {
        return std::string(buf);
    }
    return "";
}

std::string MuPDFDocument::author() const {
    if (!m_ctx || !m_doc) return "";
    char buf[256];
    if (fz_lookup_metadata(m_ctx, m_doc, FZ_META_INFO_AUTHOR, buf, sizeof(buf)) > 0) {
        return std::string(buf);
    }
    return "";
}
