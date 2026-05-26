#include "twitchapiclient.h"
#include "logger.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QEventLoop>
#include <QNetworkRequest>
#include <QNetworkReply>

TwitchApiClient::TwitchApiClient(QObject* parent)
    : QObject(parent)
    , m_isStubMode(false)
{
    loadStubData();
}

void TwitchApiClient::loadStubData() {
    QFile file("config/test_stub.json");
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        file.close();

        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isNull()) {
            QJsonObject obj = doc.object();
            m_isStubMode = obj.value("stub_mode").toBool(true);

            if (m_isStubMode) {
                // 1. mock_following のパース
                QJsonArray followingArr = obj.value("mock_following").toArray();
                m_stubFollowingList.clear();
                for (const auto& val : followingArr) {
                    QJsonObject itemObj = val.toObject();
                    FollowerItem item;
                    item.loginName = itemObj.value("loginName").toString();
                    item.displayName = itemObj.value("displayName").toString();
                    item.relationship = ""; // 後で算出
                    item.followedAt = QDateTime::fromString(itemObj.value("followedAt").toString(), Qt::ISODate);
                    item.channelUrl = QString("https://twitch.tv/%1").arg(item.loginName);
                    m_stubFollowingList.append(item);
                }

                // 2. mock_followers のパース
                QJsonArray followersArr = obj.value("mock_followers").toArray();
                m_stubFollowersList.clear();
                for (const auto& val : followersArr) {
                    QJsonObject itemObj = val.toObject();
                    FollowerItem item;
                    item.loginName = itemObj.value("loginName").toString();
                    item.displayName = itemObj.value("displayName").toString();
                    item.relationship = ""; // 後で算出
                    item.followedAt = QDateTime::fromString(itemObj.value("followedAt").toString(), Qt::ISODate);
                    item.channelUrl = QString("https://twitch.tv/%1").arg(item.loginName);
                    m_stubFollowersList.append(item);
                }

                Logger::logInfo(QString("Stub Client Loaded. MockFollowing: %1, MockFollowers: %2")
                                .arg(m_stubFollowingList.size())
                                .arg(m_stubFollowersList.size()));
                return;
            }
        }
    }

    // デフォルトのテストスタブデータ（テスト仕様書 ST-E2E-01 に完全に準拠）
    m_isStubMode = false; // デフォルトでは実機動作。stub.jsonが存在する場合のみスタブモードに遷移。
    
    // 以下はスタブモードが明示的にONになったがJSONの中身が空だった場合のフォールバック用
    m_stubFollowingList.clear();
    m_stubFollowersList.clear();

    FollowerItem a { "UserA", "ユーザーA", "", QDateTime::currentDateTime().addDays(-5), "https://twitch.tv/UserA" };
    FollowerItem b { "UserB", "ユーザーB", "", QDateTime::currentDateTime().addDays(-4), "https://twitch.tv/UserB" };
    FollowerItem c { "UserC", "ユーザーC", "", QDateTime::currentDateTime().addDays(-3), "https://twitch.tv/UserC" };
    m_stubFollowingList.append(a);
    m_stubFollowingList.append(b);
    m_stubFollowingList.append(c);

    FollowerItem b_f { "UserB", "ユーザーB", "", QDateTime::currentDateTime().addDays(-2), "https://twitch.tv/UserB" };
    FollowerItem c_f { "UserC", "ユーザーC", "", QDateTime::currentDateTime().addDays(-1), "https://twitch.tv/UserC" };
    FollowerItem d_f { "UserD", "ユーザーD", "", QDateTime::currentDateTime(), "https://twitch.tv/UserD" };
    m_stubFollowersList.append(b_f);
    m_stubFollowersList.append(c_f);
    m_stubFollowersList.append(d_f);
}

QString TwitchApiClient::fetchCurrentUserId(const QString& token) {
    if (m_isStubMode) return "MOCK_USER_ID_12345";

    QNetworkRequest request(QUrl("https://api.twitch.tv/helix/users"));
    request.setRawHeader("Authorization", QString("Bearer %1").arg(token).toUtf8());
    request.setRawHeader("Client-Id", "gp762nuuoqcoxypju8c569th9wz7q5");

    QNetworkReply* reply = m_networkManager.get(request);
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    QString userId = "";
    if (reply->error() == QNetworkReply::NoError) {
        QJsonObject doc = QJsonDocument::fromJson(reply->readAll()).object();
        QJsonArray data = doc.value("data").toArray();
        if (!data.isEmpty()) {
            userId = data.first().toObject().value("id").toString();
        }
    }
    reply->deleteLater();
    return userId;
}

QList<FollowerItem> TwitchApiClient::fetchFollowing(const QString& token) {
    if (m_isStubMode) {
        Logger::logInfo(QString("Stub: Returning %1 followed channels.").arg(m_stubFollowingList.size()));
        return m_stubFollowingList;
    }

    QString userId = fetchCurrentUserId(token);
    if (userId.isEmpty()) {
        Logger::logError("Failed to fetch current Twitch User ID. Authentication may be invalid.");
        return QList<FollowerItem>();
    }

    QList<FollowerItem> resultList;
    QString cursor = "";

    do {
        QString urlStr = QString("https://api.twitch.tv/helix/channels/followed?user_id=%1&first=100").arg(userId);
        if (!cursor.isEmpty()) {
            urlStr.append("&after=" + cursor);
        }

        QNetworkRequest request((QUrl(urlStr)));
        request.setRawHeader("Authorization", QString("Bearer %1").arg(token).toUtf8());
        request.setRawHeader("Client-Id", "gp762nuuoqcoxypju8c569th9wz7q5");

        QNetworkReply* reply = m_networkManager.get(request);
        QEventLoop loop;
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        if (reply->error() != QNetworkReply::NoError) {
            Logger::logError(QString("Failed to fetch followed channels: %1").arg(reply->errorString()));
            reply->deleteLater();
            break;
        }

        QJsonObject doc = QJsonDocument::fromJson(reply->readAll()).object();
        QJsonArray data = doc.value("data").toArray();

        for (const auto& val : data) {
            QJsonObject obj = val.toObject();
            FollowerItem item;
            item.loginName = obj.value("broadcaster_login").toString();
            item.displayName = obj.value("broadcaster_name").toString();
            item.relationship = ""; // 照合で設定
            item.followedAt = QDateTime::fromString(obj.value("followed_at").toString(), Qt::ISODate);
            item.channelUrl = QString("https://twitch.tv/%1").arg(item.loginName);
            resultList.append(item);
        }

        cursor = doc.value("pagination").toObject().value("cursor").toString();
        reply->deleteLater();
    } while (!cursor.isEmpty());

    Logger::logInfo(QString("Fetched %1 followed channels from Twitch API.").arg(resultList.size()));
    return resultList;
}

QList<FollowerItem> TwitchApiClient::fetchFollowers(const QString& token) {
    if (m_isStubMode) {
        Logger::logInfo(QString("Stub: Returning %1 channel followers.").arg(m_stubFollowersList.size()));
        return m_stubFollowersList;
    }

    QString userId = fetchCurrentUserId(token);
    if (userId.isEmpty()) {
        Logger::logError("Failed to fetch current Twitch User ID. Authentication may be invalid.");
        return QList<FollowerItem>();
    }

    QList<FollowerItem> resultList;
    QString cursor = "";

    do {
        QString urlStr = QString("https://api.twitch.tv/helix/channels/followers?broadcaster_id=%1&first=100").arg(userId);
        if (!cursor.isEmpty()) {
            urlStr.append("&after=" + cursor);
        }

        QNetworkRequest request((QUrl(urlStr)));
        request.setRawHeader("Authorization", QString("Bearer %1").arg(token).toUtf8());
        request.setRawHeader("Client-Id", "gp762nuuoqcoxypju8c569th9wz7q5");

        QNetworkReply* reply = m_networkManager.get(request);
        QEventLoop loop;
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        if (reply->error() != QNetworkReply::NoError) {
            Logger::logError(QString("Failed to fetch channel followers: %1").arg(reply->errorString()));
            reply->deleteLater();
            break;
        }

        QJsonObject doc = QJsonDocument::fromJson(reply->readAll()).object();
        QJsonArray data = doc.value("data").toArray();

        for (const auto& val : data) {
            QJsonObject obj = val.toObject();
            FollowerItem item;
            item.loginName = obj.value("user_login").toString();
            item.displayName = obj.value("user_name").toString();
            item.relationship = ""; // 照合で設定
            item.followedAt = QDateTime::fromString(obj.value("followed_at").toString(), Qt::ISODate);
            item.channelUrl = QString("https://twitch.tv/%1").arg(item.loginName);
            resultList.append(item);
        }

        cursor = doc.value("pagination").toObject().value("cursor").toString();
        reply->deleteLater();
    } while (!cursor.isEmpty());

    Logger::logInfo(QString("Fetched %1 channel followers from Twitch API.").arg(resultList.size()));
    return resultList;
}

QList<FollowerItem> TwitchApiClient::compareLists(const QList<FollowerItem>& followingList, 
                                                 const QList<FollowerItem>& followerList) {
    QList<FollowerItem> mergedList;
    
    // 高速検索用にQMapにマッピング（キー: loginName）
    // 安全のためインデックスアクセスではなくイテレータを使用
    QMap<QString, FollowerItem> followersMap;
    for (const auto& item : followerList) {
        followersMap.insert(item.loginName, item);
    }

    QMap<QString, FollowerItem> followingMap;
    for (const auto& item : followingList) {
        followingMap.insert(item.loginName, item);
    }

    // 1. フォローしている人がフォロワーマップにあるかを確認
    for (const auto& item : followingList) {
        FollowerItem mergedItem = item;
        if (followersMap.contains(item.loginName)) {
            mergedItem.relationship = "相互";
        } else {
            mergedItem.relationship = "フォローのみ";
        }
        mergedList.append(mergedItem);
    }

    // 2. 自分をフォローしてくれているが、自分がフォローしていない人を抽出 (フォロワーのみ)
    for (const auto& item : followerList) {
        if (!followingMap.contains(item.loginName)) {
            FollowerItem mergedItem = item;
            mergedItem.relationship = "フォロワーのみ";
            mergedList.append(mergedItem);
        }
    }

    Logger::logInfo(QString("Comparison complete. Total classified relations: %1").arg(mergedList.size()));
    return mergedList;
}
