#ifndef TWITCHAUTH_H
#define TWITCHAUTH_H

#include <QObject>
#include <QTcpServer>

/**
 * @brief TwitchのOAuth認可フロー（ブラウザ自動連携＋ループバックサーバ）および起動時認証を担当するクラス
 * 
 * テスト時は `config/test_stub.json` を読み込むことで擬似認証システム（スタブ）として振る舞います。
 */
class TwitchAuth : public QObject {
    Q_OBJECT
public:
    /**
     * @brief コンストラクタ
     * @param parent 親オブジェクト (Input)
     */
    explicit TwitchAuth(QObject* parent = nullptr);

    /**
     * @brief デストラクタ。起動しているサーバーがあれば終了する
     */
    ~TwitchAuth();

    /**
     * @brief 起動時にアクセストークンの有効性を確認（キルスイッチ）する
     * @param token 埋め込まれたAPIトークン (Input)
     * @return 認証が正常に通ればtrue、失敗・キルスイッチ発動時はfalse (Output)
     */
    bool verifyStartupToken(const QString& token);

    /**
     * @brief OAuthの認可フローを開始する（ブラウザを起動）
     * @return なし
     */
    void startAuthFlow();

    /**
     * @brief 取得されたTwitchアクセストークンを取得する
     * @return アクセストークン文字列 (Output)
     */
    QString accessToken() const;

    /**
     * @brief 現在スタブモード（テストモード）で動いているかを確認する
     * @return スタブモードならtrue (Output)
     */
    bool isStubMode() const;

signals:
    /**
     * @brief ログイン認可が成功し、アクセストークンが取得されたときに発火するシグナル
     * @param token 取得されたアクセストークン (Output)
     */
    void authSuccess(const QString& token);

    /**
     * @brief ログイン認可が失敗したときに発火するシグナル
     * @param error エラー内容 (Output)
     */
    void authFailed(const QString& error);

private slots:
    /**
     * @brief ループバックHTTPサーバーにリダイレクト接続があった際に呼び出されるスロット
     */
    void handleNewConnection();

private:
    /**
     * @brief テスト用スタブ設定ファイルから期待する動作パラメータを読み込む
     */
    void loadStubConfig();

    QTcpServer* m_tcpServer;        ///< ローカル一時HTTPサーバー
    QString m_accessToken;          ///< メモリ保持されるアクセストークン
    bool m_isStubMode;              ///< スタブモードフラグ
    bool m_stubVerifySuccess;       ///< スタブ：起動時認証結果の成否設定
    bool m_stubOAuthSuccess;        ///< スタブ：OAuthログイン結果の成否設定
};

#endif // TWITCHAUTH_H
