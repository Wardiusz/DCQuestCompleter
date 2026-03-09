#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QNetworkReply>
#include <QProcess>
#include <QHash>
#include <QDateTime>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QTimeEdit>

class QComboBox;
class ButtonDelegate;
class QVBoxLayout;
class QHBoxLayout;
class QLineEdit;
class QPushButton;
class QTableView;
class QLabel;
class QTabWidget;
class GameModel;

struct ProcessInfo {
    QProcess* process = nullptr;
    QString gameName;
    QString gameId;
    QString exeName;
    QString workingDir;
    QDateTime startTime;
    qint64 countdownSeconds = -1;
};

struct CustomAppEntry {
    QString name;
    QString folderPath;
    QString exeName;
};

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    auto static isSystemDarkMode() -> bool;

    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    auto GamesBrowserTab() -> void;
    auto RunningAppsTab() -> void;
    auto CustomAppsTab() -> void;

private:
    auto setupUi() -> void;

    static QString cachePath();
    auto loadFromCacheIfAvailable() const -> void;
    auto fetchDetectable() -> void;

    auto static tempGameDirFor(const QString& gameName, const QString& gameId) -> QString;
    auto static copyRunnerAsGameExe(const QString& destDir, QString* outExePath, QString* error, const QString& targetName = QString()) -> bool;

    auto stopProcess(const QString& instanceKey) -> void;
    auto updateRunningAppsView() -> void;
    auto tickCountdowns() -> void;
    auto applyTheme() -> void;
    auto configureGamesTableColumns() const -> void;

    // persistence
    auto static customAppsSettingsPath() -> QString;
    auto loadCustomApps() -> void;
    auto saveCustomApps() const -> void;
    auto refreshCustomAppsTable() -> void;

    // slots
    auto onCustomBrowse() -> void;
    auto onCustomAdd() -> void;
    auto onCustomLaunch(const QModelIndex& index) -> void;
    auto onCustomRemove(const QModelIndex& index) -> void;

private slots:
    auto onDownloadFinished() -> void;
    auto onStartFromRow(const QModelIndex& idxProxy) -> void;
    auto onStopFromRow(const QModelIndex& idxProxy) -> void;
    auto onStopFromRunningTab(const QModelIndex& index) -> void;
    auto onStopAll() -> void;
    auto onSetTimer() -> void;

private:
    QHBoxLayout* m_addRow = nullptr;
    QHBoxLayout* m_exeRow = nullptr;
    QLabel* m_exeLabel = nullptr;
    QLabel* m_hdr = nullptr;
    QLabel* m_sub_hdr = nullptr;
    QWidget* m_formGroup = nullptr;
    QVBoxLayout* m_formLayout = nullptr;
    QHBoxLayout* m_nameRow = nullptr;
    QLabel* m_nameLabel = nullptr;
    QHBoxLayout* m_pathRow = nullptr;
    QLabel* m_pathLabel = nullptr;
    QStandardItem* m_folderItem = nullptr;
    QStandardItem* m_launchItem = nullptr;
    QStandardItem* m_removeItem = nullptr;

    // Custom Apps Tab widgets
    QWidget* m_customTab = nullptr;
    QVBoxLayout* m_customLayout = nullptr;
    QLineEdit* m_customNameEdit = nullptr;
    QLineEdit* m_customPathEdit = nullptr;
    QLineEdit* m_customExeEdit = nullptr;
    QPushButton* m_customBrowseBtn = nullptr;
    QPushButton* m_customAddBtn = nullptr;
    QTableView* m_customAppsTable = nullptr;
    QStandardItemModel* m_customAppsModel = nullptr;
    ButtonDelegate* m_customLaunchDelegate = nullptr;
    ButtonDelegate* m_customRemoveDelegate = nullptr;

    // Main Components
    QWidget* m_central = nullptr;
    QVBoxLayout* m_mainLayout = nullptr;
    QHBoxLayout* m_topBar = nullptr;
    QTimer* m_updateTimer = nullptr;
    QVBoxLayout* m_layout = nullptr;
    QProcess* m_proc = nullptr;
    QWidget* m_gamesTab = nullptr;
    QVBoxLayout* m_gamesLayout = nullptr;
    QHBoxLayout* m_searchRow = nullptr;
    QLabel* m_searchLabel = nullptr;
    QWidget* m_runningTab = nullptr;
    QLabel* m_runningLabel = nullptr;
    QVBoxLayout* m_runningLayout = nullptr;
    QLabel* m_runningHeader = nullptr;
    QPushButton* m_stopAllBtn = nullptr;
    QPushButton* m_setTimerBtn = nullptr;
    QHBoxLayout* m_runningBtnRow = nullptr;
    QLabel* m_label = nullptr;
    QComboBox* m_combo = nullptr;
    QHBoxLayout* m_buttonBox = nullptr;
    QPushButton* m_okBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;
    QStandardItem* m_nameItem = nullptr;
    QStandardItem* m_exeItem = nullptr;
    QStandardItem* m_pidItem = nullptr;
    QStandardItem* m_timeItem = nullptr;
    QStandardItem* m_actionItem = nullptr;
    QLabel* m_infoLabel = nullptr;
    QVBoxLayout* m_dlgLayout = nullptr;
    QLabel* m_timeLabel = nullptr;
    QTimeEdit* m_timeEdit = nullptr;
    QHBoxLayout* m_btnRow = nullptr;

    ButtonDelegate* runningDelegate = nullptr;
    ButtonDelegate* delegate = nullptr;

    // UI Components - Games Tab
    QTabWidget* m_tabWidget = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLineEdit* m_search = nullptr;
    QPushButton* m_refreshBtn = nullptr;
    QPushButton* m_themeBtn = nullptr;
    QTableView* m_table = nullptr;
    GameModel* m_model = nullptr;
    QSortFilterProxyModel* m_proxy = nullptr;

    // UI Components - Running Apps Tab
    QTableView* m_runningAppsTable = nullptr;
    QStandardItemModel* m_runningAppsModel = nullptr;

    // Status
    QLabel* m_status = nullptr;

    // Network
    QNetworkAccessManager m_nam;
    QNetworkReply* m_reply = nullptr;

    // Process management
    QHash<QString, ProcessInfo> m_runningProcesses;

    QList<CustomAppEntry> m_customApps;

    // Theme
    bool m_darkMode = true;
};

#endif // MAINWINDOW_H
