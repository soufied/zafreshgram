/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/basic_types.h"
#include "base/bytes.h"
#include "base/timer.h"
#include "mtproto/details/mtproto_client_hello_profile.h"
#include "scheme.h"

#include <QtCore/QByteArray>

#include <optional>

namespace MTP::details {

struct SyntheticPskOffer {
	bytes::vector identity;
	uint32 obfuscatedTicketAge = 0;
	int binderLength = 0;
};

struct ClientHello {
	QByteArray data;
	QByteArray digest;
	TimeId timestamp = 0;
};

struct ClientHelloFragmentationPlan {
	int firstSize = 0;
	crl::time secondDelay = 0;

	[[nodiscard]] explicit operator bool() const {
		return firstSize > 0;
	}
};

enum class ClientHelloFragmentationMode {
	Off,
	Soft,
};

struct ClientHelloGenerationOptions {
	bool deterministic = false;
};

[[nodiscard]] MTPTlsClientHello PrepareClientHelloRules(
	ClientHelloProfileKind profile);

[[nodiscard]] ClientHello PrepareClientHello(
	const MTPTlsClientHello &rules,
	bytes::const_span domain,
	bytes::const_span key,
	ClientHelloProfileKind profile,
	std::optional<SyntheticPskOffer> pskOffer,
	ClientHelloGenerationOptions options = {});

void SetClientHelloFragmentation(ClientHelloFragmentationMode mode);
[[nodiscard]] ClientHelloFragmentationMode ConfiguredClientHelloFragmentation();

[[nodiscard]] ClientHelloFragmentationPlan PrepareClientHelloFragmentation(
	const QByteArray &data,
	ClientHelloFragmentationMode mode);

}
