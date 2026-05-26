# TwitchFollowerChecker 詳細設計書

本ドキュメントは、基本設計に基づき、主要なクラスの内部設計、データ構造、およびアルゴリズム（擬似コード含む）を定義します。開発時は本ドキュメントに準拠してコーディングを行います。

---

## 1. ⚙️ セキュリティ ＆ 認証詳細設計

### 1.1. ヌル文字（`\0`）の混入防止ロジック
パディングや外部データ受信時に発生し得るヌル文字 `\0` を完全に排除するための、安全な `QString` 変換・サニタイズ関数を定義します。

```cpp
/**
 * @brief バイト配列から安全にヌル文字を除去してQStringを生成する
 * @param rawData 復号データまたは通信受信データ (Input)
 * @return ヌル文字が除去・トリミングされた安全な文字列 (Output)
 */
inline QString sanitizeSecureString(const QByteArray& rawData) {
    // 最初のヌル文字の位置で切り捨て、または中間のヌル文字を除去
    QString cleanStr = QString::fromUtf8(rawData.constData());
    cleanStr.remove(QChar('\0'));
    return cleanStr.trimmed();
}
```

### 1.2. 起動時トークン認証（キルスイッチ）フロー
アプリ起動時に `TwitchAuth` または初期化クラスがさくらインターネットサーバーの認証ゲートウェイと通信し、トークンの検証を行います。

* **通信方式**: `QNetworkAccessManager` を使用した同期（またはイベントループによるブロッキング）通信
* **認証エンドポイント**: `https://streamers-tool.sakura.ne.jp/TransCipher/api.php`
* **認証要求ペイロード（JSON）**:
```json
{
  "action": "verify",
  "token": "埋め込まれたTRANSCIPHER_API_TOKEN",
  "system_name": "TwitchFollowerChecker"
}
```
* **ロジックフロー**:
```cpp
/**
 * @brief 起動時に埋め込まれたトークンの有効性をリモートサーバーで検証する
 * @param token 埋め込まれたAPIトークン (Input)
 * @return 有効であればtrue、無効（キルスイッチ発動含む）またはエラー時はfalse (Output)
 */
bool verifyStartupToken(const QString& token) {
    QNetworkAccessManager manager;
    QNetworkRequest request(QUrl("https://streamers-tool.sakura.ne.jp/TransCipher/api.php"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject payload;
    payload["action"] = "verify";
    payload["token"] = token;
    payload["system_name"] = "TwitchFollowerChecker";

    QByteArray rawJson = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    QNetworkReply* reply = manager.post(request, rawJson);

    // 同期処理のためのイベントループ
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    bool isValid = false;
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();
        QJsonObject responseJson = QJsonDocument::fromJson(responseData).object();
        if (responseJson.value("status").toString() == "success") {
            isValid = true;
        }
    }
    reply->deleteLater();
    return isValid;
}
```

---

## 2. 📝 ログ難読化・固定長アペンド設計

クラッシュ耐性を最大化し、かつ高速なログ読み書き・破損検出を実現するため、**「行単位固定長暗号化」**を採用します。

### 2.1. レコード設計
* **生データ最大長**: **512バイト**（ヘッダー情報、タイムスタンプ、メッセージ本体を含む）
* **固定長パディング**: 512バイト未満のログは、暗号化する前に末尾を特定の空白文字（スペース ` `）で埋めて**正確に512バイトに揃えます。**
* **暗号化出力長**: `CipherEngine::encrypt` を適用しBase64エンコードされた暗号テキストは、必ず固定の長さ（例: 約700文字前後）になります。これに改行コード `\n` を付与して `logs/app.tcf` へ追加書き込みします。
* **メリット**: ファイル破損が発生しても、固定バイト数（改行区切り）で正確にブロックをシークできるため、読み込み・復号時にズレが一切発生しません。

### 2.2. Logger クラス定義と擬似コード
```cpp
// Logger.h
#pragma once
#include <QString>
#include <QFile>

class Logger {
public:
    static void init(const QString& logFolderPath);
    static void logInfo(const QString& message);
    static void logWarning(const QString& message);
    static void logError(const QString& message);

private:
    static void writeLog(const QString& level, const QString& message);
    
    static QString s_logPath;
    static const int LOG_RECORD_SIZE = 512;          // 生データの固定長
    static const QString LOG_FIXED_KEY;             // "BLUE000_LOG_FIXED_KEY"
};
```

```cpp
// Logger.cpp
#include "Logger.h"
#include "cipher_engine.h"
#include <QDateTime>
#include <QTextStream>
#include <QDir>

QString Logger::s_logPath = "";
const QString Logger::LOG_FIXED_KEY = "BLUE000_LOG_FIXED_KEY";

void Logger::init(const QString& logFolderPath) {
    QDir dir(logFolderPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    s_logPath = logFolderPath + "/app.tcf";
}

void Logger::writeLog(const QString& level, const QString& message) {
    if (s_logPath.isEmpty()) return;

    // 1. ログ文字列の組み立て
    // [YYYY-MM-DD hh:mm:ss.zzz] [LEVEL] Message
    QString timeStr = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString logLine = QString("[%1] [%2] %3").arg(timeStr, level, message);

    // 2. 固定長(512バイト)へのパディング処理
    QByteArray rawData = logLine.toUtf8();
    if (rawData.size() > LOG_RECORD_SIZE) {
        rawData = rawData.left(LOG_RECORD_SIZE); // 切り詰め
    } else {
        // 残りの領域を空白スペースでパディング
        rawData.append(QByteArray(LOG_RECORD_SIZE - rawData.size(), ' '));
    }

    // 3. ローカルライブラリを使用した暗号化の実行
    CipherResult result = CipherEngine::encrypt(rawData, LOG_FIXED_KEY, AesMode::Mandatory);
    if (!result.isSuccess()) {
        return; // 暗号化失敗時は書き込まない
    }

    // 4. 追加書き込み (Append)
    QFile file(s_logPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        // 暗号データのBase64テキストを1行として出力
        out << result.data().toBase64() << "\n";
        file.close();
    }
}

void Logger::logInfo(const QString& message) { writeLog("INFO", message); }
void Logger::logWarning(const QString& message) { writeLog("WARN", message); }
void Logger::logError(const QString& message) { writeLog("ERROR", message); }
```

---

## 3. 📂 設定マネージャー（ConfigManager）詳細設計

設定ファイル `config/config.tcf` の読み書きを担当するクラス設計です。

```cpp
// ConfigManager.h
#pragma once
#include <QObject>
#include <QMap>
#include <QVariant>
#include <QFont>

class ConfigManager : public QObject {
    Q_OBJECT
public:
    explicit ConfigManager(const QString& configFolderPath, QObject* parent = nullptr);
    
    bool load();
    bool save();
    
    QVariant get(const QString& key, const QVariant& defaultValue = QVariant()) const;
    void set(const QString& key, const QVariant& value);
    
    // フォントのゲッター/セッター（シリアライズ対応）
    QFont getFont() const;
    void setFont(const QFont& font);

private:
    QString m_configPath;
    QMap<QString, QVariant> m_settings;
    static const QString CONFIG_CIPHER_KEY; // "BLUE000_CONFIG_SECURE_KEY"
};
```

```cpp
// ConfigManager.cpp
#include "ConfigManager.h"
#include "cipher_engine.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>

const QString ConfigManager::CONFIG_CIPHER_KEY = "BLUE000_CONFIG_SECURE_KEY";

ConfigManager::ConfigManager(const QString& configFolderPath, QObject* parent)
    : QObject(parent)
{
    m_configPath = configFolderPath + "/config.tcf";
}

bool ConfigManager::load() {
    QFile file(m_configPath);
    if (!file.exists()) {
        // 初回起動時はデフォルト値を投入して新規作成
        m_settings["get_following"] = true;
        m_settings["get_followers"] = true;
        m_settings["compare_lists"] = true;
        m_settings["custom_text_color"] = "#E1E1E6";
        save();
        return true;
    }

    if (!file.open(QIODevice::ReadOnly)) return false;
    QByteArray base64Data = file.readAll();
    file.close();

    // 1. Base64デコード ＆ 復号
    QByteArray cipherData = QByteArray::fromBase64(base64Data);
    CipherResult result = CipherEngine::decrypt(cipherData, CONFIG_CIPHER_KEY);
    if (!result.isSuccess()) return false;

    // 2. ヌル文字の混入防止処理を施してJSONパース
    QString jsonString = QString::fromUtf8(result.data().constData());
    jsonString.remove(QChar('\0')); // 安全対策
    
    QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8());
    QJsonObject obj = doc.object();
    
    m_settings.clear();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        m_settings.insert(it.key(), it.value().toVariant());
    }
    return true;
}

bool ConfigManager::save() {
    QJsonObject obj;
    for (auto it = m_settings.begin(); it != m_settings.end(); ++it) {
        obj.insert(it.key(), QJsonValue::fromVariant(it.value()));
    }

    QJsonDocument doc(obj);
    QByteArray rawJson = doc.toJson(QJsonDocument::Compact);

    // 1. 暗号化
    CipherResult result = CipherEngine::encrypt(rawJson, CONFIG_CIPHER_KEY, AesMode::Mandatory);
    if (!result.isSuccess()) return false;

    // 2. フォルダ自動作成 ＆ 保存
    QFileInfo info(m_configPath);
    QDir().mkpath(info.absolutePath());

    QFile file(m_configPath);
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(result.data().toBase64());
    file.close();
    return true;
}

QVariant ConfigManager::get(const QString& key, const QVariant& defaultValue) const {
    return m_settings.value(key, defaultValue);
}

void ConfigManager::set(const QString& key, const QVariant& value) {
    m_settings.insert(key, value);
}

QFont ConfigManager::getFont() const {
    QString fontStr = get("custom_font", "").toString();
    QFont font;
    if (!fontStr.isEmpty()) {
        font.fromString(fontStr);
    }
    return font;
}

void ConfigManager::setFont(const QFont& font) {
    set("custom_font", font.toString());
}
```

---

## 4. 🗃️ Qt Model/View アーキテクチャ詳細設計

### 4.1. FollowerModel（QAbstractTableModelの実装）
表示データの一元管理およびUI（QTableView）とのバインドを行います。

```cpp
// FollowerModel.h
#pragma once
#include <QAbstractTableModel>
#include <QList>
#include <QDateTime>

struct FollowerItem {
    QString loginName;
    QString displayName;
    QString relationship; // "相互", "フォローのみ", "フォロワーのみ"
    QDateTime followedAt;
    QString channelUrl;
};

class FollowerModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit FollowerModel(QObject* parent = nullptr) : QAbstractTableModel(parent) {}

    void setFollowers(const QList<FollowerItem>& list) {
        beginResetModel();
        m_followerList = list;
        endResetModel();
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        Q_UNUSED(parent);
        return m_followerList.size();
    }

    int columnCount(const QModelIndex& parent = QModelIndex()) const override {
        Q_UNUSED(parent);
        return 5; // ログイン名, 表示名, 関係, フォロー開始日, URL
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || index.row() >= m_followerList.size()) return QVariant();

        const auto& item = m_followerList.at(index.row()); // インデックスループの排除

        if (role == Qt::DisplayRole) {
            switch (index.column()) {
                case 0: return item.displayName; // 表示名（先頭に表示）
                case 1: return item.loginName;   // ログイン名
                case 2: return item.relationship;
                case 3: return item.followedAt.toString("yyyy-MM-dd hh:mm:ss");
                case 4: return item.channelUrl;
            }
        }
        // URLカラムはリンク色（青）で表示
        if (role == Qt::ForegroundRole && index.column() == 4) {
            return QColor("#5B9EFF");
        }
        return QVariant();
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override {
        if (role != Qt::DisplayRole || orientation != Qt::Horizontal) return QVariant();
        switch (section) {
            case 0: return "表示名"; // カラム順: 表示名→ログイン名→関係→フォロー開始日→URL
            case 1: return "ログイン名";
            case 2: return "関係";
            case 3: return "フォロー開始日";
            case 4: return "チャンネルURL";
        }
        return QVariant();
    }

private:
    QList<FollowerItem> m_followerList;
};
```
