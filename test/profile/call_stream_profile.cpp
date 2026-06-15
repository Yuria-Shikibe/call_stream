#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

import mo_yanxi.call_stream;

#ifndef __cpp_lib_move_only_function
#error "call_stream profile target requires std::move_only_function support."
#endif

#if defined(_MSC_VER)
#define PROFILE_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define PROFILE_NOINLINE __attribute__((noinline))
#else
#define PROFILE_NOINLINE
#endif

namespace profile {

constexpr std::size_t kDefaultCallCount = 1024;
constexpr std::uint64_t kDefaultBatch = 8192;

struct options {
	std::string_view case_name = "call_stream_ctx_stateless";
	std::size_t calls = kDefaultCallCount;
	double seconds = 8.0;
	std::uint64_t iterations = 0;
	std::uint64_t batch = kDefaultBatch;
};

struct run_result {
	std::uint64_t executions = 0;
	std::uint64_t sink = 0;
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

alignas(64) std::uint64_t g_sink = 0x9e3779b97f4a7c15ULL;

PROFILE_NOINLINE void compiler_barrier(std::uint64_t value) noexcept {
	g_sink ^= value + 0x9e3779b97f4a7c15ULL;
	std::atomic_signal_fence(std::memory_order_seq_cst);
}

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

void consume_context_short(bench_context& context, std::uint64_t value) noexcept {
	context.value += value;
	context.lanes[context.value & 7U] ^= context.value;
}

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

void consume_void_short(std::uint64_t value) noexcept {
	g_sink += value;
}

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

template <typename Calls, typename Append>
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

template <typename Step>
PROFILE_NOINLINE run_result run_for(const options& opts, Step&& step) {
	run_result result;
	if(opts.iterations > 0) {
		for(; result.executions < opts.iterations; ++result.executions) {
			result.sink ^= static_cast<std::uint64_t>(step());
		}
		return result;
	}

	const auto deadline = std::chrono::steady_clock::now() +
		std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(opts.seconds));

	do {
		for(std::uint64_t i = 0; i < opts.batch; ++i) {
			result.sink ^= static_cast<std::uint64_t>(step());
		}
		result.executions += opts.batch;
	} while(std::chrono::steady_clock::now() < deadline);

	return result;
}

PROFILE_NOINLINE run_result run_call_stream_ctx_stateless(const options& opts) {
	auto calls = make_calls<mo_yanxi::call_stream<void(bench_context&) noexcept>>(
		opts.calls,
		[](auto& target, std::size_t) { target.template emplace_back<context_stateless_short>(); });
	bench_context context;
	auto result = run_for(opts, [&]() noexcept {
		calls.reset_and_execute(context);
		return context.value;
	});
	result.sink ^= context.value;
	compiler_barrier(result.sink);
	return result;
}

PROFILE_NOINLINE run_result run_vector_ctx_stateless(const options& opts) {
	auto calls = make_calls<std::vector<std::move_only_function<void(bench_context&)>>>(
		opts.calls,
		[](auto& target, std::size_t) { target.emplace_back(context_stateless_short{}); });
	bench_context context;
	auto result = run_for(opts, [&]() noexcept {
		for(auto& call : calls) {
			call(context);
		}
		return context.value;
	});
	result.sink ^= context.value;
	compiler_barrier(result.sink);
	return result;
}

PROFILE_NOINLINE run_result run_call_stream_ctx_payload(const options& opts) {
	auto calls = make_calls<mo_yanxi::call_stream<void(bench_context&) noexcept>>(
		opts.calls,
		[](auto& target, std::size_t index) {
			target.template emplace_back<context_payload_short>(seed_for(index));
		});
	bench_context context;
	auto result = run_for(opts, [&]() noexcept {
		calls.reset_and_execute(context);
		return context.value;
	});
	result.sink ^= context.value;
	compiler_barrier(result.sink);
	return result;
}

PROFILE_NOINLINE run_result run_vector_ctx_payload(const options& opts) {
	auto calls = make_calls<std::vector<std::move_only_function<void(bench_context&)>>>(
		opts.calls,
		[](auto& target, std::size_t index) { target.emplace_back(context_payload_short{seed_for(index)}); });
	bench_context context;
	auto result = run_for(opts, [&]() noexcept {
		for(auto& call : calls) {
			call(context);
		}
		return context.value;
	});
	result.sink ^= context.value;
	compiler_barrier(result.sink);
	return result;
}

PROFILE_NOINLINE run_result run_call_stream_result_stateless(const options& opts) {
	auto calls = make_calls<mo_yanxi::call_stream<std::uint64_t(std::uint64_t) noexcept>>(
		opts.calls,
		[](auto& target, std::size_t) { target.template emplace_back<result_stateless_short>(); });
	std::uint64_t arg = 0x510e527fade682d1ULL;
	std::uint64_t result_sink = 0;
	auto result = run_for(opts, [&]() noexcept {
		calls.reset_and_execute(arg, [&result_sink](std::uint64_t value) noexcept {
			result_sink += value;
		});
		arg = mix(arg + result_sink);
		return result_sink ^ arg;
	});
	result.sink ^= result_sink ^ arg;
	compiler_barrier(result.sink);
	return result;
}

PROFILE_NOINLINE run_result run_vector_result_stateless(const options& opts) {
	auto calls = make_calls<std::vector<std::move_only_function<std::uint64_t(std::uint64_t)>>>(
		opts.calls,
		[](auto& target, std::size_t) { target.emplace_back(result_stateless_short{}); });
	std::uint64_t arg = 0x510e527fade682d1ULL;
	std::uint64_t result_sink = 0;
	auto result = run_for(opts, [&]() noexcept {
		for(auto& call : calls) {
			result_sink += call(arg);
		}
		arg = mix(arg + result_sink);
		return result_sink ^ arg;
	});
	result.sink ^= result_sink ^ arg;
	compiler_barrier(result.sink);
	return result;
}

PROFILE_NOINLINE run_result run_call_stream_result_payload(const options& opts) {
	auto calls = make_calls<mo_yanxi::call_stream<std::uint64_t(std::uint64_t) noexcept>>(
		opts.calls,
		[](auto& target, std::size_t index) {
			target.template emplace_back<result_payload_short>(seed_for(index));
		});
	std::uint64_t arg = 0x510e527fade682d1ULL;
	std::uint64_t result_sink = 0;
	auto result = run_for(opts, [&]() noexcept {
		calls.reset_and_execute(arg, [&result_sink](std::uint64_t value) noexcept {
			result_sink += value;
		});
		arg = mix(arg + result_sink);
		return result_sink ^ arg;
	});
	result.sink ^= result_sink ^ arg;
	compiler_barrier(result.sink);
	return result;
}

PROFILE_NOINLINE run_result run_vector_result_payload(const options& opts) {
	auto calls = make_calls<std::vector<std::move_only_function<std::uint64_t(std::uint64_t)>>>(
		opts.calls,
		[](auto& target, std::size_t index) { target.emplace_back(result_payload_short{seed_for(index)}); });
	std::uint64_t arg = 0x510e527fade682d1ULL;
	std::uint64_t result_sink = 0;
	auto result = run_for(opts, [&]() noexcept {
		for(auto& call : calls) {
			result_sink += call(arg);
		}
		arg = mix(arg + result_sink);
		return result_sink ^ arg;
	});
	result.sink ^= result_sink ^ arg;
	compiler_barrier(result.sink);
	return result;
}

[[nodiscard]] std::size_t construction_stream_bytes_per_call() {
	static const std::size_t bytes = [] {
		mo_yanxi::call_stream<void() noexcept> calls;
		calls.template emplace_back<construction_trivial_call>(seed_for(0));
		return calls.size();
	}();
	return bytes;
}

void append_construction_trivial(mo_yanxi::call_stream<void() noexcept>& calls, std::size_t index) {
	calls.template emplace_back<construction_trivial_call>(seed_for(index));
}

void append_construction_trivial(std::vector<std::move_only_function<void()>>& calls, std::size_t index) {
	calls.emplace_back(construction_trivial_call{seed_for(index)});
}

template <typename Calls>
void append_construction_trivial_calls(Calls& calls, std::size_t count) {
	for(std::size_t i = 0; i < count; ++i) {
		append_construction_trivial(calls, i);
	}
}

PROFILE_NOINLINE std::uint64_t construct_call_stream_from_zero_trivial_once(std::size_t count) {
	mo_yanxi::call_stream<void() noexcept> calls;
	append_construction_trivial_calls(calls, count);
	calls.reset_and_execute();
	return calls.size() ^ g_sink;
}

PROFILE_NOINLINE std::uint64_t construct_vector_from_zero_trivial_once(std::size_t count) {
	std::vector<std::move_only_function<void()>> calls;
	append_construction_trivial_calls(calls, count);
	for(auto& call : calls) {
		call();
	}
	return calls.size() ^ g_sink;
}

PROFILE_NOINLINE std::uint64_t construct_call_stream_reserved_trivial_once(std::size_t count) {
	mo_yanxi::call_stream<void() noexcept> calls;
	calls.reserve(count * construction_stream_bytes_per_call());
	append_construction_trivial_calls(calls, count);
	calls.reset_and_execute();
	return calls.size() ^ g_sink;
}

PROFILE_NOINLINE std::uint64_t construct_vector_reserved_trivial_once(std::size_t count) {
	std::vector<std::move_only_function<void()>> calls;
	calls.reserve(count);
	append_construction_trivial_calls(calls, count);
	for(auto& call : calls) {
		call();
	}
	return calls.size() ^ g_sink;
}

PROFILE_NOINLINE run_result run_construct_call_stream_from_zero_trivial(const options& opts) {
	auto result = run_for(opts, [&]() noexcept {
		return construct_call_stream_from_zero_trivial_once(opts.calls);
	});
	compiler_barrier(result.sink);
	return result;
}

PROFILE_NOINLINE run_result run_construct_vector_from_zero_trivial(const options& opts) {
	auto result = run_for(opts, [&]() noexcept {
		return construct_vector_from_zero_trivial_once(opts.calls);
	});
	compiler_barrier(result.sink);
	return result;
}

PROFILE_NOINLINE run_result run_construct_call_stream_reserved_trivial(const options& opts) {
	auto result = run_for(opts, [&]() noexcept {
		return construct_call_stream_reserved_trivial_once(opts.calls);
	});
	compiler_barrier(result.sink);
	return result;
}

PROFILE_NOINLINE run_result run_construct_vector_reserved_trivial(const options& opts) {
	auto result = run_for(opts, [&]() noexcept {
		return construct_vector_reserved_trivial_once(opts.calls);
	});
	compiler_barrier(result.sink);
	return result;
}

[[noreturn]] void print_usage_and_exit(const char* exe) {
	std::cerr
		<< "Usage: " << exe << " --case <name> [--calls 1024] [--seconds 8] [--iterations N] [--batch N]\n"
		<< "Cases:\n"
		<< "  call_stream_ctx_stateless\n"
		<< "  vector_ctx_stateless\n"
		<< "  call_stream_ctx_payload\n"
		<< "  vector_ctx_payload\n"
		<< "  call_stream_result_stateless\n"
		<< "  vector_result_stateless\n"
		<< "  call_stream_result_payload\n"
		<< "  vector_result_payload\n"
		<< "  construct_call_stream_from_zero_trivial\n"
		<< "  construct_vector_from_zero_trivial\n"
		<< "  construct_call_stream_reserved_trivial\n"
		<< "  construct_vector_reserved_trivial\n";
	std::exit(2);
}

std::string_view require_value(int& index, int argc, char** argv) {
	if(index + 1 >= argc) {
		print_usage_and_exit(argv[0]);
	}
	++index;
	return argv[index];
}

std::uint64_t parse_u64(std::string_view text) {
	std::uint64_t value = 0;
	for(char ch : text) {
		if(ch < '0' || ch > '9') {
			throw std::invalid_argument("expected unsigned integer");
		}
		value = value * 10 + static_cast<std::uint64_t>(ch - '0');
	}
	return value;
}

double parse_double(std::string_view text) {
	return std::stod(std::string(text));
}

options parse_options(int argc, char** argv) {
	options opts;
	for(int i = 1; i < argc; ++i) {
		std::string_view arg = argv[i];
		if(arg == "--case") {
			opts.case_name = require_value(i, argc, argv);
		} else if(arg == "--calls") {
			opts.calls = static_cast<std::size_t>(parse_u64(require_value(i, argc, argv)));
		} else if(arg == "--seconds") {
			opts.seconds = parse_double(require_value(i, argc, argv));
		} else if(arg == "--iterations") {
			opts.iterations = parse_u64(require_value(i, argc, argv));
		} else if(arg == "--batch") {
			opts.batch = parse_u64(require_value(i, argc, argv));
		} else if(arg == "--help" || arg == "-h") {
			print_usage_and_exit(argv[0]);
		} else {
			print_usage_and_exit(argv[0]);
		}
	}
	if(opts.calls == 0 || opts.batch == 0 || (opts.iterations == 0 && opts.seconds <= 0.0)) {
		print_usage_and_exit(argv[0]);
	}
	return opts;
}

run_result run_selected(const options& opts) {
	if(opts.case_name == "call_stream_ctx_stateless") return run_call_stream_ctx_stateless(opts);
	if(opts.case_name == "vector_ctx_stateless") return run_vector_ctx_stateless(opts);
	if(opts.case_name == "call_stream_ctx_payload") return run_call_stream_ctx_payload(opts);
	if(opts.case_name == "vector_ctx_payload") return run_vector_ctx_payload(opts);
	if(opts.case_name == "call_stream_result_stateless") return run_call_stream_result_stateless(opts);
	if(opts.case_name == "vector_result_stateless") return run_vector_result_stateless(opts);
	if(opts.case_name == "call_stream_result_payload") return run_call_stream_result_payload(opts);
	if(opts.case_name == "vector_result_payload") return run_vector_result_payload(opts);
	if(opts.case_name == "construct_call_stream_from_zero_trivial") {
		return run_construct_call_stream_from_zero_trivial(opts);
	}
	if(opts.case_name == "construct_vector_from_zero_trivial") return run_construct_vector_from_zero_trivial(opts);
	if(opts.case_name == "construct_call_stream_reserved_trivial") return run_construct_call_stream_reserved_trivial(opts);
	if(opts.case_name == "construct_vector_reserved_trivial") return run_construct_vector_reserved_trivial(opts);
	print_usage_and_exit("call_stream.profile");
}

} // namespace profile

int main(int argc, char** argv) {
	try {
		auto opts = profile::parse_options(argc, argv);
		const auto start = std::chrono::steady_clock::now();
		auto result = profile::run_selected(opts);
		const auto stop = std::chrono::steady_clock::now();
		const auto elapsed = std::chrono::duration<double>(stop - start).count();
		const auto calls_executed = static_cast<long double>(result.executions) *
			static_cast<long double>(opts.calls);

		std::cout
			<< "case=" << opts.case_name << '\n'
			<< "calls=" << opts.calls << '\n'
			<< "executions=" << result.executions << '\n'
			<< "elapsed_seconds=" << elapsed << '\n'
			<< "calls_per_second=" << static_cast<double>(calls_executed / elapsed) << '\n'
			<< "sink=" << (result.sink ^ profile::g_sink) << '\n';
		return 0;
	} catch(const std::exception& error) {
		std::cerr << "error: " << error.what() << '\n';
		return 1;
	}
}
