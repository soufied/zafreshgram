/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include <atomic>
#include "mtproto/details/mtproto_client_hello_profile.h"

#include <array>

namespace MTP::details {

namespace {

std::atomic<int> ConfiguredProfile = int(ClientHelloProfileKind::Auto);

}
namespace {

constexpr auto kWithheldReason
	= "measured refused: 3/12 answered on a filtered network, 12/12 for "
	"every profile whose last extension is not a GREASE one carrying a zero "
	"byte, on the same relay in the same minutes";

constexpr auto kProfiles = std::array{
	ClientHelloProfileInfo{
		.profile = ClientHelloProfileKind::Yandex,
		.id = "yandex",
		.validation = ClientHelloProfileValidation::Claimed,
	},
	ClientHelloProfileInfo{
		.profile = ClientHelloProfileKind::ChromeModern,
		.id = "chrome_modern",
		.validation = ClientHelloProfileValidation::Validated,
		.captureSource = "local_chrome_headless_capture",
		.captureVersion = "Google Chrome 149.0.7827.155",
		.expectedJa4 = "t13d1516h2_8daaf6152771_d8a2da3f94cd",
		.withheld = true,
		.withheldReason = kWithheldReason,
	},
	ClientHelloProfileInfo{
		.profile = ClientHelloProfileKind::AndroidChrome,
		.id = "android_chrome",
		.validation = ClientHelloProfileValidation::Claimed,
		.withheld = true,
		.withheldReason = kWithheldReason,
	},
	ClientHelloProfileInfo{
		.profile = ClientHelloProfileKind::Firefox,
		.id = "firefox",
		.validation = ClientHelloProfileValidation::Claimed,
	},
	ClientHelloProfileInfo{
		.profile = ClientHelloProfileKind::FirefoxAndroid,
		.id = "firefox_android",
		.validation = ClientHelloProfileValidation::Claimed,
	},
	ClientHelloProfileInfo{
		.profile = ClientHelloProfileKind::AndroidOkHttp,
		.id = "android_okhttp",
		.validation = ClientHelloProfileValidation::Claimed,
	},
};

}

const ClientHelloProfileInfo &ClientHelloProfile(ClientHelloProfileKind profile) {
	const auto normalized = (profile == ClientHelloProfileKind::Auto
		|| profile == ClientHelloProfileKind::AutoRotate)
		? DefaultClientHelloProfile()
		: profile;
	for (const auto &info : kProfiles) {
		if (info.profile == normalized) {
			return info;
		}
	}
	return kProfiles[0];
}

ClientHelloProfileKind EffectiveClientHelloProfile(ClientHelloProfileKind profile) {
	const auto &info = ClientHelloProfile(profile);
	return info.withheld ? DefaultClientHelloProfile() : info.profile;
}

ClientHelloProfileKind DefaultClientHelloProfile() {
	return ClientHelloProfileKind::Yandex;
}

bool IsClientHelloProfileValidated(ClientHelloProfileKind profile) {
	return ClientHelloProfile(profile).validation
		== ClientHelloProfileValidation::Validated;
}

void SetConfiguredClientHelloProfile(ClientHelloProfileKind profile) {
	ConfiguredProfile = int(profile);
}

ClientHelloProfileKind ConfiguredClientHelloProfile() {
	return ClientHelloProfileKind(ConfiguredProfile.load());
}

}
