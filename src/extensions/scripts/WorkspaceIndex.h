#pragma once

#include <QObject>
#include <QReadWriteLock>
#include <QStringList>
#include <QVector>

#include <QFutureWatcher>
#include <QList>
#include <QHash>
#include <QScopedPointer>

class ProjectPaths;

class WorkspaceIndex : public QObject
{
    Q_OBJECT

public:
    struct Location {
        QString filePath;
        int line = -1;
        int column = -1;
        QString preview;
    };

    struct SearchHit {
        QString filePath;
        int line = -1;
        QString lineText;
    };

    explicit WorkspaceIndex(QObject *parent = nullptr);
    ~WorkspaceIndex() override;

    void setProjectPaths(ProjectPaths *paths);
    void setRoots(const QStringList &roots);

    QList<Location> findSymbol(const QString &name) const;
    QList<Location> findLabel(const QString &label) const;
    QList<SearchHit> search(const QString &query) const;

signals:
    void indexingStarted();
    void indexingFinished();

private:
    struct IndexData {
        QHash<QString, QList<Location>> symbols;
        QHash<QString, QList<Location>> labels;
    };

    ProjectPaths *m_paths = nullptr;
    QStringList m_roots;
    mutable QReadWriteLock m_lock;
    QHash<QString, QList<Location>> m_symbols;
    QHash<QString, QList<Location>> m_labels;
    QScopedPointer<QFutureWatcher<IndexData>> m_watcher;

    void scheduleRebuild();
    static IndexData buildIndex(const QStringList &roots);
    static void indexFile(const QString &filePath, IndexData &data);
};

