#include "MainWindow.h"

#include <iostream>

#include "GameModel.h"
#include <QPushButton>
#include <QBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QStandardPaths>
#include <QTableView>
#include <QTimer>
#include <QUrl>
#include <QNetworkReply>
#include <QTabWidget>
#include <QComboBox>
#include <QMessageBox>
#include <QDateTime>
#include <QDialog>
#include <QTimeEdit>
#include <QItemSelectionModel>
#include <QFileDialog>
#include <QSettings>

#include "ButtonDelegate.h"

static const QUrl kDetectableUrl("https://discordapp.com/api/v9/applications/detectable");

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    m_darkMode = isSystemDarkMode();

    setupUi();
    loadFromCacheIfAvailable();
    fetchDetectable();
    loadCustomApps();

    m_updateTimer = new QTimer(this);

    connect(m_updateTimer, &QTimer::timeout, this, [this]() {
        tickCountdowns();
        updateRunningAppsView();
    });

    m_updateTimer->start(1000);
}

MainWindow::~MainWindow() {
    for (const auto& info : m_runningProcesses) {
        if (info.process && info.process->state() != QProcess::NotRunning) {
            info.process->kill();
            info.process->waitForFinished(3000);
        }
    }
}

auto MainWindow::GamesBrowserTab() -> void {
    m_gamesTab = new QWidget();
    m_gamesLayout = new QVBoxLayout(m_gamesTab);
    m_gamesLayout->setContentsMargins(10, 10, 10, 10);
    m_gamesLayout->setSpacing(10);

    m_searchRow = new QHBoxLayout();
    m_searchRow->setSpacing(10);

    m_searchLabel = new QLabel("Search:");
    m_searchLabel->setObjectName("searchLabel");

    m_search = new QLineEdit();
    m_search->setPlaceholderText("Search games by name...");
    m_search->setObjectName("searchBox");

    m_refreshBtn = new QPushButton("Refresh");
    m_refreshBtn->setObjectName("refreshBtn");

    m_searchRow->addWidget(m_searchLabel);
    m_searchRow->addWidget(m_search, 1);
    m_searchRow->addWidget(m_refreshBtn);
    m_gamesLayout->addLayout(m_searchRow);

    m_model = new GameModel(this);
    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setFilterKeyColumn(GameModel::Name);

    m_table = new QTableView();
    m_table->setObjectName("gamesTable");
    m_table->setModel(m_proxy);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);

    m_gamesLayout->addWidget(m_table, 1);

    delegate = new ButtonDelegate(m_table);
    m_table->setItemDelegateForColumn(2, delegate);
    connect(delegate, &ButtonDelegate::startClicked, this, &MainWindow::onStartFromRow);

    QTimer::singleShot(0, this, [this] { configureGamesTableColumns(); });

    m_tabWidget->addTab(m_gamesTab, "Games Browser");
}

auto MainWindow::RunningAppsTab() -> void {
    m_runningTab = new QWidget();
    m_runningLayout = new QVBoxLayout(m_runningTab);
    m_runningLayout->setContentsMargins(10, 10, 10, 10);
    m_runningLayout->setSpacing(10);

    m_runningHeader = new QLabel("Currently Running Applications");
    m_runningHeader->setObjectName("runningHeader");
    m_runningLayout->addWidget(m_runningHeader);

    m_runningAppsTable = new QTableView();
    m_runningAppsTable->setObjectName("runningAppsTable");
    m_runningAppsModel = new QStandardItemModel(0, 5, this);
    m_runningAppsModel->setHorizontalHeaderLabels({"Game", "Executable", "Process ID", "Timer", "Action"});
    m_runningAppsTable->setModel(m_runningAppsModel);
    m_runningAppsTable->horizontalHeader()->setStretchLastSection(false);
    m_runningAppsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_runningAppsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_runningAppsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_runningAppsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_runningAppsTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_runningAppsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_runningAppsTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_runningAppsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_runningAppsTable->setAlternatingRowColors(true);

    runningDelegate = new ButtonDelegate(m_runningAppsTable);
    m_runningAppsTable->setItemDelegateForColumn(4, runningDelegate);
    connect(runningDelegate, &ButtonDelegate::stopClicked, this, &MainWindow::onStopFromRunningTab);

    m_runningLayout->addWidget(m_runningAppsTable, 1);

    m_runningBtnRow = new QHBoxLayout();
    m_runningBtnRow->setSpacing(10);

    m_setTimerBtn = new QPushButton("Set Timer");
    m_setTimerBtn->setObjectName("setTimerBtn");
    m_setTimerBtn->setCursor(QCursor(Qt::PointingHandCursor));
    connect(m_setTimerBtn, &QPushButton::clicked, this, &MainWindow::onSetTimer);

    m_stopAllBtn = new QPushButton("Stop All");
    m_stopAllBtn->setObjectName("stopAllBtn");
    m_stopAllBtn->setCursor(QCursor(Qt::PointingHandCursor));
    connect(m_stopAllBtn, &QPushButton::clicked, this, &MainWindow::onStopAll);

    m_runningBtnRow->addWidget(m_setTimerBtn);
    m_runningBtnRow->addStretch();
    m_runningBtnRow->addWidget(m_stopAllBtn);
    m_runningLayout->addLayout(m_runningBtnRow);

    m_tabWidget->addTab(m_runningTab, "Running Apps (0)");
}

auto MainWindow::setupUi() -> void {
    m_central = new QWidget(this);
    setCentralWidget(m_central);

    m_mainLayout = new QVBoxLayout(m_central);
    m_mainLayout->setContentsMargins(10, 10, 10, 10);
    m_mainLayout->setSpacing(10);

    // Top bar
    m_topBar = new QHBoxLayout();
    m_topBar->setSpacing(10);
    m_mainLayout->addLayout(m_topBar);

    m_titleLabel = new QLabel("Quest Completer - for Discord");
    m_titleLabel->setObjectName("appTitle");
    m_topBar->addWidget(m_titleLabel);

    m_topBar->addStretch();

    m_mainLayout->addLayout(m_topBar);

    m_tabWidget = new QTabWidget();

    GamesBrowserTab();
    RunningAppsTab();
    CustomAppsTab();

    m_mainLayout->addWidget(m_tabWidget, 1);

    m_status = new QLabel("Ready.");
    m_status->setObjectName("statusBar");
    m_mainLayout->addWidget(m_status);

    connect(m_refreshBtn, &QPushButton::clicked, this, &MainWindow::fetchDetectable);
    connect(m_search, &QLineEdit::textChanged, m_proxy, &QSortFilterProxyModel::setFilterFixedString);

    resize(1000, 700);
    setWindowTitle("DCQuestCompleter");

    m_refreshBtn->setObjectName("refreshBtn");
    m_customBrowseBtn->setObjectName("customBrowseBtn");
    m_customAddBtn->setObjectName("customAddBtn");
    m_setTimerBtn->setObjectName("setTimerBtn");
    m_stopAllBtn->setObjectName("stopAllBtn");
    m_status->setObjectName("statusLabel");
    m_runningAppsTable->setObjectName("runningAppsTable");
    m_customTab->setObjectName("customFormGroup");

    applyTheme();
}

auto MainWindow::CustomAppsTab() -> void {
    m_customTab = new QWidget();
    m_customLayout = new QVBoxLayout(m_customTab);
    m_customLayout->setContentsMargins(10, 10, 10, 10);
    m_customLayout->setSpacing(10);

    // Header
    m_hdr = new QLabel("Custom Applications");
    m_hdr->setObjectName("firstTabName");
    m_customLayout->addWidget(m_hdr);

    m_sub_hdr = new QLabel(
        "Add a custom app by name, target folder, and desired exe filename. "
        "When you click Launch, runner.exe will be copied into that folder "
        "under the given name (the folder will be created if it doesn't exist)."
        );
    m_sub_hdr->setWordWrap(true);
    m_sub_hdr->setObjectName("subTabName");
    m_customLayout->addWidget(m_sub_hdr);

    m_formGroup = new QWidget();
    m_formGroup->setObjectName("customFormGroup");
    m_formLayout = new QVBoxLayout(m_formGroup);
    m_formLayout->setContentsMargins(12, 12, 12, 12);
    m_formLayout->setSpacing(8);

    // Row 1 – App name
    m_nameRow = new QHBoxLayout();
    m_nameLabel = new QLabel("App Name:");
    m_nameLabel->setObjectName("nameLabel");
    m_nameLabel->setFixedWidth(100);
    m_customNameEdit = new QLineEdit();
    m_customNameEdit->setPlaceholderText("e.g. My Game");
    m_customNameEdit->setObjectName("customNameEdit");
    m_nameRow->addWidget(m_nameLabel);
    m_nameRow->addWidget(m_customNameEdit, 1);
    m_formLayout->addLayout(m_nameRow);

    // Row 2 – Folder path
    m_pathRow = new QHBoxLayout();
    m_pathLabel = new QLabel("Folder Path:");
    m_pathLabel->setObjectName("pathLabel");
    m_pathLabel->setFixedWidth(100);
    m_customPathEdit = new QLineEdit();
    m_customPathEdit->setPlaceholderText(R"(C:\Path\To\Game\)");
    m_customPathEdit->setObjectName("customPathEdit");
    m_customBrowseBtn = new QPushButton("Browse…");
    m_customBrowseBtn->setObjectName("customBrowseBtn");
    m_customBrowseBtn->setFixedWidth(100);
    connect(m_customBrowseBtn, &QPushButton::clicked, this, &MainWindow::onCustomBrowse);
    m_pathRow->addWidget(m_pathLabel);
    m_pathRow->addWidget(m_customPathEdit, 1);
    m_pathRow->addWidget(m_customBrowseBtn);
    m_formLayout->addLayout(m_pathRow);

    // Row 3 – Exe name
    m_exeRow = new QHBoxLayout();
    m_exeLabel = new QLabel("Executable Name:");
    m_exeLabel->setObjectName("exeLabel");
    m_exeLabel->setFixedWidth(100);
    m_customExeEdit = new QLineEdit();
    m_customExeEdit->setPlaceholderText("game.exe");
    m_customExeEdit->setObjectName("customExeEdit");
    m_exeRow->addWidget(m_exeLabel);
    m_exeRow->addWidget(m_customExeEdit, 1);
    m_formLayout->addLayout(m_exeRow);

    // Row 3 – Add button
    m_addRow = new QHBoxLayout();
    m_addRow->addStretch();
    m_customAddBtn = new QPushButton("＋  Add Application");
    m_customAddBtn->setObjectName("customAddBtn");
    m_customAddBtn->setMinimumWidth(160);
    connect(m_customAddBtn, &QPushButton::clicked, this, &MainWindow::onCustomAdd);
    m_addRow->addWidget(m_customAddBtn);
    m_formLayout->addLayout(m_addRow);

    m_customLayout->addWidget(m_formGroup);

    // Saved custom apps table
    m_customAppsTable = new QTableView();
    m_customAppsTable->setObjectName("customAppsTable");
    m_customAppsModel = new QStandardItemModel(0, 5, this);
    m_customAppsModel->setHorizontalHeaderLabels({"Name", "Folder", "Exe Name", "Launch", "Remove"});
    m_customAppsTable->setModel(m_customAppsModel);
    m_customAppsTable->horizontalHeader()->setStretchLastSection(false);
    m_customAppsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_customAppsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_customAppsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_customAppsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_customAppsTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_customAppsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_customAppsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_customAppsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_customAppsTable->setAlternatingRowColors(true);

    // "Launch" column delegate
    m_customLaunchDelegate = new ButtonDelegate(m_customAppsTable);
    m_customAppsTable->setItemDelegateForColumn(3, m_customLaunchDelegate);
    connect(m_customLaunchDelegate, &ButtonDelegate::startClicked,
            this, &MainWindow::onCustomLaunch);

    // "Remove" column delegate
    m_customRemoveDelegate = new ButtonDelegate(m_customAppsTable);
    m_customAppsTable->setItemDelegateForColumn(4, m_customRemoveDelegate);
    connect(m_customRemoveDelegate, &ButtonDelegate::stopClicked,
            this, &MainWindow::onCustomRemove);

    m_customLayout->addWidget(m_customAppsTable, 1);

    m_tabWidget->addTab(m_customTab, "Custom Apps");
}

auto MainWindow::customAppsSettingsPath() -> QString {
    if (auto base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation); base.isEmpty())
        base = QDir::homePath() + "/.dcquestcompleter_cache";

    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("custom_apps.json");
}

auto MainWindow::loadCustomApps() -> void {
    QFile f(customAppsSettingsPath());
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) return;

    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isArray()) return;

    m_customApps.clear();

    for (const auto& v : doc.array()) {
        const auto obj = v.toObject();
        CustomAppEntry e;
        e.name = obj["name"].toString();
        e.folderPath = obj["folderPath"].toString();
        e.exeName = obj["exeName"].toString();
        if (!e.name.isEmpty() && !e.folderPath.isEmpty() && !e.exeName.isEmpty())
            m_customApps.append(e);
    }

    refreshCustomAppsTable();
}

auto MainWindow::saveCustomApps() const -> void {
    auto base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);

    QJsonArray arr;
    for (const auto&[name, folderPath, exeName] : m_customApps) {
        QJsonObject obj;
        obj["name"] = name;
        obj["folderPath"] = folderPath;
        obj["exeName"] = exeName;
        arr.append(obj);
    }

    if (QFile f(customAppsSettingsPath()); f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(arr).toJson());
}

auto MainWindow::refreshCustomAppsTable() -> void {
    m_customAppsModel->removeRows(0, m_customAppsModel->rowCount());

    for (int i = 0; i < m_customApps.size(); ++i) {
        const auto&[name, folderPath, exeName] = m_customApps[i];

        m_nameItem = new QStandardItem(name);

        m_folderItem = new QStandardItem(folderPath);
        m_folderItem->setToolTip(folderPath);

        m_exeItem = new QStandardItem(exeName);

        m_launchItem = new QStandardItem("Launch");
        m_launchItem->setData("Launch", Qt::UserRole);

        m_removeItem = new QStandardItem("Remove");
        m_removeItem->setData("Stop", Qt::UserRole);

        m_customAppsModel->setItem(i, 0, m_nameItem);
        m_customAppsModel->setItem(i, 1, m_folderItem);
        m_customAppsModel->setItem(i, 2, m_exeItem);
        m_customAppsModel->setItem(i, 3, m_launchItem);
        m_customAppsModel->setItem(i, 4, m_removeItem);
    }
}

auto MainWindow::onCustomBrowse() -> void {
    const QString dir = QFileDialog::getExistingDirectory(
        this,
        "Select Game Folder",
        QDir::homePath(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    if (dir.isEmpty()) return;

    m_customPathEdit->setText(QDir::toNativeSeparators(dir));
}

auto MainWindow::onCustomAdd() -> void {
    const QString name = m_customNameEdit->text().trimmed();
    const QString folderPath = m_customPathEdit->text().trimmed();
    QString exeName = m_customExeEdit->text().trimmed();

    if (name.isEmpty()) {
        QMessageBox::warning(this, "Missing Name", "Please enter an app name.");
        m_customNameEdit->setFocus();
        return;
    }
    if (folderPath.isEmpty()) {
        QMessageBox::warning(this, "Missing Folder", "Please enter or browse to the target folder.");
        m_customPathEdit->setFocus();
        return;
    }
    if (exeName.isEmpty()) {
        QMessageBox::warning(this, "Missing Exe Name",
            "Please enter the desired executable filename (e.g. game.exe).\n");
        m_customExeEdit->setFocus();
        return;
    }

    if (!exeName.endsWith(".exe", Qt::CaseInsensitive))
        exeName += ".exe";

    for (const auto& e : m_customApps) {
        if (e.folderPath == folderPath && e.exeName == exeName) {
            QMessageBox::information(this, "Duplicate",
                "An entry already exists for that folder and exe name.");
            return;
        }
    }

    CustomAppEntry entry;
    entry.name = name;
    entry.folderPath = folderPath;
    entry.exeName = exeName;
    m_customApps.append(entry);

    saveCustomApps();
    refreshCustomAppsTable();

    m_customNameEdit->clear();
    m_customPathEdit->clear();
    m_customExeEdit->clear();
    m_status->setText(
        QString("Custom app '%1' added. Click Launch to copy runner.exe and start it.").arg(name)
    );
}

auto MainWindow::onCustomLaunch(const QModelIndex& index) -> void {
    if (!index.isValid()) return;
    const int i = index.row();
    if (i < 0 || i >= m_customApps.size()) return;

    const auto&[name, folderPath, exeName] = m_customApps[i];
    const QString fullPath  = QDir(folderPath).filePath(exeName);
    const QString instanceKey = QString("custom::%1").arg(fullPath);

    if (m_runningProcesses.contains(instanceKey)) {
        m_status->setText(QString("'%1' is already running.").arg(name));
        return;
    }

    // Locate runner.exe
    const QString runnerSrc =
        QDir(QCoreApplication::applicationDirPath()).filePath("runner.exe");

    if (!QFile::exists(runnerSrc)) {
        QMessageBox::critical(this, "Runner Not Found",
            QString("Cannot find runner.exe at:\n\n  %1\n\n"
                    "Make sure runner.exe is in the same folder as this application.").arg(runnerSrc));
        return;
    }

    // Create folder if it doesn't exist
    if (const QDir targetDir(folderPath); !targetDir.exists()) {
        const auto btn = QMessageBox::question(
            this, "Create Folder?",
            QString("The target folder does not exist:\n\n  %1\n\n"
                    "Create it and copy runner.exe as \"%2\" to launch?")
                .arg(folderPath, exeName),
            QMessageBox::Yes | QMessageBox::No
        );
        if (btn != QMessageBox::Yes) return;

        if (!QDir().mkpath(folderPath)) {
            QMessageBox::critical(this, "Error",
                QString("Failed to create folder path:\n\n  %1").arg(folderPath));
            return;
        }
    }

    // Copy runner.exe to folder/exeName
    if (QFile::exists(fullPath))
        QFile::remove(fullPath);

    if (!QFile::copy(runnerSrc, fullPath)) {
        QMessageBox::critical(this, "Copy Failed",
            QString("Could not copy runner.exe to:\n\n  %1\n\n"
                    "Check that you have write permission to that folder.").arg(fullPath));
        return;
    }

    QFile rf(fullPath);
    rf.setPermissions(rf.permissions()
        | QFileDevice::ExeOwner | QFileDevice::ExeGroup | QFileDevice::ExeOther);

    m_proc = new QProcess(this);
    m_proc->setProgram(fullPath);
    m_proc->setWorkingDirectory(folderPath);

    ProcessInfo info;
    info.process = m_proc;
    info.gameName = name;
    info.gameId = instanceKey;
    info.exeName  = exeName;
    info.workingDir = folderPath;
    info.startTime = QDateTime::currentDateTime();
    info.countdownSeconds = -1;

    m_runningProcesses[instanceKey] = info;

    connect(m_proc, &QProcess::started, this, [this, name = name, exeName = exeName]() {
        m_status->setText(QString("Started '%1' (%2)").arg(name, exeName));
        updateRunningAppsView();
    });

    connect(m_proc, &QProcess::finished, this,
        [this, instanceKey, name = name](int, QProcess::ExitStatus) {
            m_status->setText(QString("'%1' stopped.").arg(name));
            if (m_runningProcesses.contains(instanceKey)) {
                m_runningProcesses[instanceKey].process->deleteLater();
                m_runningProcesses.remove(instanceKey);
            }
            updateRunningAppsView();
        });

    connect(m_proc, &QProcess::errorOccurred, this,
        [this, instanceKey, name = name](QProcess::ProcessError err) {
            m_status->setText(
                QString("Error launching '%1': %2").arg(name).arg(static_cast<int>(err)));
            if (m_runningProcesses.contains(instanceKey))
                m_runningProcesses.remove(instanceKey);
            updateRunningAppsView();
        });

    m_proc->start();
    m_tabWidget->setCurrentIndex(1);
}

auto MainWindow::onCustomRemove(const QModelIndex& index) -> void {
    if (!index.isValid()) return;
    const int i = index.row();
    if (i < 0 || i >= m_customApps.size()) return;

    const QString name = m_customApps[i].name;
    const auto btn = QMessageBox::question(
        this, "Remove Custom App",
        QString("Remove <b>%1</b> from your custom apps list?<br>"
                "(This does not delete the file.)").arg(name),
        QMessageBox::Yes | QMessageBox::No
    );
    if (btn != QMessageBox::Yes) return;

    m_customApps.removeAt(i);
    saveCustomApps();
    refreshCustomAppsTable();
    m_status->setText(QString("Removed custom app '%1'.").arg(name));
}

auto MainWindow::tickCountdowns() -> void {
    QStringList toStop;

    for (auto it = m_runningProcesses.begin(); it != m_runningProcesses.end(); ++it) {
        ProcessInfo& info = it.value();
        if (info.countdownSeconds < 0) continue;

        if (info.countdownSeconds == 0) {
            toStop.append(it.key());
        } else {
            --info.countdownSeconds;
            if (info.countdownSeconds == 0) {
                toStop.append(it.key());
            }
        }
    }

    for (const QString& key : toStop) {
        if (m_runningProcesses.contains(key)) {
            const QString name = m_runningProcesses[key].gameName;
            m_status->setText(QString("Timer expired – stopping %1").arg(name));
            stopProcess(key);
        }
    }
}

auto MainWindow::onSetTimer() -> void {
    const QModelIndexList selected = m_runningAppsTable->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        QMessageBox::information(this, "No Selection",
            "Please select one or more running applications in the table first,\n"
            "then click 'Set Timer'.");
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("Set Countdown Timer");
    dialog.setMinimumWidth(320);

    m_dlgLayout = new QVBoxLayout(&dialog);
    m_dlgLayout->setSpacing(12);
    m_dlgLayout->setContentsMargins(16, 16, 16, 16);

    const int count = selected.size();

    const auto hint1 = QString("Set a countdown timer for <b>%1</b> selected application.<br>"
                      "It will be stopped automatically when the timer reaches 00:00:00.");

    const auto hint2 = QString("Set a countdown timer for <b>%1</b> selected applications.<br>"
                      "Each will be stopped automatically when the timer reaches 00:00:00.");

    m_infoLabel = new QLabel(count == 1 ? hint1.arg(count) : hint2.arg(count));
    m_infoLabel->setWordWrap(true);
    m_dlgLayout->addWidget(m_infoLabel);

    m_timeLabel = new QLabel("Duration (HH:MM:SS):");
    m_timeLabel->setObjectName("timeLabel");
    m_dlgLayout->addWidget(m_timeLabel);

    m_timeEdit = new QTimeEdit(QTime(0, 15, 0));
    m_timeEdit->setDisplayFormat("HH:mm:ss");
    m_timeEdit->setMinimumTime(QTime(0, 0, 1));
    m_timeEdit->setObjectName("timeEdit");
    m_dlgLayout->addWidget(m_timeEdit);

    m_btnRow = new QHBoxLayout();
    m_okBtn = new QPushButton("Set Timer");
    m_okBtn->setCursor(QCursor(Qt::PointingHandCursor));
    m_okBtn->setObjectName("okBtn");
    m_cancelBtn = new QPushButton("Cancel");
    m_cancelBtn->setCursor(QCursor(Qt::PointingHandCursor));
    m_cancelBtn->setObjectName("cancelBtn");

    connect(m_okBtn,     &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(m_cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);

    m_btnRow->addStretch();
    m_btnRow->addWidget(m_okBtn);
    m_btnRow->addWidget(m_cancelBtn);
    m_dlgLayout->addLayout(m_btnRow);

    if (dialog.exec() != QDialog::Accepted) return;

    const QTime t = m_timeEdit->time();
    const qint64 totalSeconds = t.hour() * 3600 + t.minute() * 60 + t.second();
    if (totalSeconds <= 0) return;

    for (const QModelIndex& idx : selected) {
        const auto* nameItem = m_runningAppsModel->item(idx.row(), 0);

        if (!nameItem) continue;
        if (const QString instanceKey = nameItem->data(Qt::UserRole).toString(); m_runningProcesses.contains(instanceKey))
            m_runningProcesses[instanceKey].countdownSeconds = totalSeconds;
    }

    const QString timeStr = t.toString("HH:mm:ss");
    m_status->setText(
        count == 1 ?
        QString("Timer set: %1 for 1 application.").arg(timeStr)
        : QString("Timer set: %1 for %2 applications.").arg(timeStr).arg(count)
    );

    updateRunningAppsView();
}

auto static formatCountdown(const qint64 secs) -> QString {
    const int h = static_cast<int>(secs / 3600);
    const int m = static_cast<int>((secs % 3600) / 60);
    const int s = static_cast<int>(secs % 60);
    return QString("%1:%2:%3")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'));
}

auto MainWindow::updateRunningAppsView() -> void {
    QSet<QString> selectedKeys;

    for (const QModelIndexList selRows = m_runningAppsTable->selectionModel()->selectedRows(); const QModelIndex& idx : selRows) {
        const auto* item = m_runningAppsModel->item(idx.row(), 0);
        if (item) selectedKeys.insert(item->data(Qt::UserRole).toString());
    }

    m_runningAppsModel->removeRows(0, m_runningAppsModel->rowCount());

    int row = 0;
    for (auto it = m_runningProcesses.begin(); it != m_runningProcesses.end(); ++it) {
        const auto& info = it.value();

        m_nameItem = new QStandardItem(info.gameName);
        m_nameItem->setData(it.key(), Qt::UserRole);

        m_exeItem = new QStandardItem(info.exeName);

        QString pidStr = "N/A";
        if (info.process && info.process->state() == QProcess::Running) pidStr = QString::number(info.process->processId());
        m_pidItem = new QStandardItem(pidStr);

        if (info.countdownSeconds >= 0) {
            const QString countdownStr = formatCountdown(info.countdownSeconds);
            m_timeItem = new QStandardItem(countdownStr);

            if (info.countdownSeconds > 300)
                m_timeItem->setForeground(QColor(0x4CAF50));
            else if (info.countdownSeconds > 60)
                m_timeItem->setForeground(QColor(0xFF9800));
            else
                m_timeItem->setForeground(QColor(0xf44336));
            m_timeItem->setData(Qt::AlignCenter, Qt::TextAlignmentRole);
        } else {
            m_timeItem = new QStandardItem("—");
            m_timeItem->setData(Qt::AlignCenter, Qt::TextAlignmentRole);
        }

        m_actionItem = new QStandardItem("Stop");
        m_actionItem->setData("Stop", Qt::UserRole);
        m_actionItem->setData(it.key(), Qt::UserRole + 1);


        m_runningAppsModel->setItem(row, 0, m_nameItem);
        m_runningAppsModel->setItem(row, 1, m_exeItem);
        m_runningAppsModel->setItem(row, 2, m_pidItem);
        m_runningAppsModel->setItem(row, 3, m_timeItem);
        m_runningAppsModel->setItem(row, 4, m_actionItem);

        if (selectedKeys.contains(it.key())) {
            m_runningAppsTable->selectionModel()->select(
                m_runningAppsModel->index(row, 0),
                QItemSelectionModel::Select | QItemSelectionModel::Rows
            );
        }
        row++;
    }

    m_tabWidget->setTabText(1, QString("Running Apps (%1)").arg(m_runningProcesses.size()));
}

auto MainWindow::cachePath() -> QString {
    auto base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (base.isEmpty()) base = QDir::homePath() + "/.dcquestcompleter_cache";
    return QDir(base).filePath("detectable.json");
}

auto MainWindow::loadFromCacheIfAvailable() const -> void{
    QFile f(cachePath());
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) return;
    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isArray()) return;
    m_model->loadFromJsonArray(doc.array());
    m_status->setText(QString("Loaded %1 games from cache.").arg(m_model->rowCount()));
    configureGamesTableColumns();
}

auto MainWindow::fetchDetectable() -> void {
    if (m_reply) { m_reply->abort(); m_reply->deleteLater(); m_reply = nullptr; }
    QNetworkRequest req(kDetectableUrl);
    req.setHeader(QNetworkRequest::UserAgentHeader, "DCQuestCompleter");
    m_reply = m_nam.get(req);
    connect(m_reply, &QNetworkReply::finished, this, &MainWindow::onDownloadFinished);
    m_status->setText("Fetching detectable apps from Discord…");
    m_refreshBtn->setEnabled(false);
}

auto MainWindow::onDownloadFinished() -> void {
    auto* r = m_reply;
    m_reply = nullptr;
    m_refreshBtn->setEnabled(true);
    if (!r) return;

    QByteArray payload;
    if (r->error() == QNetworkReply::NoError) {
        payload = r->readAll();
        if (QFile f(cachePath()); f.open(QIODevice::WriteOnly)) f.write(payload);
    } else {
        m_status->setText(QString("Network error: %1 — falling back to cache.").arg(r->errorString()));
        QFile f(cachePath());
        if (!f.exists() || !f.open(QIODevice::ReadOnly)) { r->deleteLater(); return; }
        payload = f.readAll();
    }
    r->deleteLater();

    const auto doc = QJsonDocument::fromJson(payload);
    if (!doc.isArray()) { m_status->setText("Unexpected JSON format."); return; }
    m_model->loadFromJsonArray(doc.array());
    m_status->setText(QString("Loaded %1 games successfully.").arg(m_model->rowCount()));
    configureGamesTableColumns();
}

auto MainWindow::tempGameDirFor(const QString& gameName, const QString& gameId) -> QString {
    auto base = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (base.isEmpty()) base = QDir::tempPath();
    QString safeName = gameName;
    safeName = safeName.replace(QRegularExpression("[/\\\\:*?\"<>|]"), "_");
    safeName = safeName.replace(QRegularExpression("\\s+"), "_");
    safeName = safeName.trimmed();
    if (safeName.length() > 50) safeName = safeName.left(50);
    return QDir(base).filePath(QString("DCQuestCompleter/%1_%2").arg(safeName, gameId));
}

auto MainWindow::copyRunnerAsGameExe(const QString& destDir, QString* outExePath, QString* error, const QString& targetName) -> bool {
    const QString runnerName = "runner.exe";
    const auto src  = QDir(QCoreApplication::applicationDirPath()).filePath(runnerName);
    const QDir  dest(destDir);

    if (!dest.exists()) {
        if (!QDir().mkpath(destDir)) {
            if (error) *error = QString("Failed to create base directory: %1").arg(destDir);
            return false;
        }
    }

    QString relativePath = targetName;
    if (relativePath.isEmpty()) relativePath = "game.exe";
    relativePath = relativePath.replace("\\", "/");
    if (!relativePath.endsWith(".exe", Qt::CaseInsensitive)) relativePath += ".exe";

    if (const QFileInfo fi(relativePath); !fi.path().isEmpty() && fi.path() != ".") {
        if (!dest.mkpath(fi.path())) {
            if (error) *error = QString("Failed to create directory: %1").arg(dest.filePath(fi.path()));
            return false;
        }
    }

    const auto dst = dest.filePath(relativePath);
    if (!QFile::exists(src)) {
        if (error) *error = QString("Runner binary not found: %1").arg(src);
        return false;
    }

    if (QFile::exists(dst)) QFile::remove(dst);

    if (!QFile::copy(src, dst)) {
        if (error) *error = QString("Failed to copy runner to: %1").arg(dst);
        return false;
    }

    QFile f(dst);
    f.setPermissions(f.permissions() | QFileDevice::ExeOwner | QFileDevice::ExeGroup | QFileDevice::ExeOther);
    if (outExePath) *outExePath = dst;
    return true;
}

auto MainWindow::onStartFromRow(const QModelIndex& idxProxy) -> void  {
    const auto idx  = m_proxy->mapToSource(idxProxy);
    const auto id   = m_model->item(idx.row(), GameModel::Id)->text();
    const auto name = m_model->item(idx.row(), GameModel::Name)->text();
    const auto obj  = m_model->gameObject(idx.row());

    const QString osKey = "win32";

    QStringList executables;

    for (const auto execArr = obj["executables"].toArray(); const auto& v : execArr) {
        if (const auto execObj = v.toObject(); execObj["os"].toString() == osKey) {
            if (const QString exeName = execObj["name"].toString(); !exeName.isEmpty())
                executables.append(exeName);
        }
    }

    if (executables.isEmpty()) {
        QMessageBox::warning(this, "No Executables",
            QString("No executables found for %1 on this platform.").arg(name));
        return;
    }

    QString selectedExe;
    if (executables.size() > 1) {
        QDialog dialog(this);
        dialog.setWindowTitle("Select Executable");
        m_layout = new QVBoxLayout(&dialog);

        m_label = new QLabel(QString(
            "Multiple executables found for <b>%1</b>.<br>Please select which one to launch:").arg(name));
        m_label->setWordWrap(true);
        m_layout->addWidget(m_label);

        m_combo = new QComboBox();
        m_combo->setObjectName("combo");
        m_combo->addItems(executables);
        m_layout->addWidget(m_combo);

        m_buttonBox = new QHBoxLayout();
        m_okBtn = new QPushButton("Launch");
        m_okBtn->setCursor(Qt::PointingHandCursor);
        m_okBtn->setObjectName("okBtn");

        m_cancelBtn = new QPushButton("Cancel");
        m_cancelBtn->setCursor(Qt::PointingHandCursor);
        m_cancelBtn->setObjectName("cancelBtn");

        connect(m_okBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
        connect(m_cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);

        m_buttonBox->addStretch();
        m_buttonBox->addWidget(m_okBtn);
        m_buttonBox->addWidget(m_cancelBtn);
        m_layout->addLayout(m_buttonBox);

        if (dialog.exec() == QDialog::Accepted)
            selectedExe = m_combo->currentText();
        else
            return;
    } else {
        selectedExe = executables.first();
    }

    const auto dir = tempGameDirFor(name, id);
    QString exePath, err;
    if (!copyRunnerAsGameExe(dir, &exePath, &err, selectedExe)) {
        m_status->setText("Failed to start: " + err);
        QMessageBox::critical(this, "Launch Error", "Failed to start game:\n" + err);
        return;
    }

    QString instanceKey = QString("%1::%2").arg(id, selectedExe);
    if (m_runningProcesses.contains(instanceKey)) {
        m_status->setText(QString("%1 (%2) is already running").arg(name, selectedExe));
        return;
    }

    m_proc = new QProcess(this);
    m_proc->setProgram(exePath);
    m_proc->setWorkingDirectory(dir);

    ProcessInfo info;
    info.process = m_proc;
    info.gameName = name;
    info.gameId = id;
    info.exeName = selectedExe;
    info.workingDir = dir;
    info.startTime = QDateTime::currentDateTime();
    info.countdownSeconds = -1;

    m_runningProcesses[instanceKey] = info;

    connect(m_proc, &QProcess::started, this, [this, name = name, selectedExe = selectedExe]() {
        m_status->setText(QString("Started %1 (%2)").arg(name, selectedExe));
        updateRunningAppsView();
    });

    connect(m_proc, &QProcess::finished, this,
        [this, instanceKey, name = name, dir = dir](int, QProcess::ExitStatus) {
            m_status->setText(QString("%1 stopped.").arg(name));
            if (m_runningProcesses.contains(instanceKey)) {
                m_runningProcesses[instanceKey].process->deleteLater();
                m_runningProcesses.remove(instanceKey);
            }
            QDir(dir).removeRecursively();
            updateRunningAppsView();
        });

    connect(m_proc, &QProcess::errorOccurred, this,
        [this, instanceKey, name = name](const QProcess::ProcessError e) {
            m_status->setText(
                QString("Error running %1: %2").arg(name).arg(static_cast<int>(e)));
            if (m_runningProcesses.contains(instanceKey))
                m_runningProcesses.remove(instanceKey);
            updateRunningAppsView();
        });

    m_proc->start();
}

auto MainWindow::onStopFromRow(const QModelIndex& idxProxy) -> void {
    const auto idx = m_proxy->mapToSource(idxProxy);
    const auto id  = m_model->item(idx.row(), GameModel::Id)->text();

    QStringList keysToStop;
    for (auto it = m_runningProcesses.begin(); it != m_runningProcesses.end(); ++it)
        if (it.value().gameId == id) keysToStop.append(it.key());

    if (keysToStop.isEmpty()) {
        m_status->setText("No running processes found for this game.");
        return;
    }
    for (const QString& key : keysToStop) stopProcess(key);
}

auto MainWindow::onStopFromRunningTab(const QModelIndex& index) -> void {
    if (!index.isValid() || index.row() >= m_runningAppsModel->rowCount()) return;
    const auto* item = m_runningAppsModel->item(index.row(), 0);
    if (!item) return;
    stopProcess(item->data(Qt::UserRole).toString());
}

auto MainWindow::stopProcess(const QString& instanceKey) -> void {
    if (!m_runningProcesses.contains(instanceKey)) return;
    auto& info = m_runningProcesses[instanceKey];

    if (auto* proc = info.process; proc && proc->state() != QProcess::NotRunning) {
        proc->terminate();
        if (!proc->waitForFinished(2000)) { proc->kill(); proc->waitForFinished(2000); }
    }

    if (instanceKey.startsWith("custom::")) {
        // Only delete the runner exe
        const QString exePath = QDir(info.workingDir).filePath(info.exeName);
        QFile::remove(exePath);
    } else {
        QDir(info.workingDir).removeRecursively();
    }

    const QString statusMsg = QString("Stopped %1 (%2)").arg(info.gameName, info.exeName);
    m_runningProcesses.remove(instanceKey);
    m_status->setText(statusMsg);
    updateRunningAppsView();
}

auto MainWindow::onStopAll() -> void {
    if (m_runningProcesses.isEmpty()) {
        QMessageBox::information(this, "No Running Apps",
            "There are no applications currently running.");
        return;
    }

    const auto reply = QMessageBox::question(this, "Stop All",
        QString("Are you sure you want to stop all %1 running application(s)?")
            .arg(m_runningProcesses.size()),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        for (QStringList keys = m_runningProcesses.keys(); const QString& key : keys) stopProcess(key);
        m_status->setText("All applications stopped.");
    }
}

auto MainWindow::configureGamesTableColumns() const -> void {
    if (!m_model || !m_table) return;
    const int colCount = m_model->columnCount();
    if (colCount == 0) return;

    m_table->horizontalHeader()->setStretchLastSection(false);
    if (colCount > 0) m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    if (colCount > 1) m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    if (colCount > 2) m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    if (colCount > 3) m_table->setColumnHidden(3, true);
}

auto MainWindow::isSystemDarkMode() -> bool {
    const QSettings reg(
        R"(HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize)", QSettings::NativeFormat);

    return reg.value("AppsUseLightTheme", 1).toInt() == 0;
}

auto MainWindow::applyTheme() -> void {
    QString dynamicStyle;

    if (QFile baseStyle(":/style/style.qss"); baseStyle.open(QFile::ReadOnly)) {
        dynamicStyle += baseStyle.readAll();
        baseStyle.close();
    }

    if (QFile themeStyle(m_darkMode ? ":/style/style_dark.qss" : ":/style/style_light.qss"); themeStyle.open(QFile::ReadOnly)) {
        dynamicStyle += themeStyle.readAll();
        themeStyle.close();
    }

    setStyleSheet(dynamicStyle);

    m_refreshBtn->setCursor(Qt::PointingHandCursor);
    m_customBrowseBtn->setCursor(Qt::PointingHandCursor);
    m_customAddBtn->setCursor(Qt::PointingHandCursor);
}