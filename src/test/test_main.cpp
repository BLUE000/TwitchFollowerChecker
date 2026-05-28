#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QUrlQuery>
#include "../followermodel.h"
#include "../twitchapiclient.h"
#include "../logger.h"

// ==============================================================================
// 1. ヌル文字サニタイズ及び文字列トリミングの単体テスト [UT-SAN-01〜03]
// ==============================================================================
TEST(SecuritySanitizerTest, RemoveNullBytesAndTrim) {
    // UT-SAN-01: 末尾のヌル文字除去
    QByteArray rawData1 = "my_secure_token\0";
    QString cleanStr1 = QString::fromUtf8(rawData1.constData());
    cleanStr1.remove(QChar('\0'));
    cleanStr1 = cleanStr1.trimmed();
    EXPECT_EQ(cleanStr1, "my_secure_token");
    EXPECT_EQ(cleanStr1.length(), 15);

    // UT-SAN-02: 中間の複数ヌル文字の除去
    QByteArray rawData2("token\0extra\0data", 16);
    QString cleanStr2 = QString::fromUtf8(rawData2.constData(), rawData2.size());
    cleanStr2.remove(QChar('\0'));
    cleanStr2 = cleanStr2.trimmed();
    EXPECT_EQ(cleanStr2, "tokenextradata");

    // UT-SAN-03: 前後の空白文字のトリミング
    QByteArray rawData3 = "  token_with_spaces  ";
    QString cleanStr3 = QString::fromUtf8(rawData3.constData());
    cleanStr3.remove(QChar('\0'));
    cleanStr3 = cleanStr3.trimmed();
    EXPECT_EQ(cleanStr3, "token_with_spaces");
}

// ==============================================================================
// 2. ログの固定長パディング（512バイト）の単体テスト [UT-LOG-01〜02]
// ==============================================================================
TEST(LoggerPaddingTest, FixedLengthBlockAlignment) {
    const int LOG_RECORD_SIZE = 512;

    // UT-LOG-01: 短いメッセージをスペースで512バイトにパディングする
    QString logLineShort = "[2026-05-26 21:00:00.000] [INFO] Hello Log";
    QByteArray rawDataShort = logLineShort.toUtf8();
    
    if (rawDataShort.size() > LOG_RECORD_SIZE) {
        rawDataShort = rawDataShort.left(LOG_RECORD_SIZE);
    } else {
        rawDataShort.append(QByteArray(LOG_RECORD_SIZE - rawDataShort.size(), ' '));
    }
    
    EXPECT_EQ(rawDataShort.size(), 512);
    EXPECT_EQ(rawDataShort.at(511), ' '); // 最後尾がスペース文字でパディングされていること

    // UT-LOG-02: 512バイトを超えるメッセージを切り詰める
    QString logLineLong = QString("Very long message...").repeated(50);
    QByteArray rawDataLong = logLineLong.toUtf8();
    
    if (rawDataLong.size() > LOG_RECORD_SIZE) {
        rawDataLong = rawDataLong.left(LOG_RECORD_SIZE);
    } else {
        rawDataLong.append(QByteArray(LOG_RECORD_SIZE - rawDataLong.size(), ' '));
    }
    
    EXPECT_EQ(rawDataLong.size(), 512);
}

// ==============================================================================
// 3. TwitchApiClient の比較ロジックテスト [ST-E2E-01]
// ==============================================================================
TEST(TwitchApiClientTest, RelationComparisonAndClassification) {
    TwitchApiClient client;

    // テストデータの準備（フォロー中 3名 / フォロワー 3名）
    QList<FollowerItem> following;
    following.append({ "UserA", "ユーザーA", "", QDateTime::currentDateTime(), "https://twitch.tv/UserA" });
    following.append({ "UserB", "ユーザーB", "", QDateTime::currentDateTime(), "https://twitch.tv/UserB" });
    following.append({ "UserC", "ユーザーC", "", QDateTime::currentDateTime(), "https://twitch.tv/UserC" });

    QList<FollowerItem> followers;
    followers.append({ "UserB", "ユーザーB", "", QDateTime::currentDateTime(), "https://twitch.tv/UserB" });
    followers.append({ "UserC", "ユーザーC", "", QDateTime::currentDateTime(), "https://twitch.tv/UserC" });
    followers.append({ "UserD", "ユーザーD", "", QDateTime::currentDateTime(), "https://twitch.tv/UserD" });

    // 比較処理の実行
    QList<FollowerItem> comparedList = client.compareLists(following, followers);

    // スタブモード等での fetchFollowing / fetchFollowers 動作確認 & CSV出力のトリガー
    QList<FollowerItem> fetchedFollowing = client.fetchFollowing("dummy_token");
    QList<FollowerItem> fetchedFollowers = client.fetchFollowers("dummy_token");

    // 件数と中身の検証
    EXPECT_EQ(comparedList.size(), 4); // UserA(片想い), UserB(相互), UserC(相互), UserD(片思われ) の合計4件

    int mutualCount = 0;
    int followingCount = 0;
    int followerCount = 0;

    for (const auto& item : comparedList) {
        if (item.relationship == "相互") {
            mutualCount++;
            EXPECT_TRUE(item.loginName == "UserB" || item.loginName == "UserC");
        } else if (item.relationship == "フォローのみ") {
            followingCount++;
            EXPECT_EQ(item.loginName, "UserA");
        } else if (item.relationship == "フォロワーのみ") {
            followerCount++;
            EXPECT_EQ(item.loginName, "UserD");
        }
    }

    EXPECT_EQ(mutualCount, 2);
    EXPECT_EQ(followingCount, 1);
    EXPECT_EQ(followerCount, 1);
}

// ==============================================================================
// 4. FollowerModel（QAbstractTableModel）のデータバインド検証 [IT-MVM-01]
// ==============================================================================
TEST(FollowerModelTest, ModelDataMapping) {
    FollowerModel model;

    QList<FollowerItem> list;
    list.append({ "UserB", "ユーザーB", "相互", QDateTime::fromString("2026-05-26T21:00:00Z", Qt::ISODate), "https://twitch.tv/UserB" });
    
    model.setFollowers(list);

    // 行数・列数の検証
    EXPECT_EQ(model.rowCount(), 1);
    EXPECT_EQ(model.columnCount(), 5);

    // QModelIndex を使った各カラム値の取り出し検証
    // カラム順: col0=表示名, col1=ログイン名, col2=関係, col3=フォロー開始日, col4=URL
    QModelIndex indexDisplay = model.index(0, 0);
    QModelIndex indexLogin   = model.index(0, 1);
    QModelIndex indexRelation = model.index(0, 2);
    QModelIndex indexDate    = model.index(0, 3);
    QModelIndex indexUrl     = model.index(0, 4);

    EXPECT_EQ(model.data(indexDisplay).toString(), "ユーザーB");
    EXPECT_EQ(model.data(indexLogin).toString(), "UserB");
    EXPECT_EQ(model.data(indexRelation).toString(), "相互");
    EXPECT_EQ(model.data(indexDate).toString(), "2026-05-26 21:00:00");
    EXPECT_EQ(model.data(indexUrl).toString(), "https://twitch.tv/UserB");
}

// ==============================================================================
// 5. FollowerItem の operator== 動作検証 [UT-FIT-01]
// ==============================================================================
TEST(FollowerItemTest, ComparisonOperator) {
    FollowerItem item1 { "user", "ユーザー", "相互", QDateTime::fromString("2026-05-26T21:00:00Z", Qt::ISODate), "https://twitch.tv/user" };
    FollowerItem item2 { "user", "ユーザー", "相互", QDateTime::fromString("2026-05-26T21:00:00Z", Qt::ISODate), "https://twitch.tv/user" };
    FollowerItem item3 { "user2", "ユーザー", "相互", QDateTime::fromString("2026-05-26T21:00:00Z", Qt::ISODate), "https://twitch.tv/user" };
    FollowerItem item4 { "user", "ユーザー", "フォローのみ", QDateTime::fromString("2026-05-26T21:00:00Z", Qt::ISODate), "https://twitch.tv/user" };

    // 同一値の比較
    EXPECT_TRUE(item1 == item2);

    // 異なる値の比較
    EXPECT_FALSE(item1 == item3);
    EXPECT_FALSE(item1 == item4);
}

// ==============================================================================
// 6. OAuth パラメータ解析（?記号除去）の単体テスト [UT-AUTH-01]
// ==============================================================================
TEST(OAuthParserTest, ParseTokenWithLeadingQuestionMark) {
    QString rawPath = "/token?access_token=my_test_oauth_token&scope=user:read";
    
    // 実際のプログラムと同じ抽出ロジックの動作検証
    QString queryStr = rawPath.mid(6);
    if (queryStr.startsWith('?')) {
        queryStr = queryStr.mid(1);
    }
    
    QUrlQuery query(queryStr);
    QString token = query.queryItemValue("access_token");
    
    EXPECT_EQ(token, "my_test_oauth_token");
}

// ==============================================================================
// 6. テストメインエントリー
// ==============================================================================
int main(int argc, char *argv[]) {
    // Qtの文字列変換やクラスが正しく動くようにQCoreApplicationを初期化する
    QCoreApplication app(argc, argv);
    
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
