#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "logger.h"
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

#ifndef APP_VERSION_STRING
#define APP_VERSION_STRING "1.0.0"
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
QCheckBox* chkFollowing = nullptr;
QCheckBox* chkFollowers = nullptr;
QCheckBox* chkCompare = nullptr;
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
{
    ui->setupUi(this);

    // 1. バージョンおよびカスタムビルド判定に応じたウィンドウタイトルとステータスバー設定
    if (BUILD_IS_CUSTOMIZED) {
        setWindowTitle(QString("TwitchFollowerChecker - Ver %1 (Custom Build)").arg(APP_VERSION_STRING));
        statusBar()->showMessage("© BLUE000 (Original Creator)");
        statusBar()->setStyleSheet("color: #888888; background-color: #121214;");
    } else {
        setWindowTitle(QString("TwitchFollowerChecker - Ver %1").arg(APP_VERSION_STRING));
    }

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
}

MainWindow::~MainWindow() {
    Logger::logInfo("TwitchFollowerChecker closing normally.");
    delete ui;
}

void MainWindow::setupUiManual() {
    // 中央ウィジェットの作成とレイアウト
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);

    m_mainTabWidget = new QTabWidget(this);
    mainLayout->addWidget(m_mainTabWidget);

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
    
    authLayout->addWidget(lblAuthStatus);
    authLayout->addSpacing(20);
    authLayout->addWidget(btnLogin);
    authLayout->addStretch();
    checkerLayout->addLayout(authLayout);

    // サブタブ widget (フィルタ表示切り替え用)
    m_subTabWidget = new QTabWidget(this);
    m_subTabWidget->addTab(new QWidget(), UIConstants::SUB_TAB_ALL);
    m_subTabWidget->addTab(new QWidget(), UIConstants::SUB_TAB_MUTUAL);
    m_subTabWidget->addTab(new QWidget(), UIConstants::SUB_TAB_FOLLOWING);
    m_subTabWidget->addTab(new QWidget(), UIConstants::SUB_TAB_FOLLOWERS);
    connect(m_subTabWidget, &QTabWidget::currentChanged, this, &MainWindow::onSubTabChanged);
    checkerLayout->addWidget(m_subTabWidget);

    // テーブルビューとモデルの構築 (Model/View構造)
    m_tableView = new QTableView(this);
    m_tableView->setSortingEnabled(true);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    
    m_followerModel = new FollowerModel(this);
    
    // ソート・フィルタ用のProxyModelを構成して接続
    m_proxyModel = new QSortFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_followerModel);
    m_proxyModel->setFilterKeyColumn(2); // "関係" カラム (インデックス2) を対象にフィルタリングする
    
    m_tableView->setModel(m_proxyModel);
    checkerLayout->addWidget(m_tableView);

    // 実行・CSV出力ボタン
    QHBoxLayout* bottomBtnLayout = new QHBoxLayout();
    btnRun = new QPushButton("📊 差分チェック実行", this);
    btnRun->setCursor(Qt::PointingHandCursor);
    btnRun->setFixedHeight(35);
    btnRun->setEnabled(false); // ログインするまで無効化
    connect(btnRun, &QPushButton::clicked, this, &MainWindow::onCheckDifferenceClicked);

    btnExport = new QPushButton("📥 CSV出力", this);
    btnExport->setCursor(Qt::PointingHandCursor);
    btnExport->setFixedHeight(35);
    btnExport->setEnabled(false); // データ取得するまで無効化
    connect(btnExport, &QPushButton::clicked, this, &MainWindow::onExportCsvClicked);

    bottomBtnLayout->addWidget(btnRun);
    bottomBtnLayout->addWidget(btnExport);
    checkerLayout->addLayout(bottomBtnLayout);

    m_mainTabWidget->addTab(checkerWidget, UIConstants::MAIN_TAB_CHECKER);

    // ==========================================
    // タブ2: システム設定
    // ==========================================
    QWidget* settingsWidget = new QWidget(this);
    QVBoxLayout* settingsLayout = new QVBoxLayout(settingsWidget);

    // 各種機能ON/OFFチェックボックス
    QGroupBox* grpFunctions = new QGroupBox("取得・比較機能のON/OFF", this);
    QVBoxLayout* grpFuncLayout = new QVBoxLayout(grpFunctions);
    chkFollowing = new QCheckBox("フォローしている人の一覧を取得", this);
    chkFollowers = new QCheckBox("フォロワーの一覧を取得", this);
    chkCompare = new QCheckBox("フォロー vs フォロワーの比較を実行", this);
    
    grpFuncLayout->addWidget(chkFollowing);
    grpFuncLayout->addWidget(chkFollowers);
    grpFuncLayout->addWidget(chkCompare);
    settingsLayout->addWidget(grpFunctions);

    // APIカスタムキー上書き用
    QGroupBox* grpApi = new QGroupBox("Twitch APIキー（カスタム設定）", this);
    QVBoxLayout* grpApiLayout = new QVBoxLayout(grpApi);
    grpApiLayout->addWidget(new QLabel("Client ID (空なら埋め込みのデフォルトキーを使用):", this));
    txtClientId = new QLineEdit(this);
    grpApiLayout->addWidget(txtClientId);
    grpApiLayout->addWidget(new QLabel("Client Secret (空なら埋め込みのデフォルトキーを使用):", this));
    txtClientSecret = new QLineEdit(this);
    txtClientSecret->setEchoMode(QLineEdit::Password);
    grpApiLayout->addWidget(txtClientSecret);
    settingsLayout->addWidget(grpApi);

    // デザインカスタマイズ用
    QGroupBox* grpDesign = new QGroupBox("画面のカスタマイズ設定", this);
    QVBoxLayout* grpDesignLayout = new QVBoxLayout(grpDesign);
    
    QPushButton* btnBg = new QPushButton("🖼️ 背景画像を選択...", this);
    connect(btnBg, &QPushButton::clicked, this, &MainWindow::onSelectBackgroundImage);
    grpDesignLayout->addWidget(btnBg);

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

void MainWindow::applyCustomStyles() {
    // 1. 文字色とフォントの適用
    QString textColorStr = m_config->get("custom_text_color", "#E1E1E6").toString();
    QFont font = m_config->getFont();
    
    if (font.family().isEmpty()) {
        font = this->font(); // デフォルトフォント
    }

    // Qtスタイルシートを使ったプレミアム・ダークモードと文字色の動的適用
    QString baseStyle = QString(R"(
        QMainWindow { background-color: #121214; }
        QTabWidget::pane { border: 1px solid #29292E; background-color: #1D1D22; top: -1px; }
        QTabBar::tab { background-color: #121214; color: #A9A9B2; border: 1px solid #29292E; padding: 10px 20px; border-top-left-radius: 4px; border-top-right-radius: 4px; }
        QTabBar::tab:selected { background-color: #1D1D22; color: #FFFFFF; border-bottom-color: #1D1D22; font-weight: bold; }
        QGroupBox { border: 1px solid #29292E; border-radius: 6px; margin-top: 12px; font-weight: bold; color: #FFFFFF; }
        QLabel { color: %1; }
        QCheckBox { color: %1; }
        QLineEdit { background-color: #121214; color: #E1E1E6; border: 1px solid #29292E; border-radius: 4px; padding: 4px; }
        QPushButton { border: 1px solid #29292E; border-radius: 4px; padding: 5px; color: #FFFFFF; background-color: #29292E; }
        QPushButton:hover { background-color: #35353B; }
        QTableView { background-color: #121214; color: %1; gridline-color: #29292E; border: 1px solid #29292E; border-radius: 4px; }
        QHeaderView::section { background-color: #1D1D22; color: #A9A9B2; border: 1px solid #29292E; padding: 5px; }
    )").arg(textColorStr);

    this->setStyleSheet(baseStyle);
    this->setFont(font);
    m_tableView->setFont(font);

    // 2. 背景画像の適用
    QString bgPath = m_config->get("background_image_path", "").toString();
    if (!bgPath.isEmpty() && QFile::exists(bgPath)) {
        // ウインドウの背景として画像を設定
        QPalette palette = this->palette();
        QPixmap pixmap(bgPath);
        if (!pixmap.isNull()) {
            QPixmap scaledPixmap = pixmap.scaled(this->size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            palette.setBrush(QPalette::Window, QBrush(scaledPixmap));
            this->setPalette(palette);
            this->setAutoFillBackground(true);
            Logger::logInfo(QString("Background image applied successfully: %1").arg(bgPath));
        }
    }
}

void MainWindow::loadSettingsToUi() {
    chkFollowing->setChecked(m_config->get("get_following", true).toBool());
    chkFollowers->setChecked(m_config->get("get_followers", true).toBool());
    chkCompare->setChecked(m_config->get("compare_lists", true).toBool());
    txtClientId->setText(m_config->get("custom_client_id", "").toString());
    txtClientSecret->setText(m_config->get("custom_client_secret", "").toString());
}

void MainWindow::saveUiToSettings() {
    m_config->set("get_following", chkFollowing->isChecked());
    m_config->set("get_followers", chkFollowers->isChecked());
    m_config->set("compare_lists", chkCompare->isChecked());
    m_config->set("custom_client_id", txtClientId->text());
    m_config->set("custom_client_secret", txtClientSecret->text());
    
    if (m_config->save()) {
        QMessageBox::information(this, "保存完了", "システム設定を正常に暗号化保存しました。");
    } else {
        QMessageBox::warning(this, "保存失敗", "設定の保存に失敗しました。");
    }
}

void MainWindow::onLoginClicked() {
    btnLogin->setEnabled(false);
    lblAuthStatus->setText("Twitch連携状態: ブラウザ待機中...");
    m_auth->startAuthFlow();
}

void MainWindow::onAuthSuccess(const QString& token) {
    // ヌル文字が絶対に混入しないようにサニタイズ（安全対策の二重防御）
    m_twitchToken = token;
    m_twitchToken.remove(QChar('\0'));
    m_twitchToken = m_twitchToken.trimmed();

    lblAuthStatus->setText("Twitch連携状態: 認証済み ✅");
    lblAuthStatus->setStyleSheet("font-weight: bold; font-size: 13px; color: #55FF55;");
    
    btnLogin->setEnabled(true);
    btnRun->setEnabled(true);

    QMessageBox::information(this, "ログイン成功", "Twitch連携ログインに成功しました！\n差分チェックが実行可能です。");
    Logger::logInfo("OAuth Session established.");
}

void MainWindow::onCheckDifferenceClicked() {
    btnRun->setEnabled(false);
    btnRun->setText("処理中...");
    Logger::logInfo("Fetching Twitch follower difference data...");

    QList<FollowerItem> following;
    QList<FollowerItem> followers;

    if (chkFollowing->isChecked()) {
        following = m_apiClient->fetchFollowing(m_twitchToken);
    }
    
    if (chkFollowers->isChecked()) {
        followers = m_apiClient->fetchFollowers(m_twitchToken);
    }

    QList<FollowerItem> resultList;
    if (chkCompare->isChecked() && chkFollowing->isChecked() && chkFollowers->isChecked()) {
        resultList = m_apiClient->compareLists(following, followers);
    } else {
        // 比較しない場合は両方を合算して表示
        for (const auto& item : following) {
            FollowerItem it = item;
            it.relationship = "フォローのみ";
            resultList.append(it);
        }
        for (const auto& item : followers) {
            // 被りを除外
            bool exists = false;
            for (const auto& existing : resultList) {
                if (existing.loginName == item.loginName) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                FollowerItem it = item;
                it.relationship = "フォロワーのみ";
                resultList.append(it);
            }
        }
    }

    // テーブルビューへバインド
    m_followerModel->setFollowers(resultList);
    
    btnRun->setEnabled(true);
    btnRun->setText("📊 差分チェック実行");
    btnExport->setEnabled(resultList.size() > 0);

    QMessageBox::information(this, "取得完了", QString("差分チェックが完了しました！\n合計: %1 件を取得。").arg(resultList.size()));
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

    // 2. ヘッダー出力
    out << UIConstants::HEADER_LOGIN_NAME << ","
        << UIConstants::HEADER_DISPLAY_NAME << ","
        << UIConstants::HEADER_RELATIONSHIP << ","
        << UIConstants::HEADER_FOLLOW_DATE << ","
        << UIConstants::HEADER_CHANNEL_URL << "\n";

    // 3. 行データ出力 (安全なQList走査、インデックスアクセス排除)
    const QList<FollowerItem>& list = m_followerModel->getFollowersList();
    for (const auto& item : list) {
        QString dateStr = item.followedAt.isValid() ? item.followedAt.toString("yyyy-MM-dd hh:mm:ss") : "-";
        out << "\"" << item.loginName << "\","
            << "\"" << item.displayName << "\","
            << "\"" << item.relationship << "\","
            << "\"" << dateStr << "\","
            << "\"" << item.channelUrl << "\"\n";
    }

    file.close();
    QMessageBox::information(this, "出力完了", "BOM付きUTF-8 CSVファイルを正常に出力しました！\nExcelで直接文字化けなく閲覧可能です。");
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

    // 定数化したサブタブの対応文字を取得してフィルタリング
    switch (index) {
        case 0: // 全て
            m_proxyModel->setFilterFixedString("");
            break;
        case 1: // 相互
            m_proxyModel->setFilterFixedString(UIConstants::SUB_TAB_MUTUAL);
            break;
        case 2: // フォローのみ
            m_proxyModel->setFilterFixedString(UIConstants::SUB_TAB_FOLLOWING);
            break;
        case 3: // フォロワーのみ
            m_proxyModel->setFilterFixedString("フォロワーのみ");
            break;
    }
}
