#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QMap>
#include <QVariant>
#include <QFont>

/**
 * @brief 設定情報を難読化JSONファイルとしてローカルに保存・復元するマネージャクラス
 * 
 * 設定ファイルは config/config.tcf に保存され、
 * アプリの文字フォントや背景画像パスなどの表示設定も保持します。
 */
class ConfigManager : public QObject {
    Q_OBJECT
public:
    /**
     * @brief コンストラクタ。設定保存先のフォルダーを定義する
     * @param configFolderPath 設定保存先のディレクトリパス (Input)
     * @param parent 親オブジェクト (Input)
     */
    explicit ConfigManager(const QString& configFolderPath, QObject* parent = nullptr);

    /**
     * @brief 設定ファイル config.tcf を読み込んで復号化し、メモリ上に展開する
     * @return 読込および復号化に成功すればtrue、失敗すればfalse (Output)
     */
    bool load();

    /**
     * @brief 現在の設定値を暗号化し、config.tcf に書き込む
     * @return 暗号化と書き込みに成功すればtrue、失敗すればfalse (Output)
     */
    bool save();

    /**
     * @brief 指定したキーに対応する設定値を取得する
     * @param key 設定キー (Input)
     * @param defaultValue キーが存在しない場合の既定値 (Input)
     * @return 設定値に対応するQVariant (Output)
     */
    QVariant get(const QString& key, const QVariant& defaultValue = QVariant()) const;

    /**
     * @brief 指定したキーに設定値を書き込む（メモリ上のみ、保存にはsave()が必要）
     * @param key 設定キー (Input)
     * @param value 書き込む値 (Input)
     * @return なし
     */
    void set(const QString& key, const QVariant& value);

    /**
     * @brief ユーザーが選択したフォント設定を取得する
     * @return 設定されているQFontオブジェクト (Output)
     */
    QFont getFont() const;

    /**
     * @brief ユーザーが選択したフォントを設定に保存する
     * @param font 設定するQFontオブジェクト (Input)
     * @return なし
     */
    void setFont(const QFont& font);

private:
    QString m_configPath;                   ///< 設定ファイルの絶対パス
    QMap<QString, QVariant> m_settings;     ///< 設定値のインメモリキャッシュマップ
    static const QString CONFIG_CIPHER_KEY; ///< 難読化用秘密鍵
};

#endif // CONFIGMANAGER_H
