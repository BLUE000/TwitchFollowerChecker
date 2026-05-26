# TwitchFollowerChecker テスト仕様書

本ドキュメントは、TwitchFollowerCheckerの品質保証および自動テストを実行するためのテスト計画とテストケース（GTestおよびQTest向け）を定義します。

---

## 1. 🧪 テスト方針 ＆ 前提条件

* **テストフレームワーク**: 
  * ロジック系: **Google Test (GTest)**
  * Qt関連・GUIモデル系: **Qt Test (QTest)**
* **GUI・UI/UX確認について**: 
  * 画面デザイン、背景画像適用、文字色・フォントの視認性などのUI/UXは、開発者（BLUE000様）による手動検証とします。
  * 本仕様書は、GUIから背後のロジックが正常に呼び出され、データ処理が行われる「自動テスト」をターゲットとします。
* **認証（OAuth/キルスイッチ）のスタブ化方針**:
  * ネットワーク通信（Twitch APIおよびさくらサーバー認証）を伴うテストは、実際の通信を行わず、モッククラス（`MockNetworkAccessManager`）を利用して、事前定義されたJSONレスポンスを返す「スタブ化」を実施し、ローカル環境で100%自動実行できるようにします。

---

## 2. 🧩 単体テスト（Unit Test）仕様書 [詳細設計に準拠]

対象：詳細設計書 (`detailed_design.md`) に記載された個別ロジックおよびクラス。

### 2.1. ヌル文字サニタイズ（`sanitizeSecureString`）テスト
* **テスト手法**: GTestによるアサーション

| テストID | テスト対象 | 入力データ | 期待される結果 (Output) | テスト内容 |
| :--- | :--- | :--- | :--- | :--- |
| **UT-SAN-01** | `sanitizeSecureString` | `"my_secure_token\0"`<br>(末尾にヌル文字混入) | `"my_secure_token"` | ヌル文字が除去され、文字列長が正しく補正されること。 |
| **UT-SAN-02** | `sanitizeSecureString` | `"token\0extra\0data"`<br>(複数箇所のヌル文字) | `"tokenextradata"` | 文字列内に点在するすべてのヌル文字が安全に除去されること。 |
| **UT-SAN-03** | `sanitizeSecureString` | `"  token_with_spaces  "` | `"token_with_spaces"` | 前後の空白文字がトリミングされ、安全な文字列に整形されること。 |

### 2.2. ログ固定長パディング（`Logger::writeLog`）テスト
* **テスト手法**: GTest / ファイルI/Oアサーション

| テストID | テスト対象 | 入力メッセージ | 期待される結果 (Output) | テスト内容 |
| :--- | :--- | :--- | :--- | :--- |
| **UT-LOG-01** | `Logger` パディング | 50バイトのログ文字列 | 暗号化前の生データ長が**正確に512バイト**（末尾スペース埋め）になること。 |
| **UT-LOG-02** | `Logger` 切り詰め | 600バイトの極大ログ | 暗号化前の生データ長が**正確に512バイト**（超過分切り捨て）になること。 |

### 2.3. フォントシリアライズ（`ConfigManager::getFont/setFont`）テスト
* **テスト手法**: QTestによるフォント変換テスト

| テストID | テスト対象 | 設定フォント | 期待される結果 (Output) | テスト内容 |
| :--- | :--- | :--- | :--- | :--- |
| **UT-CFG-01** | QFontシリアライズ | `QFont("Arial", 12, QFont::Bold)` | 保存後に `QFont::toString()` のシリアライズ形式で文字列化され、再読込時に完全に同一フォントとして復元できること。 |

---

## 3. 🔗 結合テスト（Integration Test）仕様書 [基本設計に準拠]

対象：基本設計書 (`basic_design.md`) に記載された複数コンポーネント間の連携。

### 3.1. 設定マネージャー ＆ 難読化エンジン連携
* **テスト手法**: GTestを用いた `ConfigManager` ＋ `CipherEngine` によるファイル保存・復号インテグレーション

| テストID | テスト対象 | テスト手順 | 期待される結果 | テスト内容 |
| :--- | :--- | :--- | :--- | :--- |
| **IT-CFG-01** | 設定ファイル暗号保存 | 1. `ConfigManager` に設定値をセット。<br>2. `save()` を実行。<br>3. 生成された `config.tcf` を読み込む。 | 生成されたファイルはバイナリ難読化状態（プレーンテキストで読めない）であり、`load()` を実行すると元の設定が100%正しく復元されること。 | ファイルI/Oとローカル暗号ライブラリの連携検証。 |

### 3.2. ログアペンド固定長出力連携
* **テスト手法**: GTestによる `Logger` ＋ `CipherEngine` ＋ ファイル追記連携

| テストID | テスト対象 | テスト手順 | 期待される結果 | テスト内容 |
| :--- | :--- | :--- | :--- | :--- |
| **IT-LOG-01** | ログ複数行アペンド | 1. 異なる長さのログを3回出力。<br>2. `logs/app.tcf` を開く。<br>3. 1行ずつ復号する。 | ファイルの各行のBase64デコード長が完全に等しく（固定長）、復号された全3行が時系列順に正しく復元できること。 | 複数回のアペンド処理によるログファイル構造の完全性検証。 |

### 3.3. モデル/ビューモデル ＆ 関係フィルタ連携
* **テスト手法**: QTestによる `FollowerModel` ＋ `QSortFilterProxyModel` 連携

| テストID | テスト対象 | テスト手順 | 期待される結果 | テスト内容 |
| :--- | :--- | :--- | :--- | :--- |
| **IT-MVM-01** | サブタブ関係フィルタ | 1. 相互(2件)、片想い(1件)、片思われ(3件)のテストデータをモデルにセット。<br>2. ProxyModelのフィルタキーをそれぞれ変更する。 | ・フィルタなし: 6件表示<br>・"相互": 正確に2件表示<br>・"フォローのみ": 正確に1件表示<br>・"フォロワーのみ": 正確に3件表示 | ProxyModelがサブタブ（関係性）ごとに正しくデータをフィルタリングできているかの検証。**カラム順: col0=表示名, col1=ログイン名, col2=関係, col3=フォロー開始日, col4=URL。** |

---

## 4. 🏁 総合テスト（System/Acceptance Test）仕様書 [要件定義に準拠]

対象：要件定義書 (`requirements_definition.md`) のユースケースに基づく、アプリ全体の機能検証（※ネットワークはスタブ動作）。

### 4.1. エンドツーエンド検証用スタブの設定
Twitch APIを模した `MockTwitchApiClient` を使用し、以下のテストデータを返却するようスタブを設定します。
* 自分のフォロー中（Following）: `UserA`, `UserB`, `UserC`
* 自分のフォロワー（Followers）: `UserB`, `UserC`, `UserD`
  * ⇒ **想定される比較結果**: 相互=`UserB, UserC`、片想い=`UserA`、片思われ=`UserD`

### 4.2. 総合テストケース一覧

| テストID | 検証する要件 | 操作シナリオ (システム実行フロー) | 期待される判定結果 |
| :--- | :--- | :--- | :--- |
| **ST-E2E-01** | 全体の差分チェック & 比較機能 | 1. アプリを起動（起動時トークン認証スタブ: 成功）。<br>2. ログイン認証を実行（認証スタブ: 成功）。<br>3. 差分チェックを実行（Twitch APIスタブ経由）。 | 各サブタブの表示行数が想定通りになること。<br>・全て: 4件<br>・相互: 2件 (`UserB`, `UserC`)<br>・フォローのみ: 1件 (`UserA`)<br>・フォロワーのみ: 1件 (`UserD`) |
| **ST-E2E-02** | CSVエクスポート要件 | 1. ST-E2E-01の完了状態にする。<br>2. 画面上のCSV出力ボタンを押す（出力先指定スタブ: `test_out.csv`）。 | ・CSVファイルが正常に出力されること。<br>・ファイルの先頭3バイトが **UTF-8のBOM (`\xEF\xBB\xBF`)** であること。<br>・各行の関係ステータス（相互、片想い等）が正しく含まれていること。 |
| **ST-E2E-03** | 遠隔キルスイッチ動作 | 1. 起動時トークン認証スタブの返却ステータスを `"error/invalid"` に設定。<br>2. アプリケーションを起動する。 | アプリがメイン画面を開かず、エラーメッセージを表示して正常に異常終了（またはロック）すること。 |

---

## 5. 💻 テスト自動化実装サンプル（C++ / GTest）

自動テストを実装する際の実装サンプルコードです。

```cpp
#include <gtest/gtest.h>
#include "cipher_engine.h"

// 1. ヌル文字サニタイズの単体テスト実装例
TEST(SecuritySanitizerTest, NullByteRemoval) {
    QByteArray rawData = "my_secure_token\0";
    
    // サニタイズ処理を実行
    QString cleanStr = QString::fromUtf8(rawData.constData());
    cleanStr.remove(QChar('\0'));
    cleanStr = cleanStr.trimmed();

    EXPECT_EQ(cleanStr, "my_secure_token");
    EXPECT_EQ(cleanStr.length(), 15); // \0が除外されているため長さは15
}

// 2. ログ固定長パディングの単体テスト実装例
TEST(LoggerTest, FixedLengthPadding) {
    const int LOG_RECORD_SIZE = 512;
    QString logLine = "[2026-05-26 21:00:00.000] [INFO] Hello Log";
    
    QByteArray rawData = logLine.toUtf8();
    if (rawData.size() > LOG_RECORD_SIZE) {
        rawData = rawData.left(LOG_RECORD_SIZE);
    } else {
        rawData.append(QByteArray(LOG_RECORD_SIZE - rawData.size(), ' '));
    }

    // 暗号化前のバイトサイズが完全に512バイトであることを確認
    EXPECT_EQ(rawData.size(), 512);
    
    // 末尾がスペースで埋められていることを確認
    EXPECT_EQ(rawData.at(511), ' ');
}
```
