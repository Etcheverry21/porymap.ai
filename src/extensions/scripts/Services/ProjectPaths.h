#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class ProjectPaths : public QObject
{
    Q_OBJECT

public:
    explicit ProjectPaths(QObject *parent = nullptr);

    QString projectRoot() const;
    void setProjectRoot(const QString &rootPath);

    QStringList scriptSearchRoots() const;
    QString poryscriptCompilerPath() const;
    void setPoryscriptCompilerPath(const QString &path);

signals:
    void projectRootChanged(const QString &rootPath);

private:
    QString m_projectRoot;
    QString m_poryscriptCompilerPath;

    void autoDetectCompiler();
};

