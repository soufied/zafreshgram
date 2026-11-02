/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "mtproto/details/mtproto_dial_pacer.h"

#include <QtCore/QMutex>

#include <algorithm>
#include <map>
#include <utility>

namespace MTP::details {
namespace {

constexpr auto kDialSpacing = crl::time(50);

constexpr auto kFailuresBeforeBackoff = 4;
constexpr auto kFailureSpacingStep = crl::time(500);
constexpr auto kFailureSpacingMax = crl::time(4000);

constexpr auto kMaxQueueDelay = crl::time(30 * 1000);

constexpr auto kFailureMemory = crl::time(5 * 60 * 1000);

struct DialState {
	int failures = 0;
	crl::time nextFreeAt = 0;
	crl::time lastFailureAt = 0;
};

[[nodiscard]] QMutex &DialMutex() {
	static auto result = QMutex();
	return result;
}

[[nodiscard]] std::map<QString, DialState> &DialStates() {
	static auto result = std::map<QString, DialState>();
	return result;
}

[[nodiscard]] crl::time SpacingFor(int failures) {
	if (failures < kFailuresBeforeBackoff) {
		return kDialSpacing;
	}
	const auto over = failures - kFailuresBeforeBackoff + 1;
	return std::min(kFailureSpacingMax, over * kFailureSpacingStep);
}

[[nodiscard]] bool Forgettable(const DialState &state, crl::time now) {
	return state.nextFreeAt <= now
		&& (!state.failures
			|| (now - state.lastFailureAt >= kFailureMemory));
}

}

DialLease::DialLease(QString key, crl::time delay)
: _key(std::move(key))
, _delay(delay) {
}

DialLease::DialLease(DialLease &&other) noexcept
: _key(std::exchange(other._key, QString()))
, _delay(std::exchange(other._delay, crl::time(0))) {
}

DialLease &DialLease::operator=(DialLease &&other) noexcept {
	if (this != &other) {
		finish(Verdict::Cancelled);
		_key = std::exchange(other._key, QString());
		_delay = std::exchange(other._delay, crl::time(0));
	}
	return *this;
}

DialLease::~DialLease() {
	finish(Verdict::Cancelled);
}

crl::time DialLease::delay() const {
	return _delay;
}

void DialLease::proven() {
	finish(Verdict::Proven);
}

void DialLease::release() {
	finish(Verdict::Failed);
}

void DialLease::cancel() {
	finish(Verdict::Cancelled);
}

void DialLease::finish(Verdict verdict) {
	const auto key = std::exchange(_key, QString());
	_delay = 0;
	if (key.isEmpty()) {
		return;
	}
	const auto now = crl::now();
	QMutexLocker lock(&DialMutex());
	auto &states = DialStates();
	const auto i = states.find(key);
	if (i == end(states)) {
		return;
	}
	auto &state = i->second;
	if (verdict == Verdict::Proven) {
		state.failures = 0;
		state.lastFailureAt = 0;
	} else if (verdict == Verdict::Failed) {
		++state.failures;
		state.lastFailureAt = now;
	}
	if (Forgettable(state, now)) {
		states.erase(i);
	}
}

DialLease ReserveDial(const ProxyData &proxy) {
	if (proxy.type != ProxyData::Type::Mtproto) {
		return DialLease();
	}
	const auto key = u"%1:%2"_q.arg(proxy.host).arg(proxy.port);
	const auto now = crl::now();
	auto at = now;
	{
		QMutexLocker lock(&DialMutex());
		auto &state = DialStates()[key];
		at = std::max(now, state.nextFreeAt);
		at = std::min(at, now + kMaxQueueDelay);
		state.nextFreeAt = at + SpacingFor(state.failures);
	}
	return DialLease(key, at - now);
}

}
