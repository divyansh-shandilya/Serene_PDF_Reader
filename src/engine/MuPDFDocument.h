#pragma once
#include <string>
#include <vector>
#include <memory>
#include <QImage>
#include <QSize>

extern "C" {
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
}

struct PageMetadata {
    int pageNumber;
    double width;
    double height;
};

class MuPDFDocument {
public:
    MuPDFDocument(fz_context *ctx, const std::string &path);
    ~MuPDFDocument();

    bool isValid() const { return m_doc != nullptr; }
    int pageCount() const;
    PageMetadata pageMetadata(int pageIndex) const;
    
    // Core rendering function
    QImage renderPage(int pageIndex, float zoom, int rotation = 0, float dpi = 72.0f, int targetWidth = 0, int targetHeight = 0, fz_cookie* cookie = nullptr);
    
    // Metadata extraction
    std::string title() const;
    std::string author() const;

private:
    fz_context *m_ctx = nullptr;
    fz_document *m_doc = nullptr;
    mutable int m_pageCount = -1;
};
