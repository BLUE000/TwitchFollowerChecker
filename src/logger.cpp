#include "logger.h"
#include "cipher_engine.h"
#include <QDateTime>
#include <QTextStream>
#include <QDir>
#include <QFile>

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

    // 1. タイムスタンプを含めたログ文の作成
    QString timeStr = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString logLine = QString("[%1] [%2] %3").arg(timeStr, level, message);

    // 2. ヌル文字が誤って混入しないようにサニタイズ（安全対策）
    logLine.remove(QChar('\0'));

    // 3. 固定長(512バイト)へのパディング処理
    QByteArray rawData = logLine.toUtf8();
    if (rawData.size() > LOG_RECORD_SIZE) {
        rawData = rawData.left(LOG_RECORD_SIZE);
    } else {
        rawData.append(QByteArray(LOG_RECORD_SIZE - rawData.size(), ' ')); // 残りをスペース埋め
    }

    // 4. ローカル難読化ライブラリによる暗号化
    CipherResult result = CipherEngine::encrypt(rawData, LOG_FIXED_KEY, AesMode::Mandatory);
    if (!result.isSuccess()) {
        return; // 暗号化失敗時はファイルにアペンドしない
    }

    // 5. 難読化行をBase64テキストとして追記書き込み (アペンド)
    QFile file(s_logPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        out << result.data().toBase64() << "\n";
        file.close();
    }
}

void Logger::logInfo(const QString& message) { writeLog("INFO", message); }
void Logger::logWarning(const QString& message) { writeLog("WARN", message); }
void Logger::logError(const QString& message) { writeLog("ERROR", message); }
