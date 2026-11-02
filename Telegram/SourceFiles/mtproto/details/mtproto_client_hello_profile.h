/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once


namespace MTP::details {

enum class ClientHelloProfileKind {
	Auto,
	Firefox,
	AndroidChrome,
	Yandex,
	FirefoxAndroid,
	AndroidOkHttp,
	AutoRotate,
	ChromeModern,
};

enum class ClientHelloProfileValidation {
	Claimed,
	Validated,
};

struct ClientHelloProfileInfo {
	ClientHelloProfileKind profile = ClientHelloProfileKind::Auto;
	const char *id = "";
	ClientHelloProfileValidation validation
		= ClientHelloProfileValidation::Claimed;
	const char *captureSource = "";
	const char *captureVersion = "";
	const char *expectedJa4 = "";
	bool withheld = false;
	const char *withheldReason = "";
};

[[nodiscard]] const ClientHelloProfileInfo &ClientHelloProfile(
	ClientHelloProfileKind profile);

[[nodiscard]] ClientHelloProfileKind EffectiveClientHelloProfile(
	ClientHelloProfileKind profile);

[[nodiscard]] ClientHelloProfileKind DefaultClientHelloProfile();

[[nodiscard]] bool IsClientHelloProfileValidated(ClientHelloProfileKind profile);

void SetConfiguredClientHelloProfile(ClientHelloProfileKind profile);
[[nodiscard]] ClientHelloProfileKind ConfiguredClientHelloProfile();

}
