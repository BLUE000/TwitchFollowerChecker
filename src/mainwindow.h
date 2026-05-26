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
     * @brief テーブルのセルがクリックされた際に、URLカラムであればブラウザを起動するスロット
     * @param index クリックされたセルのインデックス
     */
    void onTableCellClicked(const QModelIndex& index);

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
};

#endif // MAINWINDOW_H
