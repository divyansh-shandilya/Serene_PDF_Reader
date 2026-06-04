#pragma once
#include <QAbstractListModel>
#include <QSqlDatabase>
#include <QVariantMap>
#include <vector>

struct DocumentEntry {
    int id;
    QString filePath;
    QString title;
    QString author;
    int lastPage;
    int pageCount;
    float zoom;
    int rotation;
    bool isFavorite;
    QString thumbnailPath;

    DocumentEntry(int _id, const QString& _path, const QString& _title, const QString& _author, 
                  int _lastPage, int _pageCount, float _zoom, int _rotation, bool _fav, const QString& _thumb)
        : id(_id), filePath(_path), title(_title), author(_author), lastPage(_lastPage), 
          pageCount(_pageCount), zoom(_zoom), rotation(_rotation), isFavorite(_fav), thumbnailPath(_thumb) {}
};

class LibraryModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QVariantMap recentBook READ getRecentlyOpened NOTIFY recentBookChanged)
    Q_PROPERTY(bool isReady READ isReady NOTIFY ready)
public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        FilePathRole,
        TitleRole,
        AuthorRole,
        LastPageRole,
        PageCountRole,
        ZoomRole,
        PageRotationRole,
        IsFavoriteRole,
        ThumbnailPathRole
    };

    explicit LibraryModel(QObject *parent = nullptr);
    
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void addDocument(const QString &filePath);
    Q_INVOKABLE void requestUpload();
    Q_INVOKABLE void removeDocument(int id);
    Q_INVOKABLE void toggleFavorite(int id);
    Q_INVOKABLE void updateProgress(const QString &filePath, int page, float zoom, int rotation);

    Q_INVOKABLE QVariantMap getRecentlyOpened() const;

    bool isReady() const { return m_isReady; }

signals:
    void recentBookChanged();
    void ready();

private:
    void initDatabase();
    void loadFromDatabase();
    
    QSqlDatabase m_db;
    std::vector<DocumentEntry> m_documents;
    bool m_isReady = false;
};
