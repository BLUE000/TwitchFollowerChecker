#ifndef LOGGER_H
#define LOGGER_H

#include <QString>

/**
 * @brief 実行ログを難読化（暗号化）してファイルに出力する静的ユーティリティクラス
 * 
 * ログは1行ずつ固定長（512バイト）にパディングされて暗号化され、
 * アペンド（追記）形式で logs/app.tcf に保存されます。
 */
class Logger {
public:
    /**
     * @brief ログ出力先フォルダーを初期化し、ログディレクトリを作成する
     * @param logFolderPath ログ保存先フォルダの絶対パス (Input)
     * @return なし
     */
    static void init(const QString& logFolderPath);

    /**
     * @brief INFOレベルのメッセージを暗号化出力する
     * @param message ログに記録するメッセージ (Input)
     * @return なし
     */
    static void logInfo(const QString& message);

    /**
     * @brief WARNINGレベルのメッセージを暗号化出力する
     * @param message ログに記録する警告メッセージ (Input)
     * @return なし
     */
    static void logWarning(const QString& message);

    /**
     * @brief ERRORレベルのメッセージを暗号化出力する
     * @param message ログに記録するエラーメッセージ (Input)
     * @return なし
     */
    static void logError(const QString& message);

private:
    /**
     * @brief 実際にデータを固定長パディング・暗号化してファイルへ書き込む内部関数
     * @param level ログレベル ("INFO", "WARN", "ERROR") (Input)
     * @param message ログ本文 (Input)
     * @return なし
     */
    static void writeLog(const QString& level, const QString& message);

    static QString s_logPath;                   ///< ログ出力先ファイルパス
    static const int LOG_RECORD_SIZE = 512;     ///< 暗号化前の生データ固定長（512バイト）
    static const QString LOG_FIXED_KEY;        ///< 難読化固定キー
};

#endif // LOGGER_H
