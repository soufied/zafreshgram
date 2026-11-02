/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/basic_types.h"

#include <array>
#include <deque>
#include <map>
#include <optional>


namespace MTP::WebProxy {

enum class StreamClass : uchar {
	Interactive,
	Download,
	Upload,
};
inline constexpr auto kStreamClassCount = 3;

[[nodiscard]] const char *StreamClassName(StreamClass value);

struct UplinkLimits {
	int frameSize = 64 * 1024;

	int64 uploadInFlight = 1024 * 1024;

	int minUploadFrame = 16 * 1024;

	int priorityBurst = 16;
};

struct UplinkGrant {
	uint32 streamId = 0;
	int maxBytes = 0;
};

struct UplinkStats {
	std::array<int, kStreamClassCount> streams = {};
	std::array<int, kStreamClassCount> ready = {};
	int64 uploadInFlight = 0;
};

class UplinkScheduler final {
public:
	explicit UplinkScheduler(UplinkLimits limits = {});

	void add(uint32 streamId, StreamClass streamClass);
	void remove(uint32 streamId);
	void clear();

	void markReady(uint32 streamId);

	[[nodiscard]] std::optional<UplinkGrant> next();

	void sent(uint32 streamId, int bytes);
	void acknowledged(uint32 streamId, int64 bytes);

	void setUploadInFlight(int64 bytes);

	[[nodiscard]] UplinkStats stats() const;
	[[nodiscard]] bool uploadBlocked() const;
	[[nodiscard]] const UplinkLimits &limits() const {
		return _limits;
	}

private:
	struct Entry {
		StreamClass streamClass = StreamClass::Interactive;
		int64 inFlight = 0;
		bool ready = false;
	};

	[[nodiscard]] bool hasReady(StreamClass streamClass);
	[[nodiscard]] std::optional<uint32> popReady(StreamClass streamClass);

	UplinkLimits _limits;
	std::map<uint32, Entry> _streams;
	std::array<std::deque<uint32>, kStreamClassCount> _ready;
	int64 _uploadInFlight = 0;
	int _uploadSkipped = 0;

};

struct DownlinkLimits {
	int64 streamWindow = 4 * 1024 * 1024;

	int64 downloadBudget = 2 * 1024 * 1024;

	int64 downloadMin = 128 * 1024;
	int64 downloadMax = 4 * 1024 * 1024;
};

[[nodiscard]] int64 DownlinkCreditTarget(
	const DownlinkLimits &limits,
	StreamClass streamClass,
	int downloadStreams);

[[nodiscard]] int64 DownlinkCreditRelease(
	int64 relayCredit,
	int64 withheld,
	int64 target);

struct AdaptiveWindowLimits {
	int64 initial = 1024 * 1024;
	int64 min = 64 * 1024;
	int64 max = 8 * 1024 * 1024;

	int64 targetDelay = 100;

	int64 blindMax = 2 * 1024 * 1024;

	int64 baseBucket = 10'000;
	int64 baseHistory = 60'000;

	int64 maxGrowthPercent = 200;
	int64 shrinkPercent = 75;
};

struct FlowSample {
	int64 now = 0;
	int64 interval = 0;

	int64 bytes = 0;

	bool windowLimited = false;

	std::optional<int64> delay;

	bool probing = false;

	int64 extraDelay = 0;
};

class AdaptiveWindow final {
public:
	explicit AdaptiveWindow(AdaptiveWindowLimits limits);

	void update(const FlowSample &sample);

	void hold(const FlowSample &sample);

	void reset();

	[[nodiscard]] int64 window() const {
		return _window;
	}
	[[nodiscard]] int64 rate() const {
		return _rate;
	}
	[[nodiscard]] int64 target() const {
		return _target;
	}
	[[nodiscard]] std::optional<int64> baseDelay() const;
	[[nodiscard]] std::optional<int64> lastDelay() const {
		return _lastDelay;
	}
	[[nodiscard]] int shrinks() const {
		return _shrinks;
	}
	[[nodiscard]] const AdaptiveWindowLimits &limits() const {
		return _limits;
	}

private:
	void noteDelay(int64 now, std::optional<int64> delay);

	AdaptiveWindowLimits _limits;
	std::deque<std::pair<int64, int64>> _base;
	std::optional<int64> _lastDelay;
	int64 _window = 0;
	int64 _target = 0;
	int64 _rate = 0;
	int _shrinks = 0;

};

struct CarrierHealth {
	bool connected = false;

	int64 lastDownlinkAt = 0;

	int64 lastCreditAt = 0;

	int64 unackedBytes = 0;

	int64 outstandingSince = 0;
};

struct StreamHealth {
	bool open = false;

	int64 queuedBytes = 0;

	int64 unackedBytes = 0;

	int64 lastReceivedAt = 0;

	int64 deliveredAt = 0;
};

struct LivenessLimits {
	int64 carrierStall = 20'000;

	int64 quietReply = 8'000;

	int64 busyReply = 30'000;

	int64 maxWait = 64'000;

	int64 busyWindow = 2'000;

	int64 recheck = 2'000;
};

enum class ReceiveVerdict : uchar {
	Wait,
	Fail,
};

enum class ReceiveReason : uchar {
	StreamClosed,
	CarrierDown,
	MaxWait,
	CarrierStalled,
	Queued,
	ReplyQueued,
	ReplyPending,
	ReplyMissing,
	ReplyTimeout,
};

struct ReceiveDecision {
	ReceiveVerdict verdict = ReceiveVerdict::Fail;
	ReceiveReason reason = ReceiveReason::StreamClosed;
	int64 waitMore = 0;
};

[[nodiscard]] const char *ReceiveReasonName(ReceiveReason reason);

[[nodiscard]] bool CarrierStalled(
	int64 now,
	const CarrierHealth &carrier,
	const LivenessLimits &limits);

[[nodiscard]] ReceiveDecision DecideReceiveWait(
	int64 now,
	int64 waitStartedAt,
	const CarrierHealth &carrier,
	const StreamHealth &stream,
	const LivenessLimits &limits);

}
