#include "LibraryModel.h"
#include <QFileDialog>
#include <QSqlQuery>
#include <QSqlError>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include "../engine/MuPDFEngine.h"

static QString sanitizeTitle(const QString& rawTitle, const QString& filePath) {
    QString clean = rawTitle.trimmed();

    // If empty or is literally a local file path
    if (clean.isEmpty()) {
        clean = QFileInfo(filePath).completeBaseName();
    }
    else {
        // If it starts with a Windows drive or contains slashes
        if (clean.contains('/') || clean.contains('\\')) {
            int lastSlash = qMax(clean.lastIndexOf('/'), clean.lastIndexOf('\\'));
            if (lastSlash != -1 && lastSlash < clean.length() - 1) {
                clean = clean.mid(lastSlash + 1);
            }
        }
    }

    if (clean.isEmpty()) {
        clean = QFileInfo(filePath).completeBaseName();
    }

    // Smoothly strip all case-insensitive letter extensions at the end of the title (e.g. .pdf, .pmd, .com)
    while (true) {
        int dotIdx = clean.lastIndexOf('.');
        if (dotIdx == -1 || dotIdx <= 0) {
            break;
        }
        int extLen = clean.length() - dotIdx - 1;
        if (extLen >= 2 && extLen <= 4) {
            bool isAllLetters = true;
            for (int i = dotIdx + 1; i < clean.length(); ++i) {
                QChar ch = clean[i];
                if (!ch.isLetter()) {
                    isAllLetters = false;
                    break;
                }
            }
            if (isAllLetters) {
                clean = clean.left(dotIdx);
                continue; // continue stripping recursively
            }
        }
        break;
    }

    if (clean.isEmpty()) {
        clean = QFileInfo(filePath).completeBaseName();
    }

    return clean;
}

LibraryModel::LibraryModel(QObject* parent) : QAbstractListModel(parent) {
    initDatabase();
    loadFromDatabase();
    m_isReady = true;
    QMetaObject::invokeMethod(this, [this]() { emit ready(); }, Qt::QueuedConnection);
}

void LibraryModel::initDatabase() {
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(path);

    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(path + "/library.db");

    if (!m_db.open()) return;

    QSqlQuery query;
    query.exec("CREATE TABLE IF NOT EXISTS documents ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "filepath TEXT UNIQUE,"
        "title TEXT,"
        "author TEXT,"
        "last_page INTEGER DEFAULT 0,"
        "page_count INTEGER DEFAULT 0,"
        "zoom_level REAL DEFAULT 1.0,"
        "rotation INTEGER DEFAULT 0,"
        "is_favorite INTEGER DEFAULT 0,"
        "last_accessed DATETIME)");

    // Migration for missing columns
    auto addColumnIfNeeded = [&](const QString& col, const QString& type) {
        QSqlQuery check("PRAGMA table_info(documents)");
        bool exists = false;
        while (check.next()) {
            if (check.value(1).toString() == col) { exists = true; break; }
        }
        if (!exists) query.exec(QString("ALTER TABLE documents ADD COLUMN %1 %2").arg(col, type));
        };

    addColumnIfNeeded("rotation", "INTEGER DEFAULT 0");
    addColumnIfNeeded("thumbnail_path", "TEXT");
    addColumnIfNeeded("page_count", "INTEGER DEFAULT 0");
    addColumnIfNeeded("last_accessed", "DATETIME");
}

void LibraryModel::loadFromDatabase() {
    beginResetModel();
    m_documents.clear();

    QSqlQuery query("SELECT id, filepath, title, author, last_page, page_count, zoom_level, is_favorite, thumbnail_path, rotation FROM documents");
    while (query.next()) {
        QString filePath = query.value(1).toString();
        int pageCount = query.value(5).toInt();
        QString thumbPathStr = query.value(8).toString();
        int rotation = query.value(9).toInt();

        // Self-healing: If page count or thumbnail is missing, try to regenerate.
        // page_count == -1 is our "invalid/corrupt/non-PDF" sentinel: never retry.
        if (pageCount != -1 && (pageCount == 0 || thumbPathStr.isEmpty()) && QFile::exists(filePath)) {
            auto doc = MuPDFEngine::instance().openDocument(filePath.toStdString());
            if (doc) {
                if (pageCount == 0) pageCount = doc->pageCount();

                if (thumbPathStr.isEmpty()) {
                    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
                    QString thumbDir = appData + "/thumbnails";
                    QDir().mkpath(thumbDir);
                    QString fileName = QFileInfo(filePath).completeBaseName();
                    QString thumbLocalPath = thumbDir + "/" + fileName + ".jpg";

                    QImage thumb = doc->renderPage(0, 1.0f, 0, 72.0f, 220, 310);
                    if (!thumb.isNull()) {
                        thumb.save(thumbLocalPath, "JPG", 80);
                        thumbPathStr = QUrl::fromLocalFile(thumbLocalPath).toString();
                    }
                }

                // Update database
                QSqlQuery updateQuery;
                updateQuery.prepare("UPDATE documents SET page_count = ?, thumbnail_path = ? WHERE id = ?");
                updateQuery.addBindValue(pageCount);
                updateQuery.addBindValue(thumbPathStr);
                updateQuery.addBindValue(query.value(0).toInt());
                updateQuery.exec();
            }
            else {
                // Mark invalid/corrupt/non-PDF once to avoid repeated open attempts and exception spam.
                QSqlQuery invalidateQuery;
                invalidateQuery.prepare("UPDATE documents SET page_count = -1 WHERE id = ?");
                invalidateQuery.addBindValue(query.value(0).toInt());
                invalidateQuery.exec();
                pageCount = -1;
            }
        }

        m_documents.push_back(DocumentEntry(
            query.value(0).toInt(),
            filePath,
            sanitizeTitle(query.value(2).toString(), filePath),
            query.value(3).toString(),
            query.value(4).toInt(),
            pageCount,
            query.value(6).toFloat(),
            rotation,
            query.value(7).toBool(),
            thumbPathStr
        ));
    }
    endResetModel();
}

int LibraryModel::rowCount(const QModelIndex& parent) const {
    return m_documents.size();
}

QVariant LibraryModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_documents.size()) return QVariant();

    const auto& doc = m_documents[index.row()];
    switch (role) {
    case IdRole: return doc.id;
    case FilePathRole: return doc.filePath;
    case TitleRole: return doc.title.isEmpty() ? sanitizeTitle("", doc.filePath) : doc.title;
    case AuthorRole: return doc.author;
    case LastPageRole: return doc.lastPage;
    case PageCountRole: return doc.pageCount;
    case ZoomRole: return doc.zoom;
    case PageRotationRole: return doc.rotation;
    case IsFavoriteRole: return doc.isFavorite;
    case ThumbnailPathRole: return doc.thumbnailPath;
    }
    return QVariant();
}

QHash<int, QByteArray> LibraryModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[IdRole] = "id";
    roles[FilePathRole] = "filePath";
    roles[TitleRole] = "title";
    roles[AuthorRole] = "author";
    roles[LastPageRole] = "lastPage";
    roles[PageCountRole] = "pageCount";
    roles[ZoomRole] = "zoom";
    roles[PageRotationRole] = "pageRotation";
    roles[IsFavoriteRole] = "isFavorite";
    roles[ThumbnailPathRole] = "thumbnailPath";
    return roles;
}

void LibraryModel::requestUpload() {
    QString filePath = QFileDialog::getOpenFileName(nullptr, tr("Select PDF"), "", tr("PDF Files (*.pdf)"));
    if (!filePath.isEmpty()) {
        addDocument(filePath);
    }
}

#include <QUrl>

void LibraryModel::addDocument(const QString& filePath) {
    if (!QFile::exists(filePath)) return;

    auto doc = MuPDFEngine::instance().openDocument(filePath.toStdString());
    if (!doc || !doc->isValid()) return;

    QString title = sanitizeTitle(QString::fromStdString(doc->title()), filePath);
    QString author = QString::fromStdString(doc->author());
    int pageCount = doc->pageCount();

    // Generate Thumbnail
    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString thumbDir = appData + "/thumbnails";
    QDir().mkpath(thumbDir);

    QString fileName = QFileInfo(filePath).completeBaseName();
    QString thumbPath = thumbDir + "/" + fileName + ".jpg";

    if (!QFile::exists(thumbPath)) {
        QImage thumb = doc->renderPage(0, 1.0f, 0, 72.0f, 220, 310);
        if (!thumb.isNull()) {
            thumb.save(thumbPath, "JPG", 80);
        }
    }

    QSqlQuery query;
    query.prepare("INSERT OR IGNORE INTO documents (filepath, title, author, page_count, thumbnail_path) VALUES (?, ?, ?, ?, ?)");
    query.addBindValue(filePath);
    query.addBindValue(title);
    query.addBindValue(author);
    query.addBindValue(pageCount);
    query.addBindValue(QUrl::fromLocalFile(thumbPath).toString());

    if (query.exec()) {
        loadFromDatabase();
    }
}

void LibraryModel::removeDocument(int id) {
    QSqlQuery query;
    query.prepare("DELETE FROM documents WHERE id = ?");
    query.addBindValue(id);
    if (query.exec()) {
        for (size_t i = 0; i < m_documents.size(); ++i) {
            if (m_documents[i].id == id) {
                beginRemoveRows(QModelIndex(), i, i);
                m_documents.erase(m_documents.begin() + i);
                endRemoveRows();
                break;
            }
        }
    }
}

void LibraryModel::toggleFavorite(int id) {
    QSqlQuery query;
    query.prepare("UPDATE documents SET is_favorite = NOT is_favorite WHERE id = ?");
    query.addBindValue(id);
    if (query.exec()) {
        for (size_t i = 0; i < m_documents.size(); ++i) {
            if (m_documents[i].id == id) {
                m_documents[i].isFavorite = !m_documents[i].isFavorite;
                emit dataChanged(index(i), index(i), { IsFavoriteRole });
                break;
            }
        }
    }
}

void LibraryModel::updateProgress(const QString& filePath, int page, float zoom, int rotation) {
    float clampedZoom = qBound(0.1f, zoom, 4.0f);
    QSqlQuery query;
    query.prepare("UPDATE documents SET last_page = ?, zoom_level = ?, rotation = ?, last_accessed = CURRENT_TIMESTAMP WHERE filepath = ?");
    query.addBindValue(page);
    query.addBindValue(clampedZoom);
    query.addBindValue(rotation);
    query.addBindValue(filePath);
    if (query.exec()) {
        // Update local cache to ensure immediate UI feedback/correct open state
        for (size_t i = 0; i < m_documents.size(); ++i) {
            if (m_documents[i].filePath == filePath) {
                m_documents[i].lastPage = page;
                m_documents[i].zoom = clampedZoom;
                m_documents[i].rotation = rotation;
                emit dataChanged(index(i), index(i), { LastPageRole, ZoomRole, PageRotationRole });
                break;
            }
        }
        emit this->recentBookChanged();
    }
}

QVariantMap LibraryModel::getRecentlyOpened() const {
    QSqlQuery query("SELECT id, filepath, title, last_page, page_count, thumbnail_path, zoom_level, rotation FROM documents WHERE last_accessed IS NOT NULL ORDER BY last_accessed DESC LIMIT 1");
    if (query.next()) {
        QVariantMap map;
        map["id"] = query.value(0);
        map["filePath"] = query.value(1);
        map["title"] = sanitizeTitle(query.value(2).toString(), query.value(1).toString());
        map["lastPage"] = query.value(3);
        map["pageCount"] = query.value(4);
        map["thumbnailPath"] = query.value(5);
        map["zoom"] = query.value(6);
        map["pageRotation"] = query.value(7);
        return map;
    }
    return QVariantMap();
}
