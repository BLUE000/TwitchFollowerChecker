#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "logger.h"
#include "cat_png.h"
#include <QEvent>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QColorDialog>
#include <QFontDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QHeaderView>
#include <QDesktopServices>
#include <QUrl>
#include <QTimer>
#include <QPainter>
#include <QResizeEvent>

#ifndef APP_VERSION_STRING
#define APP_VERSION_STRING "1.5.0"
#endif

#ifndef BUILD_IS_CUSTOMIZED
#define BUILD_IS_CUSTOMIZED 0
#endif

namespace UIConstants {
    const QString MAIN_TAB_CHECKER = "🔍 フォロワーチェッカー";
    const QString MAIN_TAB_SETTINGS = "⚙️ システム設定";

    const QString SUB_TAB_ALL = "全て";
    const QString SUB_TAB_MUTUAL = "相互";
    const QString SUB_TAB_FOLLOWING = "フォローのみ";
    const QString SUB_TAB_FOLLOWERS = "フォロワーのみ";

    const QString HEADER_LOGIN_NAME = "ログイン名";
    const QString HEADER_DISPLAY_NAME = "表示名";
    const QString HEADER_RELATIONSHIP = "関係";
    const QString HEADER_FOLLOW_DATE = "フォロー開始日";
    const QString HEADER_CHANNEL_URL = "チャンネルURL";
}

// UI要素のポインタ定義（設定の読込・保存用）
QCheckBox* chkShowAll = nullptr;
QCheckBox* chkShowMutual = nullptr;
QCheckBox* chkShowFollowingOnly = nullptr;
QCheckBox* chkShowFollowersOnly = nullptr;
QCheckBox* chkOverrideApi = nullptr;
QLineEdit* txtClientId = nullptr;
QLineEdit* txtClientSecret = nullptr;
QLabel* lblAuthStatus = nullptr;
QPushButton* btnLogin = nullptr;
QPushButton* btnRun = nullptr;
QPushButton* btnExport = nullptr;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_config(nullptr)
    , m_auth(nullptr)
    , m_apiClient(nullptr)
    , m_followerModel(nullptr)
    , m_proxyModel(nullptr)
    , m_mainTabWidget(nullptr)
    , m_subTabWidget(nullptr)
    , m_tableView(nullptr)
    , m_statusInfoLabel(nullptr)
    , m_autoFetchTimer(nullptr)
    , m_idleTimer(nullptr)
    , m_idleSeconds(0)
    , m_catContainer(nullptr)
{
    ui->setupUi(this);

    // 1. バージョンおよびカスタムビルド判定に応じたウィンドウタイトルとステータスバー設定
    if (BUILD_IS_CUSTOMIZED) {
        setWindowTitle(QString("TwitchFollowerChecker - Ver %1 (Custom Build)").arg(APP_VERSION_STRING));
        statusBar()->showMessage("© BLUE000 (Original Creator)");
        statusBar()->setStyleSheet("color: #888888; background-color: #121214; padding: 2px 0px;");
    } else {
        setWindowTitle(QString("TwitchFollowerChecker - Ver %1").arg(APP_VERSION_STRING));
        statusBar()->setStyleSheet("background-color: #121214; padding: 2px 0px;");
    }

    // ステータスバー右側に通知用ラベルを追加（改ざん判定の左側メッセージを上書きしない）
    m_statusInfoLabel = new QLabel(this);
    m_statusInfoLabel->setStyleSheet("color: #55FF55; padding-right: 8px;");
    statusBar()->addPermanentWidget(m_statusInfoLabel);

    resize(900, 650);

    // 2. ディレクトリおよびログの初期化
    Logger::init("logs");
    Logger::logInfo(QString("TwitchFollowerChecker launched. Version: %1, Custom: %2")
                    .arg(APP_VERSION_STRING)
                    .arg(BUILD_IS_CUSTOMIZED ? "true" : "false"));

    m_config = new ConfigManager("config", this);
    m_config->load();

    m_auth = new TwitchAuth(this);
    m_apiClient = new TwitchApiClient(this);

    m_autoFetchTimer = new QTimer(this);
    connect(m_autoFetchTimer, &QTimer::timeout, this, &MainWindow::onAutoFetchTimeout);

    // 3. 起動時トークン認証の実行 (キルスイッチ)
    // 開発用の local_config.cmake または user_preset で設定されたトークンがあれば検証します。
    #ifdef TRANSCIPHER_API_TOKEN
    QString startupToken = TRANSCIPHER_API_TOKEN;
    #else
    QString startupToken = "";
    #endif

    // スタブモードであるか、またはトークンが空でなければ検証を実行する
    if (m_auth->isStubMode() || !startupToken.isEmpty()) {
        if (!m_auth->verifyStartupToken(startupToken)) {
            QMessageBox::critical(this, "ライセンス認証エラー", 
                                  "このアプリケーションのライセンス認証に失敗しました。\nアプリケーションを終了します。");
            Logger::logError("Startup token validation failed. Terminating app.");
            QMetaObject::invokeMethod(this, "close", Qt::QueuedConnection);
            return;
        }
    }

    // 4. GUIの構築と接続
    setupUiManual();
    applyCustomStyles();
    loadSettingsToUi();

    // 5. 無操作監視トリックのセットアップ
    m_idleSeconds = 0;
    m_idleTimer = new QTimer(this);
    connect(m_idleTimer, &QTimer::timeout, this, &MainWindow::onIdleTimeout);
    m_idleTimer->start(1000); // 1秒周期

    // アプリケーション全体の操作イベントを監視するフィルタをインストール
    qApp->installEventFilter(this);

    // ねこアイコンを並べるためのコンテナをステータスバーに追加
    m_catContainer = new QWidget(this);
    QHBoxLayout* catLayout = new QHBoxLayout(m_catContainer);
    catLayout->setContentsMargins(4, 0, 4, 0);
    catLayout->setSpacing(2);
    m_catContainer->setLayout(catLayout);
    statusBar()->insertWidget(0, m_catContainer);
}

MainWindow::~MainWindow() {
    Logger::logInfo("TwitchFollowerChecker closing normally.");
    delete ui;
}

void MainWindow::setupUiManual() {
    // 中央ウィジェットの作成とレイアウト
    QWidget* centralWidget = new QWidget(this);
    centralWidget->setObjectName("centralWidget");
    setCentralWidget(centralWidget);
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);

    m_mainTabWidget = new QTabWidget(this);
    mainLayout->addWidget(m_mainTabWidget);

    // コーナーウィジェットの作成（❓ ヘルプ ＆ ℹ️ アバウト）
    QWidget* cornerWidget = new QWidget(this);
    QHBoxLayout* cornerLayout = new QHBoxLayout(cornerWidget);
    cornerLayout->setContentsMargins(0, 0, 10, 0);
    cornerLayout->setSpacing(5);

    QPushButton* btnHelp = new QPushButton("❓ ヘルプ", this);
    btnHelp->setCursor(Qt::PointingHandCursor);
    btnHelp->setFixedWidth(80);
    btnHelp->setStyleSheet("border: 1px solid #29292E; border-radius: 4px; padding: 4px; color: #FFFFFF; background-color: #29292E;");
    connect(btnHelp, &QPushButton::clicked, this, &MainWindow::onHelpClicked);

    QPushButton* btnAbout = new QPushButton("ℹ️ アバウト", this);
    btnAbout->setCursor(Qt::PointingHandCursor);
    btnAbout->setFixedWidth(80);
    btnAbout->setStyleSheet("border: 1px solid #29292E; border-radius: 4px; padding: 4px; color: #FFFFFF; background-color: #29292E;");
    connect(btnAbout, &QPushButton::clicked, this, &MainWindow::onAboutClicked);

    cornerLayout->addWidget(btnHelp);
    cornerLayout->addWidget(btnAbout);
    cornerWidget->setLayout(cornerLayout);

    m_mainTabWidget->setCornerWidget(cornerWidget, Qt::TopRightCorner);


    // ==========================================
    // タブ1: フォロワーチェッカー
    // ==========================================
    QWidget* checkerWidget = new QWidget(this);
    QVBoxLayout* checkerLayout = new QVBoxLayout(checkerWidget);

    // Twitch認証状況エリア
    QHBoxLayout* authLayout = new QHBoxLayout();
    lblAuthStatus = new QLabel("Twitch連携状態: 未認証", this);
    lblAuthStatus->setStyleSheet("font-weight: bold; font-size: 13px; color: #FF55FF;");
    
    btnLogin = new QPushButton("🔑 認証ログイン", this);
    btnLogin->setCursor(Qt::PointingHandCursor);
    btnLogin->setFixedWidth(130);
    connect(btnLogin, &QPushButton::clicked, this, &MainWindow::onLoginClicked);

    btnRun = new QPushButton("📥 取得", this);
    btnRun->setCursor(Qt::PointingHandCursor);
    btnRun->setFixedWidth(100);
    btnRun->setEnabled(false); // ログインするまで無効化
    connect(btnRun, &QPushButton::clicked, this, &MainWindow::onCheckDifferenceClicked);

    btnExport = new QPushButton("📥 CSV出力", this);
    btnExport->setCursor(Qt::PointingHandCursor);
    btnExport->setFixedWidth(120);
    btnExport->setEnabled(false); // データ取得するまで無効化
    connect(btnExport, &QPushButton::clicked, this, &MainWindow::onExportCsvClicked);
    
    authLayout->addWidget(lblAuthStatus);
    authLayout->addSpacing(20);
    authLayout->addWidget(btnLogin);
    authLayout->addSpacing(10);
    authLayout->addWidget(btnRun);
    authLayout->addSpacing(10);
    authLayout->addWidget(btnExport);
    authLayout->addStretch();
    checkerLayout->addLayout(authLayout);

    // サブタブ widget (フィルタ表示切り替え用、動的に設定のON/OFFで表示更新)
    m_subTabWidget = new QTabBar(this);
    m_subTabWidget->setShape(QTabBar::RoundedNorth);
    m_subTabWidget->setDrawBase(true);
    connect(m_subTabWidget, &QTabBar::currentChanged, this, &MainWindow::onSubTabChanged);
    checkerLayout->addWidget(m_subTabWidget);

    // テーブルビューとモデルの構築 (Model/View構造)
    m_tableView = new QTableView(this);
    m_tableView->setSortingEnabled(true);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    // 列幅をInteractiveにして横スクロールを有効化
    m_tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_tableView->horizontalHeader()->setMinimumSectionSize(80);
    m_tableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_tableView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    // 初期列幅を設定（ウィンドウサイズより大きい場合に横スクロールが出る）
    m_tableView->setColumnWidth(0, 140); // 表示名
    m_tableView->setColumnWidth(1, 140); // ログイン名
    m_tableView->setColumnWidth(2, 100); // 関係
    m_tableView->setColumnWidth(3, 155); // フォロー開始日
    m_tableView->setColumnWidth(4, 220); // チャンネルURL
    
    m_followerModel = new FollowerModel(this);
    
    // ソート・フィルタ用のProxyModelを構成して接続
    m_proxyModel = new QSortFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_followerModel);
    m_proxyModel->setFilterKeyColumn(2); // "関係" カラム (インデックス2) を対象にフィルタリングする
    
    m_tableView->setModel(m_proxyModel);
    // URLカラム（4列目）のクリックでブラウザ起動
    connect(m_tableView, &QTableView::clicked, this, &MainWindow::onTableCellClicked);
    m_tableView->setCursor(Qt::PointingHandCursor);
    checkerLayout->addWidget(m_tableView);

    m_mainTabWidget->addTab(checkerWidget, UIConstants::MAIN_TAB_CHECKER);

    // ==========================================
    // タブ2: システム設定
    // ==========================================
    QWidget* settingsWidget = new QWidget(this);
    QVBoxLayout* settingsLayout = new QVBoxLayout(settingsWidget);

    // 各種機能ON/OFFチェックボックス
    QGroupBox* grpFunctions = new QGroupBox("リスト表示・取得機能のON/OFF", this);
    QVBoxLayout* grpFuncLayout = new QVBoxLayout(grpFunctions);
    chkShowAll = new QCheckBox("「全て」リストの表示", this);
    chkShowMutual = new QCheckBox("「相互」リストの表示", this);
    chkShowFollowingOnly = new QCheckBox("「フォローのみ」リストの表示", this);
    chkShowFollowersOnly = new QCheckBox("「フォロワーのみ」リストの表示", this);
    
    grpFuncLayout->addWidget(chkShowAll);
    grpFuncLayout->addWidget(chkShowMutual);
    grpFuncLayout->addWidget(chkShowFollowingOnly);
    grpFuncLayout->addWidget(chkShowFollowersOnly);
    settingsLayout->addWidget(grpFunctions);

    // APIカスタムキー上書き用
    QGroupBox* grpApi = new QGroupBox("Twitch APIキー（カスタム設定）", this);
    QVBoxLayout* grpApiLayout = new QVBoxLayout(grpApi);
    
    chkOverrideApi = new QCheckBox("カスタムAPIキーを入力する", this);
    grpApiLayout->addWidget(chkOverrideApi);

    QWidget* apiContainerWidget = new QWidget(this);
    QVBoxLayout* apiContainerLayout = new QVBoxLayout(apiContainerWidget);
    apiContainerLayout->setContentsMargins(0, 0, 0, 0);

    apiContainerLayout->addWidget(new QLabel("Client ID (空なら埋め込みのデフォルトキーを使用):", this));
    txtClientId = new QLineEdit(this);
    apiContainerLayout->addWidget(txtClientId);
    
    apiContainerLayout->addWidget(new QLabel("Client Secret (空なら埋め込みのデフォルトキーを使用):", this));
    txtClientSecret = new QLineEdit(this);
    txtClientSecret->setEchoMode(QLineEdit::Password);
    apiContainerLayout->addWidget(txtClientSecret);

    grpApiLayout->addWidget(apiContainerWidget);
    settingsLayout->addWidget(grpApi);

    // デフォルトで非表示にし、チェックボックスのトグルで表示切り替え
    apiContainerWidget->setVisible(false);
    connect(chkOverrideApi, &QCheckBox::toggled, apiContainerWidget, &QWidget::setVisible);

    // デザインカスタマイズ用
    QGroupBox* grpDesign = new QGroupBox("画面のカスタマイズ設定", this);
    QVBoxLayout* grpDesignLayout = new QVBoxLayout(grpDesign);
    
    QPushButton* btnBg = new QPushButton("🖼️ 背景画像を選択...", this);
    connect(btnBg, &QPushButton::clicked, this, &MainWindow::onSelectBackgroundImage);
    grpDesignLayout->addWidget(btnBg);

    QPushButton* btnBgColor = new QPushButton("🎨 背景色を選択...", this);
    connect(btnBgColor, &QPushButton::clicked, this, &MainWindow::onSelectBackgroundColor);
    grpDesignLayout->addWidget(btnBgColor);


    QPushButton* btnColor = new QPushButton("🎨 文字色を選択...", this);
    connect(btnColor, &QPushButton::clicked, this, &MainWindow::onSelectTextColor);
    grpDesignLayout->addWidget(btnColor);

    QPushButton* btnFont = new QPushButton("🔤 フォントを変更...", this);
    connect(btnFont, &QPushButton::clicked, this, &MainWindow::onSelectFont);
    grpDesignLayout->addWidget(btnFont);

    settingsLayout->addWidget(grpDesign);

    // 保存ボタン
    QPushButton* btnSaveSettings = new QPushButton("💾 設定を保存する", this);
    btnSaveSettings->setCursor(Qt::PointingHandCursor);
    btnSaveSettings->setFixedHeight(35);
    connect(btnSaveSettings, &QPushButton::clicked, this, &MainWindow::saveUiToSettings);
    settingsLayout->addWidget(btnSaveSettings);

    settingsLayout->addStretch();
    m_mainTabWidget->addTab(settingsWidget, UIConstants::MAIN_TAB_SETTINGS);

    // 認証シグナルの接続
    connect(m_auth, &TwitchAuth::authSuccess, this, &MainWindow::onAuthSuccess);
    connect(m_auth, &TwitchAuth::authFailed, this, [this](const QString& err) {
        QMessageBox::warning(this, "認証エラー", QString("ログイン認証に失敗しました:\n%1").arg(err));
    });
}

// ---------------------------------------------------------------------------
// ステータスバー右側ラベルへの一時通知表示（改ざん判定の左側メッセージは上書きしない）
// ---------------------------------------------------------------------------
void MainWindow::showStatusInfo(const QString& message, int durationMs) {
    if (!m_statusInfoLabel) return;
    m_statusInfoLabel->setText(message);
    if (durationMs > 0) {
        QTimer::singleShot(durationMs, this, [this]() {
            if (m_statusInfoLabel) m_statusInfoLabel->clear();
        });
    }
}

void MainWindow::applyCustomStyles() {
    // 1. 文字色とフォントの適用
    QString textColorStr = m_config->get("custom_text_color", "#E1E1E6").toString();
    QFont font = m_config->getFont();
    
    if (font.family().isEmpty()) {
        font = this->font(); // デフォルトフォント
    }

    QString bgColorStr = m_config->get("custom_bg_color", "#121214").toString();
    QString bgPath = m_config->get("background_image_path", "").toString();
    bool hasBgImage = !bgPath.isEmpty() && QFile::exists(bgPath);

    QString paneBg;
    QString tableBg;
    QString tabBg;
    QString windowBgStyle;

    if (hasBgImage) {
        paneBg = "rgba(29, 29, 34, 0.75)";
        tableBg = "rgba(18, 18, 20, 0.7)";
        tabBg = "rgba(18, 18, 20, 0.7)";
        windowBgStyle = "background-color: transparent;";
    } else {
        paneBg = "#1D1D22";
        tableBg = bgColorStr;
        tabBg = bgColorStr;
        windowBgStyle = QString("background-color: %1;").arg(bgColorStr);
    }

    // Qtスタイルシートを使ったプレミアム・ダークモードと文字色の動的適用
    QString baseStyle = QString(R"(
        QMainWindow { %2 }
        QTabWidget::pane { border: 1px solid #29292E; background-color: %3; top: -1px; }
        QTabBar::tab { background-color: %4; color: #A9A9B2; border: 1px solid #29292E; padding: 10px 20px; border-top-left-radius: 4px; border-top-right-radius: 4px; }
        QTabBar::tab:selected { background-color: %3; color: #FFFFFF; border-bottom-color: %3; font-weight: bold; }
        
        QTabWidget > QWidget { background: transparent; }

        /* QGroupBox のボーダー重なりと内部コンテンツの被りを解決 */
        QGroupBox {
            border: 1px solid #29292E;
            border-radius: 6px;
            margin-top: 12px;
            padding-top: 16px;
            font-weight: bold;
            color: #FFFFFF;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 10px;
            top: 6px; /* ボーダーラインの真上に綺麗に重なるよう調整 */
            padding: 0 6px;
            background-color: %3;
            color: #E1E1E6;
        }

        /* QColorDialog の色選択ボタンが背景と同化するのを防ぐため、枠線を追加 */
        QColorDialog QPushButton {
            border: 1px solid #B0B0B0;
            border-radius: 2px;
        }

        /* 標準ダイアログ（QColorDialog/QFontDialog）への文字色漏洩を防ぐため、#centralWidget 内に限定 */
        #centralWidget QLabel { color: %1; }
        #centralWidget QCheckBox { color: %1; }
        #centralWidget QLineEdit { background-color: %4; color: #E1E1E6; border: 1px solid #29292E; border-radius: 4px; padding: 4px; }
        #centralWidget QPushButton { border: 1px solid #29292E; border-radius: 4px; padding: 5px; color: #FFFFFF; background-color: #29292E; }
        #centralWidget QPushButton:hover { background-color: #35353B; }
        #centralWidget QTableView { background-color: %5; color: %1; gridline-color: #29292E; border: 1px solid #29292E; border-radius: 4px; }
        #centralWidget QHeaderView::section { background-color: %3; color: #A9A9B2; border: 1px solid #29292E; padding: 5px; }
    )")
    .arg(textColorStr)
    .arg(windowBgStyle)
    .arg(paneBg)
    .arg(tabBg)
    .arg(tableBg);

    this->setStyleSheet(baseStyle);
    this->setFont(font);
    m_tableView->setFont(font);

    updateBackground();
}

void MainWindow::updateBackground() {
    QString bgColorStr = m_config->get("custom_bg_color", "#121214").toString();
    QColor bgColor(bgColorStr);
    QString bgPath = m_config->get("background_image_path", "").toString();

    QPalette palette = this->palette();
    if (!bgPath.isEmpty() && QFile::exists(bgPath)) {
        QPixmap pixmap(bgPath);
        if (!pixmap.isNull()) {
            QSize size = this->size();
            if (size.width() <= 0 || size.height() <= 0) {
                size = QSize(900, 650);
            }
            QPixmap bgPixmap(size);
            bgPixmap.fill(bgColor);

            QPainter painter(&bgPixmap);
            QPixmap scaledPixmap = pixmap.scaled(size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            // センタリングして描画
            int x = (size.width() - scaledPixmap.width()) / 2;
            int y = (size.height() - scaledPixmap.height()) / 2;
            painter.drawPixmap(x, y, scaledPixmap);
            painter.end();

            palette.setBrush(QPalette::Window, QBrush(bgPixmap));
            this->setPalette(palette);
            this->setAutoFillBackground(true);
            Logger::logInfo(QString("Background image overlaid on background color (%1) applied successfully: %2").arg(bgColorStr, bgPath));
        } else {
            palette.setColor(QPalette::Window, bgColor);
            this->setPalette(palette);
            this->setAutoFillBackground(true);
        }
    } else {
        palette.setColor(QPalette::Window, bgColor);
        this->setPalette(palette);
        this->setAutoFillBackground(true);
    }
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    updateBackground();
}

void MainWindow::onSelectBackgroundColor() {
    QColor color = QColorDialog::getColor(QColor(m_config->get("custom_bg_color", "#121214").toString()), this, "🎨 背景色を選択");
    if (color.isValid()) {
        m_config->set("custom_bg_color", color.name());
        applyCustomStyles();
    }
}

void MainWindow::onHelpClicked() {
    QDesktopServices::openUrl(QUrl("https://github.com/BLUE000/TwitchFollowerChecker"));
    Logger::logInfo("Opening help URL (GitHub README).");
}

void MainWindow::onAboutClicked() {
    QString aboutText = QString(R"(
        <h3>TwitchFollowerChecker</h3>
        <p>バージョン: %1</p>
        <p>Twitchの相互フォロー・片思い関係をチェックするツールです。</p>
        <hr/>
        <h4>ライセンス表記</h4>
        
        <h5>1. TwitchFollowerChecker (MIT License)</h5>
        <p>Copyright (c) 2026 BLUE000<br/>
        Licensed under the MIT License.</p>
        
        <h5>2. TransCipher Library</h5>
        <p>This software uses TransCipher library.<br/>
        Copyright (c) 2026 BLUE000. All rights reserved.</p>
        
        <h5>3. Qt6 (LGPL v3)</h5>
        <p>This software uses Qt6 under the GNU Lesser General Public License (LGPL v3).<br/>
        For details, please visit <a href="https://www.qt.io/">https://www.qt.io/</a>.</p>
    )").arg(APP_VERSION_STRING);

    QMessageBox::about(this, "TwitchFollowerChecker について", aboutText);
}

void MainWindow::loadSettingsToUi() {
    // デフォルト値: 「フォローのみ」と「相互」のON、「全て」と「フォロワーのみ」はOFF
    chkShowAll->setChecked(m_config->get("show_all", false).toBool());
    chkShowMutual->setChecked(m_config->get("show_mutual", true).toBool());
    chkShowFollowingOnly->setChecked(m_config->get("show_following_only", true).toBool());
    chkShowFollowersOnly->setChecked(m_config->get("show_followers_only", false).toBool());
    
    QString customClientId = m_config->get("custom_client_id", "").toString();
    QString customClientSecret = m_config->get("custom_client_secret", "").toString();
    
    txtClientId->setText(customClientId);
    txtClientSecret->setText(customClientSecret);

    if (chkOverrideApi) {
        chkOverrideApi->setChecked(!customClientId.isEmpty());
    }

    updateSubTabsVisibility();
}

void MainWindow::saveUiToSettings() {
    m_config->set("show_all", chkShowAll->isChecked());
    m_config->set("show_mutual", chkShowMutual->isChecked());
    m_config->set("show_following_only", chkShowFollowingOnly->isChecked());
    m_config->set("show_followers_only", chkShowFollowersOnly->isChecked());
    
    if (chkOverrideApi && chkOverrideApi->isChecked()) {
        m_config->set("custom_client_id", txtClientId->text().trimmed());
        m_config->set("custom_client_secret", txtClientSecret->text().trimmed());
    } else {
        if (txtClientId) txtClientId->clear();
        if (txtClientSecret) txtClientSecret->clear();
        m_config->set("custom_client_id", "");
        m_config->set("custom_client_secret", "");
    }
    
    if (m_config->save()) {
        updateSubTabsVisibility();
        showStatusInfo("✅ システム設定を正常に暗号化保存しました。");
    } else {
        showStatusInfo("⚠️ 設定の保存に失敗しました。");
    }
}

void MainWindow::onLoginClicked() {
    // セッション継続中は再認証フローを走らせず、ボタンの一時無効化→有効化（ちらつきアピール）のみ行う
    if (!m_twitchToken.isEmpty()) {
        btnLogin->setEnabled(false);
        QTimer::singleShot(300, this, [this]() {
            btnLogin->setEnabled(true);
        });
        return;
    }
    btnLogin->setEnabled(false);
    lblAuthStatus->setText("Twitch連携状態: ブラウザ待機中...");
    m_auth->startAuthFlow();
}

void MainWindow::onAuthSuccess(const QString& token) {
    // ヌル文字が絶対に混入しないようにサニタイズ（安全対策の二重防御）
    QString sanitizedToken = token;
    sanitizedToken.remove(QChar('\0'));
    sanitizedToken = sanitizedToken.trimmed();

    if (m_twitchToken == sanitizedToken) {
        return; // 既に同じトークンで処理済みの場合は重複ダイアログの表示をスキップ
    }
    m_twitchToken = sanitizedToken;

    lblAuthStatus->setText("Twitch連携状態: 認証済み ✅");
    lblAuthStatus->setStyleSheet("font-weight: bold; font-size: 13px; color: #55FF55;");
    
    btnLogin->setEnabled(true);
    btnRun->setEnabled(true);

    showStatusInfo("✅ Twitch連携ログインに成功しました！自動バックグラウンド取得を開始します。");
    Logger::logInfo("OAuth Session established.");

    // 初回の自動取得（裏で実行）
    fetchData(true);

    // 5分おき（300,000ミリ秒）に自動取得タイマーを起動
    m_autoFetchTimer->start(300000);
}

void MainWindow::onCheckDifferenceClicked() {
    fetchData(false);
}

#include <QDir>
#include <QFileInfo>

static bool autoSaveCsv(const QString& filepath, const QList<FollowerItem>& list) {
    QFileInfo fileInfo(filepath);
    QDir().mkpath(fileInfo.absolutePath());

    QFile file(filepath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        Logger::logError(QString("Auto Export CSV: Failed to open file for writing: %1").arg(filepath));
        return false;
    }

    file.write("\xEF\xBB\xBF"); // UTF-8 BOM

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    out << "表示名,ログイン名,関係,フォロー開始日,チャンネルURL\n";

    for (const auto& item : list) {
        QString dateStr = item.followedAt.isValid() ? item.followedAt.toString("yyyy-MM-dd hh:mm:ss") : "-";
        out << "\"" << item.displayName << "\","
            << "\"" << item.loginName << "\","
            << "\"" << item.relationship << "\","
            << "\"" << dateStr << "\","
            << "\"" << item.channelUrl << "\"\n";
    }

    file.close();
    Logger::logInfo(QString("Auto Export CSV: Successfully saved %1 items to %2").arg(list.size()).arg(filepath));
    return true;
}

void MainWindow::fetchData(bool isSilent) {
    if (m_twitchToken.isEmpty()) {
        return;
    }

    if (!isSilent) {
        btnRun->setEnabled(false);
        btnRun->setText("処理中...");
    }
    Logger::logInfo(QString("Fetching Twitch follower difference data (silent: %1)...").arg(isSilent ? "true" : "false"));

    QList<FollowerItem> following = m_apiClient->fetchFollowing(m_twitchToken);
    QList<FollowerItem> followers = m_apiClient->fetchFollowers(m_twitchToken);

    QList<FollowerItem> rawList = m_apiClient->compareLists(following, followers);
    autoSaveCsv("logs/merged_all_relations.csv", rawList);

    QList<FollowerItem> resultList;

    for (const auto& item : rawList) {
        if (item.relationship == "相互") {
            if (chkShowMutual && chkShowMutual->isChecked()) {
                resultList.append(item);
            }
        } else if (item.relationship == "フォローのみ") {
            if (chkShowFollowingOnly && chkShowFollowingOnly->isChecked()) {
                resultList.append(item);
            }
        } else if (item.relationship == "フォロワーのみ") {
            if (chkShowFollowersOnly && chkShowFollowersOnly->isChecked()) {
                resultList.append(item);
            }
        }
    }

    autoSaveCsv("logs/displayed_list.csv", resultList);

    // データの変化があるかどうかを検証
    const QList<FollowerItem>& currentList = m_followerModel->getFollowersList();
    bool isChanged = (currentList != resultList);

    if (isChanged) {
        // テーブルビューへバインド
        m_followerModel->setFollowers(resultList);
        
        // データ更新後に列幅をコンテンツに合わせて自動調整
        m_tableView->resizeColumnsToContents();

        // 各サブタブの件数を更新
        updateSubTabCounts();

        Logger::logInfo("Follower data changed. List view updated.");
        if (isSilent) {
            showStatusInfo(QString("🔄 【自動更新】データに変化がありました。合計: %1 件。").arg(resultList.size()));
        }
    } else {
        Logger::logInfo("Follower data has no changes. List view update skipped.");
    }

    // CSV出力ボタンの状態は最新件数に同期
    btnExport->setEnabled(resultList.size() > 0);

    if (!isSilent) {
        btnRun->setEnabled(true);
        btnRun->setText("📥 取得");
        if (isChanged) {
            showStatusInfo(QString("✅ データの取得が完了しました！合計: %1 件。").arg(resultList.size()));
        } else {
            showStatusInfo("✅ データの取得が完了しました（変更なし）。");
        }
    }
}

void MainWindow::onAutoFetchTimeout() {
    Logger::logInfo("Auto fetch timer triggered.");
    fetchData(true);
}

void MainWindow::onExportCsvClicked() {
    QString savePath = QFileDialog::getSaveFileName(this, "CSVを出力", "twitch_followers.csv", "CSV Files (*.csv)");
    if (savePath.isEmpty()) return;

    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "エクスポート失敗", "指定されたファイルを開けませんでした。");
        Logger::logError(QString("Failed to open export CSV file: %1").arg(savePath));
        return;
    }

    // 1. Windows Excel文字化け対策: UTF-8 BOM (\xEF\xBB\xBF) を先頭に書き込む
    file.write("\xEF\xBB\xBF");

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8); // UTF-8指定

    // 2. ヘッダー出力（カラム順: 表示名→ログイン名→関係→フォロー開始日→URL）
    out << UIConstants::HEADER_DISPLAY_NAME << ","
        << UIConstants::HEADER_LOGIN_NAME << ","
        << UIConstants::HEADER_RELATIONSHIP << ","
        << UIConstants::HEADER_FOLLOW_DATE << ","
        << UIConstants::HEADER_CHANNEL_URL << "\n";

    // 3. 行データ出力 (安全なQList走査、インデックスアクセス排除)
    const QList<FollowerItem>& list = m_followerModel->getFollowersList();
    for (const auto& item : list) {
        QString dateStr = item.followedAt.isValid() ? item.followedAt.toString("yyyy-MM-dd hh:mm:ss") : "-";
        out << "\"" << item.displayName << "\","
            << "\"" << item.loginName << "\","
            << "\"" << item.relationship << "\","
            << "\"" << dateStr << "\","
            << "\"" << item.channelUrl << "\"\n";
    }

    file.close();
    showStatusInfo("✅ BOM付きUTF-8 CSVを正常に出力しました！Excelで直接文字化けなく閲覧可能です。");
    Logger::logInfo(QString("Successfully exported %1 rows to CSV with UTF-8 BOM.").arg(list.size()));
}

void MainWindow::onSelectBackgroundImage() {
    QString path = QFileDialog::getOpenFileName(this, "🖼️ 背景画像を選択", "", "Images (*.png *.jpg *.jpeg)");
    if (!path.isEmpty()) {
        m_config->set("background_image_path", path);
        applyCustomStyles();
    }
}

void MainWindow::onSelectTextColor() {
    QColor color = QColorDialog::getColor(QColor(m_config->get("custom_text_color", "#E1E1E6").toString()), this, "🎨 文字色を選択");
    if (color.isValid()) {
        m_config->set("custom_text_color", color.name());
        applyCustomStyles();
    }
}

void MainWindow::onSelectFont() {
    bool ok = false;
    QFont currentFont = m_config->getFont();
    QFont font = QFontDialog::getFont(&ok, currentFont, this, "🔤 フォントを変更");
    if (ok) {
        m_config->setFont(font);
        applyCustomStyles();
    }
}

void MainWindow::onSubTabChanged(int index) {
    if (!m_proxyModel) return;
    if (index < 0 || index >= m_subTabWidget->count()) return;

    // タブ名に件数が付加されている場合もあるため、ベース名でプレフィックスマッチする
    QString tabText = m_subTabWidget->tabText(index);

    if (tabText.startsWith("全て")) {
        m_proxyModel->setFilterFixedString("");
    } else if (tabText.startsWith("相互")) {
        m_proxyModel->setFilterFixedString("相互");
    } else if (tabText.startsWith("フォローのみ")) {
        m_proxyModel->setFilterFixedString("フォローのみ");
    } else if (tabText.startsWith("フォロワーのみ")) {
        m_proxyModel->setFilterFixedString("フォロワーのみ");
    }
}

void MainWindow::updateSubTabsVisibility() {
    if (!m_subTabWidget) return;

    // 現在のタブ名を一時保存して再選択できるようにする
    QString currentText = "";
    if (m_subTabWidget->count() > 0 && m_subTabWidget->currentIndex() >= 0) {
        currentText = m_subTabWidget->tabText(m_subTabWidget->currentIndex());
    }

    // タブ再構築中の不要なインデックス変更シグナルを遮断
    m_subTabWidget->blockSignals(true);

    // すべてのタブをクリア
    while (m_subTabWidget->count() > 0) {
        m_subTabWidget->removeTab(0);
    }

    int indexToSelect = 0;
    int currentNewIndex = 0;

    if (chkShowAll && chkShowAll->isChecked()) {
        m_subTabWidget->addTab("全て");
        if (currentText.startsWith("全て")) indexToSelect = currentNewIndex;
        currentNewIndex++;
    }
    if (chkShowMutual && chkShowMutual->isChecked()) {
        m_subTabWidget->addTab("相互");
        if (currentText.startsWith("相互")) indexToSelect = currentNewIndex;
        currentNewIndex++;
    }
    if (chkShowFollowingOnly && chkShowFollowingOnly->isChecked()) {
        m_subTabWidget->addTab("フォローのみ");
        if (currentText.startsWith("フォローのみ")) indexToSelect = currentNewIndex;
        currentNewIndex++;
    }
    if (chkShowFollowersOnly && chkShowFollowersOnly->isChecked()) {
        m_subTabWidget->addTab("フォロワーのみ");
        if (currentText.startsWith("フォロワーのみ")) indexToSelect = currentNewIndex;
        currentNewIndex++;
    }

    // 既にデータが取得されている場合は件数を再付与する
    if (m_followerModel && !m_followerModel->getFollowersList().isEmpty()) {
        updateSubTabCounts();
    }

    m_subTabWidget->blockSignals(false);

    if (m_subTabWidget->count() > 0) {
        m_subTabWidget->setCurrentIndex(qMin(indexToSelect, m_subTabWidget->count() - 1));
        // 手動でフィルターの更新をトリガー
        onSubTabChanged(m_subTabWidget->currentIndex());
    }
}

// ---------------------------------------------------------------------------
// URLカラムクリック：プロキシモデル経由のインデックスをソースインデックスに変換してURL取得
// ---------------------------------------------------------------------------
void MainWindow::onTableCellClicked(const QModelIndex& proxyIndex) {
    if (!proxyIndex.isValid()) return;
    // URLカラムは列インデックス4
    if (proxyIndex.column() != 4) return;

    // ProxyIndex→SourceIndexの変換により正しいソースインデックスを取得
    QModelIndex sourceIndex = m_proxyModel->mapToSource(proxyIndex);
    const QList<FollowerItem>& list = m_followerModel->getFollowersList();
    if (sourceIndex.row() < 0 || sourceIndex.row() >= list.size()) return;

    QString url = list.at(sourceIndex.row()).channelUrl;
    if (!url.isEmpty()) {
        QDesktopServices::openUrl(QUrl(url));
        Logger::logInfo(QString("Opening channel URL: %1").arg(url));
    }
}

// ---------------------------------------------------------------------------
// 各サブタブのラベルに件数を付加する
// ---------------------------------------------------------------------------
void MainWindow::updateSubTabCounts() {
    if (!m_subTabWidget || !m_followerModel) return;

    // 各カテゴリの件数を集計
    int countAll = 0, countMutual = 0, countFollowingOnly = 0, countFollowersOnly = 0;
    const QList<FollowerItem>& list = m_followerModel->getFollowersList();
    for (const auto& item : list) {
        if (item.relationship == "相互") {
            countMutual++;
        } else if (item.relationship == "フォローのみ") {
            countFollowingOnly++;
        } else if (item.relationship == "フォロワーのみ") {
            countFollowersOnly++;
        }
    }
    countAll = list.size(); // 「全て」は全件数

    // 各タブのラベルを「全て (6)」形式で更新
    for (int i = 0; i < m_subTabWidget->count(); ++i) {
        QString tabText = m_subTabWidget->tabText(i);
        if (tabText.startsWith("全て")) {
            m_subTabWidget->setTabText(i, QString("全て (%1)").arg(countAll));
        } else if (tabText.startsWith("相互")) {
            m_subTabWidget->setTabText(i, QString("相互 (%1)").arg(countMutual));
        } else if (tabText.startsWith("フォローのみ")) {
            m_subTabWidget->setTabText(i, QString("フォローのみ (%1)").arg(countFollowingOnly));
        } else if (tabText.startsWith("フォロワーのみ")) {
            m_subTabWidget->setTabText(i, QString("フォロワーのみ (%1)").arg(countFollowersOnly));
        }
    }
}

// ---------------------------------------------------------------------------
// イースターエッグ：無操作監視およびねこの出現トリックの実装
// ---------------------------------------------------------------------------

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    // マウス移動、クリック、キー入力、スクロール等を検知した場合はねこを消去してリセット
    if (event->type() == QEvent::MouseMove ||
        event->type() == QEvent::MouseButtonPress ||
        event->type() == QEvent::MouseButtonRelease ||
        event->type() == QEvent::KeyPress ||
        event->type() == QEvent::KeyRelease ||
        event->type() == QEvent::Wheel) {
        clearCatIcons();
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::onIdleTimeout() {
    m_idleSeconds++;

    // 5分（300秒）無操作で1匹目、その後1分（60秒）無操作が継続するごとに1匹追加
    if (m_idleSeconds >= 300) {
        if ((m_idleSeconds - 300) % 60 == 0) {
            addCatIcon();
        }
    }
}

void MainWindow::addCatIcon() {
    if (!m_catContainer || !m_catContainer->layout()) return;

    QPixmap pixmap;
    // ヘッダー埋め込みのバイナリ配列から安全に読み込み
    if (pixmap.loadFromData(cat_png, cat_png_len)) {
        // ステータスバー（2px高く拡張済み）に綺麗に収まる高さ20pxに縮小
        QPixmap scaled = pixmap.scaledToHeight(20, Qt::SmoothTransformation);

        QLabel* label = new QLabel(m_catContainer);
        label->setPixmap(scaled);
        m_catContainer->layout()->addWidget(label);

        Logger::logInfo(QString("Easter Egg: A cat appeared! (Total: %1)").arg(m_catContainer->layout()->count()));
    }
}

void MainWindow::clearCatIcons() {
    m_idleSeconds = 0; // タイマー秒数をゼロリセット

    if (!m_catContainer || !m_catContainer->layout()) return;

    // コンテナ内のねこアイコン（QLabel）をすべて安全に消去
    QLayoutItem* item;
    bool hasCats = false;
    while ((item = m_catContainer->layout()->takeAt(0)) != nullptr) {
        if (item->widget()) {
            hasCats = true;
            item->widget()->deleteLater();
        }
        delete item;
    }

    if (hasCats) {
        Logger::logInfo("Easter Egg: Cats cleared due to user activity.");
    }
}
