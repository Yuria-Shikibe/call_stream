module;

#include <cassert>

export module mo_yanxi.call_stream;

import std;


#ifndef __has_cpp_attribute
#define __has_cpp_attribute(attr) 0
#endif

#if defined(__clang__)
#define MO_YANXI_CALL_STREAM_COMPILER_CLANG 1
#else
#define MO_YANXI_CALL_STREAM_COMPILER_CLANG 0
#endif

#if defined(_MSC_VER) && !MO_YANXI_CALL_STREAM_COMPILER_CLANG
#define MO_YANXI_CALL_STREAM_COMPILER_MSVC 1
#else
#define MO_YANXI_CALL_STREAM_COMPILER_MSVC 0
#endif

#if defined(__GNUC__) && !MO_YANXI_CALL_STREAM_COMPILER_CLANG
#define MO_YANXI_CALL_STREAM_COMPILER_GCC 1
#else
#define MO_YANXI_CALL_STREAM_COMPILER_GCC 0
#endif

#if MO_YANXI_CALL_STREAM_COMPILER_MSVC && __has_cpp_attribute(msvc::forceinline)
#define MO_YANXI_CALL_STREAM_FORCE_INLINE [[msvc::forceinline]]
#elif __has_cpp_attribute(gnu::always_inline)
#define MO_YANXI_CALL_STREAM_FORCE_INLINE [[gnu::always_inline]]
#else
#define MO_YANXI_CALL_STREAM_FORCE_INLINE
#endif

#if MO_YANXI_CALL_STREAM_COMPILER_MSVC && __has_cpp_attribute(msvc::no_unique_address)
#define MO_YANXI_CALL_STREAM_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#elif __has_cpp_attribute(no_unique_address)
#define MO_YANXI_CALL_STREAM_NO_UNIQUE_ADDRESS [[no_unique_address]]
#else
#define MO_YANXI_CALL_STREAM_NO_UNIQUE_ADDRESS
#endif

#if MO_YANXI_CALL_STREAM_COMPILER_MSVC && __has_cpp_attribute(msvc::forceinline_calls)
#define MO_YANXI_CALL_STREAM_FORCEINLINE_CALLS [[msvc::forceinline_calls]]
#else
#define MO_YANXI_CALL_STREAM_FORCEINLINE_CALLS
#endif

#if __has_cpp_attribute(assume)
#define MO_YANXI_CALL_STREAM_ASSUME(expr) \
	do{ \
		assert(expr); \
		[[assume(expr)]]; \
	} while(false)
#elif MO_YANXI_CALL_STREAM_COMPILER_MSVC
#define MO_YANXI_CALL_STREAM_ASSUME(expr) \
	do{ \
		assert(expr); \
		__assume(expr); \
	} while(false)
#elif MO_YANXI_CALL_STREAM_COMPILER_CLANG
#define MO_YANXI_CALL_STREAM_ASSUME(expr) \
	do{ \
		assert(expr); \
		__builtin_assume(expr); \
	} while(false)
#elif MO_YANXI_CALL_STREAM_COMPILER_GCC
#define MO_YANXI_CALL_STREAM_ASSUME(expr) \
	do{ \
		assert(expr); \
		if(!(expr)) __builtin_unreachable(); \
	} while(false)
#else
#define MO_YANXI_CALL_STREAM_ASSUME(expr) \
	do{ \
		assert(expr); \
	} while(false)
#endif

#ifndef MO_YANXI_CALL_STREAM_USE_TAIL_DISPATCH
#define MO_YANXI_CALL_STREAM_USE_TAIL_DISPATCH 1
#endif

#if MO_YANXI_CALL_STREAM_USE_TAIL_DISPATCH && MO_YANXI_CALL_STREAM_COMPILER_MSVC
#define MO_YANXI_CALL_STREAM_HAS_TAIL_DISPATCH 0
#define MO_YANXI_CALL_STREAM_MUST_TAIL
#elif MO_YANXI_CALL_STREAM_USE_TAIL_DISPATCH && MO_YANXI_CALL_STREAM_COMPILER_CLANG && __has_cpp_attribute(clang::musttail)
#define MO_YANXI_CALL_STREAM_HAS_TAIL_DISPATCH 1
#define MO_YANXI_CALL_STREAM_MUST_TAIL [[clang::musttail]]
#elif MO_YANXI_CALL_STREAM_USE_TAIL_DISPATCH && MO_YANXI_CALL_STREAM_COMPILER_GCC && __has_cpp_attribute(gnu::musttail)
#define MO_YANXI_CALL_STREAM_HAS_TAIL_DISPATCH 1
#define MO_YANXI_CALL_STREAM_MUST_TAIL [[gnu::musttail]]
#else
#define MO_YANXI_CALL_STREAM_HAS_TAIL_DISPATCH 0
#define MO_YANXI_CALL_STREAM_MUST_TAIL
#endif

// Keep the MSVC path on loop dispatch until its musttail diagnostics are strict enough for this trampoline.


namespace mo_yanxi{
export template <typename Allocator = std::allocator<std::byte>>
class call_stream_buffer{
public:
	using allocator_type = Allocator;
	using value_type = std::byte;
	using pointer = std::allocator_traits<allocator_type>::pointer;
	using const_pointer = std::allocator_traits<allocator_type>::const_pointer;

private:
	MO_YANXI_CALL_STREAM_NO_UNIQUE_ADDRESS allocator_type alloc_;
	pointer data_{nullptr};
	std::size_t size_{0};
	std::size_t capacity_{0};

public:
	explicit call_stream_buffer(const allocator_type& alloc = allocator_type()) noexcept
		: alloc_(alloc){
	}

	call_stream_buffer(const call_stream_buffer&) = delete;
	call_stream_buffer& operator=(const call_stream_buffer&) = delete;

	call_stream_buffer(call_stream_buffer&& other) noexcept
		: alloc_(std::move(other.alloc_)),
		  data_(std::exchange(other.data_, nullptr)),
		  size_(std::exchange(other.size_, 0)),
		  capacity_(std::exchange(other.capacity_, 0)){
	}

	call_stream_buffer& operator=(call_stream_buffer&& other) noexcept{
		if(this == &other) return *this;
		clear_and_deallocate();
		alloc_ = std::move(other.alloc_);
		data_ = std::exchange(other.data_, nullptr);
		size_ = std::exchange(other.size_, 0);
		capacity_ = std::exchange(other.capacity_, 0);
		return *this;
	}

	~call_stream_buffer(){
		clear_and_deallocate();
	}


	template <typename RelocationCallback>
		requires std::is_nothrow_invocable_v<RelocationCallback, pointer, pointer>
	pointer allocate_uninitialized(std::size_t extra_size, RelocationCallback&& on_relocate){
		if(size_ + extra_size > capacity_){
			this->grow_(size_ + extra_size, std::forward<RelocationCallback>(on_relocate));
		}
		pointer result = data_ + size_;
		size_ += extra_size;
		return result;
	}

	template <typename RelocationCallback>
		requires std::is_nothrow_invocable_v<RelocationCallback, pointer, pointer>
	void reserve(std::size_t required_capacity, RelocationCallback&& on_relocate){
		if(required_capacity > capacity_){
			this->grow_(required_capacity, std::forward<RelocationCallback>(on_relocate));
		}
	}


	template <typename RelocationCallback>
		requires std::is_nothrow_invocable_v<RelocationCallback, pointer, pointer>
	void append(const void* src, std::size_t bytes, RelocationCallback&& on_relocate){
		pointer dest = this->allocate_uninitialized(bytes, std::forward<RelocationCallback>(on_relocate));
		std::memcpy(dest, src, bytes);
	}


	template <typename RelocationCallback>
		requires std::is_nothrow_invocable_v<RelocationCallback, pointer, pointer>
	void append_zeros(std::size_t bytes, RelocationCallback&& on_relocate){
		pointer dest = this->allocate_uninitialized(bytes, std::forward<RelocationCallback>(on_relocate));
		std::memset(dest, 0, bytes);
	}

	template <typename RelocationCallback>
		requires std::is_nothrow_invocable_v<RelocationCallback, pointer, pointer>
	void append_uninitialized(std::size_t bytes, RelocationCallback&& on_relocate){
		this->allocate_uninitialized(bytes, std::forward<RelocationCallback>(on_relocate));
	}

	[[nodiscard]] pointer data() noexcept{ return data_; }
	[[nodiscard]] const_pointer data() const noexcept{ return data_; }
	[[nodiscard]] std::size_t size() const noexcept{ return size_; }
	[[nodiscard]] std::size_t capacity() const noexcept{ return capacity_; }
	[[nodiscard]] bool empty() const noexcept{ return size_ == 0; }
	[[nodiscard]] allocator_type get_allocator() const noexcept{ return alloc_; }

	void clear() noexcept{
		size_ = 0;
	}

	void rollback_size(std::size_t previous_size) noexcept{
		assert(previous_size <= size_ && "Cannot rollback to a larger size");
		size_ = previous_size;
	}

private:
	void clear_and_deallocate() noexcept{
		if(data_){
			std::allocator_traits<allocator_type>::deallocate(alloc_, data_, capacity_);
			data_ = nullptr;
			size_ = 0;
			capacity_ = 0;
		}
	}


	template <typename RelocationCallback>
	MO_YANXI_CALL_STREAM_FORCE_INLINE void grow_(std::size_t required_capacity, RelocationCallback&& on_relocate){
		std::size_t new_capacity = capacity_ < 512 ? 512 : capacity_ * 2;
		while(new_capacity < required_capacity){
			new_capacity *= 2;
		}

		pointer new_data = std::allocator_traits<allocator_type>::allocate(alloc_, new_capacity);

		if(data_ != nullptr){
			if(size_ > 0){
				std::memcpy(new_data, data_, size_);

				MO_YANXI_CALL_STREAM_FORCEINLINE_CALLS on_relocate(data_, new_data);
			}

			std::allocator_traits<allocator_type>::deallocate(alloc_, data_, capacity_);
		}

		data_ = new_data;
		capacity_ = new_capacity;
	}
};
}


namespace mo_yanxi{
template <typename Result, typename Ret>
concept call_stream_return_compatible =
	std::is_void_v<Ret> || std::convertible_to<Result, Ret>;

template <typename Ret, typename... Args>
struct static_call_operator_probe{
	template <typename Result>
		requires call_stream_return_compatible<Result, Ret>
	static std::true_type test(Result(*)(Args...));

	static std::false_type test(...);
};

template <typename T, typename Ret, typename... Args>
concept has_compatible_static_call_operator = requires{
	requires decltype(static_call_operator_probe<Ret, Args...>::test(&T::operator()))::value;
};

template <typename Ret, typename Fn, typename... Args>
concept call_stream_invocable = std::is_invocable_r_v<Ret, Fn, Args...>;


using invoke_fn_return_type =
#if MO_YANXI_CALL_STREAM_HAS_TAIL_DISPATCH
void
#else
std::byte*
#endif
;

using invoker_fn = invoke_fn_return_type(*)(std::byte* current_instr_base, const std::byte* end, void* invoke_args_ptr);


struct alignas(16) instr_header{
	invoker_fn invoker;
	std::uint32_t prev_res_offset;
	std::uint32_t flags_or_padding;
};


export template <typename Fn>
struct cmd_call;

export
template <typename Allocator, typename Ret, typename... Args>
class basic_call_stream;

template <typename Allocator, typename Fn>
struct basic_call_stream_selector;

template <typename Allocator, typename Ret, typename... Args>
struct basic_call_stream_selector<Allocator, Ret(Args...)>{
	using type = basic_call_stream<Allocator, Ret, Args...>;
};


template <typename Ret, bool TriviallyDestructible = std::is_trivially_destructible_v<Ret>>
class call_stream_result_slot;

template <typename Ret>
	requires(!std::is_reference_v<Ret> && !std::is_trivially_destructible_v<Ret>)
class call_stream_result_slot<Ret, false>{
	alignas(Ret) std::byte storage_[sizeof(Ret)]{};
	bool engaged_{false};

	Ret* ptr() noexcept{
		return std::launder(reinterpret_cast<Ret*>(storage_));
	}

public:
	call_stream_result_slot() = default;
	call_stream_result_slot(const call_stream_result_slot&) = delete;
	call_stream_result_slot& operator=(const call_stream_result_slot&) = delete;

	~call_stream_result_slot(){
		assert(!engaged_);
	}

	template <typename Value>
	void emplace(Value&& value){
		assert(!engaged_);
		new(static_cast<void*>(storage_)) Ret(std::forward<Value>(value));
		engaged_ = true;
	}

	Ret&& get() noexcept{
		assert(engaged_);
		return std::move(*ptr());
	}

	[[nodiscard]] bool engaged() const noexcept{
		return engaged_;
	}

	void destroy() noexcept(std::is_nothrow_destructible_v<Ret>){
		if(!engaged_) return;
		ptr()->~Ret();
		engaged_ = false;
	}
};

template <typename Ret>
	requires(!std::is_reference_v<Ret> && std::is_trivially_destructible_v<Ret>)
class call_stream_result_slot<Ret, true>{
	alignas(Ret) std::byte storage_[sizeof(Ret)]{};
#ifndef NDEBUG
	bool engaged_{false};
#endif

	Ret* ptr() noexcept{
		return std::launder(reinterpret_cast<Ret*>(storage_));
	}

public:
	call_stream_result_slot() = default;
	call_stream_result_slot(const call_stream_result_slot&) = delete;
	call_stream_result_slot& operator=(const call_stream_result_slot&) = delete;

	~call_stream_result_slot(){
#ifndef NDEBUG
		assert(!engaged_);
#endif
	}

	template <typename Value>
	void emplace(Value&& value){
#ifndef NDEBUG
		assert(!engaged_);
#endif
		new(static_cast<void*>(storage_)) Ret(std::forward<Value>(value));
#ifndef NDEBUG
		engaged_ = true;
#endif
	}

	Ret&& get() noexcept{
#ifndef NDEBUG
		assert(engaged_);
#endif
		return std::move(*ptr());
	}

	[[nodiscard]] bool engaged() const noexcept{
#ifndef NDEBUG
		return engaged_;
#else
		return true;
#endif
	}

	void destroy() noexcept{
		ptr()->~Ret();
#ifndef NDEBUG
		engaged_ = false;
#endif
	}
};

template <typename Ret, bool TriviallyDestructible>
	requires std::is_reference_v<Ret>
class call_stream_result_slot<Ret, TriviallyDestructible>{
	using value_type = std::remove_reference_t<Ret>;

	value_type* ptr_{nullptr};

public:
	template <typename Value>
	void emplace(Value&& value) noexcept{
		static_assert(std::is_lvalue_reference_v<Value>,
		              "call_stream reference return values must be produced from lvalue references");
		ptr_ = std::addressof(value);
	}

	Ret&& get() const noexcept{
		assert(ptr_ != nullptr);
		return static_cast<Ret&&>(*ptr_);
	}

	[[nodiscard]] bool engaged() const noexcept{
		return ptr_ != nullptr;
	}

	void destroy() noexcept{
		ptr_ = nullptr;
	}
};

template <typename Ret, typename ArgsTuple>
struct call_stream_result_state{
	MO_YANXI_CALL_STREAM_NO_UNIQUE_ADDRESS ArgsTuple args;
	call_stream_result_slot<Ret> result;
	std::byte* next;
};

template <typename Ret, typename ArgsTuple>
	requires std::is_void_v<Ret>
struct call_stream_result_state<Ret, ArgsTuple>{
};

template <typename Ret, typename ArgsTuple, typename Callback>
struct call_stream_result_context{
	call_stream_result_state<Ret, ArgsTuple> state;
	Callback* callback;
};

template <typename T>
struct is_cmd_call : std::false_type{
};

template <typename Fn>
struct is_cmd_call<cmd_call<Fn>> : std::true_type{
};

template <typename T>
concept not_cmd_call = !is_cmd_call<std::remove_cvref_t<T>>::value;

template <typename Fn, typename... CallArgs>
MO_YANXI_CALL_STREAM_FORCE_INLINE decltype(auto) invoke_callable(Fn&& fn, CallArgs&&... args){
	if constexpr(requires{
		std::forward<Fn>(fn)(std::forward<CallArgs>(args)...);
	}){
		return std::forward<Fn>(fn)(std::forward<CallArgs>(args)...);
	} else{
		return std::invoke(std::forward<Fn>(fn), std::forward<CallArgs>(args)...);
	}
}

MO_YANXI_CALL_STREAM_FORCE_INLINE constexpr std::size_t align_forward(std::size_t offset, std::size_t alignment) noexcept{
	assert(std::has_single_bit(alignment));
	std::size_t aligned = (offset + alignment - 1) & ~(alignment - 1);
	assert(aligned >= offset);
	return aligned;
}

template <typename Allocator, typename Ret, typename... Args>
class basic_call_stream{
public:
	static_assert(std::is_same_v<typename std::allocator_traits<Allocator>::value_type, std::byte>,
	              "allocator value type must be std::byte");

	using allocator_type = Allocator;
	using return_type = Ret;
	using invoke_args = std::tuple<Args...>;
	using result_state = call_stream_result_state<Ret, invoke_args>;


	using resource_handle_fn = void(*)(basic_call_stream& stream, void* old_base, void* new_base) noexcept;

private:
	call_stream_buffer<Allocator> buffer_;
	std::size_t ip_{0};


	std::uint32_t last_res_offset_{~0U};

	void clear_res_() noexcept{
		std::uint32_t curr = last_res_offset_;
		while(curr != ~0U){
			auto* header = std::launder(reinterpret_cast<instr_header*>(buffer_.data() + curr));
			auto handler = *std::launder(
				reinterpret_cast<resource_handle_fn*>(buffer_.data() + curr + sizeof(instr_header)));
			std::uint32_t next = header->prev_res_offset;

			handler(*this, buffer_.data() + curr, nullptr);
			curr = next;
		}
		last_res_offset_ = ~0U;
	}

	auto get_relocate_cb() noexcept{
		return [this](std::byte* old_base, std::byte* new_base) noexcept{
			std::uint32_t curr = last_res_offset_;
			while(curr != ~0U){
				auto* header = std::launder(reinterpret_cast<instr_header*>(old_base + curr));
				auto handler = *std::launder(
					reinterpret_cast<resource_handle_fn*>(old_base + curr + sizeof(instr_header)));

				handler(*this, old_base + curr, new_base + curr);
				curr = header->prev_res_offset;
			}
		};
	}


	void ensure_header_alignment(){
		std::size_t current_size = buffer_.size();
		std::size_t padding = align_forward(current_size, alignof(instr_header)) - current_size;
		if(padding > 0){
			buffer_.append_zeros(padding, get_relocate_cb());
		}
	}

	template <typename Fn, std::size_t... Is>
	static decltype(auto) invoke_with_args_(Fn&& fn, invoke_args& args, std::index_sequence<Is...>){
		return invoke_callable(std::forward<Fn>(fn), std::forward<Args>(std::get<Is>(args))...);
	}

	template <typename Fn>
	static void invoke_payload_(Fn&& fn, void* invoke_args_ptr) requires(std::is_void_v<Ret>){
		if constexpr(sizeof...(Args) == 0){
			(void)invoke_callable(std::forward<Fn>(fn));
		} else{
			auto& args = *static_cast<invoke_args*>(invoke_args_ptr);
			(void)basic_call_stream::invoke_with_args_(std::forward<Fn>(fn), args,
			                                                std::index_sequence_for<Args...>{});
		}
	}

	template <typename Fn>
	static void invoke_payload_(Fn&& fn, void* context_ptr) requires(!std::is_void_v<Ret>){
		auto& context = *static_cast<result_state*>(context_ptr);
		if constexpr(sizeof...(Args) == 0){
			context.result.emplace(invoke_callable(std::forward<Fn>(fn)));
		} else{
			context.result.emplace(
				basic_call_stream::invoke_with_args_(
					std::forward<Fn>(fn),
					context.args,
					std::index_sequence_for<Args...>{}));
		}
	}

	template <typename T, std::size_t... Is>
	static decltype(auto) invoke_static_with_args_(invoke_args& args, std::index_sequence<Is...>){
		return T::operator()(std::forward<Args>(std::get<Is>(args))...);
	}

	template <typename T>
	static void invoke_static_payload_(void* invoke_args_ptr) requires(std::is_void_v<Ret>){
		if constexpr(sizeof...(Args) == 0){
			(void)T::operator()();
		} else{
			auto& args = *static_cast<invoke_args*>(invoke_args_ptr);
			(void)basic_call_stream::invoke_static_with_args_<
				T>(args, std::index_sequence_for<Args...>{});
		}
	}

	template <typename T>
	static void invoke_static_payload_(void* context_ptr) requires(!std::is_void_v<Ret>){
		auto& context = *static_cast<result_state*>(context_ptr);
		if constexpr(sizeof...(Args) == 0){
			context.result.emplace(T::operator()());
		} else{
			context.result.emplace(
				basic_call_stream::invoke_static_with_args_<T>(
					context.args,
					std::index_sequence_for<Args...>{}));
		}
	}

	template <typename FnTy, bool AllowStaticDispatch, typename... CtorArgs>
		requires(std::constructible_from<FnTy, CtorArgs&&...> && call_stream_invocable<Ret, FnTy&, Args...>)
	void emplace_call_(CtorArgs&&... args);

	static invoke_fn_return_type finish_result_instruction_(void* context_ptr,
	                                                        std::byte* next_ptr) requires(!std::is_void_v<Ret>){
#if MO_YANXI_CALL_STREAM_HAS_TAIL_DISPATCH
		auto& context = *static_cast<result_state*>(context_ptr);
		context.next = next_ptr;
#else
		(void)context_ptr;
		return next_ptr;
#endif
	}

	template <typename ResultContext>
	MO_YANXI_CALL_STREAM_FORCE_INLINE void execute_result_context_(ResultContext& context) requires(!std::is_void_v<Ret>){
		std::byte* ptr = buffer_.data() + ip_;
		const std::byte* end = buffer_.data() + buffer_.size();
		if(std::greater_equal<>{}(ptr, end)) return;

		try{
			while(ptr < end){
				auto invoker = std::launder(reinterpret_cast<instr_header*>(ptr))->invoker;
				MO_YANXI_CALL_STREAM_ASSUME(invoker != nullptr);
#if MO_YANXI_CALL_STREAM_HAS_TAIL_DISPATCH
				context.state.next = nullptr;
				invoker(ptr, end, std::addressof(context.state));
				ptr = context.state.next;
#else
				ptr = invoker(ptr, end, std::addressof(context.state));
#endif
				MO_YANXI_CALL_STREAM_ASSUME(ptr != nullptr);

				assert(context.state.result.engaged());
				if constexpr(std::predicate<decltype(*context.callback), Ret&&>){
					bool stop = false;
					try{
						stop = static_cast<bool>(invoke_callable(*context.callback, context.state.result.get()));
					} catch(...){
						context.state.result.destroy();
						throw;
					}
					context.state.result.destroy();
					if(stop) break;
				} else{
					try{
						(void)invoke_callable(*context.callback, context.state.result.get());
					} catch(...){
						context.state.result.destroy();
						throw;
					}
					context.state.result.destroy();
				}
			}
			ip_ = buffer_.size();
		} catch(...){
			ip_ = buffer_.size();
			throw;
		}
	}

	MO_YANXI_CALL_STREAM_FORCE_INLINE void execute_context_(void* context_ptr){
		std::byte* ptr = buffer_.data() + ip_;
		const std::byte* end = buffer_.data() + buffer_.size();
		if(std::greater_equal<>{}(ptr, end)) return;

		try{
#if MO_YANXI_CALL_STREAM_HAS_TAIL_DISPATCH

			auto invoker = std::launder(reinterpret_cast<instr_header*>(ptr))->invoker;
			invoker(ptr, end, context_ptr);
#else

			while(ptr < end){
				auto invoker = std::launder(reinterpret_cast<instr_header*>(ptr))->invoker;
				MO_YANXI_CALL_STREAM_ASSUME(invoker != nullptr);
				ptr = invoker(ptr, end, context_ptr);
			}
#endif
			ip_ = buffer_.size();
		} catch(...){
			ip_ = buffer_.size();
			throw;
		}
	}

public:
	explicit basic_call_stream(const allocator_type& alloc = allocator_type())
		: buffer_(alloc){
	}

	basic_call_stream(const basic_call_stream&) = delete;
	basic_call_stream& operator=(const basic_call_stream&) = delete;

	basic_call_stream(basic_call_stream&& other) noexcept
		: buffer_(std::move(other.buffer_)),
		  ip_(std::exchange(other.ip_, 0)),
		  last_res_offset_(std::exchange(other.last_res_offset_, ~0U)){
	}

	basic_call_stream& operator=(basic_call_stream&& other) noexcept{
		if(this == &other) return *this;
		clear_res_();
		buffer_ = std::move(other.buffer_);
		ip_ = std::exchange(other.ip_, 0);
		last_res_offset_ = std::exchange(other.last_res_offset_, ~0U);
		return *this;
	}

	~basic_call_stream(){
		clear_res_();
	}

	void reserve(std::size_t size){
		buffer_.reserve(size, get_relocate_cb());
	}

	allocator_type get_allocator() const noexcept{ return buffer_.get_allocator(); }


	void emit_instruction(invoker_fn invoker, const void* payload, std::size_t size, std::size_t alignment){
		ensure_header_alignment();
		std::size_t payload_offset = align_forward(sizeof(instr_header), alignment);
		std::size_t total_bytes = align_forward(payload_offset + size, alignof(instr_header));

		if(!std::in_range<std::uint32_t>(total_bytes)){
			throw std::bad_alloc();
		}

		auto* raw_mem = buffer_.allocate_uninitialized(total_bytes, get_relocate_cb());
		new(raw_mem) instr_header{
				.invoker = invoker,
				.prev_res_offset = 0
			};

		if(size > 0 && payload != nullptr){
			std::memcpy(raw_mem + payload_offset, payload, size);
		}
	}


	void emit_non_trivial_call_heap(invoker_fn invoker, void* heap_ptr, resource_handle_fn handler){
		assert(invoker != nullptr);
		assert(heap_ptr != nullptr);
		assert(handler != nullptr);

		const auto checkpoint = buffer_.size();

		try{
			ensure_header_alignment();
			std::size_t current_size = buffer_.size();

			std::size_t handler_offset = sizeof(instr_header);
			std::size_t payload_offset = align_forward(handler_offset + sizeof(resource_handle_fn), alignof(void*));
			std::size_t total_bytes = align_forward(payload_offset + sizeof(void*), alignof(instr_header));

			if(!std::in_range<std::uint32_t>(total_bytes)){
				throw std::bad_alloc();
			}

			auto* raw_mem = buffer_.allocate_uninitialized(total_bytes, get_relocate_cb());
			new(raw_mem) instr_header{
					.invoker = invoker,
					.prev_res_offset = last_res_offset_
				};

			last_res_offset_ = static_cast<std::uint32_t>(current_size);

			new(raw_mem + handler_offset) resource_handle_fn(handler);

			void* obj_ptr = raw_mem + payload_offset;
			std::memcpy(obj_ptr, &heap_ptr, sizeof(void*));
		} catch(...){
			buffer_.rollback_size(checkpoint);
			throw;
		}
	}


	template <typename T, typename... CtorArgs>
		requires std::is_nothrow_move_constructible_v<T>
	void emit_non_trivial_call_inline(invoker_fn invoker, CtorArgs&&... args){
		assert(invoker != nullptr);
		const auto checkpoint = buffer_.size();

		try{
			ensure_header_alignment();
			std::size_t current_size = buffer_.size();
			std::size_t handler_offset = sizeof(instr_header);

			std::size_t payload_offset = align_forward(handler_offset + sizeof(resource_handle_fn), alignof(T));
			std::size_t total_bytes = align_forward(payload_offset + sizeof(T), alignof(instr_header));

			if(!std::in_range<std::uint32_t>(total_bytes)){
				throw std::bad_alloc();
			}

			auto* raw_mem = buffer_.allocate_uninitialized(total_bytes, get_relocate_cb());
			new(raw_mem) instr_header{
					.invoker = invoker,
					.prev_res_offset = last_res_offset_
				};

			last_res_offset_ = static_cast<std::uint32_t>(current_size);

			static constexpr auto res_handler = +[](basic_call_stream&, void* old_base, void* new_base) noexcept{
				constexpr std::size_t p_offset = align_forward(sizeof(instr_header) + sizeof(resource_handle_fn),
				                                               alignof(T));
				auto* typed_src = std::launder(reinterpret_cast<T*>(static_cast<std::byte*>(old_base) + p_offset));

				if(new_base){
					auto* typed_dst = reinterpret_cast<T*>(static_cast<std::byte*>(new_base) + p_offset);
					new(typed_dst) T(std::move(*typed_src));
					typed_src->~T();
				} else{
					typed_src->~T();
				}
			};

			new(raw_mem + handler_offset) resource_handle_fn(res_handler);

			void* obj_ptr = raw_mem + payload_offset;
			new(obj_ptr) T(std::forward<CtorArgs>(args)...);
		} catch(...){
			buffer_.rollback_size(checkpoint);
			throw;
		}
	}

	void emit_noop(){
		this->emit_instruction(+[](std::byte* base, const std::byte* end,
		                           void* invoke_args_ptr) static -> invoke_fn_return_type{
			std::byte* next_ptr = base + sizeof(instr_header);

			if constexpr(std::is_void_v<Ret>){
#if MO_YANXI_CALL_STREAM_HAS_TAIL_DISPATCH
				if(next_ptr < end){
					auto next_invoker = std::launder(reinterpret_cast<instr_header*>(next_ptr))->invoker;
					MO_YANXI_CALL_STREAM_MUST_TAIL return next_invoker(next_ptr, end, invoke_args_ptr);
				}
#else
				return next_ptr;
#endif
			} else{
				return basic_call_stream::finish_result_instruction_(invoke_args_ptr, next_ptr);
			}
		}, nullptr, 0, 1);
	}


	MO_YANXI_CALL_STREAM_FORCE_INLINE void execute(Args... args) requires(std::is_void_v<Ret>){
		if(empty()) return;

		if constexpr(sizeof...(Args) == 0){
			this->execute_context_(nullptr);
		} else{
			invoke_args packed_args(std::forward<Args>(args)...);
			this->execute_context_(std::addressof(packed_args));
		}
	}

	template <std::invocable<Ret&&> Callback>
	MO_YANXI_CALL_STREAM_FORCE_INLINE void execute(Args... args, Callback&& callback) requires(!std::is_void_v<Ret>){
		if(empty()) return;

		using CallbackT = std::remove_reference_t<Callback>;
		call_stream_result_context<Ret, invoke_args, CallbackT> context{
				.state = result_state{
					.args = invoke_args(std::forward<Args>(args)...),
					.result = {},
					.next = nullptr
				},
				.callback = std::addressof(callback)
			};
		this->execute_result_context_(context);
	}

	void reset_ip(std::size_t new_ip = 0) noexcept{
		assert(new_ip <= buffer_.size());
		ip_ = new_ip;
	}

	[[nodiscard]] std::size_t current_ip() const noexcept{ return ip_; }
	[[nodiscard]] bool empty() const noexcept{ return buffer_.empty(); }
	[[nodiscard]] std::size_t size() const noexcept{ return buffer_.size(); }

	void clear() noexcept{
		clear_res_();
		buffer_.clear();
		ip_ = 0;
	}

	void merge(basic_call_stream&& other){
		if(this == &other || other.empty()) return;

		if(this->empty()){
			*this = std::move(other);
			return;
		}

		ensure_header_alignment();

		const std::size_t base_offset = buffer_.size();
		const std::size_t other_size = other.buffer_.size();

		if(!std::in_range<std::uint32_t>(base_offset + other_size)){
			throw std::bad_alloc();
		}

		auto* dest = buffer_.allocate_uninitialized(other_size, get_relocate_cb());

		std::memcpy(dest, other.buffer_.data(), other_size);
		std::uint32_t curr = other.last_res_offset_;

		while(curr != ~0U){
			auto* old_header = std::launder(reinterpret_cast<instr_header*>(other.buffer_.data() + curr));
			auto* new_header = std::launder(reinterpret_cast<instr_header*>(dest + curr));

			auto handler = *std::launder(
				reinterpret_cast<resource_handle_fn*>(other.buffer_.data() + curr + sizeof(instr_header)));

			handler(*this, other.buffer_.data() + curr, dest + curr);

			std::uint32_t next = old_header->prev_res_offset;

			if(next != ~0U){
				new_header->prev_res_offset = static_cast<std::uint32_t>(base_offset + next);
			} else{
				new_header->prev_res_offset = last_res_offset_;
			}

			curr = next;
		}

		if(other.last_res_offset_ != ~0U){
			last_res_offset_ = static_cast<std::uint32_t>(base_offset + other.last_res_offset_);
		}

		other.last_res_offset_ = ~0U;
		other.buffer_.clear();
		other.ip_ = 0;
	}

	template <typename Fn>
		requires(std::constructible_from<Fn, const Fn&> && call_stream_invocable<Ret, Fn&, Args...>)
	void push_back(const cmd_call<Fn>& call);

	template <typename Fn>
		requires(std::constructible_from<Fn, Fn&&> && call_stream_invocable<Ret, Fn&, Args...>)
	void push_back(cmd_call<Fn>&& call);

	template <typename Fn>
		requires(std::constructible_from<std::decay_t<Fn>, Fn&&> &&
			call_stream_invocable<Ret, std::decay_t<Fn>&, Args...>)
	void emplace_back(Fn&& fn);

	template <typename FnTy, typename... Ts>
		requires(std::constructible_from<FnTy, Ts&&...> && call_stream_invocable<Ret, FnTy&, Args...>)
	void emplace_back(Ts&&... args);

	friend void swap(basic_call_stream& lhs,
	                 basic_call_stream& rhs) noexcept(std::is_nothrow_swappable_v<decltype(buffer_)>){
		std::ranges::swap(lhs.buffer_, rhs.buffer_);
		std::ranges::swap(lhs.ip_, rhs.ip_);
		std::ranges::swap(lhs.last_res_offset_, rhs.last_res_offset_);
	}
};


#if MO_YANXI_CALL_STREAM_HAS_TAIL_DISPATCH
#define MO_YANXI_CALL_STREAM_CMD_CALL_TAIL_DISPATCH(next, end_ptr, invoke_args_ptr) \
	if ((next) >= (end_ptr)) return; \
	auto* next_invoker = std::launder(reinterpret_cast<instr_header*>(next))->invoker; \
	MO_YANXI_CALL_STREAM_ASSUME(next_invoker != nullptr); \
	MO_YANXI_CALL_STREAM_MUST_TAIL return next_invoker((next), (end_ptr), (invoke_args_ptr));
#else
#define MO_YANXI_CALL_STREAM_CMD_CALL_TAIL_DISPATCH(next, end_ptr, invoke_args_ptr) return (next)
#endif

template <typename Fn>
struct cmd_call{
	MO_YANXI_CALL_STREAM_NO_UNIQUE_ADDRESS Fn callable;

	template <typename... Args>
		requires(std::constructible_from<Fn, Args&&...>)
	[[nodiscard]] explicit(false) cmd_call(Args&&... args)
		: callable(std::forward<Args>(args)...){
	}

	MO_YANXI_CALL_STREAM_FORCE_INLINE decltype(auto) operator()() & requires std::invocable<Fn&>{
		return invoke_callable(this->callable);
	}

	MO_YANXI_CALL_STREAM_FORCE_INLINE decltype(auto) operator()() && requires std::invocable<Fn>{
		return invoke_callable(std::move(this->callable));
	}

	MO_YANXI_CALL_STREAM_FORCE_INLINE decltype(auto) operator()() const & requires std::invocable<const Fn&>{
		return invoke_callable(this->callable);
	}
};

template <typename Fn>
cmd_call(Fn&&) -> cmd_call<std::decay_t<Fn>>;

export template <typename FnSign = void(), typename Allocator = std::allocator<std::byte>>
using call_stream = basic_call_stream_selector<Allocator, FnSign>::type;

#pragma region StreamImpl
template <typename Allocator, typename Ret, typename... Args>
template <typename FnTy, bool AllowStaticDispatch, typename... CtorArgs>
	requires(std::constructible_from<FnTy, CtorArgs&&...> && call_stream_invocable<Ret, FnTy&, Args...>)
void basic_call_stream<Allocator, Ret, Args...>::emplace_call_(CtorArgs&&... args){
	using PayloadT = FnTy;


	if constexpr(AllowStaticDispatch){
		constexpr bool is_static = has_compatible_static_call_operator<PayloadT, Ret, Args...>;
		constexpr bool is_empty = std::is_empty_v<PayloadT> &&
			std::default_initializable<PayloadT> &&
			call_stream_invocable<Ret, const PayloadT&, Args...>;
		if constexpr(!std::is_pointer_v<PayloadT> && (is_static || is_empty)){
			this->emit_instruction(+[](std::byte* base, const std::byte* end,
			                           void* invoke_args_ptr) static -> invoke_fn_return_type{
				MO_YANXI_CALL_STREAM_FORCEINLINE_CALLS {
					if constexpr(is_static){
						basic_call_stream::invoke_static_payload_<PayloadT>(invoke_args_ptr);
					} else{
						static const PayloadT fn_raw{};
						basic_call_stream::invoke_payload_(fn_raw, invoke_args_ptr);
					}
				};

				std::byte* next_ptr = base + sizeof(instr_header);
				if constexpr(std::is_void_v<Ret>){
					MO_YANXI_CALL_STREAM_CMD_CALL_TAIL_DISPATCH(next_ptr, end, invoke_args_ptr);
				} else{
					return basic_call_stream::finish_result_instruction_(invoke_args_ptr, next_ptr);
				}
			}, nullptr, 0, 1);
			return;
		}
	}


	constexpr bool is_trivial = std::is_trivially_copyable_v<PayloadT> &&
		std::is_trivially_destructible_v<PayloadT>;


	static constexpr auto fptr = +[](std::byte* base, const std::byte* end,
	                                 void* invoke_args_ptr) static -> invoke_fn_return_type{
		static constexpr std::size_t offset = []{
			if constexpr(is_trivial){
				return align_forward(sizeof(instr_header), alignof(PayloadT));
			} else{
				return align_forward(
					sizeof(instr_header) + sizeof(resource_handle_fn),
					alignof(PayloadT));
			}
		}();
		static constexpr std::size_t total_size = align_forward(offset + sizeof(PayloadT),
		                                                        alignof(instr_header));

		MO_YANXI_CALL_STREAM_FORCEINLINE_CALLS {
			auto& obj = *std::launder(reinterpret_cast<PayloadT*>(base + offset));
			basic_call_stream::invoke_payload_(obj, invoke_args_ptr);
		}

		std::byte* next_ptr = base + total_size;
		if constexpr(std::is_void_v<Ret>){
			MO_YANXI_CALL_STREAM_CMD_CALL_TAIL_DISPATCH(next_ptr, end, invoke_args_ptr);
		} else{
			return basic_call_stream::finish_result_instruction_(invoke_args_ptr, next_ptr);
		}
	};


	if constexpr(is_trivial){
		PayloadT payload(std::forward<CtorArgs>(args)...);
		this->emit_instruction(fptr, &payload, sizeof(PayloadT), alignof(PayloadT));
	} else if constexpr(std::is_nothrow_move_constructible_v<PayloadT>){
		this->template emit_non_trivial_call_inline<PayloadT>(fptr, std::forward<CtorArgs>(args)...);
	} else{
		using TypedAlloc = std::allocator_traits<Allocator>::template rebind_alloc<PayloadT>;
		TypedAlloc alloc(this->get_allocator());
		PayloadT* heap_obj = std::allocator_traits<TypedAlloc>::allocate(alloc, 1);

		try{
			std::allocator_traits<TypedAlloc>::construct(alloc, heap_obj, std::forward<CtorArgs>(args)...);
		} catch(...){
			std::allocator_traits<TypedAlloc>::deallocate(alloc, heap_obj, 1);
			throw;
		}

		try{
			this->emit_non_trivial_call_heap(
				+[](std::byte* base, const std::byte* end, void* invoke_args_ptr) static -> invoke_fn_return_type{
					static constexpr std::size_t payload_offset = align_forward(
						sizeof(instr_header) + sizeof(resource_handle_fn),
						alignof(PayloadT*));
					static constexpr std::size_t total_size = align_forward(
						payload_offset + sizeof(PayloadT*), alignof(instr_header));

					MO_YANXI_CALL_STREAM_FORCEINLINE_CALLS {
						auto& obj_ptr = *std::launder(reinterpret_cast<PayloadT**>(base + payload_offset));
						basic_call_stream::invoke_payload_(*obj_ptr, invoke_args_ptr);
					}

					std::byte* next_ptr = base + total_size;
					if constexpr(std::is_void_v<Ret>){
						MO_YANXI_CALL_STREAM_CMD_CALL_TAIL_DISPATCH(next_ptr, end, invoke_args_ptr);
					} else{
						return basic_call_stream::finish_result_instruction_(invoke_args_ptr, next_ptr);
					}
				},
				heap_obj,
				+[] MO_YANXI_CALL_STREAM_FORCE_INLINE (basic_call_stream& s, void* old_base, void* new_base) noexcept{
					if(new_base) return;
					constexpr std::size_t p_off = align_forward(
						sizeof(instr_header) + sizeof(resource_handle_fn),
						alignof(PayloadT*));
					auto* typed_ptr =
						*std::launder(reinterpret_cast<PayloadT**>(static_cast<std::byte*>(old_base) + p_off));

					TypedAlloc del_alloc(s.get_allocator());
					std::allocator_traits<TypedAlloc>::destroy(del_alloc, typed_ptr);
					std::allocator_traits<TypedAlloc>::deallocate(del_alloc, typed_ptr, 1);
				}
			);
		} catch(...){
			std::allocator_traits<TypedAlloc>::destroy(alloc, heap_obj);
			std::allocator_traits<TypedAlloc>::deallocate(alloc, heap_obj, 1);
			throw;
		}
	}
}

template <typename Allocator, typename Ret, typename... Args>
template <typename Fn>
	requires(std::constructible_from<std::decay_t<Fn>, Fn&&> &&
		call_stream_invocable<Ret, std::decay_t<Fn>&, Args...>)
void basic_call_stream<Allocator, Ret, Args...>::emplace_back(Fn&& fn){
	this->template emplace_call_<std::decay_t<Fn>, true>(std::forward<Fn>(fn));
}

template <typename Allocator, typename Ret, typename... Args>
template <typename FnTy, typename... Ts>
	requires(std::constructible_from<FnTy, Ts&&...> && call_stream_invocable<Ret, FnTy&, Args...>)
void basic_call_stream<Allocator, Ret, Args...>::emplace_back(Ts&&... args){
	this->template emplace_call_<FnTy, sizeof...(Ts) == 0>(std::forward<Ts>(args)...);
}

template <typename Allocator, typename Ret, typename... Args>
template <typename Fn>
	requires(std::constructible_from<Fn, const Fn&> && call_stream_invocable<Ret, Fn&, Args...>)
void basic_call_stream<Allocator, Ret, Args...>::push_back(const cmd_call<Fn>& call){
	this->emplace_back(call.callable);
}

template <typename Allocator, typename Ret, typename... Args>
template <typename Fn>
	requires(std::constructible_from<Fn, Fn&&> && call_stream_invocable<Ret, Fn&, Args...>)
void basic_call_stream<Allocator, Ret, Args...>::push_back(cmd_call<Fn>&& call){
	this->emplace_back(std::move(call.callable));
}

export template <typename Allocator, typename Ret, typename... Args, typename Fn>
	requires(not_cmd_call<Fn> &&
		requires(basic_call_stream<Allocator, Ret, Args...>& stream, Fn&& call){
			stream.emplace_back(std::forward<Fn>(call));
		})
basic_call_stream<Allocator, Ret, Args...>& operator<<(
	basic_call_stream<Allocator, Ret, Args...>& stream, Fn&& call){
	stream.emplace_back(std::forward<Fn>(call));
	return stream;
}

export template <typename Allocator, typename Ret, typename... Args, typename Fn>
	requires requires(basic_call_stream<Allocator, Ret, Args...>& stream, cmd_call<Fn>&& call){
		stream.push_back(std::move(call));
	}
basic_call_stream<Allocator, Ret, Args...>& operator<<(
	basic_call_stream<Allocator, Ret, Args...>& stream, cmd_call<Fn>&& call){
	stream.push_back(std::move(call));
	return stream;
}
#pragma endregion

}
