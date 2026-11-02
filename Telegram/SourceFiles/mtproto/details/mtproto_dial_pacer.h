/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/basic_types.h"
#include "mtproto/mtproto_proxy_data.h"

#include <crl/crl_time.h>

namespace MTP::details {

class DialLease final {
public:
	DialLease() = default;
	DialLease(const DialLease &other) = delete;
	DialLease &operator=(const DialLease &other) = delete;
	DialLease(DialLease &&other) noexcept;
	DialLease &operator=(DialLease &&other) noexcept;
	~DialLease();

	[[nodiscard]] crl::time delay() const;

	void proven();

	void release();

	void cancel();

private:
	friend DialLease ReserveDial(const ProxyData &proxy);

	enum class Verdict {
		Proven,
		Failed,
		Cancelled,
	};

	DialLease(QString key, crl::time delay);
	void finish(Verdict verdict);

	QString _key;
	crl::time _delay = 0;

};

[[nodiscard]] DialLease ReserveDial(const ProxyData &proxy);

}
