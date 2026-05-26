#include "followermodel.h"

FollowerModel::FollowerModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

void FollowerModel::setFollowers(const QList<FollowerItem>& list) {
    beginResetModel();
    m_followerList = list;
    endResetModel();
}

int FollowerModel::rowCount(const QModelIndex& parent) const {
    Q_UNUSED(parent);
    return m_followerList.size();
}

int FollowerModel::columnCount(const QModelIndex& parent) const {
    Q_UNUSED(parent);
    return 5; // ログイン名, 表示名, 関係, フォロー開始日, チャンネルURL の5列
}

QVariant FollowerModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_followerList.size()) {
        return QVariant();
    }

    // 安全のためインデックスループを排除し、安全に配列要素を取り出す
    const auto& item = m_followerList.at(index.row());

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case 0: return item.displayName;
            case 1: return item.loginName;
            case 2: return item.relationship;
            case 3: 
                if (item.followedAt.isValid()) {
                    return item.followedAt.toString("yyyy-MM-dd hh:mm:ss");
                }
                return "-";
            case 4: return item.channelUrl;
        }
    }

    // URLカラム（4列目）をリンク色（青）で表示
    if (role == Qt::ForegroundRole && index.column() == 4) {
        return QColor("#5B9EFF");
    }

    return QVariant();
}

QVariant FollowerModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) {
        return QVariant();
    }

    switch (section) {
        case 0: return "表示名";
        case 1: return "ログイン名";
        case 2: return "関係";
        case 3: return "フォロー開始日";
        case 4: return "チャンネルURL";
    }
    return QVariant();
}

const QList<FollowerItem>& FollowerModel::getFollowersList() const {
    return m_followerList;
}
