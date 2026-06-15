#include <benchmark/benchmark.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

import mo_yanxi.call_stream;

#ifndef __cpp_lib_move_only_function
#error "call_stream benchmark requires std::move_only_function support."
#endif

namespace {

constexpr std::size_t kSmallCallCount = 64;
constexpr std::size_t kLargeCallCount = 1024;
constexpr std::size_t kConstructionBatch = 64;

enum class workload {
	stateless_short,
	payload_short,
	stateless_complex,
	payload_complex,
	mixed
};

enum class construction_payload {
	trivial_copyable,
	manual_move,
	mixed
};

struct bench_context {
	std::uint64_t value = 0x243f6a8885a308d3ULL;
	std::array<std::uint64_t, 8> lanes = {
		0x13198a2e03707344ULL,
		0xa4093822299f31d0ULL,
		0x082efa98ec4e6c89ULL,
		0x452821e638d01377ULL,
		0xbe5466cf34e90c6cULL,
		0xc0ac29b7c97c50ddULL,
		0x3f84d5b5b5470917ULL,
		0x9216d5d98979fb1bULL
	};
};

alignas(64) std::uint64_t g_void_sink = 0x9e3779b97f4a7c15ULL;

[[nodiscard]] constexpr std::uint64_t seed_for(std::size_t index) noexcept {
	return 0xd1b54a32d192ed03ULL + index * 0x9e3779b97f4a7c15ULL;
}

[[nodiscard]] constexpr std::uint64_t mix(std::uint64_t value) noexcept {
	value ^= value >> 30;
	value *= 0xbf58476d1ce4e5b9ULL;
	value ^= value >> 27;
	value *= 0x94d049bb133111ebULL;
	value ^= value >> 31;
	return value;
}

constexpr std::array<std::uint64_t, 4> kStatelessComplexLanes = {
	mix(0x01ULL),
	mix(0x02ULL),
	mix(0x03ULL),
	mix(0x04ULL)
};

void consume_void_short(std::uint64_t value) noexcept {
	g_void_sink += value;
}

void consume_void_complex(std::uint64_t seed, const std::array<std::uint64_t, 4>& lanes) noexcept {
	std::uint64_t value = g_void_sink + seed;
	for(std::uint64_t lane : lanes) {
		value = mix(value + lane);
	}
	g_void_sink ^= value;
}

void consume_context_short(bench_context& context, std::uint64_t value) noexcept {
	context.value += value;
	context.lanes[context.value & 7U] ^= context.value;
}

void consume_context_complex(bench_context& context, std::uint64_t seed) noexcept {
	std::uint64_t value = context.value + seed;
	for(std::size_t i = 0; i < context.lanes.size(); ++i) {
		value = mix(value ^ (context.lanes[i] + seed_for(i)));
		context.lanes[i] = value;
	}
	context.value ^= value;
}

void consume_context_complex(bench_context& context,
                             std::uint64_t seed,
                             const std::array<std::uint64_t, 4>& lanes) noexcept {
	std::uint64_t value = context.value + seed;
	for(std::uint64_t lane : lanes) {
		value = mix(value + lane);
	}
	context.value ^= value;
	context.lanes[value & 7U] += value;
}

[[nodiscard]] std::array<std::uint64_t, 4> payload_lanes(std::uint64_t seed) noexcept {
	return {
		mix(seed + 0x01ULL),
		mix(seed + 0x02ULL),
		mix(seed + 0x03ULL),
		mix(seed + 0x04ULL)
	};
}

struct void0_stateless_short {
	void operator()() const noexcept {
		consume_void_short(1);
	}
};

struct void0_payload_short {
	std::uint64_t value;

	explicit void0_payload_short(std::uint64_t seed) noexcept
		: value(seed) {
	}

	void operator()() const noexcept {
		consume_void_short(value);
	}
};

struct void0_stateless_complex {
	void operator()() const noexcept {
		consume_void_complex(0x517cc1b727220a95ULL, kStatelessComplexLanes);
	}
};

struct void0_payload_complex {
	std::uint64_t seed;
	std::array<std::uint64_t, 4> lanes;

	explicit void0_payload_complex(std::uint64_t seed_in) noexcept
		: seed(seed_in), lanes(payload_lanes(seed_in)) {
	}

	void operator()() const noexcept {
		consume_void_complex(seed, lanes);
	}
};

struct context_stateless_short {
	void operator()(bench_context& context) const noexcept {
		consume_context_short(context, 1);
	}
};

struct context_payload_short {
	std::uint64_t value;

	explicit context_payload_short(std::uint64_t seed) noexcept
		: value(seed) {
	}

	void operator()(bench_context& context) const noexcept {
		consume_context_short(context, value);
	}
};

struct context_stateless_complex {
	void operator()(bench_context& context) const noexcept {
		consume_context_complex(context, 0x94d049bb133111ebULL);
	}
};

struct context_payload_complex {
	std::uint64_t seed;
	std::array<std::uint64_t, 4> lanes;

	explicit context_payload_complex(std::uint64_t seed_in) noexcept
		: seed(seed_in), lanes(payload_lanes(seed_in)) {
	}

	void operator()(bench_context& context) const noexcept {
		consume_context_complex(context, seed, lanes);
	}
};

struct context_arg_stateless_short {
	void operator()(bench_context& context, std::uint64_t arg) const noexcept {
		consume_context_short(context, arg);
	}
};

struct context_arg_payload_short {
	std::uint64_t value;

	explicit context_arg_payload_short(std::uint64_t seed) noexcept
		: value(seed) {
	}

	void operator()(bench_context& context, std::uint64_t arg) const noexcept {
		consume_context_short(context, arg + value);
	}
};

struct context_arg_stateless_complex {
	void operator()(bench_context& context, std::uint64_t arg) const noexcept {
		consume_context_complex(context, arg + 0x632be59bd9b4e019ULL);
	}
};

struct context_arg_payload_complex {
	std::uint64_t seed;
	std::array<std::uint64_t, 4> lanes;

	explicit context_arg_payload_complex(std::uint64_t seed_in) noexcept
		: seed(seed_in), lanes(payload_lanes(seed_in)) {
	}

	void operator()(bench_context& context, std::uint64_t arg) const noexcept {
		consume_context_complex(context, arg + seed, lanes);
	}
};

struct result_stateless_short {
	[[nodiscard]] std::uint64_t operator()(std::uint64_t value) const noexcept {
		return value + 1;
	}
};

struct result_payload_short {
	std::uint64_t value;

	explicit result_payload_short(std::uint64_t seed) noexcept
		: value(seed) {
	}

	[[nodiscard]] std::uint64_t operator()(std::uint64_t arg) const noexcept {
		return arg + value;
	}
};

struct result_stateless_complex {
	[[nodiscard]] std::uint64_t operator()(std::uint64_t value) const noexcept {
		for(std::uint64_t lane : kStatelessComplexLanes) {
			value = mix(value + lane);
		}
		return value;
	}
};

struct result_payload_complex {
	std::uint64_t seed;
	std::array<std::uint64_t, 4> lanes;

	explicit result_payload_complex(std::uint64_t seed_in) noexcept
		: seed(seed_in), lanes(payload_lanes(seed_in)) {
	}

	[[nodiscard]] std::uint64_t operator()(std::uint64_t arg) const noexcept {
		std::uint64_t value = arg + seed;
		for(std::uint64_t lane : lanes) {
			value = mix(value + lane);
		}
		return value;
	}
};

struct construction_trivial_call {
	std::uint64_t value;

	explicit construction_trivial_call(std::uint64_t seed) noexcept
		: value(seed) {
	}

	void operator()() const noexcept {
		consume_void_short(value);
	}
};

static_assert(std::is_trivially_copyable_v<construction_trivial_call>);

struct construction_manual_move_call {
	std::uint64_t value;

	explicit construction_manual_move_call(std::uint64_t seed) noexcept
		: value(seed) {
	}

	construction_manual_move_call(const construction_manual_move_call&) = delete;
	construction_manual_move_call& operator=(const construction_manual_move_call&) = delete;

	construction_manual_move_call(construction_manual_move_call&& other) noexcept
		: value(std::exchange(other.value, 0)) {
	}

	construction_manual_move_call& operator=(construction_manual_move_call&& other) noexcept {
		value = std::exchange(other.value, 0);
		return *this;
	}

	void operator()() const noexcept {
		consume_void_short(value);
	}
};

static_assert(!std::is_trivially_copyable_v<construction_manual_move_call>);
static_assert(std::is_nothrow_move_constructible_v<construction_manual_move_call>);

void record_items(benchmark::State& state, std::size_t calls_per_iteration) {
	state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(calls_per_iteration));
}

void observe_context(bench_context& context) {
	auto* lanes = context.lanes.data();
	benchmark::DoNotOptimize(context.value);
	benchmark::DoNotOptimize(lanes);
	benchmark::ClobberMemory();
}

void observe_scalar(std::uint64_t& value) {
	benchmark::DoNotOptimize(value);
	benchmark::ClobberMemory();
}

template <workload Workload, mo_yanxi::call_stream_exception_policy ExceptionPolicy>
void append_void0(mo_yanxi::basic_call_stream_impl<ExceptionPolicy, std::allocator<std::byte>, void>& calls,
                  std::size_t index) {
	if constexpr(Workload == workload::stateless_short) {
		calls.template emplace_back<void0_stateless_short>();
	} else if constexpr(Workload == workload::payload_short) {
		calls.template emplace_back<void0_payload_short>(seed_for(index));
	} else if constexpr(Workload == workload::stateless_complex) {
		calls.template emplace_back<void0_stateless_complex>();
	} else if constexpr(Workload == workload::payload_complex) {
		calls.template emplace_back<void0_payload_complex>(seed_for(index));
	} else {
		switch(index & 3U) {
		case 0: calls.template emplace_back<void0_stateless_short>(); break;
		case 1: calls.template emplace_back<void0_payload_short>(seed_for(index)); break;
		case 2: calls.template emplace_back<void0_stateless_complex>(); break;
		default: calls.template emplace_back<void0_payload_complex>(seed_for(index)); break;
		}
	}
}

template <workload Workload>
void append_void0(std::vector<std::move_only_function<void()>>& calls, std::size_t index) {
	if constexpr(Workload == workload::stateless_short) {
		calls.emplace_back(void0_stateless_short{});
	} else if constexpr(Workload == workload::payload_short) {
		calls.emplace_back(void0_payload_short{seed_for(index)});
	} else if constexpr(Workload == workload::stateless_complex) {
		calls.emplace_back(void0_stateless_complex{});
	} else if constexpr(Workload == workload::payload_complex) {
		calls.emplace_back(void0_payload_complex{seed_for(index)});
	} else {
		switch(index & 3U) {
		case 0: calls.emplace_back(void0_stateless_short{}); break;
		case 1: calls.emplace_back(void0_payload_short{seed_for(index)}); break;
		case 2: calls.emplace_back(void0_stateless_complex{}); break;
		default: calls.emplace_back(void0_payload_complex{seed_for(index)}); break;
		}
	}
}

template <workload Workload, mo_yanxi::call_stream_exception_policy ExceptionPolicy>
void append_context(
	mo_yanxi::basic_call_stream_impl<ExceptionPolicy, std::allocator<std::byte>, void, bench_context&>& calls,
	std::size_t index) {
	if constexpr(Workload == workload::stateless_short) {
		calls.template emplace_back<context_stateless_short>();
	} else if constexpr(Workload == workload::payload_short) {
		calls.template emplace_back<context_payload_short>(seed_for(index));
	} else if constexpr(Workload == workload::stateless_complex) {
		calls.template emplace_back<context_stateless_complex>();
	} else if constexpr(Workload == workload::payload_complex) {
		calls.template emplace_back<context_payload_complex>(seed_for(index));
	} else {
		switch(index & 3U) {
		case 0: calls.template emplace_back<context_stateless_short>(); break;
		case 1: calls.template emplace_back<context_payload_short>(seed_for(index)); break;
		case 2: calls.template emplace_back<context_stateless_complex>(); break;
		default: calls.template emplace_back<context_payload_complex>(seed_for(index)); break;
		}
	}
}

template <workload Workload>
void append_context(std::vector<std::move_only_function<void(bench_context&)>>& calls, std::size_t index) {
	if constexpr(Workload == workload::stateless_short) {
		calls.emplace_back(context_stateless_short{});
	} else if constexpr(Workload == workload::payload_short) {
		calls.emplace_back(context_payload_short{seed_for(index)});
	} else if constexpr(Workload == workload::stateless_complex) {
		calls.emplace_back(context_stateless_complex{});
	} else if constexpr(Workload == workload::payload_complex) {
		calls.emplace_back(context_payload_complex{seed_for(index)});
	} else {
		switch(index & 3U) {
		case 0: calls.emplace_back(context_stateless_short{}); break;
		case 1: calls.emplace_back(context_payload_short{seed_for(index)}); break;
		case 2: calls.emplace_back(context_stateless_complex{}); break;
		default: calls.emplace_back(context_payload_complex{seed_for(index)}); break;
		}
	}
}

template <workload Workload, mo_yanxi::call_stream_exception_policy ExceptionPolicy>
void append_context_arg(mo_yanxi::basic_call_stream_impl<
	                        ExceptionPolicy,
	                        std::allocator<std::byte>,
	                        void,
	                        bench_context&,
	                        std::uint64_t>& calls,
                        std::size_t index) {
	if constexpr(Workload == workload::stateless_short) {
		calls.template emplace_back<context_arg_stateless_short>();
	} else if constexpr(Workload == workload::payload_short) {
		calls.template emplace_back<context_arg_payload_short>(seed_for(index));
	} else if constexpr(Workload == workload::stateless_complex) {
		calls.template emplace_back<context_arg_stateless_complex>();
	} else if constexpr(Workload == workload::payload_complex) {
		calls.template emplace_back<context_arg_payload_complex>(seed_for(index));
	} else {
		switch(index & 3U) {
		case 0: calls.template emplace_back<context_arg_stateless_short>(); break;
		case 1: calls.template emplace_back<context_arg_payload_short>(seed_for(index)); break;
		case 2: calls.template emplace_back<context_arg_stateless_complex>(); break;
		default: calls.template emplace_back<context_arg_payload_complex>(seed_for(index)); break;
		}
	}
}

template <workload Workload>
void append_context_arg(std::vector<std::move_only_function<void(bench_context&, std::uint64_t)>>& calls,
                        std::size_t index) {
	if constexpr(Workload == workload::stateless_short) {
		calls.emplace_back(context_arg_stateless_short{});
	} else if constexpr(Workload == workload::payload_short) {
		calls.emplace_back(context_arg_payload_short{seed_for(index)});
	} else if constexpr(Workload == workload::stateless_complex) {
		calls.emplace_back(context_arg_stateless_complex{});
	} else if constexpr(Workload == workload::payload_complex) {
		calls.emplace_back(context_arg_payload_complex{seed_for(index)});
	} else {
		switch(index & 3U) {
		case 0: calls.emplace_back(context_arg_stateless_short{}); break;
		case 1: calls.emplace_back(context_arg_payload_short{seed_for(index)}); break;
		case 2: calls.emplace_back(context_arg_stateless_complex{}); break;
		default: calls.emplace_back(context_arg_payload_complex{seed_for(index)}); break;
		}
	}
}

template <workload Workload, mo_yanxi::call_stream_exception_policy ExceptionPolicy>
void append_result(
	mo_yanxi::basic_call_stream_impl<ExceptionPolicy, std::allocator<std::byte>, std::uint64_t, std::uint64_t>& calls,
	std::size_t index) {
	if constexpr(Workload == workload::stateless_short) {
		calls.template emplace_back<result_stateless_short>();
	} else if constexpr(Workload == workload::payload_short) {
		calls.template emplace_back<result_payload_short>(seed_for(index));
	} else if constexpr(Workload == workload::stateless_complex) {
		calls.template emplace_back<result_stateless_complex>();
	} else if constexpr(Workload == workload::payload_complex) {
		calls.template emplace_back<result_payload_complex>(seed_for(index));
	} else {
		switch(index & 3U) {
		case 0: calls.template emplace_back<result_stateless_short>(); break;
		case 1: calls.template emplace_back<result_payload_short>(seed_for(index)); break;
		case 2: calls.template emplace_back<result_stateless_complex>(); break;
		default: calls.template emplace_back<result_payload_complex>(seed_for(index)); break;
		}
	}
}

template <workload Workload>
void append_result(std::vector<std::move_only_function<std::uint64_t(std::uint64_t)>>& calls, std::size_t index) {
	if constexpr(Workload == workload::stateless_short) {
		calls.emplace_back(result_stateless_short{});
	} else if constexpr(Workload == workload::payload_short) {
		calls.emplace_back(result_payload_short{seed_for(index)});
	} else if constexpr(Workload == workload::stateless_complex) {
		calls.emplace_back(result_stateless_complex{});
	} else if constexpr(Workload == workload::payload_complex) {
		calls.emplace_back(result_payload_complex{seed_for(index)});
	} else {
		switch(index & 3U) {
		case 0: calls.emplace_back(result_stateless_short{}); break;
		case 1: calls.emplace_back(result_payload_short{seed_for(index)}); break;
		case 2: calls.emplace_back(result_stateless_complex{}); break;
		default: calls.emplace_back(result_payload_complex{seed_for(index)}); break;
		}
	}
}

template <construction_payload Payload>
void append_construction_call(mo_yanxi::call_stream<void() noexcept>& calls, std::size_t index) {
	if constexpr(Payload == construction_payload::trivial_copyable) {
		calls.template emplace_back<construction_trivial_call>(seed_for(index));
	} else if constexpr(Payload == construction_payload::manual_move) {
		calls.template emplace_back<construction_manual_move_call>(seed_for(index));
	} else {
		if((index & 1U) == 0) {
			calls.template emplace_back<construction_trivial_call>(seed_for(index));
		} else {
			calls.template emplace_back<construction_manual_move_call>(seed_for(index));
		}
	}
}

template <construction_payload Payload>
void append_construction_call(std::vector<std::move_only_function<void()>>& calls, std::size_t index) {
	if constexpr(Payload == construction_payload::trivial_copyable) {
		calls.emplace_back(construction_trivial_call{seed_for(index)});
	} else if constexpr(Payload == construction_payload::manual_move) {
		calls.emplace_back(construction_manual_move_call{seed_for(index)});
	} else {
		if((index & 1U) == 0) {
			calls.emplace_back(construction_trivial_call{seed_for(index)});
		} else {
			calls.emplace_back(construction_manual_move_call{seed_for(index)});
		}
	}
}

template <typename PayloadT>
[[nodiscard]] std::size_t construction_stream_bytes_per_call() {
	static const std::size_t bytes = [] {
		mo_yanxi::call_stream<void() noexcept> calls;
		calls.template emplace_back<PayloadT>(seed_for(0));
		return calls.size();
	}();
	return bytes;
}

template <construction_payload Payload>
[[nodiscard]] std::size_t construction_stream_reserve_bytes(std::size_t count) {
	const auto trivial_bytes = construction_stream_bytes_per_call<construction_trivial_call>();
	const auto manual_move_bytes = construction_stream_bytes_per_call<construction_manual_move_call>();
	if constexpr(Payload == construction_payload::trivial_copyable) {
		return count * trivial_bytes;
	} else if constexpr(Payload == construction_payload::manual_move) {
		return count * manual_move_bytes;
	} else {
		return ((count + 1) / 2) * trivial_bytes + (count / 2) * manual_move_bytes;
	}
}

template <construction_payload Payload>
void append_construction_calls(auto& calls, std::size_t count) {
	for(std::size_t i = 0; i < count; ++i) {
		append_construction_call<Payload>(calls, i);
	}
}

template <construction_payload Payload>
void reserve_construction_calls(mo_yanxi::call_stream<void() noexcept>& calls, std::size_t count) {
	calls.reserve(construction_stream_reserve_bytes<Payload>(count));
}

template <construction_payload Payload>
void reserve_construction_calls(std::vector<std::move_only_function<void()>>& calls, std::size_t count) {
	calls.reserve(count);
}

void observe_construction_calls(mo_yanxi::call_stream<void() noexcept>& calls) {
	calls.reset_and_execute();
	observe_scalar(g_void_sink);
}

void observe_construction_calls(std::vector<std::move_only_function<void()>>& calls) {
	for(auto& call : calls) {
		call();
	}
	observe_scalar(g_void_sink);
}

template <construction_payload Payload>
void bm_build_call_stream_from_zero(benchmark::State& state) {
	const auto count = static_cast<std::size_t>(state.range(0));

	for(auto _ : state) {
		(void)_;
		double elapsed = 0.0;
		for(std::size_t batch = 0; batch < kConstructionBatch; ++batch) {
			const auto start = std::chrono::steady_clock::now();
			mo_yanxi::call_stream<void() noexcept> calls;
			append_construction_calls<Payload>(calls, count);
			benchmark::DoNotOptimize(calls);
			benchmark::ClobberMemory();
			const auto stop = std::chrono::steady_clock::now();
			elapsed += std::chrono::duration<double>(stop - start).count();
			observe_construction_calls(calls);
		}
		state.SetIterationTime(elapsed / static_cast<double>(kConstructionBatch));
	}

	record_items(state, count);
}

template <construction_payload Payload>
void bm_build_vector_from_zero(benchmark::State& state) {
	const auto count = static_cast<std::size_t>(state.range(0));

	for(auto _ : state) {
		(void)_;
		double elapsed = 0.0;
		for(std::size_t batch = 0; batch < kConstructionBatch; ++batch) {
			const auto start = std::chrono::steady_clock::now();
			std::vector<std::move_only_function<void()>> calls;
			append_construction_calls<Payload>(calls, count);
			benchmark::DoNotOptimize(calls);
			benchmark::ClobberMemory();
			const auto stop = std::chrono::steady_clock::now();
			elapsed += std::chrono::duration<double>(stop - start).count();
			observe_construction_calls(calls);
		}
		state.SetIterationTime(elapsed / static_cast<double>(kConstructionBatch));
	}

	record_items(state, count);
}

template <construction_payload Payload>
void bm_build_call_stream_reserved(benchmark::State& state) {
	const auto count = static_cast<std::size_t>(state.range(0));

	for(auto _ : state) {
		(void)_;
		double elapsed = 0.0;
		for(std::size_t batch = 0; batch < kConstructionBatch; ++batch) {
			const auto start = std::chrono::steady_clock::now();
			mo_yanxi::call_stream<void() noexcept> calls;
			reserve_construction_calls<Payload>(calls, count);
			append_construction_calls<Payload>(calls, count);
			benchmark::DoNotOptimize(calls);
			benchmark::ClobberMemory();
			const auto stop = std::chrono::steady_clock::now();
			elapsed += std::chrono::duration<double>(stop - start).count();
			observe_construction_calls(calls);
		}
		state.SetIterationTime(elapsed / static_cast<double>(kConstructionBatch));
	}

	record_items(state, count);
}

template <construction_payload Payload>
void bm_build_vector_reserved(benchmark::State& state) {
	const auto count = static_cast<std::size_t>(state.range(0));

	for(auto _ : state) {
		(void)_;
		double elapsed = 0.0;
		for(std::size_t batch = 0; batch < kConstructionBatch; ++batch) {
			const auto start = std::chrono::steady_clock::now();
			std::vector<std::move_only_function<void()>> calls;
			reserve_construction_calls<Payload>(calls, count);
			append_construction_calls<Payload>(calls, count);
			benchmark::DoNotOptimize(calls);
			benchmark::ClobberMemory();
			const auto stop = std::chrono::steady_clock::now();
			elapsed += std::chrono::duration<double>(stop - start).count();
			observe_construction_calls(calls);
		}
		state.SetIterationTime(elapsed / static_cast<double>(kConstructionBatch));
	}

	record_items(state, count);
}

template <construction_payload Payload>
void bm_build_call_stream_repeated(benchmark::State& state) {
	const auto count = static_cast<std::size_t>(state.range(0));
	mo_yanxi::call_stream<void() noexcept> calls;
	append_construction_calls<Payload>(calls, count);
	calls.clear();

	for(auto _ : state) {
		(void)_;
		double elapsed = 0.0;
		for(std::size_t batch = 0; batch < kConstructionBatch; ++batch) {
			const auto start = std::chrono::steady_clock::now();
			append_construction_calls<Payload>(calls, count);
			benchmark::DoNotOptimize(calls);
			benchmark::ClobberMemory();
			const auto stop = std::chrono::steady_clock::now();
			elapsed += std::chrono::duration<double>(stop - start).count();
			observe_construction_calls(calls);
			calls.clear();
		}
		state.SetIterationTime(elapsed / static_cast<double>(kConstructionBatch));
	}

	record_items(state, count);
}

template <construction_payload Payload>
void bm_build_vector_repeated(benchmark::State& state) {
	const auto count = static_cast<std::size_t>(state.range(0));
	std::vector<std::move_only_function<void()>> calls;
	append_construction_calls<Payload>(calls, count);
	calls.clear();

	for(auto _ : state) {
		(void)_;
		double elapsed = 0.0;
		for(std::size_t batch = 0; batch < kConstructionBatch; ++batch) {
			const auto start = std::chrono::steady_clock::now();
			append_construction_calls<Payload>(calls, count);
			benchmark::DoNotOptimize(calls);
			benchmark::ClobberMemory();
			const auto stop = std::chrono::steady_clock::now();
			elapsed += std::chrono::duration<double>(stop - start).count();
			observe_construction_calls(calls);
			calls.clear();
		}
		state.SetIterationTime(elapsed / static_cast<double>(kConstructionBatch));
	}

	record_items(state, count);
}

template <typename Calls, workload Workload, typename Append>
Calls make_calls(std::size_t count, Append append) {
	Calls calls;
	if constexpr(requires(Calls& target, std::size_t n) { target.reserve(n); }) {
		calls.reserve(count);
	}

	for(std::size_t i = 0; i < count; ++i) {
		append(calls, i);
	}
	return calls;
}

template <typename Stream, workload Workload>
void bm_call_stream_void0_impl(benchmark::State& state) {
	const auto count = static_cast<std::size_t>(state.range(0));
	auto calls = make_calls<Stream, Workload>(
		count,
		[](auto& target, std::size_t index) { append_void0<Workload>(target, index); });

	for(auto _ : state) {
		(void)_;
		calls.reset_and_execute();
		observe_scalar(g_void_sink);
	}

	record_items(state, count);
}

template <workload Workload>
void bm_call_stream_void0(benchmark::State& state) {
	bm_call_stream_void0_impl<mo_yanxi::call_stream<void() noexcept>, Workload>(state);
}

template <workload Workload>
void bm_call_stream_allow_exception_void0(benchmark::State& state) {
	bm_call_stream_void0_impl<mo_yanxi::call_stream<void()>, Workload>(state);
}

template <workload Workload>
void bm_vector_void0(benchmark::State& state) {
	const auto count = static_cast<std::size_t>(state.range(0));
	auto calls = make_calls<std::vector<std::move_only_function<void()>>, Workload>(
		count,
		[](auto& target, std::size_t index) { append_void0<Workload>(target, index); });

	for(auto _ : state) {
		(void)_;
		for(auto& call : calls) {
			call();
		}
		observe_scalar(g_void_sink);
	}

	record_items(state, count);
}

template <typename Stream, workload Workload>
void bm_call_stream_context_impl(benchmark::State& state) {
	const auto count = static_cast<std::size_t>(state.range(0));
	auto calls = make_calls<Stream, Workload>(
		count,
		[](auto& target, std::size_t index) { append_context<Workload>(target, index); });
	bench_context context;

	for(auto _ : state) {
		(void)_;
		calls.reset_and_execute(context);
		observe_context(context);
	}

	record_items(state, count);
}

template <workload Workload>
void bm_call_stream_context(benchmark::State& state) {
	bm_call_stream_context_impl<mo_yanxi::call_stream<void(bench_context&) noexcept>, Workload>(state);
}

template <workload Workload>
void bm_call_stream_allow_exception_context(benchmark::State& state) {
	bm_call_stream_context_impl<mo_yanxi::call_stream<void(bench_context&)>, Workload>(state);
}

template <workload Workload>
void bm_vector_context(benchmark::State& state) {
	const auto count = static_cast<std::size_t>(state.range(0));
	auto calls = make_calls<std::vector<std::move_only_function<void(bench_context&)>>, Workload>(
		count,
		[](auto& target, std::size_t index) { append_context<Workload>(target, index); });
	bench_context context;

	for(auto _ : state) {
		(void)_;
		for(auto& call : calls) {
			call(context);
		}
		observe_context(context);
	}

	record_items(state, count);
}

template <typename Stream, workload Workload>
void bm_call_stream_context_arg_impl(benchmark::State& state) {
	const auto count = static_cast<std::size_t>(state.range(0));
	auto calls = make_calls<Stream, Workload>(
		count,
		[](auto& target, std::size_t index) { append_context_arg<Workload>(target, index); });
	bench_context context;
	std::uint64_t arg = 0x3c6ef372fe94f82bULL;

	for(auto _ : state) {
		(void)_;
		calls.reset_and_execute(context, arg);
		arg = mix(arg + context.value);
		observe_context(context);
		observe_scalar(arg);
	}

	record_items(state, count);
}

template <workload Workload>
void bm_call_stream_context_arg(benchmark::State& state) {
	bm_call_stream_context_arg_impl<mo_yanxi::call_stream<void(bench_context&, std::uint64_t) noexcept>, Workload>(
		state);
}

template <workload Workload>
void bm_call_stream_allow_exception_context_arg(benchmark::State& state) {
	bm_call_stream_context_arg_impl<mo_yanxi::call_stream<void(bench_context&, std::uint64_t)>, Workload>(state);
}

template <workload Workload>
void bm_vector_context_arg(benchmark::State& state) {
	const auto count = static_cast<std::size_t>(state.range(0));
	auto calls = make_calls<std::vector<std::move_only_function<void(bench_context&, std::uint64_t)>>, Workload>(
		count,
		[](auto& target, std::size_t index) { append_context_arg<Workload>(target, index); });
	bench_context context;
	std::uint64_t arg = 0x3c6ef372fe94f82bULL;

	for(auto _ : state) {
		(void)_;
		for(auto& call : calls) {
			call(context, arg);
		}
		arg = mix(arg + context.value);
		observe_context(context);
		observe_scalar(arg);
	}

	record_items(state, count);
}

template <typename Stream, workload Workload>
void bm_call_stream_result_impl(benchmark::State& state) {
	const auto count = static_cast<std::size_t>(state.range(0));
	auto calls = make_calls<Stream, Workload>(
		count,
		[](auto& target, std::size_t index) { append_result<Workload>(target, index); });
	std::uint64_t arg = 0x510e527fade682d1ULL;
	std::uint64_t result_sink = 0;

	for(auto _ : state) {
		(void)_;
		calls.reset_and_execute(arg, [&result_sink](std::uint64_t result) noexcept {
			result_sink += result;
		});
		arg = mix(arg + result_sink);
		observe_scalar(arg);
		observe_scalar(result_sink);
	}

	record_items(state, count);
}

template <workload Workload>
void bm_call_stream_result(benchmark::State& state) {
	bm_call_stream_result_impl<mo_yanxi::call_stream<std::uint64_t(std::uint64_t) noexcept>, Workload>(state);
}

template <workload Workload>
void bm_call_stream_allow_exception_result(benchmark::State& state) {
	bm_call_stream_result_impl<mo_yanxi::call_stream<std::uint64_t(std::uint64_t)>, Workload>(state);
}

template <workload Workload>
void bm_vector_result(benchmark::State& state) {
	const auto count = static_cast<std::size_t>(state.range(0));
	auto calls = make_calls<std::vector<std::move_only_function<std::uint64_t(std::uint64_t)>>, Workload>(
		count,
		[](auto& target, std::size_t index) { append_result<Workload>(target, index); });
	std::uint64_t arg = 0x510e527fade682d1ULL;
	std::uint64_t result_sink = 0;

	for(auto _ : state) {
		(void)_;
		for(auto& call : calls) {
			result_sink += call(arg);
		}
		arg = mix(arg + result_sink);
		observe_scalar(arg);
		observe_scalar(result_sink);
	}

	record_items(state, count);
}

#define REGISTER_BENCHMARK_GROUP(SUFFIX, SIGNATURE, LABEL, WORKLOAD_VALUE) \
	benchmark::RegisterBenchmark( \
		"call_stream/" SIGNATURE "/" LABEL, \
		&bm_call_stream_##SUFFIX<workload::WORKLOAD_VALUE>) \
		->Arg(kSmallCallCount) \
		->Arg(kLargeCallCount); \
	benchmark::RegisterBenchmark( \
		"call_stream_allow_exception/" SIGNATURE "/" LABEL, \
		&bm_call_stream_allow_exception_##SUFFIX<workload::WORKLOAD_VALUE>) \
		->Arg(kSmallCallCount) \
		->Arg(kLargeCallCount); \
	benchmark::RegisterBenchmark( \
		"std_vector_move_only_function/" SIGNATURE "/" LABEL, \
		&bm_vector_##SUFFIX<workload::WORKLOAD_VALUE>) \
		->Arg(kSmallCallCount) \
		->Arg(kLargeCallCount)

#define REGISTER_WORKLOADS(SUFFIX, SIGNATURE) \
	REGISTER_BENCHMARK_GROUP(SUFFIX, SIGNATURE, "stateless_short", stateless_short); \
	REGISTER_BENCHMARK_GROUP(SUFFIX, SIGNATURE, "payload_short", payload_short); \
	REGISTER_BENCHMARK_GROUP(SUFFIX, SIGNATURE, "stateless_complex", stateless_complex); \
	REGISTER_BENCHMARK_GROUP(SUFFIX, SIGNATURE, "payload_complex", payload_complex); \
	REGISTER_BENCHMARK_GROUP(SUFFIX, SIGNATURE, "mixed", mixed)

#define REGISTER_CONSTRUCTION_GROUP(LABEL, PAYLOAD_VALUE) \
	benchmark::RegisterBenchmark( \
		"construction/call_stream/from_zero/" LABEL, \
		&bm_build_call_stream_from_zero<construction_payload::PAYLOAD_VALUE>) \
		->Arg(kSmallCallCount) \
		->Arg(kLargeCallCount) \
		->UseManualTime(); \
	benchmark::RegisterBenchmark( \
		"construction/std_vector_move_only_function/from_zero/" LABEL, \
		&bm_build_vector_from_zero<construction_payload::PAYLOAD_VALUE>) \
		->Arg(kSmallCallCount) \
		->Arg(kLargeCallCount) \
		->UseManualTime(); \
	benchmark::RegisterBenchmark( \
		"construction/call_stream/reserved/" LABEL, \
		&bm_build_call_stream_reserved<construction_payload::PAYLOAD_VALUE>) \
		->Arg(kSmallCallCount) \
		->Arg(kLargeCallCount) \
		->UseManualTime(); \
	benchmark::RegisterBenchmark( \
		"construction/std_vector_move_only_function/reserved/" LABEL, \
		&bm_build_vector_reserved<construction_payload::PAYLOAD_VALUE>) \
		->Arg(kSmallCallCount) \
		->Arg(kLargeCallCount) \
		->UseManualTime(); \
	benchmark::RegisterBenchmark( \
		"construction/call_stream/repeated/" LABEL, \
		&bm_build_call_stream_repeated<construction_payload::PAYLOAD_VALUE>) \
		->Arg(kSmallCallCount) \
		->Arg(kLargeCallCount) \
		->UseManualTime(); \
	benchmark::RegisterBenchmark( \
		"construction/std_vector_move_only_function/repeated/" LABEL, \
		&bm_build_vector_repeated<construction_payload::PAYLOAD_VALUE>) \
		->Arg(kSmallCallCount) \
		->Arg(kLargeCallCount) \
		->UseManualTime()

void register_construction_benchmarks() {
	REGISTER_CONSTRUCTION_GROUP("trivial_copyable", trivial_copyable);
	REGISTER_CONSTRUCTION_GROUP("manual_move", manual_move);
	REGISTER_CONSTRUCTION_GROUP("mixed", mixed);
}

#undef REGISTER_CONSTRUCTION_GROUP

void register_benchmarks() {
	REGISTER_WORKLOADS(void0, "void()");
	REGISTER_WORKLOADS(context, "void(bench_context&)");
	REGISTER_WORKLOADS(context_arg, "void(bench_context&,uint64_t)");
	REGISTER_WORKLOADS(result, "uint64_t(uint64_t)");
	register_construction_benchmarks();
}

#undef REGISTER_WORKLOADS
#undef REGISTER_BENCHMARK_GROUP

} // namespace

int main(int argc, char** argv) {
	register_benchmarks();
	benchmark::Initialize(&argc, argv);
	if(benchmark::ReportUnrecognizedArguments(argc, argv)) {
		return 1;
	}
	benchmark::RunSpecifiedBenchmarks();
	benchmark::Shutdown();
}
