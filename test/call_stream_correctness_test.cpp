#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <type_traits>
#include <vector>

import mo_yanxi.call_stream;

namespace {

using byte_allocator = std::allocator<std::byte>;

template <typename... Args>
using void_stream = mo_yanxi::basic_call_stream<byte_allocator, void, Args...>;

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

TEST(CallStreamCorrectnessTest, ResultCallbackReceivesAllResultsWhenItReturnsVoid) {
	mo_yanxi::call_stream<int(int)> stream;
	std::vector<int> results;

	stream.emplace_back([](int value) { return value + 1; });
	stream.emplace_back([](int value) { return value + 2; });
	stream.emplace_back([](int value) { return value + 3; });

	stream.execute(10, [&](int result) { results.push_back(result); });

	EXPECT_EQ(results, (std::vector<int>{11, 12, 13}));
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
