#pragma once

#include <QFutureWatcher>
#include <QModelIndex>
#include <QWidget>

#include "extensions/scripts/WorkspaceIndex.h"

class ProjectPaths;
class BuildRunner;
class ProblemsView;
class PoryHighlighter;

class QFileSystemModel;
class QFileSystemWatcher;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSplitter;
class QTabWidget;
class QToolBar;
class QTreeView;
class QAction;

class ScriptEditorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ScriptEditorWidget(QWidget *parent = nullptr);

    void setProjectPaths(ProjectPaths *paths);

signals:
    void scriptFilesChanged();

public slots:
    void setProjectContext(const QString &projectRoot);
    void setActiveMapContext(const QString &mapName);

private slots:
    void openFileDialog();
    void saveCurrentFile();
    void compilePoryscripts();
    void buildEntireProject();
    void findInWorkspace();
    void goToDefinition();
    void onFileActivated(const QModelIndex &index);
    void onProblemActivated(const QString &file, int line, int column);
    void onSearchFinished();
    void onDefinitionFinished();
    void onWorkspaceIndexingStarted();
    void onWorkspaceIndexingFinished();
    void handleBuildOutput(const QString &line);
    void handleBuildDiagnostics(const QList<ProblemsView::Problem> &problems);
    void handleBuildFinished(int exitCode);
    void handleBuildError(const QString &message);
    void findNext();
    void findPrevious();
    void goToLine();
    void editorTextChanged();
    void reloadExternallyModified();

private:
    class CodeEditor;

    void setupUi();
    void setupActions();
    void loadFile(const QString &path, int line = -1, int column = -1);
    bool saveFile(const QString &path);
    void updateWindowTitle();
    void updateActionStates();
    void setCurrentFilePath(const QString &path);
    QString currentWordUnderCursor() const;
    void watchCurrentFile();
    void applySearchResults(const QList<WorkspaceIndex::SearchHit> &results);

    ProjectPaths *m_projectPaths;
    WorkspaceIndex *m_workspaceIndex;
    BuildRunner *m_buildRunner;

    QFileSystemModel *m_fileModel;
    QTreeView *m_treeView;
    CodeEditor *m_editor;
    PoryHighlighter *m_highlighter;
    ProblemsView *m_problemsView;
    QPlainTextEdit *m_buildOutput;
    QTabWidget *m_bottomTabs;
    QToolBar *m_toolBar;
    QFileSystemWatcher *m_fileWatcher;
    QLabel *m_statusLabel;
    QLineEdit *m_findInput;
    QAction *m_openAction;
    QAction *m_saveAction;
    QAction *m_compileAction;
    QAction *m_buildAction;
    QAction *m_findAction;
    QAction *m_definitionAction;

    QString m_projectRoot;
    QString m_currentFile;
    QString m_currentMap;
    bool m_documentModified = false;
    bool m_externalChange = false;

    QFutureWatcher<QList<WorkspaceIndex::SearchHit>> m_searchWatcher;
    QFutureWatcher<QList<WorkspaceIndex::Location>> m_definitionWatcher;
};

