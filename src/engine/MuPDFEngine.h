#pragma once
#include <mutex>
#include <memory>
#include "MuPDFDocument.h"

class MuPDFEngine {
public:
    static MuPDFEngine& instance() {
        static MuPDFEngine instance;
        return instance;
    }

    fz_context* context() { return m_ctx; }

    std::unique_ptr<MuPDFDocument> openDocument(const std::string &path);

private:
    MuPDFEngine();
    ~MuPDFEngine();

    MuPDFEngine(const MuPDFEngine&) = delete;
    MuPDFEngine& operator=(const MuPDFEngine&) = delete;

    fz_context *m_ctx = nullptr;
    std::mutex m_mutex;
};
