/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "mtproto/details/mtproto_abstract_socket.h"

#include <QtNetwork/QSslSocket>

namespace MTP::details {

void SetWssTransportEnabled(bool enabled);
[[nodiscard]] bool WssTransportEnabled();

[[nodiscard]] bool WssRouteAvailable(int16 protocolDcId);

struct WssCustomRoute {
	QString host;
	int port = 443;
	QString path;
	QString domain;
};
void SetWssCustomRoute(const WssCustomRoute &route);
[[nodiscard]] WssCustomRoute WssCustomRouteConfig();

class WssSocket final : public AbstractSocket {
public:
	WssSocket(
		not_null<QThread*> thread,
		const QNetworkProxy &proxy,
		bool protocolForFiles,
		int16 protocolDcId);
	~WssSocket();

	void connectToHost(const QString &address, int port) override;
	bool isGoodStartNonce(bytes::const_span nonce) override;
	void timedOut() override;
	bool isConnected() override;
	bool hasBytesAvailable() override;
	int64 read(bytes::span buffer) override;
	void write(bytes::const_span prefix, bytes::const_span buffer) override;

	int32 debugState() override;
	QString debugPostfix() const override;

private:
	void connectToNextHost();
	[[nodiscard]] bool moveToNextHost();
	void sendHttpUpgrade();
	[[nodiscard]] bool tryFinishUpgrade();
	[[nodiscard]] bool checkUpgradeAccept(const QByteArray &header) const;
	void onReadyRead();
	void parseFrames();
	void sendFrame(quint8 opcode, bytes::const_span data);
	void handleError(int errorCode);

	QSslSocket _socket;
	int16 _dcId = 0;
	QString _domain;
	QString _path;
	quint16 _port = 443;
	QStringList _hosts;
	int _hostIndex = 0;
	QString _currentHost;
	QString _secWebSocketKey;
	QByteArray _incoming;
	QByteArray _readBuffer;
	qint64 _bytesReceived = 0;
	bool _upgraded = false;

};

}
