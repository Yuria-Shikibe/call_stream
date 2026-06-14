#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

import mo_yanxi.call_stream;

namespace {

using byte_allocator = std::allocator<std::byte>;

template <typename... Args>
using void_stream = mo_yanxi::basic_call_stream<byte_allocator, void, Args...>;

template <typename Stream>
concept can_emit_noop = requires(Stream& stream) {
	stream.emit_noop();
};

template <typename Stream, typename Fn>
concept can_emplace_default = requires(Stream& stream) {
	stream.template emplace_back<Fn>();
};

template <typename Stream, typename Callback>
concept can_execute_with_callback = requires(Stream& stream, Callback callback) {
	stream.execute(callback);
};

template <typename Stream, typename Fn>
concept can_shift_call = requires(Stream& stream, Fn fn) {
	stream << fn;
};

template <typename Stream, typename Fn>
concept can_shift_cmd_call = requires(Stream& stream, mo_yanxi::cmd_call<Fn> call) {
	stream << std::move(call);
};

struct tracking_byte_allocator {
	using value_type = std::byte;

	static inline std::size_t last_allocate = 0;
	static inline std::size_t last_deallocate = 0;

	static void reset() noexcept {
		last_allocate = 0;
		last_deallocate = 0;
	}

	[[nodiscard]] std::byte* allocate(std::size_t n) {
		last_allocate = n;
		return std::allocator<std::byte>{}.allocate(n);
	}

	void deallocate(std::byte* ptr, std::size_t n) noexcept {
		last_deallocate = n;
		std::allocator<std::byte>{}.deallocate(ptr, n);
	}
};

struct heap_tracked_call {
	static inline int alive = 0;
	static inline int destroyed = 0;
	static inline int calls = 0;

	int value{};
	std::vector<int>* sink{};

	static void reset() noexcept {
		alive = 0;
		destroyed = 0;
		calls = 0;
	}

	heap_tracked_call(int value_in, std::vector<int>* sink_in)
		: value(value_in), sink(sink_in) {
		++alive;
	}

	heap_tracked_call(const heap_tracked_call& other)
		: value(other.value), sink(other.sink) {
		++alive;
	}

	heap_tracked_call(heap_tracked_call&& other) noexcept(false)
		: value(other.value), sink(other.sink) {
		++alive;
		other.value = 0;
		other.sink = nullptr;
	}

	~heap_tracked_call() {
		--alive;
		++destroyed;
	}

	void operator()() {
		++calls;
		if(sink != nullptr) {
			sink->push_back(value);
		}
	}
};

static_assert(!std::is_trivially_copyable_v<heap_tracked_call>);
static_assert(!std::is_nothrow_move_constructible_v<heap_tracked_call>);

struct overloaded_static_call {
	static inline int alive = 0;
	static inline int moves = 0;
	static inline int calls = 0;

	int value{};

	static void reset() noexcept {
		alive = 0;
		moves = 0;
		calls = 0;
	}

	explicit overloaded_static_call(int value_in)
		: value(value_in) {
		++alive;
	}

	overloaded_static_call(const overloaded_static_call& other)
		: value(other.value) {
		++alive;
	}

	overloaded_static_call(overloaded_static_call&& other) noexcept(false)
		: value(other.value) {
		++alive;
		++moves;
		other.value = 0;
	}

	~overloaded_static_call() {
		--alive;
	}

	static int operator()(int value_in) noexcept {
		++calls;
		return value_in + 7;
	}

	static int operator()(double) noexcept {
		return -1;
	}
};

static_assert(!std::is_trivially_copyable_v<overloaded_static_call>);
static_assert(!std::is_nothrow_move_constructible_v<overloaded_static_call>);

struct empty_non_static_call {
	static inline int calls = 0;

	static void reset() noexcept {
		calls = 0;
	}

	void operator()() const noexcept {
		++calls;
	}
};

static_assert(std::is_empty_v<empty_non_static_call>);

struct noexcept_void_call {
	void operator()() const noexcept {
	}
};

struct throwing_void_call {
	void operator()() const {
	}
};

struct noexcept_int_call {
	int operator()() const noexcept {
		return 1;
	}
};

struct throwing_int_call {
	int operator()() const {
		return 1;
	}
};

struct noexcept_int_callback {
	void operator()(int) const noexcept {
	}
};

struct throwing_int_callback {
	void operator()(int) const {
	}
};

using noexcept_void_stream = mo_yanxi::noexcept_call_stream<void(), byte_allocator>;
using noexcept_int_stream = mo_yanxi::noexcept_call_stream<int(), byte_allocator>;

static_assert(can_emplace_default<noexcept_void_stream, noexcept_void_call>);
static_assert(!can_emplace_default<noexcept_void_stream, throwing_void_call>);
static_assert(can_emplace_default<noexcept_int_stream, noexcept_int_call>);
static_assert(!can_emplace_default<noexcept_int_stream, throwing_int_call>);
static_assert(can_shift_call<noexcept_void_stream, noexcept_void_call>);
static_assert(!can_shift_call<noexcept_void_stream, throwing_void_call>);
static_assert(can_shift_cmd_call<noexcept_void_stream, noexcept_void_call>);
static_assert(!can_shift_cmd_call<noexcept_void_stream, throwing_void_call>);
static_assert(can_execute_with_callback<noexcept_int_stream, noexcept_int_callback>);
static_assert(!can_execute_with_callback<noexcept_int_stream, throwing_int_callback>);
static_assert(noexcept(std::declval<noexcept_void_stream&>().execute()));
static_assert(noexcept(std::declval<noexcept_int_stream&>().execute(noexcept_int_callback{})));
static_assert(!noexcept(std::declval<mo_yanxi::call_stream<void()>&>().execute()));

struct tracked_result {
	static inline int alive = 0;
	static inline int destroyed = 0;

	int value{};

	static void reset() noexcept {
		alive = 0;
		destroyed = 0;
	}

	explicit tracked_result(int value_in)
		: value(value_in) {
		++alive;
	}

	tracked_result(const tracked_result&) = delete;
	tracked_result& operator=(const tracked_result&) = delete;

	tracked_result(tracked_result&& other) noexcept
		: value(other.value) {
		++alive;
		other.value = 0;
	}

	~tracked_result() {
		--alive;
		++destroyed;
	}
};

static_assert(can_emit_noop<void_stream<>>);
static_assert(can_emit_noop<mo_yanxi::call_stream<int()>>);
static_assert(can_emit_noop<mo_yanxi::call_stream<std::string()>>);
static_assert(!can_emit_noop<mo_yanxi::call_stream<tracked_result()>>);

struct checked_value_arg {
	int value{};

	explicit checked_value_arg(int value_in)
		: value(value_in) {
	}

	checked_value_arg(const checked_value_arg&) = default;
	checked_value_arg& operator=(const checked_value_arg&) = default;

	checked_value_arg(checked_value_arg&& other) noexcept
		: value(other.value) {
		other.value = -1;
	}

	checked_value_arg& operator=(checked_value_arg&& other) noexcept {
		value = other.value;
		other.value = -1;
		return *this;
	}
};

struct alignas(64) over_aligned_call {
	static inline int alive = 0;
	static inline int destroyed = 0;

	int value{};
	std::vector<int>* sink{};

	static void reset() noexcept {
		alive = 0;
		destroyed = 0;
	}

	over_aligned_call(int value_in, std::vector<int>* sink_in)
		: value(value_in), sink(sink_in) {
		++alive;
	}

	over_aligned_call(const over_aligned_call& other)
		: value(other.value), sink(other.sink) {
		++alive;
	}

	over_aligned_call(over_aligned_call&& other) noexcept
		: value(other.value), sink(other.sink) {
		++alive;
		other.value = 0;
		other.sink = nullptr;
	}

	~over_aligned_call() {
		--alive;
		++destroyed;
	}

	void operator()() {
		if(sink != nullptr) {
			sink->push_back(value);
		}
	}
};

static_assert(alignof(over_aligned_call) > alignof(void*));

std::vector<int> sequence(int first, int last) {
	std::vector<int> values;
	for(int value = first; value <= last; ++value) {
		values.push_back(value);
	}
	return values;
}

} // namespace

TEST(CallStreamCorrectnessTest, ExecutesVoidCallsInInsertionOrder) {
	void_stream<> stream;
	std::vector<int> values;

	stream << [&] { values.push_back(1); };
	stream.emplace_back([&] { values.push_back(2); });
	stream << mo_yanxi::cmd_call{[&] { values.push_back(3); }};

	ASSERT_FALSE(stream.empty());

	stream.execute();
	EXPECT_EQ(values, (std::vector<int>{1, 2, 3}));
	EXPECT_EQ(stream.current_ip(), stream.size());

	stream.execute();
	EXPECT_EQ(values, (std::vector<int>{1, 2, 3}));

	stream.reset_ip();
	stream.execute();
	EXPECT_EQ(values, (std::vector<int>{1, 2, 3, 1, 2, 3}));
}

TEST(CallStreamCorrectnessTest, PassesArgumentsToEveryCallable) {
	void_stream<int&, const int&> stream;

	stream.emplace_back([](int& value, const int& delta) { value += delta; });
	stream.emplace_back([](int& value, const int& delta) { value *= delta; });

	int value = 3;
	const int delta = 4;
	stream.execute(value, delta);

	EXPECT_EQ(value, 28);
}

TEST(CallStreamCorrectnessTest, ByValueArgumentsAreCopiedForEachCallable) {
	mo_yanxi::call_stream<void(checked_value_arg)> stream;
	std::vector<int> seen;

	stream.emplace_back([&](checked_value_arg arg) { seen.push_back(arg.value); });
	stream.emplace_back([&](checked_value_arg arg) { seen.push_back(arg.value); });

	stream.execute(checked_value_arg{42});

	EXPECT_EQ(seen, (std::vector<int>{42, 42}));
}

TEST(CallStreamCorrectnessTest, OverloadedStaticCallOperatorUsesZeroPayloadDispatch) {
	overloaded_static_call::reset();
	mo_yanxi::call_stream<int(int)> stream;

	stream.emplace_back(overloaded_static_call{123});

	EXPECT_EQ(overloaded_static_call::alive, 0);
	EXPECT_EQ(overloaded_static_call::moves, 0);

	std::vector<int> results;
	stream.execute(5, [&](int result) { results.push_back(result); });

	EXPECT_EQ(overloaded_static_call::calls, 1);
	EXPECT_EQ(results, (std::vector<int>{12}));
	EXPECT_EQ(overloaded_static_call::alive, 0);
}

TEST(CallStreamCorrectnessTest, EmptyNonStaticCallOperatorDoesNotUseStaticDispatch) {
	empty_non_static_call::reset();
	void_stream<> stream;

	stream.emplace_back(empty_non_static_call{});
	stream.execute();

	EXPECT_EQ(empty_non_static_call::calls, 1);
}

TEST(CallStreamCorrectnessTest, NoopInVoidStreamSkipsToNextInstruction) {
	void_stream<> stream;
	std::vector<int> values;

	stream.emplace_back([&] { values.push_back(1); });
	stream.emit_noop();
	stream.emplace_back([&] { values.push_back(2); });

	stream.execute();

	EXPECT_EQ(values, (std::vector<int>{1, 2}));
}

TEST(CallStreamCorrectnessTest, ResultCallbackReceivesAllResultsWhenItReturnsVoid) {
	mo_yanxi::call_stream<int(int)> stream;
	std::vector<int> results;

	stream.emplace_back([](int value) { return value + 1; });
	stream.emplace_back([](int value) { return value + 2; });
	stream.emplace_back([](int value) { return value + 3; });

	stream.execute(10, [&](int result) { results.push_back(result); });

	EXPECT_EQ(results, (std::vector<int>{11, 12, 13}));
}

TEST(CallStreamCorrectnessTest, NoopInResultStreamEmitsDefaultConstructedResult) {
	mo_yanxi::call_stream<int()> stream;
	std::vector<int> results;

	stream.emplace_back([] { return 7; });
	stream.emit_noop();
	stream.emplace_back([] { return 9; });

	stream.execute([&](int result) { results.push_back(result); });

	EXPECT_EQ(results, (std::vector<int>{7, 0, 9}));
}

TEST(CallStreamCorrectnessTest, NoopInNonScalarResultStreamConstructsResultSlot) {
	mo_yanxi::call_stream<std::string()> stream;
	std::vector<std::string> results;

	stream.emit_noop();

	stream.execute([&](std::string&& result) {
		results.push_back(std::move(result));
	});

	ASSERT_EQ(results.size(), 1U);
	EXPECT_TRUE(results.front().empty());
}

TEST(CallStreamCorrectnessTest, ResultCallbackCanStopDispatch) {
	mo_yanxi::call_stream<int(int)> stream;
	std::vector<int> results;

	stream.emplace_back([](int value) { return value + 1; });
	stream.emplace_back([](int value) { return value + 2; });
	stream.emplace_back([](int value) { return value + 3; });

	stream.execute(10, [&](int result) {
		results.push_back(result);
		return result == 12;
	});

	EXPECT_EQ(results, (std::vector<int>{11, 12}));
	EXPECT_EQ(stream.current_ip(), stream.size());
}

TEST(CallStreamCorrectnessTest, ResumableVoidStreamRetriesThrowingInstruction) {
	mo_yanxi::call_stream<void()> stream;
	std::vector<int> seen;
	int throws_remaining = 2;

	stream.emplace_back([&] { seen.push_back(1); });
	stream.emplace_back([&] {
		seen.push_back(2);
		if(throws_remaining > 0) {
			--throws_remaining;
			throw std::runtime_error("transient");
		}
	});
	stream.emplace_back([&] { seen.push_back(3); });

	EXPECT_THROW(stream.execute(), std::runtime_error);
	EXPECT_EQ(seen, (std::vector<int>{1, 2}));
	const std::size_t failing_ip = stream.current_ip();
	EXPECT_GT(failing_ip, 0U);
	EXPECT_LT(failing_ip, stream.size());

	EXPECT_THROW(stream.execute(), std::runtime_error);
	EXPECT_EQ(seen, (std::vector<int>{1, 2, 2}));
	EXPECT_EQ(stream.current_ip(), failing_ip);

	stream.execute();
	EXPECT_EQ(seen, (std::vector<int>{1, 2, 2, 2, 3}));
	EXPECT_EQ(stream.current_ip(), stream.size());
}

TEST(CallStreamCorrectnessTest, ResumableResultStreamRetriesThrowingInstruction) {
	mo_yanxi::call_stream<int()> stream;
	std::vector<int> results;
	int throws_remaining = 2;

	stream.emplace_back([] { return 1; });
	stream.emplace_back([&] {
		if(throws_remaining > 0) {
			--throws_remaining;
			throw std::runtime_error("transient");
		}
		return 2;
	});
	stream.emplace_back([] { return 3; });

	EXPECT_THROW(stream.execute([&](int result) { results.push_back(result); }), std::runtime_error);
	EXPECT_EQ(results, (std::vector<int>{1}));
	const std::size_t failing_ip = stream.current_ip();
	EXPECT_GT(failing_ip, 0U);
	EXPECT_LT(failing_ip, stream.size());

	EXPECT_THROW(stream.execute([&](int result) { results.push_back(result); }), std::runtime_error);
	EXPECT_EQ(results, (std::vector<int>{1}));
	EXPECT_EQ(stream.current_ip(), failing_ip);

	stream.execute([&](int result) { results.push_back(result); });
	EXPECT_EQ(results, (std::vector<int>{1, 2, 3}));
	EXPECT_EQ(stream.current_ip(), stream.size());
}

TEST(CallStreamCorrectnessTest, ResumableNonScalarResultStreamRetriesThrowingInstruction) {
	mo_yanxi::call_stream<std::string()> stream;
	std::vector<std::string> results;
	int throws_remaining = 2;

	stream.emplace_back([] { return std::string("a"); });
	stream.emplace_back([&] {
		if(throws_remaining > 0) {
			--throws_remaining;
			throw std::runtime_error("transient");
		}
		return std::string("b");
	});
	stream.emplace_back([] { return std::string("c"); });

	EXPECT_THROW(stream.execute([&](std::string&& result) { results.push_back(std::move(result)); }),
	             std::runtime_error);
	EXPECT_EQ(results, (std::vector<std::string>{"a"}));
	const std::size_t failing_ip = stream.current_ip();
	EXPECT_GT(failing_ip, 0U);
	EXPECT_LT(failing_ip, stream.size());

	EXPECT_THROW(stream.execute([&](std::string&& result) { results.push_back(std::move(result)); }),
	             std::runtime_error);
	EXPECT_EQ(results, (std::vector<std::string>{"a"}));
	EXPECT_EQ(stream.current_ip(), failing_ip);

	stream.execute([&](std::string&& result) { results.push_back(std::move(result)); });
	EXPECT_EQ(results, (std::vector<std::string>{"a", "b", "c"}));
	EXPECT_EQ(stream.current_ip(), stream.size());
}

TEST(CallStreamCorrectnessTest, ResultCallbackExceptionDestroysResultAndResumesAtNextInstruction) {
	tracked_result::reset();
	mo_yanxi::call_stream<tracked_result()> stream;
	std::vector<int> results;
	bool throw_once = true;

	stream.emplace_back([] { return tracked_result(1); });
	stream.emplace_back([] { return tracked_result(2); });

	EXPECT_THROW(stream.execute([&](tracked_result&& result) {
		             results.push_back(result.value);
		             if(throw_once) {
			             throw_once = false;
			             throw std::runtime_error("callback");
		             }
	             }),
	             std::runtime_error);
	EXPECT_EQ(results, (std::vector<int>{1}));
	EXPECT_EQ(tracked_result::alive, 0);
	EXPECT_GT(stream.current_ip(), 0U);
	EXPECT_LT(stream.current_ip(), stream.size());

	stream.execute([&](tracked_result&& result) {
		results.push_back(result.value);
	});
	EXPECT_EQ(results, (std::vector<int>{1, 2}));
	EXPECT_EQ(tracked_result::alive, 0);
	EXPECT_EQ(stream.current_ip(), stream.size());
}

TEST(CallStreamCorrectnessTest, NoexceptStreamExecutesNoexceptCalls) {
	mo_yanxi::noexcept_call_stream<void()> stream;
	int seen = 0;

	stream.emplace_back([&] noexcept { seen = seen * 10 + 1; });
	stream.emplace_back([&] noexcept { seen = seen * 10 + 2; });

	stream.execute();
	EXPECT_EQ(seen, 12);
	EXPECT_EQ(stream.current_ip(), stream.size());
}

TEST(CallStreamCorrectnessTest, MoveOnlyResultsAreDestroyedAfterCallback) {
	tracked_result::reset();
	mo_yanxi::call_stream<tracked_result(int)> stream;
	std::vector<int> results;

	stream.emplace_back([](int value) { return tracked_result(value + 1); });
	stream.emplace_back([](int value) { return tracked_result(value + 2); });

	stream.execute(10, [&](tracked_result&& result) {
		results.push_back(result.value);
	});

	EXPECT_EQ(results, (std::vector<int>{11, 12}));
	EXPECT_EQ(tracked_result::alive, 0);
	EXPECT_GE(tracked_result::destroyed, 2);
}

TEST(CallStreamCorrectnessTest, BufferReserveKeepsAllocatorCapacity) {
	tracking_byte_allocator::reset();

	{
		mo_yanxi::call_stream_buffer<tracking_byte_allocator> buffer;
		buffer.reserve(1, [](std::byte*, std::byte*) noexcept {});

		EXPECT_GE(buffer.capacity(), std::size_t{1});
		EXPECT_EQ(buffer.capacity(), tracking_byte_allocator::last_allocate);
	}

	EXPECT_EQ(tracking_byte_allocator::last_deallocate, tracking_byte_allocator::last_allocate);
}

TEST(CallStreamCorrectnessTest, ClearDestroysHeapAllocatedCallablesExactlyOnce) {
	heap_tracked_call::reset();
	std::vector<int> seen;

	{
		void_stream<> stream;
		for(int value = 1; value <= 40; ++value) {
			stream.emplace_back<heap_tracked_call>(value, &seen);
		}

		EXPECT_EQ(heap_tracked_call::alive, 40);

		stream.execute();
		EXPECT_EQ(heap_tracked_call::calls, 40);
		EXPECT_EQ(seen, sequence(1, 40));

		stream.clear();
		EXPECT_TRUE(stream.empty());
		EXPECT_EQ(heap_tracked_call::alive, 0);
		EXPECT_EQ(heap_tracked_call::destroyed, 40);

		stream.execute();
		EXPECT_EQ(heap_tracked_call::calls, 40);
	}

	EXPECT_EQ(heap_tracked_call::alive, 0);
	EXPECT_EQ(heap_tracked_call::destroyed, 40);
}

TEST(CallStreamCorrectnessTest, OverAlignedCallablesUseHeapStorageAndDestroyOnce) {
	over_aligned_call::reset();
	std::vector<int> seen;

	{
		void_stream<> stream;
		for(int value = 1; value <= 8; ++value) {
			stream.emplace_back<over_aligned_call>(value, &seen);
		}

		EXPECT_EQ(over_aligned_call::alive, 8);

		stream.execute();
		EXPECT_EQ(seen, sequence(1, 8));

		stream.clear();
		EXPECT_EQ(over_aligned_call::alive, 0);
		EXPECT_EQ(over_aligned_call::destroyed, 8);
	}

	EXPECT_EQ(over_aligned_call::alive, 0);
	EXPECT_EQ(over_aligned_call::destroyed, 8);
}

TEST(CallStreamCorrectnessTest, MergePreservesOrderAndTransfersHeapOwnership) {
	heap_tracked_call::reset();
	std::vector<int> seen;

	void_stream<> first;
	void_stream<> second;

	first.emplace_back<heap_tracked_call>(1, &seen);
	first.emplace_back<heap_tracked_call>(2, &seen);
	second.emplace_back<heap_tracked_call>(3, &seen);
	second.emplace_back<heap_tracked_call>(4, &seen);

	first.merge(std::move(second));

	EXPECT_TRUE(second.empty());
	EXPECT_EQ(heap_tracked_call::alive, 4);

	first.execute();
	EXPECT_EQ(seen, (std::vector<int>{1, 2, 3, 4}));

	first.clear();
	second.clear();
	EXPECT_EQ(heap_tracked_call::alive, 0);
	EXPECT_EQ(heap_tracked_call::destroyed, 4);
}

TEST(CallStreamCorrectnessTest, MoveConstructionTransfersHeapOwnedCalls) {
	heap_tracked_call::reset();
	std::vector<int> seen;

	void_stream<> source;
	source.emplace_back<heap_tracked_call>(7, &seen);
	source.emplace_back<heap_tracked_call>(8, &seen);

	void_stream<> moved(std::move(source));

	EXPECT_TRUE(source.empty());
	EXPECT_EQ(heap_tracked_call::alive, 2);

	moved.execute();
	EXPECT_EQ(seen, (std::vector<int>{7, 8}));

	moved.clear();
	EXPECT_EQ(heap_tracked_call::alive, 0);
	EXPECT_EQ(heap_tracked_call::destroyed, 2);
}

int main(int argc, char** argv) {
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
