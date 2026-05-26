#include "configmanager.h"
#include "cipher_engine.h"
#include "logger.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QFileInfo>

const QString ConfigManager::CONFIG_CIPHER_KEY = "BLUE000_CONFIG_SECURE_KEY";

ConfigManager::ConfigManager(const QString& configFolderPath, QObject* parent)
    : QObject(parent)
{
    m_configPath = configFolderPath + "/config.tcf";
}

bool ConfigManager::load() {
    QFile file(m_configPath);
    if (!file.exists()) {
        Logger::logInfo("Config file config.tcf not found. Initializing with default settings.");
        // 初期値の格納
        m_settings["show_all"] = true;
        m_settings["show_mutual"] = true;
        m_settings["show_following_only"] = true;
        m_settings["show_followers_only"] = true;
        m_settings["custom_text_color"] = "#E1E1E6";
        m_settings["custom_bg_color"] = "#121214";
        m_settings["background_image_path"] = "";
        m_settings["custom_font"] = "";
        m_settings["custom_client_id"] = "";
        m_settings["custom_client_secret"] = "";
        save();
        return true;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        Logger::logError("Failed to open config.tcf for reading.");
        return false;
    }
    QByteArray base64Data = file.readAll();
    file.close();

    // 1. Base64デコード ＆ 復号
    QByteArray cipherData = QByteArray::fromBase64(base64Data);
    CipherResult result = CipherEngine::decrypt(cipherData, CONFIG_CIPHER_KEY);
    if (!result.isSuccess()) {
        Logger::logError(QString("Failed to decrypt config file config.tcf: %1").arg(result.message()));
        return false;
    }

    // 2. ヌル文字が絶対に混入しないようにサニタイズ
    QByteArray decryptedData = result.data();
    QString jsonString = QString::fromUtf8(decryptedData.constData());
    jsonString.remove(QChar('\0')); // ヌル文字除去の徹底
    jsonString = jsonString.trimmed();

    // 3. JSONパース
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8(), &parseError);
    if (doc.isNull()) {
        Logger::logError(QString("JSON parse error for config.tcf: %1").arg(parseError.errorString()));
        return false;
    }

    QJsonObject obj = doc.object();
    m_settings.clear();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        m_settings.insert(it.key(), it.value().toVariant());
    }

    Logger::logInfo("Successfully loaded and decrypted configuration config.tcf.");
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
    if (!result.isSuccess()) {
        Logger::logError(QString("Failed to encrypt configuration: %1").arg(result.message()));
        return false;
    }

    // 2. 保存先フォルダの自動作成
    QFileInfo info(m_configPath);
    QDir().mkpath(info.absolutePath());

    // 3. 書き込み
    QFile file(m_configPath);
    if (!file.open(QIODevice::WriteOnly)) {
        Logger::logError("Failed to open config.tcf for writing.");
        return false;
    }
    file.write(result.data().toBase64());
    file.close();

    Logger::logInfo("Successfully encrypted and saved configuration config.tcf.");
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
