/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "mtproto/mtproto_proxy_data.h"

#include <optional>


namespace MTP::WebProxy {

[[nodiscard]] std::optional<ProxyData> SelfTestProxy();

void StartSelfTest();

}
