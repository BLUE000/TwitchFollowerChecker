#ifndef FOLLERMODEL_H
#define FOLLERMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include <QDateTime>
#include <QColor>

/**
 * @brief フォロワー情報を保持する構造体
 */
struct FollowerItem {
    QString loginName;      ///< ログイン名（英名ID）
    QString displayName;    ///< 表示名（ニックネーム）
    QString relationship;   ///< 関係ステータス ("相互", "フォローのみ", "フォローされているだけ")
    QDateTime followedAt;   ///< フォローされた（またはフォローした）日時
    QString channelUrl;     ///< TwitchのチャンネルURL

    bool operator==(const FollowerItem& other) const {
        return loginName == other.loginName &&
               displayName == other.displayName &&
               relationship == other.relationship &&
               followedAt == other.followedAt &&
               channelUrl == other.channelUrl;
    }
};

/**
 * @brief QtのModel/Viewアーキテクチャに準拠したフォロワーデータ管理テーブルモデル
 */
class FollowerModel : public QAbstractTableModel {
    Q_OBJECT
public:
    /**
     * @brief コンストラクタ
     * @param parent 親オブジェクト (Input)
     */
    explicit FollowerModel(QObject* parent = nullptr);

    /**
     * @brief テーブル内のデータ一覧をセットし、ビューを更新する
     * @param list 新しいフォロワー一覧データ (Input)
     * @return なし
     */
    void setFollowers(const QList<FollowerItem>& list);

    /**
     * @brief 行数を取得する
     * @param parent 親インデックス (Input)
     * @return 行数 (Output)
     */
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    /**
     * @brief 列数を取得する
     * @param parent 親インデックス (Input)
     * @return 列数 (5列固定) (Output)
     */
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;

    /**
     * @brief セル内の値を取得する
     * @param index 対象セルインデックス (Input)
     * @param role 表示用のロール (Input)
     * @return 取得したデータ (Output)
     */
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    /**
     * @brief テーブルヘッダーの表示文字を取得する
     * @param section 列番号 (Input)
     * @param orientation 方向 (横方向のみ対応) (Input)
     * @param role 表示用のロール (Input)
     * @return ヘッダー名 (Output)
     */
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    /**
     * @brief 現在モデルが管理している生データ一覧を取得する（インデックスアクセス回避用）
     * @return QListの参照 (Output)
     */
    const QList<FollowerItem>& getFollowersList() const;

private:
    QList<FollowerItem> m_followerList; ///< 管理データ一覧
};

#endif // FOLLERMODEL_H
