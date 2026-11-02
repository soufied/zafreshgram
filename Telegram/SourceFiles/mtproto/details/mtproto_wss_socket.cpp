/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mtproto/details/mtproto_wss_socket.h"

#include "mtproto/details/mtproto_abstract_socket.h"
#include "base/bytes.h"
#include "base/invoke_queued.h"
#include "base/random.h"
#include "logs.h"
#include <crl/crl_time.h>

#include <QtCore/QMutex>
#include <QtCore/QtEndian>
#include <QtCore/QCryptographicHash>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <map>

namespace MTP::details {
namespace {

constexpr auto kWssMaxFrame = 2 * 1024 * 1024;
constexpr auto kWssHeaderLimit = 32 * 1024;

constexpr auto kIngressRetryAfter = 2 * 60 * crl::time(1000);

constexpr auto kDcSuspendFor = 2 * 60 * crl::time(1000);

std::atomic<bool> WssEnabled = true;

QMutex RelayMutex;
std::map<int, QString> PreferredIngressByDc;
std::map<QString, crl::time> FailedIngressAt;
std::map<int, crl::time> SuspendedUntilByDc;

QMutex CustomRouteMutex;
WssCustomRoute CustomRoute;

[[nodiscard]] QStringList OfficialIngresses(int dcId) {
	switch (dcId) {
	case 1:
	case 3: return { u"149.154.174.100"_q };
	case 2:
	case 4: return { u"149.154.167.220"_q, u"149.154.167.99"_q };
	case 5: return { u"149.154.170.100"_q };
	}
	return {};
}

[[nodiscard]] bool CustomRouteSet() {
	QMutexLocker lock(&CustomRouteMutex);
	return !CustomRoute.host.isEmpty();
}

[[nodiscard]] QString RelayDomain(int16 dcId) {
	const auto raw = int(dcId);
	const auto name = u"kws%1"_q.arg(std::abs(raw));
	return (raw < 0)
		? (name + u"-1.web.telegram.org"_q)
		: (name + u".web.telegram.org"_q);
}

[[nodiscard]] QStringList RelayHosts(int dcId, const QString &domain) {
	if (CustomRouteSet()) {
		QMutexLocker lock(&CustomRouteMutex);
		return { CustomRoute.host };
	}
	const auto ingresses = OfficialIngresses(dcId);
	const auto now = crl::now();
	auto order = QStringList();
	auto result = QStringList();
	auto failed = QStringList();
	QMutexLocker lock(&RelayMutex);
	const auto preferred = PreferredIngressByDc.find(dcId);
	if (preferred != end(PreferredIngressByDc)
		&& ingresses.contains(preferred->second)) {
		order.push_back(preferred->second);
	}
	for (const auto &address : ingresses) {
		if (!order.contains(address)) {
			order.push_back(address);
		}
	}
	for (const auto &address : order) {
		const auto i = FailedIngressAt.find(address);
		const auto recentlyFailed = (i != end(FailedIngressAt))
			&& (now - i->second < kIngressRetryAfter);
		(recentlyFailed ? failed : result).push_back(address);
	}
	result.append(failed);
	result.push_back(domain);
	return result;
}

[[nodiscard]] QByteArray RandomBytes(int count) {
	auto result = QByteArray(count, char(0));
	bytes::set_random(bytes::make_detached_span(result));
	return result;
}

}

void SetWssTransportEnabled(bool enabled) {
	WssEnabled = enabled;
}

bool WssTransportEnabled() {
	return WssEnabled.load();
}

void SetWssCustomRoute(const WssCustomRoute &route) {
	auto normalized = route;
	normalized.host = normalized.host.trimmed();
	normalized.domain = normalized.domain.trimmed();
	normalized.path = normalized.path.trimmed();
	if (normalized.path.isEmpty()) {
		normalized.path = u"/apiws"_q;
	} else if (!normalized.path.startsWith('/')) {
		normalized.path.prepend(u'/');
	}
	if (normalized.port <= 0 || normalized.port > 65535) {
		normalized.port = 443;
	}
	QMutexLocker lock(&CustomRouteMutex);
	CustomRoute = normalized;
}

WssCustomRoute WssCustomRouteConfig() {
	QMutexLocker lock(&CustomRouteMutex);
	return CustomRoute;
}

bool WssRouteAvailable(int16 protocolDcId) {
	const auto dc = std::abs(int(protocolDcId));
	// Test DCs are offset by 10000 and never go through a relay.
	if (!WssTransportEnabled() || dc <= 0 || dc >= 1000) {
		return false;
	}
	if (!CustomRouteSet() && OfficialIngresses(dc).isEmpty()) {
		return false;
	}
	QMutexLocker lock(&RelayMutex);
	const auto i = SuspendedUntilByDc.find(dc);
	return (i == end(SuspendedUntilByDc)) || (i->second <= crl::now());
}

WssSocket::WssSocket(
		not_null<QThread*> thread,
		const QNetworkProxy &proxy,
		bool protocolForFiles,
		int16 protocolDcId)
: AbstractSocket(thread)
, _dcId(protocolDcId)

{
	const auto custom = WssCustomRouteConfig();
	if (custom.host.isEmpty()) {
		_domain = RelayDomain(protocolDcId);
		_path = u"/apiws"_q;
	} else {
		_domain = custom.domain.isEmpty() ? custom.host : custom.domain;
		_path = custom.path;
		_port = quint16(custom.port);
	}
	_socket.moveToThread(thread);
	_socket.setProxy(proxy);
	_socket.setPeerVerifyMode(QSslSocket::VerifyPeer);
	if (protocolForFiles) {
		_socket.setSocketOption(
			QAbstractSocket::SendBufferSizeSocketOption,
			kFilesSendBufferSize);
		_socket.setSocketOption(
			QAbstractSocket::ReceiveBufferSizeSocketOption,
			kFilesReceiveBufferSize);
	}
	const auto wrap = [&](auto handler) {
		return [=](auto &&...args) {
			InvokeQueued(this, [=] { handler(args...); });
		};
	};
	using Error = QAbstractSocket::SocketError;
	connect(
		&_socket,
		&QSslSocket::encrypted,
		wrap([=] {
			_socket.setSocketOption(QAbstractSocket::LowDelayOption, 1);
			sendHttpUpgrade();
		}));
	connect(
		&_socket,
		&QSslSocket::disconnected,
		wrap([=] { _disconnected.fire({}); }));
	connect(
		&_socket,
		&QSslSocket::readyRead,
		wrap([=] { onReadyRead(); }));
	connect(
		&_socket,
		&QAbstractSocket::errorOccurred,
		wrap([=](Error e) { handleError(e); }));
}

WssSocket::~WssSocket() = default;

void WssSocket::connectToHost(const QString &address, int port) {
	Q_UNUSED(address);
	Q_UNUSED(port);
	// The relay picks the DC from the domain, so the endpoint address is unused.
	_hosts = RelayHosts(std::abs(int(_dcId)), _domain);
	_hostIndex = 0;
	connectToNextHost();
}

void WssSocket::connectToNextHost() {
	_currentHost = _hosts[_hostIndex];
	_incoming.clear();
	_upgraded = false;
	_socket.abort();
	_socket.setPeerVerifyName(_domain);
	_socket.connectToHostEncrypted(_currentHost, _port, _domain);
}

bool WssSocket::moveToNextHost() {
	{
		QMutexLocker lock(&RelayMutex);
		FailedIngressAt[_currentHost] = crl::now();
	}
	if (_hostIndex + 1 >= int(_hosts.size())) {
		return false;
	}
	++_hostIndex;
	connectToNextHost();
	return true;
}

void WssSocket::sendHttpUpgrade() {
	_secWebSocketKey = QString::fromLatin1(RandomBytes(16).toBase64());
	const auto request = (u"GET %1 HTTP/1.1\r\n"
		u"Host: %2\r\n"
		u"Upgrade: websocket\r\n"
		u"Connection: Upgrade\r\n"
		u"Sec-WebSocket-Key: %3\r\n"
		u"Sec-WebSocket-Version: 13\r\n"
		u"Sec-WebSocket-Protocol: binary\r\n"
		u"Origin: https://web.telegram.org\r\n"
		u"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
		u"AppleWebKit/537.36 (KHTML, like Gecko) "
		u"Chrome/131.0.0.0 Safari/537.36\r\n"
		u"\r\n"_q).arg(_path, _domain, _secWebSocketKey);
	_socket.write(request.toUtf8());
}

bool WssSocket::tryFinishUpgrade() {
	const auto end = _incoming.indexOf("\r\n\r\n");
	if (end < 0) {
		if (_incoming.size() > kWssHeaderLimit) {
			logError(0, u"WSS HTTP response too large"_q);
			_error.fire({});
		}
		return false;
	}
	const auto header = _incoming.left(end);
	_incoming.remove(0, end + 4);
	// A wrong ingress answers with a redirect, so it is treated as a failure.
	if (!header.contains(" 101 ") && !header.contains(" 101\r")) {
		logError(0, u"WSS HTTP upgrade rejected"_q);
		if (!moveToNextHost()) {
			_error.fire({});
		}
		return false;
	}
	if (!checkUpgradeAccept(header)) {
		logError(0, u"WSS Sec-WebSocket-Accept mismatch"_q);
		_error.fire({});
		return false;
	}
	_upgraded = true;
	{
		QMutexLocker lock(&RelayMutex);
		const auto dc = std::abs(int(_dcId));
		if (_currentHost != _domain) {
			PreferredIngressByDc[dc] = _currentHost;
		}
		FailedIngressAt.erase(_currentHost);
		SuspendedUntilByDc.erase(dc);
	}
	_connected.fire({});
	return true;
}

bool WssSocket::checkUpgradeAccept(const QByteArray &header) const {
	const auto expected = QCryptographicHash::hash(
		_secWebSocketKey.toLatin1()
			+ QByteArrayLiteral("258EAFA5-E914-47DA-95CA-C5AB0DC85B11"),
		QCryptographicHash::Sha1).toBase64();
	const auto lowered = header.toLower();
	const auto marker = QByteArrayLiteral("sec-websocket-accept:");
	const auto pos = lowered.indexOf(marker);
	if (pos < 0) {
		return false;
	}
	auto valueEnd = header.indexOf('\n', pos);
	if (valueEnd < 0) {
		valueEnd = header.size();
	}
	const auto from = pos + marker.size();
	const auto value = header.mid(from, valueEnd - from).trimmed();
	return (value == expected);
}

void WssSocket::onReadyRead() {
	_incoming += _socket.readAll();
	if (!_upgraded && !tryFinishUpgrade()) {
		return;
	}
	parseFrames();
}

void WssSocket::parseFrames() {
	auto produced = false;
	auto offset = 0;
	const auto total = int(_incoming.size());
	const auto data = bytes::make_span(_incoming.constData(), total);
	const auto byteAt = [&](int index) {
		return gsl::to_integer<quint8>(data[index]);
	};
	while (total - offset >= 2) {
		const auto opcode = (byteAt(offset) & 0x0f);
		const auto masked = ((byteAt(offset + 1) & 0x80) != 0);
		auto length = quint64(byteAt(offset + 1) & 0x7f);
		auto headerLen = 2;
		if (length == 126) {
			if (total - offset < 4) {
				break;
			}
			length = (quint64(byteAt(offset + 2)) << 8)
				| quint64(byteAt(offset + 3));
			headerLen = 4;
		} else if (length == 127) {
			if (total - offset < 10) {
				break;
			}
			length = 0;
			for (auto i = 0; i != 8; ++i) {
				length = (length << 8) | quint64(byteAt(offset + 2 + i));
			}
			headerLen = 10;
		}
		if (length > kWssMaxFrame) {
			logError(0, u"WSS frame too large"_q);
			_error.fire({});
			return;
		}
		const auto maskLen = masked ? 4 : 0;
		const auto frameLen = quint64(headerLen) + maskLen + length;
		if (quint64(total - offset) < frameLen) {
			break;
		}
		const auto mask = data.subspan(offset + headerLen, maskLen);
		const auto payload = data.subspan(
			offset + headerLen + maskLen,
			int(length));
		if (opcode == 0x8) { 
			logError(0, u"WSS close frame received"_q);
			_error.fire({});
			return;
		} else if (opcode == 0x9) { 
			sendFrame(0xA, payload);
		} else if (opcode == 0x0 || opcode == 0x1 || opcode == 0x2) {
			if (length > 0) {
				_bytesReceived += qint64(length);
				const auto at = int(_readBuffer.size());
				const auto count = int(length);
				_readBuffer.resize(at + count);
				auto out = bytes::make_detached_span(_readBuffer).subspan(at);
				if (masked) {
					for (auto i = 0; i != count; ++i) {
						out[i] = bytes::type(
							byteAt(offset + headerLen + maskLen + i)
							^ gsl::to_integer<quint8>(mask[i % 4]));
					}
				} else {
					bytes::copy(out, payload);
				}
				produced = true;
			}
		}
		offset += int(frameLen);
	}
	if (offset > 0) {
		_incoming.remove(0, offset);
	}
	if (produced) {
		_readyRead.fire({});
	}
}

void WssSocket::sendFrame(quint8 opcode, bytes::const_span data) {
	const auto size = int(data.size());
	auto frame = QByteArray();
	frame.reserve(size + 14);
	frame.append(char(0x80 | opcode));
	if (size < 126) {
		frame.append(char(0x80 | size));
	} else if (size <= 0xffff) {
		frame.append(char(0x80 | 126));
		frame.append(char((size >> 8) & 0xff));
		frame.append(char(size & 0xff));
	} else {
		frame.append(char(0x80 | 127));
		for (auto i = 7; i >= 0; --i) {
			frame.append(char((quint64(size) >> (i * 8)) & 0xff));
		}
	}
	const auto mask = RandomBytes(4);
	frame.append(mask);
	const auto maskBytes = bytes::make_span(mask);
	const auto base = int(frame.size());
	frame.resize(base + size);
	auto out = bytes::make_detached_span(frame).subspan(base);
	for (auto i = 0; i != size; ++i) {
		out[i] = bytes::type(
			gsl::to_integer<quint8>(data[i])
			^ gsl::to_integer<quint8>(maskBytes[i % 4]));
	}
	_socket.write(frame);
}

void WssSocket::handleError(int errorCode) {
	if (!_upgraded && moveToNextHost()) {
		return;
	}
	if (!_upgraded) {
		
		QMutexLocker lock(&RelayMutex);
		SuspendedUntilByDc[std::abs(int(_dcId))] = crl::now() + kDcSuspendFor;
	}
	logError(errorCode, _socket.errorString());
	_error.fire({});
}

void WssSocket::timedOut() {
}

bool WssSocket::isGoodStartNonce(bytes::const_span nonce) {
	Expects(nonce.size() >= 2 * sizeof(uint32));
	const auto bytes = nonce.data();
	const auto zero = gsl::to_integer<uchar>(nonce.front());
	const auto first = qFromUnaligned<uint32>(bytes);
	const auto second = qFromUnaligned<uint32>(bytes + sizeof(uint32));
	const auto reserved01 = 0x000000EFU;
	const auto reserved11 = 0x44414548U;
	const auto reserved12 = 0x54534F50U;
	const auto reserved13 = 0x20544547U;
	const auto reserved14 = 0xEEEEEEEEU;
	const auto reserved15 = 0xDDDDDDDDU;
	const auto reserved16 = 0x02010316U;
	const auto reserved21 = 0x00000000U;
	return (zero != reserved01)
		&& (first != reserved11)
		&& (first != reserved12)
		&& (first != reserved13)
		&& (first != reserved14)
		&& (first != reserved15)
		&& (first != reserved16)
		&& (second != reserved21);
}

bool WssSocket::isConnected() {
	return _upgraded
		&& (_socket.state() == QAbstractSocket::ConnectedState);
}

bool WssSocket::hasBytesAvailable() {
	return !_readBuffer.isEmpty();
}

int64 WssSocket::read(bytes::span buffer) {
	const auto count = std::min(
		int64(buffer.size()),
		int64(_readBuffer.size()));
	if (count <= 0) {
		return 0;
	}
	bytes::copy(
		buffer,
		bytes::make_span(_readBuffer.constData(), count));
	_readBuffer.remove(0, int(count));
	return count;
}

void WssSocket::write(bytes::const_span prefix, bytes::const_span buffer) {
	Expects(!buffer.empty());

	
	if (prefix.empty()) {
		sendFrame(0x2, buffer);
		return;
	}
	auto combined = bytes::vector(prefix.size() + buffer.size());
	auto combinedBytes = bytes::make_span(combined);
	bytes::copy(combinedBytes, prefix);
	bytes::copy(combinedBytes.subspan(prefix.size()), buffer);
	sendFrame(0x2, bytes::make_span(combined));
}

int32 WssSocket::debugState() {
	return _socket.state();
}

QString WssSocket::debugPostfix() const {
	return u"WSS"_q;
}

} 
