#include "extensions/scripts/ScriptEditorWidget.h"

#include <QAbstractItemView>
#include <QAction>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFileSystemWatcher>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QPlainTextEdit>
#include <QShortcut>
#include <QSplitter>
#include <QTabWidget>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextFormat>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <QtConcurrentRun>

#include "extensions/scripts/BuildRunner.h"
#include "extensions/scripts/ProblemsView.h"
#include "extensions/scripts/Services/ProjectPaths.h"
#include "extensions/scripts/Syntax/PoryHighlighter.h"
#include "extensions/scripts/WorkspaceIndex.h"

namespace {
const QStringList kEditorFilters = {
    QStringLiteral("*.pory"), QStringLiteral("*.c"), QStringLiteral("*.h"),
    QStringLiteral("*.inc"), QStringLiteral("*.s"), QStringLiteral("*.json")
};
}

class ScriptEditorWidget::CodeEditor : public QPlainTextEdit
{
private:
    class LineNumberArea;

public:
    explicit CodeEditor(QWidget *parent = nullptr)
        : QPlainTextEdit(parent)
        , m_lineNumberArea(new LineNumberArea(this))
    {
        setFrameStyle(QFrame::NoFrame);
        setTabStopDistance(fontMetrics().horizontalAdvance(QLatin1Char(' ')) * 4);
        connect(this, &CodeEditor::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);
        connect(this, &CodeEditor::updateRequest, this, &CodeEditor::updateLineNumberArea);
        connect(this, &CodeEditor::cursorPositionChanged, this, &CodeEditor::highlightCurrentLine);
        updateLineNumberAreaWidth(0);
        highlightCurrentLine();
    }

    int lineNumberAreaWidth() const
    {
        int digits = 1;
        int max = qMax(1, blockCount());
        while (max >= 10) {
            max /= 10;
            ++digits;
        }
        int space = 3 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
        return space;
    }

    void lineNumberAreaPaintEvent(QPaintEvent *event)
    {
        QPainter painter(m_lineNumberArea);
        painter.fillRect(event->rect(), palette().alternateBase());

        QTextBlock block = firstVisibleBlock();
        int blockNumber = block.blockNumber();
        int top = static_cast<int>(blockBoundingGeometry(block).translated(contentOffset()).top());
        int bottom = top + static_cast<int>(blockBoundingRect(block).height());

        while (block.isValid() && top <= event->rect().bottom()) {
            if (block.isVisible() && bottom >= event->rect().top()) {
                const QString number = QString::number(blockNumber + 1);
                painter.setPen(palette().color(QPalette::Text));
                painter.drawText(0, top, m_lineNumberArea->width() - 4, fontMetrics().height(),
                                 Qt::AlignRight, number);
            }

            block = block.next();
            top = bottom;
            bottom = top + static_cast<int>(blockBoundingRect(block).height());
            ++blockNumber;
        }
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QPlainTextEdit::resizeEvent(event);
        QRect cr = contentsRect();
        m_lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            QTextCursor cursor = textCursor();
            cursor.movePosition(QTextCursor::StartOfBlock);
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            const QString currentLine = cursor.selectedText();
            QString indentation;
            for (QChar ch : currentLine) {
                if (ch == QLatin1Char(' ') || ch == QLatin1Char('\t'))
                    indentation.append(ch);
                else
                    break;
            }
            QPlainTextEdit::keyPressEvent(event);
            QPlainTextEdit::insertPlainText(indentation);
            return;
        }

        if (event->key() == Qt::Key_BraceLeft) {
            QPlainTextEdit::keyPressEvent(event);
            QPlainTextEdit::insertPlainText(QLatin1String("}"));
            QTextCursor cursor = textCursor();
            cursor.movePosition(QTextCursor::Left);
            setTextCursor(cursor);
            return;
        }

        QPlainTextEdit::keyPressEvent(event);
    }

private:
    void updateLineNumberAreaWidth(int newBlockCount)
    {
        Q_UNUSED(newBlockCount);
        setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
    }

    void highlightCurrentLine()
    {
        QList<QTextEdit::ExtraSelection> extraSelections;
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(palette().alternateBase().color().lighter(120));
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
        setExtraSelections(extraSelections);
    }

    void updateLineNumberArea(const QRect &rect, int dy)
    {
        if (dy)
            m_lineNumberArea->scroll(0, dy);
        else
            m_lineNumberArea->update(0, rect.y(), m_lineNumberArea->width(), rect.height());

        if (rect.contains(viewport()->rect()))
            updateLineNumberAreaWidth(0);
    }

    LineNumberArea *m_lineNumberArea;
};

class ScriptEditorWidget::CodeEditor::LineNumberArea : public QWidget
{
public:
    explicit LineNumberArea(CodeEditor *editor)
        : QWidget(editor)
        , m_editor(editor)
    {
        setAutoFillBackground(false);
    }

    QSize sizeHint() const override
    {
        return QSize(m_editor->lineNumberAreaWidth(), 0);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        m_editor->lineNumberAreaPaintEvent(event);
    }

private:
    CodeEditor *m_editor;
};

ScriptEditorWidget::ScriptEditorWidget(QWidget *parent)
    : QWidget(parent)
    , m_projectPaths(nullptr)
    , m_workspaceIndex(new WorkspaceIndex(this))
    , m_buildRunner(new BuildRunner(this))
    , m_fileModel(nullptr)
    , m_treeView(nullptr)
    , m_editor(nullptr)
    , m_highlighter(nullptr)
    , m_problemsView(nullptr)
    , m_buildOutput(nullptr)
    , m_bottomTabs(nullptr)
    , m_toolBar(nullptr)
    , m_fileWatcher(new QFileSystemWatcher(this))
    , m_statusLabel(nullptr)
    , m_findInput(nullptr)
{
    setupUi();
    setupActions();

    connect(&m_searchWatcher, &QFutureWatcher<QList<WorkspaceIndex::SearchHit>>::finished,
            this, &ScriptEditorWidget::onSearchFinished);
    connect(&m_definitionWatcher, &QFutureWatcher<QList<WorkspaceIndex::Location>>::finished,
            this, &ScriptEditorWidget::onDefinitionFinished);

    connect(m_workspaceIndex, &WorkspaceIndex::indexingStarted,
            this, &ScriptEditorWidget::onWorkspaceIndexingStarted);
    connect(m_workspaceIndex, &WorkspaceIndex::indexingFinished,
            this, &ScriptEditorWidget::onWorkspaceIndexingFinished);

    connect(m_buildRunner, &BuildRunner::outputLine, this, &ScriptEditorWidget::handleBuildOutput);
    connect(m_buildRunner, &BuildRunner::diagnosticsReady, this, &ScriptEditorWidget::handleBuildDiagnostics);
    connect(m_buildRunner, &BuildRunner::buildFinished, this, &ScriptEditorWidget::handleBuildFinished);
    connect(m_buildRunner, &BuildRunner::errorOccurred, this, &ScriptEditorWidget::handleBuildError);
    connect(m_buildRunner, &BuildRunner::buildStarted, this, [this]() {
        if (m_buildOutput) {
            m_buildOutput->clear();
            if (m_bottomTabs)
                m_bottomTabs->setCurrentWidget(m_buildOutput);
        }
        if (m_statusLabel)
            m_statusLabel->setText(tr("Building project..."));
    });

    connect(m_treeView, &QTreeView::activated, this, &ScriptEditorWidget::onFileActivated);
    connect(m_treeView, &QTreeView::doubleClicked, this, &ScriptEditorWidget::onFileActivated);
    connect(m_problemsView, &ProblemsView::problemActivated, this, &ScriptEditorWidget::onProblemActivated);
    connect(m_editor, &QPlainTextEdit::textChanged, this, &ScriptEditorWidget::editorTextChanged);
    connect(m_fileWatcher, &QFileSystemWatcher::fileChanged, this, &ScriptEditorWidget::reloadExternallyModified);

    updateActionStates();
    updateWindowTitle();
}

void ScriptEditorWidget::setProjectPaths(ProjectPaths *paths)
{
    m_projectPaths = paths;
    m_buildRunner->setProjectPaths(paths);
    m_workspaceIndex->setProjectPaths(paths);
}

void ScriptEditorWidget::setProjectContext(const QString &projectRoot)
{
    m_projectRoot = projectRoot;

    if (!m_fileModel)
        return;

    if (projectRoot.isEmpty()) {
        m_fileModel->setRootPath(QString());
        m_treeView->setRootIndex(QModelIndex());
        m_workspaceIndex->setRoots({});
        m_currentMap.clear();
        m_currentFile.clear();
        m_editor->clear();
        m_documentModified = false;
        updateWindowTitle();
        return;
    }

    QModelIndex index = m_fileModel->setRootPath(projectRoot);
    m_treeView->setRootIndex(index);
    m_workspaceIndex->setRoots(m_projectPaths ? m_projectPaths->scriptSearchRoots() : QStringList{projectRoot});
    if (!m_currentFile.startsWith(projectRoot)) {
        m_editor->clear();
        setCurrentFilePath(QString());
        m_documentModified = false;
    }
    updateActionStates();
}

void ScriptEditorWidget::setActiveMapContext(const QString &mapName)
{
    m_currentMap = mapName;
    updateWindowTitle();
}

void ScriptEditorWidget::openFileDialog()
{
    QString directory = m_currentFile.isEmpty() ? m_projectRoot : QFileInfo(m_currentFile).absolutePath();
    const QString file = QFileDialog::getOpenFileName(this, tr("Open File"), directory,
                                                     kEditorFilters.join(QLatin1Char(' ')));
    if (file.isEmpty())
        return;

    loadFile(file);
}

void ScriptEditorWidget::saveCurrentFile()
{
    if (m_currentFile.isEmpty()) {
        QString directory = m_projectRoot;
        const QString file = QFileDialog::getSaveFileName(this, tr("Save File"), directory,
                                                          kEditorFilters.join(QLatin1Char(' ')));
        if (file.isEmpty())
            return;
        if (!saveFile(file))
            return;
        setCurrentFilePath(file);
    } else {
        saveFile(m_currentFile);
    }
}

void ScriptEditorWidget::compilePoryscripts()
{
    if (!m_buildRunner)
        return;
    m_buildRunner->buildPoryOnly();
}

void ScriptEditorWidget::buildEntireProject()
{
    if (!m_buildRunner)
        return;
    m_buildRunner->buildAll();
}

void ScriptEditorWidget::findInWorkspace()
{
    QString query = m_findInput ? m_findInput->text() : QString();
    if (query.isEmpty())
        query = QInputDialog::getText(this, tr("Find in Workspace"), tr("Search term:"));
    if (query.isEmpty())
        return;

    if (m_searchWatcher.isRunning()) {
        m_searchWatcher.cancel();
        m_searchWatcher.waitForFinished();
    }

    auto future = QtConcurrent::run([this, query]() { return m_workspaceIndex->search(query); });
    m_searchWatcher.setFuture(future);
    if (m_statusLabel)
        m_statusLabel->setText(tr("Searching for '%1'...").arg(query));
}

void ScriptEditorWidget::goToDefinition()
{
    const QString symbol = currentWordUnderCursor();
    if (symbol.isEmpty())
        return;

    if (m_definitionWatcher.isRunning()) {
        m_definitionWatcher.cancel();
        m_definitionWatcher.waitForFinished();
    }

    auto future = QtConcurrent::run([this, symbol]() {
        QList<WorkspaceIndex::Location> locations = m_workspaceIndex->findSymbol(symbol);
        if (locations.isEmpty())
            locations = m_workspaceIndex->findLabel(symbol);
        return locations;
    });
    m_definitionWatcher.setFuture(future);
    if (m_statusLabel)
        m_statusLabel->setText(tr("Finding definition for '%1'...").arg(symbol));
}

void ScriptEditorWidget::onFileActivated(const QModelIndex &index)
{
    if (!m_fileModel)
        return;

    const QString path = m_fileModel->filePath(index);
    if (path.isEmpty() || QFileInfo(path).isDir())
        return;

    loadFile(path);
}

void ScriptEditorWidget::onProblemActivated(const QString &file, int line, int column)
{
    if (file.isEmpty())
        return;

    loadFile(file, line, column);
}

void ScriptEditorWidget::onSearchFinished()
{
    if (!m_searchWatcher.isFinished())
        return;

    const QList<WorkspaceIndex::SearchHit> results = m_searchWatcher.result();
    applySearchResults(results);
}

void ScriptEditorWidget::onDefinitionFinished()
{
    if (!m_definitionWatcher.isFinished())
        return;

    const QList<WorkspaceIndex::Location> locations = m_definitionWatcher.result();
    if (locations.isEmpty()) {
        QMessageBox::information(this, tr("Go to Definition"), tr("No definition found."));
        return;
    }

    const WorkspaceIndex::Location location = locations.first();
    loadFile(location.filePath, location.line, location.column);
}

void ScriptEditorWidget::onWorkspaceIndexingStarted()
{
    if (m_statusLabel)
        m_statusLabel->setText(tr("Indexing workspace..."));
}

void ScriptEditorWidget::onWorkspaceIndexingFinished()
{
    if (m_statusLabel)
        m_statusLabel->setText(tr("Workspace indexed."));
}

void ScriptEditorWidget::handleBuildOutput(const QString &line)
{
    if (!m_buildOutput)
        return;

    m_buildOutput->appendPlainText(line);
}

void ScriptEditorWidget::handleBuildDiagnostics(const QList<ProblemsView::Problem> &problems)
{
    if (!m_problemsView)
        return;

    m_problemsView->setProblems(problems);
    m_bottomTabs->setCurrentWidget(m_problemsView);
}

void ScriptEditorWidget::handleBuildFinished(int exitCode)
{
    if (m_statusLabel)
        m_statusLabel->setText(exitCode == 0 ? tr("Build completed successfully.")
                                              : tr("Build finished with errors."));
}

void ScriptEditorWidget::handleBuildError(const QString &message)
{
    QMessageBox::warning(this, tr("Build Error"), message);
}

void ScriptEditorWidget::findNext()
{
    if (!m_editor || !m_findInput)
        return;

    const QString text = m_findInput->text();
    if (text.isEmpty())
        return;

    if (!m_editor->find(text)) {
        QTextCursor cursor = m_editor->textCursor();
        cursor.movePosition(QTextCursor::Start);
        m_editor->setTextCursor(cursor);
        m_editor->find(text);
    }
}

void ScriptEditorWidget::findPrevious()
{
    if (!m_editor || !m_findInput)
        return;

    const QString text = m_findInput->text();
    if (text.isEmpty())
        return;

    if (!m_editor->find(text, QTextDocument::FindBackward)) {
        QTextCursor cursor = m_editor->textCursor();
        cursor.movePosition(QTextCursor::End);
        m_editor->setTextCursor(cursor);
        m_editor->find(text, QTextDocument::FindBackward);
    }
}

void ScriptEditorWidget::goToLine()
{
    if (!m_editor)
        return;

    const int maxLine = m_editor->document()->blockCount();
    bool ok = false;
    int line = QInputDialog::getInt(this, tr("Go to Line"), tr("Line number:"), 1, 1, maxLine, 1, &ok);
    if (!ok)
        return;

    QTextCursor cursor = m_editor->textCursor();
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, line - 1);
    m_editor->setTextCursor(cursor);
    m_editor->setFocus();
}

void ScriptEditorWidget::editorTextChanged()
{
    if (!m_documentModified) {
        m_documentModified = true;
        updateWindowTitle();
    }
}

void ScriptEditorWidget::reloadExternallyModified()
{
    if (m_currentFile.isEmpty())
        return;

    if (m_documentModified) {
        m_externalChange = true;
        if (m_statusLabel)
            m_statusLabel->setText(tr("File changed on disk. Please reload."));
        return;
    }

    loadFile(m_currentFile);
}

void ScriptEditorWidget::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    layout->addWidget(splitter);

    m_fileModel = new QFileSystemModel(this);
    m_fileModel->setNameFilters(kEditorFilters);
    m_fileModel->setNameFilterDisables(false);
    m_fileModel->setFilter(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Files);

    m_treeView = new QTreeView(splitter);
    m_treeView->setModel(m_fileModel);
    m_treeView->setHeaderHidden(true);
    m_treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_treeView->setSelectionMode(QAbstractItemView::SingleSelection);
    splitter->addWidget(m_treeView);

    auto *rightContainer = new QWidget(splitter);
    splitter->addWidget(rightContainer);
    splitter->setStretchFactor(1, 1);

    auto *rightLayout = new QVBoxLayout(rightContainer);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    m_toolBar = new QToolBar(rightContainer);
    m_toolBar->setIconSize(QSize(16, 16));
    rightLayout->addWidget(m_toolBar);

    auto *findBar = new QWidget(rightContainer);
    auto *findLayout = new QHBoxLayout(findBar);
    findLayout->setContentsMargins(4, 2, 4, 2);
    findLayout->addWidget(new QLabel(tr("Find:"), findBar));
    m_findInput = new QLineEdit(findBar);
    findLayout->addWidget(m_findInput);
    auto *nextButton = new QToolButton(findBar);
    nextButton->setText(tr("Next"));
    findLayout->addWidget(nextButton);
    auto *prevButton = new QToolButton(findBar);
    prevButton->setText(tr("Previous"));
    findLayout->addWidget(prevButton);
    rightLayout->addWidget(findBar);

    m_editor = new CodeEditor(rightContainer);
    rightLayout->addWidget(m_editor, 1);
    m_highlighter = new PoryHighlighter(m_editor->document());

    m_bottomTabs = new QTabWidget(rightContainer);
    m_problemsView = new ProblemsView(m_bottomTabs);
    m_buildOutput = new QPlainTextEdit(m_bottomTabs);
    m_buildOutput->setReadOnly(true);
    m_bottomTabs->addTab(m_problemsView, tr("Problems"));
    m_bottomTabs->addTab(m_buildOutput, tr("Build Output"));
    rightLayout->addWidget(m_bottomTabs, 0);

    m_statusLabel = new QLabel(rightContainer);
    rightLayout->addWidget(m_statusLabel);

    connect(nextButton, &QToolButton::clicked, this, &ScriptEditorWidget::findNext);
    connect(prevButton, &QToolButton::clicked, this, &ScriptEditorWidget::findPrevious);
    connect(m_findInput, &QLineEdit::returnPressed, this, &ScriptEditorWidget::findNext);
}

void ScriptEditorWidget::setupActions()
{
    m_openAction = m_toolBar->addAction(tr("Open"), this, &ScriptEditorWidget::openFileDialog);
    m_openAction->setShortcut(QKeySequence::Open);

    m_saveAction = m_toolBar->addAction(tr("Save"), this, &ScriptEditorWidget::saveCurrentFile);
    m_saveAction->setShortcut(QKeySequence::Save);

    m_toolBar->addSeparator();

    m_compileAction = m_toolBar->addAction(tr("Compile Poryscript"), this, &ScriptEditorWidget::compilePoryscripts);
    m_compileAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_B));

    m_buildAction = m_toolBar->addAction(tr("Build Project"), this, &ScriptEditorWidget::buildEntireProject);
    m_buildAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_B));

    m_toolBar->addSeparator();

    m_findAction = m_toolBar->addAction(tr("Find in Workspace"), this, &ScriptEditorWidget::findInWorkspace);
    m_findAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F));

    m_definitionAction = m_toolBar->addAction(tr("Go to Definition"), this, &ScriptEditorWidget::goToDefinition);
    m_definitionAction->setShortcut(QKeySequence(Qt::Key_F12));

    auto *shortcutFind = new QShortcut(QKeySequence::Find, this);
    connect(shortcutFind, &QShortcut::activated, [this]() { m_findInput->setFocus(); });

    auto *shortcutGoToLine = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_G), this);
    connect(shortcutGoToLine, &QShortcut::activated, this, &ScriptEditorWidget::goToLine);
}

void ScriptEditorWidget::loadFile(const QString &path, int line, int column)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Open File"), tr("Failed to open %1").arg(QDir::toNativeSeparators(path)));
        return;
    }

    const QString text = QString::fromUtf8(file.readAll());
    m_editor->setPlainText(text);
    m_documentModified = false;
    m_externalChange = false;
    setCurrentFilePath(path);
    file.close();

    if (line > 0) {
        QTextCursor cursor = m_editor->textCursor();
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, line - 1);
        if (column > 0)
            cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, column - 1);
        m_editor->setTextCursor(cursor);
        m_editor->centerCursor();
    }

    updateActionStates();
}

bool ScriptEditorWidget::saveFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Save File"), tr("Failed to save %1").arg(QDir::toNativeSeparators(path)));
        return false;
    }

    const QByteArray data = m_editor->toPlainText().toUtf8();
    if (file.write(data) != data.size()) {
        QMessageBox::warning(this, tr("Save File"), tr("Could not write to %1").arg(QDir::toNativeSeparators(path)));
        return false;
    }

    file.close();
    m_documentModified = false;
    m_externalChange = false;
    emit scriptFilesChanged();
    updateWindowTitle();
    return true;
}

void ScriptEditorWidget::updateWindowTitle()
{
    QString label = m_currentFile;
    if (label.isEmpty())
        label = tr("Untitled");
    if (m_documentModified)
        label.append('*');
    if (!m_currentMap.isEmpty())
        label = m_currentMap + QStringLiteral(" — ") + label;
    if (m_statusLabel)
        m_statusLabel->setText(label);
}

void ScriptEditorWidget::updateActionStates()
{
    const bool hasFile = !m_currentFile.isEmpty();
    if (m_saveAction)
        m_saveAction->setEnabled(hasFile || m_documentModified);
    if (m_compileAction)
        m_compileAction->setEnabled(!m_projectRoot.isEmpty());
    if (m_buildAction)
        m_buildAction->setEnabled(!m_projectRoot.isEmpty());
    if (m_definitionAction)
        m_definitionAction->setEnabled(m_workspaceIndex && !m_projectRoot.isEmpty());
    if (m_findAction)
        m_findAction->setEnabled(!m_projectRoot.isEmpty());
}

void ScriptEditorWidget::setCurrentFilePath(const QString &path)
{
    m_currentFile = path;
    watchCurrentFile();
    updateWindowTitle();
}

QString ScriptEditorWidget::currentWordUnderCursor() const
{
    QTextCursor cursor = m_editor->textCursor();
    if (cursor.hasSelection())
        return cursor.selectedText();

    cursor.select(QTextCursor::WordUnderCursor);
    return cursor.selectedText();
}

void ScriptEditorWidget::watchCurrentFile()
{
    m_fileWatcher->removePaths(m_fileWatcher->files());
    if (!m_currentFile.isEmpty())
        m_fileWatcher->addPath(m_currentFile);
}

void ScriptEditorWidget::applySearchResults(const QList<WorkspaceIndex::SearchHit> &results)
{
    QList<ProblemsView::Problem> problems;
    problems.reserve(results.size());
    for (const WorkspaceIndex::SearchHit &hit : results) {
        ProblemsView::Problem problem;
        problem.severity = ProblemsView::Severity::Info;
        problem.file = hit.filePath;
        problem.line = hit.line;
        problem.message = hit.lineText;
        problems.append(problem);
    }
    if (results.isEmpty()) {
        if (m_statusLabel)
            m_statusLabel->setText(tr("No matches found."));
        m_problemsView->clear();
    } else {
        if (m_statusLabel)
            m_statusLabel->setText(tr("%1 matches found.").arg(results.size()));
        m_problemsView->setProblems(problems);
        m_bottomTabs->setCurrentWidget(m_problemsView);
    }
}

