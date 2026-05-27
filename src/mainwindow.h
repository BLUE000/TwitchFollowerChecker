#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include <QTabBar>
#include <QTableView>
#include <QSortFilterProxyModel>
#include <QLabel>
#include "configmanager.h"
#include "followermodel.h"
#include "twitchauth.h"
#include "twitchapiclient.h"
#include <QTimer>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

/**
 * @brief TwitchFollowerChecker のメインウィンドウUIコントローラクラス
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    /**
     * @brief Twitch認可画面を開くログインアクション
     */
    void onLoginClicked();

    /**
     * @brief Twitch認可が正常に成功した際のスロット
     * @param token 取得されたアクセストークン
     */
    void onAuthSuccess(const QString& token);

    /**
     * @brief 差分チェックの実行処理を行うスロット
     */
    void onCheckDifferenceClicked();

    /**
     * @brief 画面に表示されているリストをCSVとしてエクスポートするスロット
     */
    void onExportCsvClicked();

    /**
     * @brief システム設定画面でカスタム背景画像を選択するスロット
     */
    void onSelectBackgroundImage();

    /**
     * @brief システム設定画面で文字色を選択するスロット
     */
    void onSelectTextColor();

    /**
     * @brief システム設定画面でフォントを選択するスロット
     */
    void onSelectFont();

    /**
     * @brief サブタブが切り替わった際にテーブルのProxyModelフィルターを動的に更新するスロット
     * @param index 切り替わったタブのインデックス
     */
    void onSubTabChanged(int index);

    /**
     * @brief システム設定画面で背景色を選択するスロット
     */
    void onSelectBackgroundColor();

    /**
     * @brief ヘルプボタンをクリックしたときにGitHubのREADMEを表示するスロット
     */
    void onHelpClicked();

    /**
     * @brief アバウトボタンをクリックしたときにライセンス等の情報を表示するスロット
     */
    void onAboutClicked();

    /**
     * @brief テーブルのセルがクリックされた際に、URLカラムであればブラウザを起動するスロット
     * @param index クリックされたセルのインデックス
     */
    void onTableCellClicked(const QModelIndex& index);

    /**
     * @brief 定期的な自動取得タイマーがタイムアウトした際のスロット
     */
    void onAutoFetchTimeout();


protected:
    /**
     * @brief ウィンドウリサイズ時に背景画像を綺麗に拡大縮小するイベント
     */
    void resizeEvent(QResizeEvent* event) override;

    /**
     * @brief アプリケーション全体の操作イベントをフックして無操作検出を行うイベントフィルタ
     */
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /**
     * @brief UIコントロール群の初期組み立てとレイアウト構築
     */
    void setupUiManual();

    /**
     * @brief 適用されている文字色、背景画像、カスタムフォントをGUI全体に動的に反映する
     */
    void applyCustomStyles();

    /**
     * @brief 背景画像と背景色を設定から読み込み、ウィンドウに適用する
     */
    void updateBackground();


    /**
     * @brief 設定情報（ON/OFF状態やカスタムAPI情報）のロードと設定画面コントロールへのバインド
     */
    void loadSettingsToUi();

    /**
     * @brief 設定画面コントロールの変更値をConfigManagerを通じて保存する
     */
    void saveUiToSettings();

    /**
     * @brief 設定のON/OFFに基づいて、フォロワーチェッカー画面のサブタブの表示状態を更新する
     */
    void updateSubTabsVisibility();

    /**
     * @brief 各サブタブに表示件数を付加したラベルで更新する
     */
    void updateSubTabCounts();

    /**
     * @brief ステータスバー右側のインフォラベルに一時メッセージを表示する
     *        改ざん防止用の左側メッセージは上書きしない
     * @param message 表示するメッセージ
     * @param durationMs 表示持続時間(ミリ秒)。0 の場合は消去しない
     */
    void showStatusInfo(const QString& message, int durationMs = 5000);

    /**
     * @brief Twitchからフォロワー差分データを取得し、変更があればリストを更新する
     * @param isSilent true の場合、UIのボタン変更や開始通知を無効化し、データに変更があった場合のみ通知する
     */
    void fetchData(bool isSilent);

    /**
     * @brief 無操作状態を監視するタイマースロット
     */
    void onIdleTimeout();

    /**
     * @brief ステータスバーの左側にねこのアイコンを1匹追加する
     */
    void addCatIcon();

    /**
     * @brief 表示されているねこのアイコンをすべて消去し、カウンタをリセットする
     */
    void clearCatIcons();

    Ui::MainWindow *ui;                 ///< UIデザイナー用ポインタ
    ConfigManager* m_config;            ///< 設定マネージャー
    TwitchAuth* m_auth;                 ///< Twitch認証マネージャー
    TwitchApiClient* m_apiClient;       ///< Twitch APIクライアント

    // Model/View
    FollowerModel* m_followerModel;     ///< テーブルデータモデル
    QSortFilterProxyModel* m_proxyModel;///< 関係フィルタ用のProxyModel

    // UIコンポーネントの直接参照用ポインタ
    QTabWidget* m_mainTabWidget;
    QTabBar* m_subTabWidget;
    QTableView* m_tableView;
    QLabel* m_statusInfoLabel;          ///< ステータスバー右側の通知ラベル（改ざん表示を上書きしない）

    QString m_twitchToken;              ///< メモリ保持されるTwitchアクセストークン
    QTimer* m_autoFetchTimer;           ///< 自動取得用タイマー
    QTimer* m_idleTimer;                ///< 無操作監視用の1秒周期タイマー
    int m_idleSeconds;                  ///< 無操作の経過秒数
    QWidget* m_catContainer;            ///< ねこアイコンを並べるためのコンテナ
};

#endif // MAINWINDOW_H
