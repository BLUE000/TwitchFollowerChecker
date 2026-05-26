#include "twitchauth.h"
#include "logger.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>
#include <QDesktopServices>
#include <QUrl>
#include <QUrlQuery>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

TwitchAuth::TwitchAuth(QObject* parent)
    : QObject(parent)
    , m_tcpServer(nullptr)
    , m_isStubMode(false)
    , m_stubVerifySuccess(true)
    , m_stubOAuthSuccess(true)
{
    loadStubConfig();
}

TwitchAuth::~TwitchAuth() {
    if (m_tcpServer) {
        m_tcpServer->close();
        delete m_tcpServer;
    }
}

void TwitchAuth::loadStubConfig() {
    // config/test_stub.json からテスト用設定を読み込む
    QFile file("config/test_stub.json");
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        file.close();

        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
        if (!doc.isNull()) {
            QJsonObject obj = doc.object();
            m_isStubMode = obj.value("stub_mode").toBool(true);
            m_stubVerifySuccess = obj.value("startup_verification_success").toBool(true);
            m_stubOAuthSuccess = obj.value("oauth_success").toBool(true);
            Logger::logInfo(QString("Stub Mode Enabled. VerifySuccess: %1, OAuthSuccess: %2")
                            .arg(m_stubVerifySuccess ? "true" : "false")
                            .arg(m_stubOAuthSuccess ? "true" : "false"));
            return;
        }
    }
    m_isStubMode = false;
    m_stubVerifySuccess = true;
    m_stubOAuthSuccess = true;
}

bool TwitchAuth::verifyStartupToken(const QString& token) {
    // ヌル文字が混入していた場合は即座に除去
    QString sanitizedToken = token;
    sanitizedToken.remove(QChar('\0'));
    sanitizedToken = sanitizedToken.trimmed();

    if (m_isStubMode) {
        Logger::logInfo(QString("Stub mode token verification: returns %1")
                        .arg(m_stubVerifySuccess ? "success" : "failure"));
        return m_stubVerifySuccess;
    }

    if (sanitizedToken.isEmpty()) {
        Logger::logWarning("Startup token verification skipped (token is empty).");
        return false;
    }

    Logger::logInfo("Connecting to Sakura server for token verification...");

    QNetworkAccessManager manager;
    QNetworkRequest request(QUrl("https://streamers-tool.sakura.ne.jp/TransCipher/api.php"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject payload;
    payload["action"] = "verify";
    payload["token"] = sanitizedToken;
    payload["system_name"] = "TwitchFollowerChecker";

    QByteArray rawJson = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    QNetworkReply* reply = manager.post(request, rawJson);

    // 同期通信用のイベントループ
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    bool isValid = false;
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();
        QJsonObject responseJson = QJsonDocument::fromJson(responseData).object();
        if (responseJson.value("status").toString() == "success") {
            isValid = true;
            Logger::logInfo("Token successfully validated by Sakura Server.");
        } else {
            Logger::logWarning(QString("Sakura Server returned verify failure: %1")
                               .arg(responseJson.value("message").toString()));
        }
    } else {
        Logger::logError(QString("Sakura Server connection error during verification: %1")
                         .arg(reply->errorString()));
    }
    
    reply->deleteLater();
    return isValid;
}

void TwitchAuth::startAuthFlow() {
    if (m_isStubMode) {
        if (m_stubOAuthSuccess) {
            m_accessToken = "MOCK_TWITCH_OAUTH_TOKEN_ABC123";
            Logger::logInfo("OAuth Stub Successful. Emitting authSuccess.");
            emit authSuccess(m_accessToken);
        } else {
            Logger::logWarning("OAuth Stub Failed. Emitting authFailed.");
            emit authFailed("Stub Login Failure.");
        }
        return;
    }

    // 1. ローカルサーバー(Port 38080)の起動
    if (!m_tcpServer) {
        m_tcpServer = new QTcpServer(this);
        connect(m_tcpServer, &QTcpServer::newConnection, this, &TwitchAuth::handleNewConnection);
    }
    
    if (!m_tcpServer->isListening()) {
        if (!m_tcpServer->listen(QHostAddress::LocalHost, 38080)) {
            Logger::logError("Failed to start local redirect server on port 38080.");
            emit authFailed("Local server start failure.");
            return;
        }
        Logger::logInfo("Local redirect server listening on http://localhost:38080");
    }

    // 2. ブラウザでTwitch認可画面を開く
    // ※今回はサンプルとして、BLUE000様のClient IDがあればそれを使う。設定されていない場合はデフォルトを使用。
    // 通常のTwitch OAuth Implicit Grantフロー
    #ifdef TWITCH_DEFAULT_CLIENT_ID
    QString clientId = TWITCH_DEFAULT_CLIENT_ID;
    #else
    QString clientId = "gp762nuuoqcoxypju8c569th9wz7q5"; // デフォルト Client ID (例)
    #endif
    
    QString authUrl = QString("https://id.twitch.tv/oauth2/authorize"
                              "?client_id=%1"
                              "&redirect_uri=http://localhost:38080"
                              "&response_type=token"
                              "&scope=user:read:follows+moderator:read:followers")
                      .arg(clientId);

    Logger::logInfo("Opening Twitch Authorization Page in browser...");
    QDesktopServices::openUrl(QUrl(authUrl));
}

void TwitchAuth::handleNewConnection() {
    QTcpSocket* socket = m_tcpServer->nextPendingConnection();
    if (!socket) return;

    connect(socket, &QTcpSocket::readyRead, [this, socket]() {
        QByteArray request = socket->readAll();
        QString requestStr = QString::fromUtf8(request);

        // 1. リクエスト行の解析
        QStringList requestLines = requestStr.split("\r\n");
        if (requestLines.isEmpty()) return;
        QString firstLine = requestLines.first();
        QStringList tokens = firstLine.split(" ");
        if (tokens.size() < 2) return;
        
        QString path = tokens[1];

        // 2. HTMLの返却
        QTextStream out(socket);
        out << "HTTP/1.1 200 OK\r\n";
        out << "Content-Type: text/html; charset=utf-8\r\n";
        out << "Connection: close\r\n\r\n";

        if (path.startsWith("/token")) {
            // JSリダイレクトからトークンが渡ってきた場合
            QUrlQuery query(path.mid(6)); // "/token?" の後を解析
            QString token = query.queryItemValue("access_token");
            
            // ヌル文字が混入していた場合は即座に除去（安全対策）
            token.remove(QChar('\0'));
            token = token.trimmed();

            if (!token.isEmpty()) {
                m_accessToken = token;
                out << "<html><body><h2>ログインに成功しました！</h2><p>このウィンドウを閉じてアプリケーションに戻ってください。</p></body></html>";
                Logger::logInfo("OAuth Access Token successfully retrieved via loopback server.");
                emit authSuccess(m_accessToken);
            } else {
                out << "<html><body><h2>ログイン失敗</h2><p>アクセストークンが空でした。</p></body></html>";
                emit authFailed("Empty access token.");
            }
            socket->disconnectFromHost();
        } else {
            // 初回リダイレクト時 (URL Fragment '#' にトークンが含まれている)
            // ブラウザは '#' 以降をサーバーに送信しないため、HTML+JSを返却してQuery Parameterに変換して再アクセスさせる
            out << "<html><body>"
                << "<h2>Twitch認証を処理しています...</h2>"
                << "<script>"
                << "if (window.location.hash) {"
                << "  var params = new URLSearchParams(window.location.hash.substring(1));"
                << "  var token = params.get('access_token');"
                << "  if (token) {"
                << "    window.location.href = '/token?access_token=' + token;"
                << "  } else {"
                << "    document.body.innerHTML = '<h2>エラー: トークンが見つかりません。</h2>';"
                << "  }"
                << "} else {"
                << "  document.body.innerHTML = '<h2>認証エラー: 不正なアクセスです。</h2>';"
                << "}"
                << "</script>"
                << "</body></html>";
            socket->disconnectFromHost();
        }
    });
}

QString TwitchAuth::accessToken() const {
    return m_accessToken;
}

bool TwitchAuth::isStubMode() const {
    return m_isStubMode;
}
