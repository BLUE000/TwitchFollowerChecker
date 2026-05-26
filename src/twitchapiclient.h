#ifndef TWITCHAPICLIENT_H
#define TWITCHAPICLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include "followermodel.h"

/**
 * @brief Twitch API (Helix) との直接の通信を担当するクライアントクラス
 * 
 * 取得ロジックには `config/test_stub.json` に基づくスタブ動作をサポートし、
 * オフラインでの自動テストやモックデータの投入を可能にします。
 */
class TwitchApiClient : public QObject {
    Q_OBJECT
public:
    /**
     * @brief コンストラクタ
     * @param parent 親オブジェクト (Input)
     */
    explicit TwitchApiClient(QObject* parent = nullptr);

    /**
     * @brief ログインしたユーザーがフォローしている配信者の一覧を取得する
     * @param token Twitch OAuthアクセストークン (Input)
     * @return フォロー中ユーザーのリスト (Output)
     */
    QList<FollowerItem> fetchFollowing(const QString& token);

    /**
     * @brief ログインしたユーザーのチャンネルをフォローしているリスナーの一覧を取得する
     * @param token Twitch OAuthアクセストークン (Input)
     * @return フォロワーのリスト (Output)
     */
    QList<FollowerItem> fetchFollowers(const QString& token);

    /**
     * @brief フォローとフォロワーのリストを照合して比較し、関係性（相互/片想い/片思われ）を算出する
     * @param followingList フォロー中リスト (Input)
     * @param followerList フォロワーリスト (Input)
     * @return 照合・分類されたマージデータリスト (Output)
     */
    QList<FollowerItem> compareLists(const QList<FollowerItem>& followingList, 
                                     const QList<FollowerItem>& followerList);

private:
    /**
     * @brief テスト用スタブのデータを読み込む
     */
    void loadStubData();

    /**
     * @brief Twitch API経由で取得したユーザーID（ログインユーザー本人）を取得する
     * @param token アクセストークン (Input)
     * @return ユーザーID (Output)
     */
    QString fetchCurrentUserId(const QString& token);

    QNetworkAccessManager m_networkManager;     ///< Qtネットワークアクセスマネージャ
    bool m_isStubMode;                          ///< スタブモードフラグ
    QList<FollowerItem> m_stubFollowingList;    ///< スタブ：モックフォローデータ
    QList<FollowerItem> m_stubFollowersList;    ///< スタブ：モックフォロワーデータ
};

#endif // TWITCHAPICLIENT_H
