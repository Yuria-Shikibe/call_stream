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

#define MO_YANXI_CALL_STREAM_UNREACHABLE() \
	do{ \
		assert(false); \
		std::unreachable(); \
	} while(false)

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
		if(!(expr)) MO_YANXI_CALL_STREAM_UNREACHABLE(); \
	} while(false)
#else
#define MO_YANXI_CALL_STREAM_ASSUME(expr) \
	do{ \
		assert(expr); \
	} while(false)
#endif

#ifndef MO_YANXI_CALL_STREAM_USE_TAIL_DISPATCH
#define MO_YANXI_CALL_STREAM_USE_TAIL_DISPATCH MO_YANXI_CALL_STREAM_COMPILER_CLANG
#endif

#ifndef MO_YANXI_CALL_STREAM_USE_NOEXCEPT_TAIL_DISPATCH
#define MO_YANXI_CALL_STREAM_USE_NOEXCEPT_TAIL_DISPATCH MO_YANXI_CALL_STREAM_COMPILER_CLANG
#endif

#ifndef MO_YANXI_CALL_STREAM_USE_SCALAR_RESULT_DISPATCH
#define MO_YANXI_CALL_STREAM_USE_SCALAR_RESULT_DISPATCH MO_YANXI_CALL_STREAM_COMPILER_CLANG
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
template <std::size_t Alignment>
struct alignas(Alignment) call_stream_buffer_storage_unit{
	static_assert(Alignment > 0);
	static_assert(std::has_single_bit(Alignment));

	std::byte bytes[Alignment];
};

export template <typename Allocator = std::allocator<std::byte>,
                 std::size_t BufferAlignment = alignof(std::max_align_t)>
class call_stream_buffer{
public:
	using allocator_type = Allocator;
	using allocator_traits = std::allocator_traits<allocator_type>;
	using value_type = std::byte;
	using pointer = typename allocator_traits::pointer;
	using const_pointer = typename allocator_traits::const_pointer;
	static constexpr std::size_t buffer_alignment = BufferAlignment;

private:
	using storage_unit = call_stream_buffer_storage_unit<buffer_alignment>;
	using storage_allocator_type = typename allocator_traits::template rebind_alloc<storage_unit>;
	using storage_allocator_traits = std::allocator_traits<storage_allocator_type>;
	using storage_pointer = typename storage_allocator_traits::pointer;

	static constexpr bool propagate_on_move_assignment_ =
		allocator_traits::propagate_on_container_move_assignment::value;
	static constexpr bool propagate_on_swap_ = allocator_traits::propagate_on_container_swap::value;
	static constexpr bool is_always_equal_ = allocator_traits::is_always_equal::value;
	static constexpr bool move_assignment_noexcept_ =
		propagate_on_move_assignment_
			? std::is_nothrow_move_assignable_v<allocator_type> &&
				std::is_nothrow_move_assignable_v<storage_allocator_type>
			: is_always_equal_;
	static constexpr bool swap_noexcept_ =
		propagate_on_swap_
			? std::is_nothrow_swappable_v<allocator_type> &&
				std::is_nothrow_swappable_v<storage_allocator_type>
			: is_always_equal_;

public:
	static_assert(std::is_same_v<typename allocator_traits::value_type, std::byte>,
	              "allocator value type must be std::byte");
	static_assert(std::is_same_v<pointer, std::byte*>,
	              "call_stream_buffer requires a raw std::byte* allocator pointer");
	static_assert(std::is_same_v<const_pointer, const std::byte*>,
	              "call_stream_buffer requires a raw const std::byte* allocator pointer");
	static_assert(std::is_same_v<storage_pointer, storage_unit*>,
	              "call_stream_buffer requires raw pointers from rebound storage allocators");

private:
	MO_YANXI_CALL_STREAM_NO_UNIQUE_ADDRESS allocator_type alloc_;
	MO_YANXI_CALL_STREAM_NO_UNIQUE_ADDRESS storage_allocator_type storage_alloc_;
	storage_pointer storage_{nullptr};
	pointer data_{nullptr};
	std::size_t size_{0};
	std::size_t capacity_{0};
	std::size_t storage_capacity_{0};

public:
	inline explicit call_stream_buffer(const allocator_type& alloc = allocator_type())
		noexcept(std::is_nothrow_copy_constructible_v<allocator_type> &&
			std::is_nothrow_constructible_v<storage_allocator_type, const allocator_type&>)
		: alloc_(alloc),
		  storage_alloc_(alloc){
	}

	inline call_stream_buffer(const call_stream_buffer&) = delete;
	inline call_stream_buffer& operator=(const call_stream_buffer&) = delete;

	inline call_stream_buffer(call_stream_buffer&& other) noexcept(
		std::is_nothrow_move_constructible_v<allocator_type> &&
		std::is_nothrow_move_constructible_v<storage_allocator_type>)
		: alloc_(std::move(other.alloc_)),
		  storage_alloc_(std::move(other.storage_alloc_)),
		  storage_(std::exchange(other.storage_, nullptr)),
		  data_(std::exchange(other.data_, nullptr)),
		  size_(std::exchange(other.size_, 0)),
		  capacity_(std::exchange(other.capacity_, 0)),
		  storage_capacity_(std::exchange(other.storage_capacity_, 0)){
	}

	inline call_stream_buffer& operator=(call_stream_buffer&& other) noexcept(move_assignment_noexcept_){
		if(this == &other) return *this;

		if constexpr(propagate_on_move_assignment_){
			clear_and_deallocate();
			alloc_ = std::move(other.alloc_);
			storage_alloc_ = std::move(other.storage_alloc_);
			steal_from_(other);
		} else{
			if constexpr(!is_always_equal_){
				if(!this->is_allocator_compatible_with(other)){
					throw std::invalid_argument("call_stream_buffer move assignment requires compatible allocators");
				}
			}

			clear_and_deallocate();
			steal_from_(other);
		}
		return *this;
	}

	inline ~call_stream_buffer(){
		clear_and_deallocate();
	}


	template <typename RelocationCallback>
		requires std::is_nothrow_invocable_v<RelocationCallback, pointer, pointer>
	inline pointer allocate_uninitialized(std::size_t extra_size, RelocationCallback&& on_relocate){
		if(extra_size > std::numeric_limits<std::size_t>::max() - size_){
			throw std::bad_alloc();
		}

		const std::size_t required_size = size_ + extra_size;
		if(required_size > capacity_){
			this->grow_(required_size, std::forward<RelocationCallback>(on_relocate));
		}

		pointer result = data_ == nullptr ? nullptr : data_ + size_;
		size_ += extra_size;
		return result;
	}

	template <typename RelocationCallback>
		requires std::is_nothrow_invocable_v<RelocationCallback, pointer, pointer>
	inline void reserve(std::size_t required_capacity, RelocationCallback&& on_relocate){
		if(required_capacity > capacity_){
			this->grow_(required_capacity, std::forward<RelocationCallback>(on_relocate));
		}
	}


	template <typename RelocationCallback>
		requires std::is_nothrow_invocable_v<RelocationCallback, pointer, pointer>
	inline void append(const void* src, std::size_t bytes, RelocationCallback&& on_relocate){
		pointer dest = this->allocate_uninitialized(bytes, std::forward<RelocationCallback>(on_relocate));
		if(bytes > 0){
			std::memcpy(dest, src, bytes);
		}
	}


	template <typename RelocationCallback>
		requires std::is_nothrow_invocable_v<RelocationCallback, pointer, pointer>
	inline void append_zeros(std::size_t bytes, RelocationCallback&& on_relocate){
		pointer dest = this->allocate_uninitialized(bytes, std::forward<RelocationCallback>(on_relocate));
		if(bytes > 0){
			std::memset(dest, 0, bytes);
		}
	}

	template <typename RelocationCallback>
		requires std::is_nothrow_invocable_v<RelocationCallback, pointer, pointer>
	inline void append_uninitialized(std::size_t bytes, RelocationCallback&& on_relocate){
		this->allocate_uninitialized(bytes, std::forward<RelocationCallback>(on_relocate));
	}

	[[nodiscard]] inline pointer data() noexcept{ return data_; }
	[[nodiscard]] inline const_pointer data() const noexcept{ return data_; }
	[[nodiscard]] inline std::size_t size() const noexcept{ return size_; }
	[[nodiscard]] inline std::size_t capacity() const noexcept{ return capacity_; }
	[[nodiscard]] inline bool empty() const noexcept{ return size_ == 0; }
	[[nodiscard]] inline allocator_type get_allocator() const noexcept{ return alloc_; }

	template <typename T>
	[[nodiscard]] inline typename allocator_traits::template rebind_alloc<T> get_rebound_allocator() const{
		return typename allocator_traits::template rebind_alloc<T>(alloc_);
	}

	[[nodiscard]] inline bool is_allocator_compatible_with(const call_stream_buffer& other) const
		noexcept(is_always_equal_ || noexcept(std::declval<const allocator_type&>() == std::declval<const allocator_type&>())){
		if constexpr(is_always_equal_){
			return true;
		} else{
			return alloc_ == other.alloc_;
		}
	}

	[[nodiscard]] inline bool can_move_assign_from(const call_stream_buffer& other) const
		noexcept(noexcept(this->is_allocator_compatible_with(other))){
		if constexpr(propagate_on_move_assignment_ || is_always_equal_){
			return true;
		} else{
			return this->is_allocator_compatible_with(other);
		}
	}

	[[nodiscard]] inline bool can_swap_with(const call_stream_buffer& other) const
		noexcept(noexcept(this->is_allocator_compatible_with(other))){
		if constexpr(propagate_on_swap_ || is_always_equal_){
			return true;
		} else{
			return this->is_allocator_compatible_with(other);
		}
	}

	inline void clear() noexcept{
		size_ = 0;
	}

	inline void rollback_size(std::size_t previous_size) noexcept{
		assert(previous_size <= size_ && "Cannot rollback to a larger size");
		size_ = previous_size;
	}

	inline void swap(call_stream_buffer& other) noexcept(swap_noexcept_){
		if(this == &other) return;

		if constexpr(propagate_on_swap_){
			std::ranges::swap(alloc_, other.alloc_);
			std::ranges::swap(storage_alloc_, other.storage_alloc_);
		} else{
			if constexpr(!is_always_equal_){
				if(!this->is_allocator_compatible_with(other)){
					throw std::invalid_argument("call_stream_buffer swap requires compatible allocators");
				}
			}
		}

		std::ranges::swap(storage_, other.storage_);
		std::ranges::swap(data_, other.data_);
		std::ranges::swap(size_, other.size_);
		std::ranges::swap(capacity_, other.capacity_);
		std::ranges::swap(storage_capacity_, other.storage_capacity_);
	}

	friend void swap(call_stream_buffer& lhs, call_stream_buffer& rhs) noexcept(swap_noexcept_){
		lhs.swap(rhs);
	}

private:
	inline void clear_and_deallocate() noexcept{
		if(storage_){
			storage_allocator_traits::deallocate(storage_alloc_, storage_, storage_capacity_);
			storage_ = nullptr;
			data_ = nullptr;
			size_ = 0;
			capacity_ = 0;
			storage_capacity_ = 0;
		}
	}

	inline void steal_from_(call_stream_buffer& other) noexcept{
		storage_ = std::exchange(other.storage_, nullptr);
		data_ = std::exchange(other.data_, nullptr);
		size_ = std::exchange(other.size_, 0);
		capacity_ = std::exchange(other.capacity_, 0);
		storage_capacity_ = std::exchange(other.storage_capacity_, 0);
	}

	[[nodiscard]] inline std::size_t max_byte_capacity_() const{
		const std::size_t max_units = storage_allocator_traits::max_size(storage_alloc_);
		constexpr std::size_t max_units_by_size =
			std::numeric_limits<std::size_t>::max() / buffer_alignment;
		return std::min(max_units, max_units_by_size) * buffer_alignment;
	}

	[[nodiscard]] inline std::size_t units_for_bytes_(std::size_t bytes) const{
		const std::size_t max_bytes = max_byte_capacity_();
		if(bytes > max_bytes){
			throw std::bad_alloc();
		}

		return (bytes + buffer_alignment - 1) / buffer_alignment;
	}

	[[nodiscard]] inline std::size_t bytes_for_units_(std::size_t units) const{
		const std::size_t max_units = storage_allocator_traits::max_size(storage_alloc_);
		if(units > max_units || units > std::numeric_limits<std::size_t>::max() / buffer_alignment){
			throw std::bad_alloc();
		}

		return units * buffer_alignment;
	}

	[[nodiscard]] inline std::size_t next_capacity_(std::size_t required_capacity) const{
		const std::size_t max_bytes = max_byte_capacity_();
		if(required_capacity > max_bytes){
			throw std::bad_alloc();
		}

		std::size_t new_capacity = capacity_ < 512 ? 512 : capacity_;
		while(new_capacity < required_capacity){
			if(new_capacity > max_bytes / 2){
				new_capacity = max_bytes;
				break;
			}
			new_capacity *= 2;
		}

		if(new_capacity < required_capacity){
			throw std::bad_alloc();
		}

		return bytes_for_units_(units_for_bytes_(new_capacity));
	}


	template <typename RelocationCallback>
	MO_YANXI_CALL_STREAM_FORCE_INLINE inline void
	grow_(std::size_t required_capacity, RelocationCallback&& on_relocate){
		const std::size_t new_capacity = next_capacity_(required_capacity);
		const std::size_t new_storage_capacity = units_for_bytes_(new_capacity);

		storage_pointer new_storage = storage_allocator_traits::allocate(storage_alloc_, new_storage_capacity);
		pointer new_data = reinterpret_cast<pointer>(new_storage);

		if(data_ != nullptr){
			if(size_ > 0){
				std::memcpy(new_data, data_, size_);

				MO_YANXI_CALL_STREAM_FORCEINLINE_CALLS on_relocate(data_, new_data);
			}

			storage_allocator_traits::deallocate(storage_alloc_, storage_, storage_capacity_);
		}

		storage_ = new_storage;
		data_ = new_data;
		capacity_ = new_capacity;
		storage_capacity_ = new_storage_capacity;
	}
};
}


namespace mo_yanxi{
export enum class call_stream_exception_policy{
	resumable,
	nothrow
};

template <typename Arg>
using call_stream_invoke_arg_t = std::conditional_t<
	std::is_reference_v<Arg>,
	Arg,
	std::add_lvalue_reference_t<Arg>>;

#pragma region ConceptHelper

template <typename Ret, typename Fn, typename... Args>
concept call_stream_invocable = std::is_invocable_r_v<Ret, Fn, call_stream_invoke_arg_t<Args>...>;

template <typename Ret, typename Fn, typename... Args>
concept call_stream_nothrow_invocable =
	call_stream_invocable<Ret, Fn, Args...> &&
	std::is_nothrow_invocable_r_v<Ret, Fn, call_stream_invoke_arg_t<Args>...>;

template <call_stream_exception_policy ExceptionPolicy, typename Ret, typename Fn, typename... Args>
concept call_stream_policy_invocable =
	call_stream_invocable<Ret, Fn, Args...> &&
	(ExceptionPolicy != call_stream_exception_policy::nothrow ||
		call_stream_nothrow_invocable<Ret, Fn, Args...>);

template <typename T, typename Alloc, typename... Args>
concept call_stream_payload_constructible =
	std::constructible_from<T, Args...> ||
	(std::uses_allocator_v<T, Alloc> &&
		requires(T* ptr, const Alloc& alloc, Args&&... args){
			std::uninitialized_construct_using_allocator(ptr, alloc, std::forward<Args>(args)...);
		});

template <typename T, typename Ret, typename... Args>
concept has_compatible_static_call_operator =
	(std::is_void_v<Ret> &&
		requires{
			T::operator()(std::declval<call_stream_invoke_arg_t<Args>>()...);
		}) ||
	(!std::is_void_v<Ret> &&
		requires{
			{ T::operator()(std::declval<call_stream_invoke_arg_t<Args>>()...) } -> std::convertible_to<Ret>;
		});

template <typename T, typename Ret, typename... Args>
concept has_compatible_nothrow_static_call_operator =
	has_compatible_static_call_operator<T, Ret, Args...> &&
	requires{
		requires noexcept(T::operator()(std::declval<call_stream_invoke_arg_t<Args>>()...));
	};

#pragma endregion

template <typename Arg>
MO_YANXI_CALL_STREAM_FORCE_INLINE decltype(auto) forward_invoke_arg_(std::remove_reference_t<Arg>& arg) noexcept{
	if constexpr(std::is_reference_v<Arg>){
		return static_cast<Arg>(arg);
	} else{
		return static_cast<Arg&>(arg);
	}
}


template <bool UseTailDispatch>
using call_stream_link_return_t = std::conditional_t<UseTailDispatch, void, std::byte*>;

template <typename RetValue>
using call_stream_throwing_invoker_fn =
RetValue(*)(std::byte* current_instr_base, const std::byte* end, void* invoke_args_ptr);

template <typename RetValue>
using call_stream_nothrow_invoker_fn =
RetValue(*)(std::byte* current_instr_base, const std::byte* end, void* invoke_args_ptr) noexcept;

template <bool IsNoexcept, typename RetValue>
using call_stream_invoker_fn_t = std::conditional_t<
	IsNoexcept,
	call_stream_nothrow_invoker_fn<RetValue>,
	call_stream_throwing_invoker_fn<RetValue>>;

export template <typename Fn>
struct cmd_call;

export
template <call_stream_exception_policy ExceptionPolicy, typename Allocator, typename Ret, typename... Args>
class basic_call_stream_impl;

template <typename Fn>
struct call_stream_default_exception_policy;

template <typename Ret, typename... Args>
struct call_stream_default_exception_policy<Ret(Args...)>{
	static constexpr call_stream_exception_policy value = call_stream_exception_policy::resumable;
};

template <typename Ret, typename... Args>
struct call_stream_default_exception_policy<Ret(Args...) noexcept>{
	static constexpr call_stream_exception_policy value = call_stream_exception_policy::nothrow;
};

template <typename Allocator, typename Fn, call_stream_exception_policy ExceptionPolicy>
struct basic_call_stream_selector;

template <typename Allocator, typename Ret, typename... Args, call_stream_exception_policy ExceptionPolicy>
struct basic_call_stream_selector<Allocator, Ret(Args...), ExceptionPolicy>{
	using type = basic_call_stream_impl<ExceptionPolicy, Allocator, Ret, Args...>;
};

template <typename Allocator, typename Ret, typename... Args, call_stream_exception_policy ExceptionPolicy>
struct basic_call_stream_selector<Allocator, Ret(Args...) noexcept, ExceptionPolicy>{
	using type = basic_call_stream_impl<ExceptionPolicy, Allocator, Ret, Args...>;
};


template <typename Ret, bool TriviallyDestructible = std::is_trivially_destructible_v<Ret>>
class call_stream_result_slot;

template <typename Ret>
	requires(!std::is_reference_v<Ret> && !std::is_trivially_destructible_v<Ret>)
class call_stream_result_slot<Ret, false>{
	union storage_type{
		Ret value;

		inline constexpr storage_type() noexcept{
		}

		inline ~storage_type() noexcept{
		}
	};

	storage_type storage_;
	bool engaged_{false};

	inline Ret* ptr() noexcept{
		return std::launder(std::addressof(storage_.value));
	}

public:
	inline call_stream_result_slot() = default;
	inline call_stream_result_slot(const call_stream_result_slot&) = delete;
	inline call_stream_result_slot& operator=(const call_stream_result_slot&) = delete;

	inline ~call_stream_result_slot(){
		assert(!engaged_);
	}

	template <typename Value>
	inline void emplace(Value&& value){
		assert(!engaged_);
		std::construct_at(std::addressof(storage_.value), std::forward<Value>(value));
		engaged_ = true;
	}

	inline void emplace_default() requires std::default_initializable<Ret>{
		assert(!engaged_);
		std::construct_at(std::addressof(storage_.value));
		engaged_ = true;
	}

	inline Ret&& get() noexcept{
		assert(engaged_);
		return std::move(*ptr());
	}

	[[nodiscard]] inline bool engaged() const noexcept{
		return engaged_;
	}

	inline void destroy() noexcept(std::is_nothrow_destructible_v<Ret>){
		if(!engaged_) return;
		std::destroy_at(ptr());
		engaged_ = false;
	}
};

template <typename Ret>
	requires(!std::is_reference_v<Ret> && std::is_trivially_destructible_v<Ret>)
class call_stream_result_slot<Ret, true>{
	union storage_type{
		Ret value;

		inline constexpr storage_type() noexcept{
		}
	};

	storage_type storage_;
#ifndef NDEBUG
	bool engaged_{false};
#endif

	inline Ret* ptr() noexcept{
		return std::launder(std::addressof(storage_.value));
	}

public:
	inline call_stream_result_slot() = default;
	inline call_stream_result_slot(const call_stream_result_slot&) = delete;
	inline call_stream_result_slot& operator=(const call_stream_result_slot&) = delete;

	inline ~call_stream_result_slot(){
#ifndef NDEBUG
		assert(!engaged_);
#endif
	}

	template <typename Value>
	inline void emplace(Value&& value){
#ifndef NDEBUG
		assert(!engaged_);
#endif
		std::construct_at(std::addressof(storage_.value), std::forward<Value>(value));
#ifndef NDEBUG
		engaged_ = true;
#endif
	}

	inline void emplace_default() requires std::default_initializable<Ret>{
#ifndef NDEBUG
		assert(!engaged_);
#endif
		std::construct_at(std::addressof(storage_.value));
#ifndef NDEBUG
		engaged_ = true;
#endif
	}

	inline Ret&& get() noexcept{
#ifndef NDEBUG
		assert(engaged_);
#endif
		return std::move(*ptr());
	}

	[[nodiscard]] inline bool engaged() const noexcept{
#ifndef NDEBUG
		return engaged_;
#else
		return true;
#endif
	}

	inline void destroy() noexcept{
		std::destroy_at(ptr());
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
	inline void emplace(Value&& value) noexcept{
		static_assert(std::is_lvalue_reference_v<Value>,
		              "call_stream reference return values must be produced from lvalue references");
		ptr_ = std::addressof(value);
	}

	inline Ret&& get() const noexcept{
		assert(ptr_ != nullptr);
		return static_cast<Ret&&>(*ptr_);
	}

	[[nodiscard]] inline bool engaged() const noexcept{
		return ptr_ != nullptr;
	}

	inline void destroy() noexcept{
		ptr_ = nullptr;
	}
};

template <typename Ret>
inline constexpr bool call_stream_uses_scalar_result_v = []{
	if constexpr(MO_YANXI_CALL_STREAM_USE_SCALAR_RESULT_DISPATCH &&
		!std::is_void_v<Ret> &&
		!std::is_reference_v<Ret>){
		return std::is_trivially_copyable_v<Ret> &&
			std::is_trivially_destructible_v<Ret> &&
			sizeof(Ret) <= sizeof(void*);
	} else{
		return false;
	}
}();

template <typename Ret, typename ArgsTuple, bool UseScalarResult = call_stream_uses_scalar_result_v<Ret>>
struct call_stream_result_state{
	MO_YANXI_CALL_STREAM_NO_UNIQUE_ADDRESS ArgsTuple args;
	call_stream_result_slot<Ret> result;
	std::byte* next;
};

template <typename Ret, typename ArgsTuple>
struct call_stream_result_state<Ret, ArgsTuple, true>{
	MO_YANXI_CALL_STREAM_NO_UNIQUE_ADDRESS ArgsTuple args;
	std::byte* next;
};

template <typename ArgsTuple>
struct call_stream_result_state<void, ArgsTuple, false>{
};

#if MO_YANXI_CALL_STREAM_USE_SCALAR_RESULT_DISPATCH
template <typename State>
concept call_stream_result_state_has_result = requires(State& state){
	state.result;
};

static_assert(!call_stream_result_state_has_result<call_stream_result_state<int, std::tuple<>>>);
#endif

template <typename Ret, typename ArgsTuple, typename Callback>
struct call_stream_result_context{
	call_stream_result_state<Ret, ArgsTuple> state;
	Callback* callback;
};

template <typename Callback, typename Ret>
inline constexpr bool call_stream_nothrow_result_callback_v = []{
	if constexpr(std::predicate<Callback&, Ret&&>){
		return std::is_nothrow_invocable_r_v<bool, Callback&, Ret&&>;
	} else{
		return std::is_nothrow_invocable_v<Callback&, Ret&&>;
	}
}();

template <typename Ret, bool UseTailDispatch>
using call_stream_invoker_return_t = std::conditional_t<
	call_stream_uses_scalar_result_v<Ret>,
	Ret,
	call_stream_link_return_t<UseTailDispatch>>;

template <bool StoreExceptionState>
struct call_stream_void_tail_dispatch_context{
	void* invoke_args;
};

template <>
struct call_stream_void_tail_dispatch_context<true>{
	void* invoke_args;
	std::byte* current;
	std::exception_ptr exception;
};

template <typename T>
struct is_cmd_call : std::false_type{
};

template <typename Fn>
struct is_cmd_call<cmd_call<Fn>> : std::true_type{
};

template <typename T>
concept not_cmd_call = !is_cmd_call<std::remove_cvref_t<T>>::value;

MO_YANXI_CALL_STREAM_FORCE_INLINE constexpr std::size_t align_forward(std::size_t offset,
                                                                      std::size_t alignment) noexcept{
	assert(std::has_single_bit(alignment));
	std::size_t aligned = (offset + alignment - 1uz) & ~(alignment - 1uz);
	assert(aligned >= offset);
	return aligned;
}

template <call_stream_exception_policy ExceptionPolicy, typename Allocator, typename Ret, typename... Args>
class basic_call_stream_impl{
public:
	static_assert(std::is_same_v<typename std::allocator_traits<Allocator>::value_type, std::byte>,
	              "allocator value type must be std::byte");
	static_assert(!std::is_rvalue_reference_v<Ret>,
	              "call_stream does not support rvalue reference return types");
	static_assert(ExceptionPolicy == call_stream_exception_policy::resumable ||
	              ExceptionPolicy == call_stream_exception_policy::nothrow,
	              "unsupported call_stream exception policy");
	static_assert(ExceptionPolicy != call_stream_exception_policy::nothrow ||
	              std::is_void_v<Ret> || std::is_nothrow_destructible_v<Ret>,
	              "nothrow call_stream requires a nothrow-destructible return type");

	using allocator_type = Allocator;
	using return_type = Ret;
	static constexpr call_stream_exception_policy exception_policy = ExceptionPolicy;
	static constexpr bool is_nothrow = ExceptionPolicy == call_stream_exception_policy::nothrow;
	using invoke_args = std::tuple<Args...>;
	using result_state = call_stream_result_state<Ret, invoke_args>;
	using resource_handle_fn = void(*)(basic_call_stream_impl& stream, void* old_base, void* new_base) noexcept;

private:
	static constexpr bool uses_scalar_result_ = call_stream_uses_scalar_result_v<Ret>;
	static constexpr bool has_tail_dispatch_ =
		MO_YANXI_CALL_STREAM_USE_NOEXCEPT_TAIL_DISPATCH &&
		MO_YANXI_CALL_STREAM_HAS_TAIL_DISPATCH &&
		std::is_void_v<Ret>;
	static constexpr bool invoker_is_nothrow_ = is_nothrow && !has_tail_dispatch_;
	using invoke_fn_return_type = call_stream_invoker_return_t<Ret, has_tail_dispatch_>;
	using invoker_fn = call_stream_invoker_fn_t<invoker_is_nothrow_, invoke_fn_return_type>;

public:
	static constexpr bool uses_musttail_dispatch = has_tail_dispatch_;

private:
	struct
#if MO_YANXI_CALL_STREAM_COMPILER_MSVC
		alignas(16)
#endif
		instr_header{
		invoker_fn invoker;
	};

	MO_YANXI_CALL_STREAM_FORCE_INLINE inline static instr_header* instruction_header_(std::byte* ptr) noexcept{
		return std::launder(std::assume_aligned<alignof(instr_header)>(
			static_cast<instr_header*>(static_cast<void*>(ptr))));
	}

	struct resource_record{
		std::uint32_t offset;
		resource_handle_fn handler;
	};

	using void_tail_dispatch_context =
	call_stream_void_tail_dispatch_context<has_tail_dispatch_ && !is_nothrow>;

	using resource_allocator_type =
	std::allocator_traits<Allocator>::template rebind_alloc<resource_record>;
	using buffer_type = call_stream_buffer<Allocator, alignof(instr_header)>;

	buffer_type buffer_;
	std::vector<resource_record, resource_allocator_type> resources_;
	std::size_t ip_{0};

	inline void clear_res_() noexcept{
		for(auto it = resources_.rbegin(); it != resources_.rend(); ++it){
			it->handler(*this, buffer_.data() + it->offset, nullptr);
		}
		resources_.clear();
	}

	inline auto get_relocate_cb() noexcept{
		return [this](std::byte* old_base, std::byte* new_base) noexcept{
			for(const resource_record& record : resources_){
				record.handler(*this, old_base + record.offset, new_base + record.offset);
			}
		};
	}

	inline void register_resource_(std::size_t offset, resource_handle_fn handler){
		assert(handler != nullptr);
		MO_YANXI_CALL_STREAM_ASSUME(handler != nullptr);
		if(!std::in_range<std::uint32_t>(offset)){
			throw std::bad_alloc();
		}
		resources_.push_back(resource_record{
				.offset = static_cast<std::uint32_t>(offset),
				.handler = handler
			});
	}

	template <typename T, typename TypedAlloc, typename... CtorArgs>
	inline static void construct_payload_(TypedAlloc& alloc, T* ptr, CtorArgs&&... args){
		if constexpr(std::uses_allocator_v<T, TypedAlloc>){
			std::uninitialized_construct_using_allocator(ptr, alloc, std::forward<CtorArgs>(args)...);
		} else{
			std::allocator_traits<TypedAlloc>::construct(alloc, ptr, std::forward<CtorArgs>(args)...);
		}
	}

	[[nodiscard]] inline bool can_transfer_storage_from_(const basic_call_stream_impl& other) const
		noexcept(noexcept(buffer_.is_allocator_compatible_with(other.buffer_))){
		return buffer_.is_allocator_compatible_with(other.buffer_);
	}

	inline void require_transfer_compatible_(const basic_call_stream_impl& other) const{
		if(!this->can_transfer_storage_from_(other)){
			throw std::invalid_argument("call_stream storage transfer requires compatible allocators");
		}
	}

	template <typename... CallArgs>
	inline static result_state make_result_state_(CallArgs&&... args)
		noexcept(std::is_nothrow_constructible_v<invoke_args, CallArgs&&...>)
		requires(!std::is_void_v<Ret>){
		if constexpr(uses_scalar_result_){
			return result_state{
					.args = invoke_args(std::forward<CallArgs>(args)...),
					.next = nullptr
				};
		} else{
			return result_state{
					.args = invoke_args(std::forward<CallArgs>(args)...),
					.result = {},
					.next = nullptr
				};
		}
	}

	inline void set_ip_to_ptr_(const std::byte* ptr) noexcept{
		const std::byte* base = buffer_.data();
		[[maybe_unused]] const std::byte* end = base + buffer_.size();
		MO_YANXI_CALL_STREAM_ASSUME(ptr >= base);
		MO_YANXI_CALL_STREAM_ASSUME(ptr <= end);
		ip_ = static_cast<std::size_t>(ptr - base);
	}

	inline static void record_current_instruction_(void* context_ptr, std::byte* current) noexcept
		requires(std::is_void_v<Ret>){
		if constexpr(has_tail_dispatch_ && !is_nothrow){
			MO_YANXI_CALL_STREAM_ASSUME(context_ptr != nullptr);
			static_cast<void_tail_dispatch_context*>(context_ptr)->current = current;
		} else{
			(void)context_ptr;
			(void)current;
		}
	}

	inline static bool has_recorded_exception_(void* context_ptr) noexcept
		requires(std::is_void_v<Ret>){
		if constexpr(has_tail_dispatch_ && !is_nothrow){
			MO_YANXI_CALL_STREAM_ASSUME(context_ptr != nullptr);
			return static_cast<void_tail_dispatch_context*>(context_ptr)->exception != nullptr;
		} else{
			(void)context_ptr;
			return false;
		}
	}

	template <typename Fn>
	inline static void invoke_void_payload_with_state_(Fn&& fn, void* context_ptr)
		noexcept(has_tail_dispatch_ || is_nothrow)
		requires(std::is_void_v<Ret>){
		if constexpr(has_tail_dispatch_){
			MO_YANXI_CALL_STREAM_ASSUME(context_ptr != nullptr);
			auto& tail_context = *static_cast<void_tail_dispatch_context*>(context_ptr);
			if constexpr(is_nothrow){
				if constexpr(sizeof...(Args) == 0){
					(void)std::invoke(std::forward<Fn>(fn));
				} else{
					auto& args = *static_cast<invoke_args*>(tail_context.invoke_args);
					(void)basic_call_stream_impl::invoke_with_args_(std::forward<Fn>(fn), args,
					                                                std::index_sequence_for<Args...>{});
				}
			} else{
				try{
					if constexpr(sizeof...(Args) == 0){
						(void)std::invoke(std::forward<Fn>(fn));
					} else{
						auto& args = *static_cast<invoke_args*>(tail_context.invoke_args);
						(void)basic_call_stream_impl::invoke_with_args_(std::forward<Fn>(fn), args,
						                                                std::index_sequence_for<Args...>{});
					}
				} catch(...){
					tail_context.exception = std::current_exception();
				}
			}
		} else{
			basic_call_stream_impl::invoke_payload_(std::forward<Fn>(fn), context_ptr);
		}
	}


	inline void ensure_header_alignment(){
		std::size_t current_size = buffer_.size();
		std::size_t padding = align_forward(current_size, alignof(instr_header)) - current_size;
		if(padding > 0){
			buffer_.append_zeros(padding, get_relocate_cb());
		}
	}

	template <typename Fn, std::size_t... Is>
	inline static decltype(auto) invoke_with_args_(Fn&& fn, invoke_args& args, std::index_sequence<Is...>)
		noexcept(std::is_nothrow_invocable_v<Fn, call_stream_invoke_arg_t<Args>...>){
		return std::invoke(std::forward<Fn>(fn), mo_yanxi::forward_invoke_arg_<Args>(std::get<Is>(args))...);
	}

	template <typename Fn>
	inline static void invoke_payload_(Fn&& fn, void* invoke_args_ptr)
		noexcept(is_nothrow && call_stream_nothrow_invocable<Ret, Fn, Args...>)
		requires(std::is_void_v<Ret>){
		if constexpr(sizeof...(Args) == 0){
			(void)std::invoke(std::forward<Fn>(fn));
		} else{
			MO_YANXI_CALL_STREAM_ASSUME(invoke_args_ptr != nullptr);
			auto& args = *static_cast<invoke_args*>(invoke_args_ptr);
			(void)basic_call_stream_impl::invoke_with_args_(std::forward<Fn>(fn), args,
			                                                std::index_sequence_for<Args...>{});
		}
	}

	template <typename Fn>
	inline static void invoke_payload_(Fn&& fn, void* context_ptr)
		noexcept(is_nothrow && call_stream_nothrow_invocable<Ret, Fn, Args...>)
		requires(!std::is_void_v<Ret>){
		MO_YANXI_CALL_STREAM_ASSUME(context_ptr != nullptr);
		auto& context = *static_cast<result_state*>(context_ptr);
		if constexpr(sizeof...(Args) == 0){
			context.result.emplace(std::invoke(std::forward<Fn>(fn)));
		} else{
			context.result.emplace(
				basic_call_stream_impl::invoke_with_args_(
					std::forward<Fn>(fn),
					context.args,
					std::index_sequence_for<Args...>{}));
		}
	}

	template <typename T, std::size_t... Is>
	inline static decltype(auto) invoke_static_with_args_(invoke_args& args, std::index_sequence<Is...>)
		noexcept(noexcept(T::operator()(forward_invoke_arg_<Args>(std::get<Is>(args))...))){
		return T::operator()(forward_invoke_arg_<Args>(std::get<Is>(args))...);
	}

	template <typename T>
	inline static void invoke_static_payload_(void* invoke_args_ptr)
		noexcept(is_nothrow && has_compatible_nothrow_static_call_operator<T, Ret, Args...>)
		requires(std::is_void_v<Ret>){
		if constexpr(sizeof...(Args) == 0){
			(void)T::operator()();
		} else{
			MO_YANXI_CALL_STREAM_ASSUME(invoke_args_ptr != nullptr);
			auto& args = *static_cast<invoke_args*>(invoke_args_ptr);
			(void)basic_call_stream_impl::invoke_static_with_args_<
				T>(args, std::index_sequence_for<Args...>{});
		}
	}

	template <typename T>
	inline static void invoke_static_payload_(void* context_ptr)
		noexcept(is_nothrow && has_compatible_nothrow_static_call_operator<T, Ret, Args...>)
		requires(!std::is_void_v<Ret>){
		MO_YANXI_CALL_STREAM_ASSUME(context_ptr != nullptr);
		auto& context = *static_cast<result_state*>(context_ptr);
		if constexpr(sizeof...(Args) == 0){
			context.result.emplace(T::operator()());
		} else{
			context.result.emplace(
				basic_call_stream_impl::invoke_static_with_args_<T>(
					context.args,
					std::index_sequence_for<Args...>{}));
		}
	}

	template <typename Fn>
	inline static Ret invoke_scalar_payload_(Fn&& fn, void* context_ptr)
		noexcept(is_nothrow && call_stream_nothrow_invocable<Ret, Fn, Args...>)
		requires(uses_scalar_result_){
		if constexpr(sizeof...(Args) == 0){
			return std::invoke(std::forward<Fn>(fn));
		} else{
			MO_YANXI_CALL_STREAM_ASSUME(context_ptr != nullptr);
			auto& context = *static_cast<result_state*>(context_ptr);
			return basic_call_stream_impl::invoke_with_args_(
				std::forward<Fn>(fn),
				context.args,
				std::index_sequence_for<Args...>{});
		}
	}

	template <typename T>
	inline static Ret invoke_static_scalar_payload_(void* context_ptr)
		noexcept(is_nothrow && has_compatible_nothrow_static_call_operator<T, Ret, Args...>)
		requires(uses_scalar_result_){
		if constexpr(sizeof...(Args) == 0){
			return T::operator()();
		} else{
			MO_YANXI_CALL_STREAM_ASSUME(context_ptr != nullptr);
			auto& context = *static_cast<result_state*>(context_ptr);
			return basic_call_stream_impl::invoke_static_with_args_<T>(
				context.args,
				std::index_sequence_for<Args...>{});
		}
	}

	template <typename PayloadT, bool IsStatic>
	inline static void invoke_zero_payload_void_(void* invoke_args_ptr)
		noexcept(has_tail_dispatch_ || is_nothrow)
		requires(std::is_void_v<Ret>){
		if constexpr(IsStatic){
			if constexpr(has_tail_dispatch_){
				basic_call_stream_impl::invoke_void_payload_with_state_(
					[](auto&&... args) -> decltype(auto){
						return PayloadT::operator()(std::forward<decltype(args)>(args)...);
					},
					invoke_args_ptr);
			} else{
				basic_call_stream_impl::invoke_static_payload_<PayloadT>(invoke_args_ptr);
			}
		} else{
			static const PayloadT fn_raw{};
			basic_call_stream_impl::invoke_void_payload_with_state_(fn_raw, invoke_args_ptr);
		}
	}

	template <typename PayloadT, std::size_t Offset>
	inline static void invoke_inline_void_payload_(std::byte* base, void* invoke_args_ptr)
		noexcept(has_tail_dispatch_ || (is_nothrow && call_stream_nothrow_invocable<Ret, PayloadT&, Args...>))
		requires(std::is_void_v<Ret>){
		auto& obj = *std::launder(std::assume_aligned<alignof(PayloadT)>(
			static_cast<PayloadT*>(static_cast<void*>(base + Offset))));
		basic_call_stream_impl::invoke_void_payload_with_state_(obj, invoke_args_ptr);
	}

	template <typename PayloadT, std::size_t PayloadOffset>
	inline static void invoke_heap_void_payload_(std::byte* base, void* invoke_args_ptr)
		noexcept(has_tail_dispatch_ || (is_nothrow && call_stream_nothrow_invocable<Ret, PayloadT&, Args...>))
		requires(std::is_void_v<Ret>){
		auto& obj_ptr = *std::launder(std::assume_aligned<alignof(PayloadT*)>(
			static_cast<PayloadT**>(static_cast<void*>(base + PayloadOffset))));
		basic_call_stream_impl::invoke_void_payload_with_state_(*obj_ptr, invoke_args_ptr);
	}

	MO_YANXI_CALL_STREAM_FORCE_INLINE inline static invoke_fn_return_type tail_dispatch_after_void_payload_(
		std::byte* next_ptr,
		const std::byte* end,
		void* invoke_args_ptr) noexcept(invoker_is_nothrow_)
		requires(std::is_void_v<Ret>){
		if constexpr(has_tail_dispatch_){
			if(basic_call_stream_impl::has_recorded_exception_(invoke_args_ptr)) return;
			if(next_ptr >= end) return;
			auto next_invoker = basic_call_stream_impl::instruction_header_(next_ptr)->invoker;
			MO_YANXI_CALL_STREAM_ASSUME(next_invoker != nullptr);
			MO_YANXI_CALL_STREAM_MUST_TAIL return next_invoker(next_ptr, end, invoke_args_ptr);
		} else{
			(void)end;
			(void)invoke_args_ptr;
			return next_ptr;
		}
	}

	template <typename FnTy, bool AllowStaticDispatch, typename... CtorArgs>
		requires(call_stream_payload_constructible<
				FnTy,
				typename std::allocator_traits<Allocator>::template rebind_alloc<FnTy>,
				CtorArgs&&...> &&
			call_stream_policy_invocable<ExceptionPolicy, Ret, FnTy&, Args...>)
	inline void emplace_call_(CtorArgs&&... args);

	inline static invoke_fn_return_type finish_result_instruction_(void* context_ptr,
	                                                               std::byte* next_ptr)
		noexcept
		requires(!std::is_void_v<Ret> && !uses_scalar_result_){
		(void)context_ptr;
		return next_ptr;
	}

	inline static void finish_scalar_result_instruction_(void* context_ptr, std::byte* next_ptr) noexcept
		requires(uses_scalar_result_){
		MO_YANXI_CALL_STREAM_ASSUME(context_ptr != nullptr);
		auto& context = *static_cast<result_state*>(context_ptr);
		context.next = next_ptr;
	}

	template <typename ResultContext>
	MO_YANXI_CALL_STREAM_FORCE_INLINE inline void execute_result_context_(ResultContext& context) noexcept(is_nothrow)
		requires(!std::is_void_v<Ret> && !uses_scalar_result_){
		std::byte* ptr = buffer_.data() + ip_;
		const std::byte* end = buffer_.data() + buffer_.size();
		if(std::greater_equal<>{}(ptr, end)) return;

		while(ptr < end){
			auto invoker = instruction_header_(ptr)->invoker;
			MO_YANXI_CALL_STREAM_ASSUME(invoker != nullptr);
			if constexpr(is_nothrow){
				ptr = invoker(ptr, end, std::addressof(context.state));
			} else{
				try{
					ptr = invoker(ptr, end, std::addressof(context.state));
				} catch(...){
					set_ip_to_ptr_(ptr);
					throw;
				}
			}
			MO_YANXI_CALL_STREAM_ASSUME(ptr != nullptr);
			if constexpr(!is_nothrow){
				set_ip_to_ptr_(ptr);
			}

			assert(context.state.result.engaged());
			if constexpr(std::predicate<decltype(*context.callback), Ret&&>){
				bool stop = false;
				if constexpr(is_nothrow){
					stop = static_cast<bool>(std::invoke(*context.callback, context.state.result.get()));
				} else{
					try{
						stop = static_cast<bool>(std::invoke(*context.callback, context.state.result.get()));
					} catch(...){
						context.state.result.destroy();
						throw;
					}
				}
				context.state.result.destroy();
				if(stop) break;
			} else{
				if constexpr(is_nothrow){
					(void)std::invoke(*context.callback, context.state.result.get());
				} else{
					try{
						(void)std::invoke(*context.callback, context.state.result.get());
					} catch(...){
						context.state.result.destroy();
						throw;
					}
				}
				context.state.result.destroy();
			}
		}
		ip_ = buffer_.size();
	}

	template <typename ResultContext>
	MO_YANXI_CALL_STREAM_FORCE_INLINE inline void execute_scalar_result_context_(
		ResultContext& context) noexcept(is_nothrow)
		requires(uses_scalar_result_){
		std::byte* ptr = buffer_.data() + ip_;
		const std::byte* end = buffer_.data() + buffer_.size();
		if(std::greater_equal<>{}(ptr, end)) return;

		while(ptr < end){
			auto invoker = instruction_header_(ptr)->invoker;
			MO_YANXI_CALL_STREAM_ASSUME(invoker != nullptr);
			context.state.next = nullptr;

			Ret result = [&]() -> Ret{
				if constexpr(is_nothrow){
					return invoker(ptr, end, std::addressof(context.state));
				} else{
					try{
						return invoker(ptr, end, std::addressof(context.state));
					} catch(...){
						set_ip_to_ptr_(ptr);
						throw;
					}
				}
			}();
			ptr = context.state.next;
			MO_YANXI_CALL_STREAM_ASSUME(ptr != nullptr);
			if constexpr(!is_nothrow){
				set_ip_to_ptr_(ptr);
			}

			if constexpr(std::predicate<decltype(*context.callback), Ret&&>){
				if(static_cast<bool>(std::invoke(*context.callback, std::move(result)))) break;
			} else{
				(void)std::invoke(*context.callback, std::move(result));
			}
		}
		ip_ = buffer_.size();
	}

	MO_YANXI_CALL_STREAM_FORCE_INLINE inline void execute_context_(void* context_ptr) noexcept(is_nothrow){
		std::byte* ptr = buffer_.data() + ip_;
		const std::byte* end = buffer_.data() + buffer_.size();
		if(std::greater_equal<>{}(ptr, end)) return;

		if constexpr(has_tail_dispatch_){
			auto invoker = instruction_header_(ptr)->invoker;
			MO_YANXI_CALL_STREAM_ASSUME(invoker != nullptr);
			if constexpr(is_nothrow){
				void_tail_dispatch_context tail_context{
						.invoke_args = context_ptr
					};
				invoker(ptr, end, std::addressof(tail_context));
			} else{
				void_tail_dispatch_context tail_context{
						.invoke_args = context_ptr,
						.current = ptr,
						.exception = nullptr
					};
				invoker(ptr, end, std::addressof(tail_context));
				if(tail_context.exception){
					set_ip_to_ptr_(tail_context.current);
					std::rethrow_exception(tail_context.exception);
				}
			}
			ip_ = buffer_.size();
		} else{
			while(ptr < end){
				auto invoker = instruction_header_(ptr)->invoker;
				MO_YANXI_CALL_STREAM_ASSUME(invoker != nullptr);
				if constexpr(is_nothrow){
					ptr = invoker(ptr, end, context_ptr);
				} else{
					try{
						ptr = invoker(ptr, end, context_ptr);
					} catch(...){
						set_ip_to_ptr_(ptr);
						throw;
					}
					set_ip_to_ptr_(ptr);
				}
			}
			ip_ = buffer_.size();
		}
	}

public:
	inline explicit basic_call_stream_impl(const allocator_type& alloc = allocator_type())
		: buffer_(alloc),
		  resources_(resource_allocator_type(alloc)){
	}

	inline basic_call_stream_impl(const basic_call_stream_impl&) = delete;
	inline basic_call_stream_impl& operator=(const basic_call_stream_impl&) = delete;

	inline basic_call_stream_impl(basic_call_stream_impl&& other) noexcept(
		std::is_nothrow_move_constructible_v<decltype(buffer_)> &&
		std::is_nothrow_move_constructible_v<decltype(resources_)>)
		: buffer_(std::move(other.buffer_)),
		  resources_(std::move(other.resources_)),
		  ip_(std::exchange(other.ip_, 0)){
		other.resources_.clear();
	}

	inline basic_call_stream_impl& operator=(basic_call_stream_impl&& other) noexcept(
		noexcept(std::declval<decltype(buffer_)&>().can_move_assign_from(std::declval<const decltype(buffer_)&>())) &&
		noexcept(std::declval<decltype(buffer_)&>() = std::declval<decltype(buffer_)&&>()) &&
		std::is_nothrow_move_assignable_v<decltype(resources_)>){
		if(this == &other) return *this;
		if constexpr(!noexcept(std::declval<decltype(buffer_)&>() = std::declval<decltype(buffer_)&&>())){
			if(!buffer_.can_move_assign_from(other.buffer_)){
				throw std::invalid_argument("call_stream move assignment requires compatible allocators");
			}
		}

		clear_res_();
		buffer_ = std::move(other.buffer_);
		resources_ = std::move(other.resources_);
		other.resources_.clear();
		ip_ = std::exchange(other.ip_, 0);
		return *this;
	}

	inline ~basic_call_stream_impl(){
		clear_res_();
	}

	inline void reserve(std::size_t size){
		buffer_.reserve(size, get_relocate_cb());
	}

	inline allocator_type get_allocator() const noexcept{ return buffer_.get_allocator(); }


	inline void emit_instruction(invoker_fn invoker, const void* payload, std::size_t size, std::size_t alignment){
		MO_YANXI_CALL_STREAM_ASSUME(invoker != nullptr);
		ensure_header_alignment();
		std::size_t payload_offset = align_forward(sizeof(instr_header), alignment);
		std::size_t total_bytes = align_forward(payload_offset + size, alignof(instr_header));

		if(!std::in_range<std::uint32_t>(total_bytes)){
			throw std::bad_alloc();
		}

		auto* raw_mem = buffer_.allocate_uninitialized(total_bytes, get_relocate_cb());
		new(std::assume_aligned<alignof(instr_header)>(
				static_cast<instr_header*>(static_cast<void*>(raw_mem)))) instr_header{
				.invoker = invoker
			};

		if(size > 0 && payload != nullptr){
			std::memcpy(raw_mem + payload_offset, payload, size);
		}
	}


	template <typename T>
	inline void emit_non_trivial_call_heap(invoker_fn invoker, T* heap_ptr, resource_handle_fn handler){
		MO_YANXI_CALL_STREAM_ASSUME(invoker != nullptr);
		MO_YANXI_CALL_STREAM_ASSUME(heap_ptr != nullptr);
		MO_YANXI_CALL_STREAM_ASSUME(handler != nullptr);

		const auto checkpoint = buffer_.size();

		try{
			ensure_header_alignment();
			std::size_t current_size = buffer_.size();

			std::size_t payload_offset = align_forward(sizeof(instr_header), alignof(T*));
			std::size_t total_bytes = align_forward(payload_offset + sizeof(T*), alignof(instr_header));

			if(!std::in_range<std::uint32_t>(total_bytes)){
				throw std::bad_alloc();
			}

			auto* raw_mem = buffer_.allocate_uninitialized(total_bytes, get_relocate_cb());
			new(std::assume_aligned<alignof(instr_header)>(
					static_cast<instr_header*>(static_cast<void*>(raw_mem)))) instr_header{
					.invoker = invoker
				};

			auto* obj_ptr = std::assume_aligned<alignof(T*)>(
				static_cast<T**>(static_cast<void*>(raw_mem + payload_offset)));
			new(obj_ptr) T*(heap_ptr);
			register_resource_(current_size, handler);
		} catch(...){
			buffer_.rollback_size(checkpoint);
			throw;
		}
	}


	template <typename T, typename... CtorArgs>
		requires std::is_nothrow_move_constructible_v<T>
	inline void emit_non_trivial_call_inline(invoker_fn invoker, CtorArgs&&... args){
		assert(invoker != nullptr);
		MO_YANXI_CALL_STREAM_ASSUME(invoker != nullptr);
		const auto checkpoint = buffer_.size();

		try{
			ensure_header_alignment();
			std::size_t current_size = buffer_.size();

			std::size_t payload_offset = align_forward(sizeof(instr_header), alignof(T));
			std::size_t total_bytes = align_forward(payload_offset + sizeof(T), alignof(instr_header));

			if(!std::in_range<std::uint32_t>(total_bytes)){
				throw std::bad_alloc();
			}

			auto* raw_mem = buffer_.allocate_uninitialized(total_bytes, get_relocate_cb());
			new(std::assume_aligned<alignof(instr_header)>(
					static_cast<instr_header*>(static_cast<void*>(raw_mem)))) instr_header{
					.invoker = invoker
				};

			static constexpr auto res_handler = +[](basic_call_stream_impl&, void* old_base, void* new_base) noexcept{
				constexpr std::size_t p_offset = align_forward(sizeof(instr_header), alignof(T));
				auto* typed_src = std::launder(std::assume_aligned<alignof(T)>(
					static_cast<T*>(static_cast<void*>(static_cast<std::byte*>(old_base) + p_offset))));

				if(new_base){
					auto* typed_dst = std::assume_aligned<alignof(T)>(
						static_cast<T*>(static_cast<void*>(static_cast<std::byte*>(new_base) + p_offset)));
					new(typed_dst) T(std::move(*typed_src));
					typed_src->~T();
				} else{
					typed_src->~T();
				}
			};

			void* obj_ptr = std::assume_aligned<alignof(T)>(
				static_cast<T*>(static_cast<void*>(raw_mem + payload_offset)));
			new(obj_ptr) T(std::forward<CtorArgs>(args)...);
			try{
				this->register_resource_(current_size, res_handler);
			} catch(...){
				res_handler(*this, raw_mem, nullptr);
				throw;
			}
		} catch(...){
			buffer_.rollback_size(checkpoint);
			throw;
		}
	}

	inline void emit_noop()
		requires((std::is_void_v<Ret> || std::default_initializable<Ret>) &&
			(ExceptionPolicy != call_stream_exception_policy::nothrow ||
				std::is_void_v<Ret> || std::is_nothrow_default_constructible_v<Ret>)){
		this->emit_instruction(+[](std::byte* base, const std::byte* end,
		                           void* invoke_args_ptr) static noexcept(invoker_is_nothrow_) -> invoke_fn_return_type{
			std::byte* next_ptr = base + sizeof(instr_header);

			if constexpr(std::is_void_v<Ret>){
				return basic_call_stream_impl::tail_dispatch_after_void_payload_(
					next_ptr, end, invoke_args_ptr);
			} else if constexpr(uses_scalar_result_){
				finish_scalar_result_instruction_(invoke_args_ptr, next_ptr);
				return Ret{};
			} else{
				MO_YANXI_CALL_STREAM_ASSUME(invoke_args_ptr != nullptr);
				auto& context = *static_cast<result_state*>(invoke_args_ptr);
				context.result.emplace_default();
				return basic_call_stream_impl::finish_result_instruction_(invoke_args_ptr, next_ptr);
			}
		}, nullptr, 0, 1);
	}


	MO_YANXI_CALL_STREAM_FORCE_INLINE inline void continue_execute(Args... args)
		noexcept(is_nothrow && std::is_nothrow_constructible_v<invoke_args, Args&&...>)
		requires(std::is_void_v<Ret>){
		if(empty()) return;

		if constexpr(sizeof...(Args) == 0){
			this->execute_context_(nullptr);
		} else{
			invoke_args packed_args(std::forward<Args>(args)...);
			this->execute_context_(std::addressof(packed_args));
		}
	}

	template <typename Callback>
		requires(std::invocable<Callback&, Ret&&> &&
			(ExceptionPolicy != call_stream_exception_policy::nothrow ||
				call_stream_nothrow_result_callback_v<std::remove_reference_t<Callback>, Ret>))
	MO_YANXI_CALL_STREAM_FORCE_INLINE inline void continue_execute(Args... args, Callback&& callback)
		noexcept(is_nothrow &&
			std::is_nothrow_constructible_v<invoke_args, Args&&...> &&
			call_stream_nothrow_result_callback_v<std::remove_reference_t<Callback>, Ret>)
		requires(!std::is_void_v<Ret>){
		if(empty()) return;

		using CallbackT = std::remove_reference_t<Callback>;
		call_stream_result_context<Ret, invoke_args, CallbackT> context{
				.state = basic_call_stream_impl::make_result_state_(std::forward<Args>(args)...),
				.callback = std::addressof(callback)
			};
		if constexpr(uses_scalar_result_){
			this->execute_scalar_result_context_(context);
		} else{
			this->execute_result_context_(context);
		}
	}

	MO_YANXI_CALL_STREAM_FORCE_INLINE inline void reset_and_execute(Args... args)
		noexcept(is_nothrow && std::is_nothrow_constructible_v<invoke_args, Args&&...>)
		requires(std::is_void_v<Ret>){
		reset_ip();
		this->continue_execute(std::forward<Args>(args)...);
	}

	template <typename Callback>
		requires(std::invocable<Callback&, Ret&&> &&
			(ExceptionPolicy != call_stream_exception_policy::nothrow ||
				call_stream_nothrow_result_callback_v<std::remove_reference_t<Callback>, Ret>))
	MO_YANXI_CALL_STREAM_FORCE_INLINE inline void reset_and_execute(Args... args, Callback&& callback)
		noexcept(is_nothrow &&
			std::is_nothrow_constructible_v<invoke_args, Args&&...> &&
			call_stream_nothrow_result_callback_v<std::remove_reference_t<Callback>, Ret>)
		requires(!std::is_void_v<Ret>){
		reset_ip();
		this->continue_execute(std::forward<Args>(args)..., std::forward<Callback>(callback));
	}

	MO_YANXI_CALL_STREAM_FORCE_INLINE inline void operator()(Args... args)
		noexcept(is_nothrow && std::is_nothrow_constructible_v<invoke_args, Args&&...>)
		requires(std::is_void_v<Ret>){
		this->reset_and_execute(std::forward<Args>(args)...);
	}

	template <typename Callback>
		requires(std::invocable<Callback&, Ret&&> &&
			(ExceptionPolicy != call_stream_exception_policy::nothrow ||
				call_stream_nothrow_result_callback_v<std::remove_reference_t<Callback>, Ret>))
	MO_YANXI_CALL_STREAM_FORCE_INLINE inline void operator()(Args... args, Callback&& callback)
		noexcept(is_nothrow &&
			std::is_nothrow_constructible_v<invoke_args, Args&&...> &&
			call_stream_nothrow_result_callback_v<std::remove_reference_t<Callback>, Ret>)
		requires(!std::is_void_v<Ret>){
		this->reset_and_execute(std::forward<Args>(args)..., std::forward<Callback>(callback));
	}

	inline void reset_ip(std::size_t new_ip = 0) noexcept{
		assert(new_ip <= buffer_.size());
		ip_ = new_ip;
	}

	[[nodiscard]] inline std::size_t current_ip() const noexcept{ return ip_; }
	[[nodiscard]] inline bool is_at_start() const noexcept{ return ip_ == 0; }
	[[nodiscard]] inline bool is_finished() const noexcept{ return ip_ == buffer_.size(); }
	[[nodiscard]] inline bool is_partially_executed() const noexcept{ return ip_ > 0 && ip_ < buffer_.size(); }
	[[nodiscard]] inline bool has_pending_instructions() const noexcept{ return ip_ < buffer_.size(); }
	[[nodiscard]] inline bool empty() const noexcept{ return buffer_.empty(); }
	[[nodiscard]] inline std::size_t size() const noexcept{ return buffer_.size(); }

	inline void clear() noexcept{
		clear_res_();
		buffer_.clear();
		ip_ = 0;
	}

	inline void append(basic_call_stream_impl&& other){
		if(this == &other || other.empty()) return;

		if(this->empty()){
			*this = std::move(other);
			return;
		}

		this->require_transfer_compatible_(other);

		ensure_header_alignment();

		const std::size_t base_offset = buffer_.size();
		const std::size_t other_size = other.buffer_.size();

		if(!std::in_range<std::uint32_t>(base_offset + other_size)){
			throw std::bad_alloc();
		}

		resources_.reserve(resources_.size() + other.resources_.size());
		auto* dest = buffer_.allocate_uninitialized(other_size, get_relocate_cb());

		std::memcpy(dest, other.buffer_.data(), other_size);

		for(const resource_record& record : other.resources_){
			const auto merged_offset = static_cast<std::uint32_t>(base_offset + record.offset);
			record.handler(*this, other.buffer_.data() + record.offset, dest + record.offset);
			resources_.push_back(resource_record{
					.offset = merged_offset,
					.handler = record.handler
				});
		}

		other.resources_.clear();
		other.buffer_.clear();
		other.ip_ = 0;
	}

	template <typename Fn>
		requires(call_stream_payload_constructible<
				Fn,
				typename std::allocator_traits<Allocator>::template rebind_alloc<Fn>,
				const Fn&> &&
			call_stream_policy_invocable<ExceptionPolicy, Ret, Fn&, Args...>)
	inline void push_back(const cmd_call<Fn>& call);

	template <typename Fn>
		requires(call_stream_payload_constructible<
				Fn,
				typename std::allocator_traits<Allocator>::template rebind_alloc<Fn>,
				Fn&&> &&
			call_stream_policy_invocable<ExceptionPolicy, Ret, Fn&, Args...>)
	inline void push_back(cmd_call<Fn>&& call);

	template <typename Fn>
		requires(call_stream_payload_constructible<
				std::decay_t<Fn>,
				typename std::allocator_traits<Allocator>::template rebind_alloc<std::decay_t<Fn>>,
				Fn&&> &&
			call_stream_policy_invocable<ExceptionPolicy, Ret, std::decay_t<Fn>&, Args...>)
	inline void emplace_back(Fn&& fn);

	template <typename FnTy, typename... Ts>
		requires(call_stream_payload_constructible<
				FnTy,
				typename std::allocator_traits<Allocator>::template rebind_alloc<FnTy>,
				Ts&&...> &&
			call_stream_policy_invocable<ExceptionPolicy, Ret, FnTy&, Args...>)
	inline void emplace_back(Ts&&... args);

	friend void swap(basic_call_stream_impl& lhs,
	                 basic_call_stream_impl& rhs) noexcept(
			noexcept(std::declval<decltype(buffer_)&>().can_swap_with(std::declval<const decltype(buffer_)&>())) &&
			noexcept(std::ranges::swap(std::declval<decltype(buffer_)&>(), std::declval<decltype(buffer_)&>())) &&
			std::is_nothrow_swappable_v<decltype(resources_)>){
		if constexpr(!noexcept(std::ranges::swap(std::declval<decltype(buffer_)&>(), std::declval<decltype(buffer_)&>()))){
			if(!lhs.buffer_.can_swap_with(rhs.buffer_)){
				throw std::invalid_argument("call_stream swap requires compatible allocators");
			}
		}

		std::ranges::swap(lhs.buffer_, rhs.buffer_);
		std::ranges::swap(lhs.resources_, rhs.resources_);
		std::ranges::swap(lhs.ip_, rhs.ip_);
	}
};


template <typename Fn>
struct cmd_call{
	MO_YANXI_CALL_STREAM_NO_UNIQUE_ADDRESS Fn callable;

	template <typename... Args>
		requires(std::constructible_from<Fn, Args&&...>)
	[[nodiscard]] inline explicit(false) cmd_call(Args&&... args)
		: callable(std::forward<Args>(args)...){
	}

	MO_YANXI_CALL_STREAM_FORCE_INLINE inline decltype(auto) operator()() & noexcept(std::is_nothrow_invocable_v<Fn&>)
		requires std::invocable<Fn&>{
		return std::invoke(this->callable);
	}

	MO_YANXI_CALL_STREAM_FORCE_INLINE inline decltype(auto) operator()() && noexcept(std::is_nothrow_invocable_v<Fn>)
		requires std::invocable<Fn>{
		return std::invoke(std::move(this->callable));
	}

	MO_YANXI_CALL_STREAM_FORCE_INLINE inline decltype(auto) operator(
	)() const & noexcept(std::is_nothrow_invocable_v<const Fn&>)
		requires std::invocable<const Fn&>{
		return std::invoke(this->callable);
	}
};

template <typename Fn>
cmd_call(Fn&&) -> cmd_call<std::decay_t<Fn>>;

export template <typename Allocator, typename Ret, typename... Args>
using basic_call_stream =
basic_call_stream_impl<call_stream_exception_policy::resumable, Allocator, Ret, Args...>;

export template <typename Allocator, typename Ret, typename... Args>
using basic_noexcept_call_stream =
basic_call_stream_impl<call_stream_exception_policy::nothrow, Allocator, Ret, Args...>;

export template <
	typename FnSign = void(),
	typename Allocator = std::allocator<std::byte>,
	call_stream_exception_policy ExceptionPolicy = call_stream_default_exception_policy<FnSign>::value>
using call_stream = basic_call_stream_selector<Allocator, FnSign, ExceptionPolicy>::type;

export template <typename FnSign = void(), typename Allocator = std::allocator<std::byte>>
using noexcept_call_stream =
basic_call_stream_selector<Allocator, FnSign, call_stream_exception_policy::nothrow>::type;

#pragma region StreamImpl
template <call_stream_exception_policy ExceptionPolicy, typename Allocator, typename Ret, typename... Args>
template <typename FnTy, bool AllowStaticDispatch, typename... CtorArgs>
	requires(call_stream_payload_constructible<
			FnTy,
			typename std::allocator_traits<Allocator>::template rebind_alloc<FnTy>,
			CtorArgs&&...> &&
		call_stream_policy_invocable<ExceptionPolicy, Ret, FnTy&, Args...>)
inline void basic_call_stream_impl<ExceptionPolicy, Allocator, Ret, Args...>::emplace_call_(CtorArgs&&... args){
	using PayloadT = FnTy;
	using TypedAlloc = std::allocator_traits<Allocator>::template rebind_alloc<PayloadT>;
	using TypedAllocTraits = std::allocator_traits<TypedAlloc>;
	static constexpr bool uses_payload_allocator = std::uses_allocator_v<PayloadT, TypedAlloc>;


	if constexpr(AllowStaticDispatch && !uses_payload_allocator){
		constexpr bool is_static = has_compatible_static_call_operator<PayloadT, Ret, Args...> &&
		(ExceptionPolicy != call_stream_exception_policy::nothrow ||
			has_compatible_nothrow_static_call_operator<PayloadT, Ret, Args...>);
		constexpr bool is_empty = std::is_empty_v<PayloadT> &&
			std::default_initializable<PayloadT> &&
			call_stream_policy_invocable<ExceptionPolicy, Ret, const PayloadT&, Args...>;
		if constexpr(!std::is_pointer_v<PayloadT> && (is_static || is_empty)){
			this->emit_instruction(+[](std::byte* base, const std::byte* end,
			                           void* invoke_args_ptr) static noexcept(invoker_is_nothrow_) ->
			                       invoke_fn_return_type{
				                       std::byte* next_ptr = base + sizeof(instr_header);
				                       if constexpr(uses_scalar_result_){
					                       Ret result = [&]{
						                       if constexpr(is_static){
							                       return basic_call_stream_impl::invoke_static_scalar_payload_<
								                       PayloadT>(invoke_args_ptr);
						                       } else{
							                       static const PayloadT fn_raw{};
							                       return basic_call_stream_impl::invoke_scalar_payload_(
								                       fn_raw, invoke_args_ptr);
						                       }
					                       }();
					                       finish_scalar_result_instruction_(invoke_args_ptr, next_ptr);
					                       return result;
				                       } else{
					                       if constexpr(std::is_void_v<Ret>){
						                       basic_call_stream_impl::record_current_instruction_(
							                       invoke_args_ptr, base);
						                       MO_YANXI_CALL_STREAM_FORCEINLINE_CALLS {
							                       basic_call_stream_impl::invoke_zero_payload_void_<
								                       PayloadT, is_static>(
								                       invoke_args_ptr);
						                       };
						                       return basic_call_stream_impl::tail_dispatch_after_void_payload_(
							                       next_ptr, end, invoke_args_ptr);
					                       } else{
						                       MO_YANXI_CALL_STREAM_FORCEINLINE_CALLS {
							                       if constexpr(is_static){
								                       basic_call_stream_impl::invoke_static_payload_<PayloadT>(
									                       invoke_args_ptr);
							                       } else{
								                       static const PayloadT fn_raw{};
								                       basic_call_stream_impl::invoke_payload_(fn_raw, invoke_args_ptr);
							                       }
						                       };

						                       return basic_call_stream_impl::finish_result_instruction_(
							                       invoke_args_ptr, next_ptr);
					                       }
				                       }
			                       }, nullptr, 0, 1);
			return;
		}
	}


	static constexpr bool is_trivial = std::is_trivially_copyable_v<PayloadT> &&
		std::is_trivially_destructible_v<PayloadT>;
	static constexpr bool can_store_inline = alignof(PayloadT) <= alignof(instr_header);


	static constexpr auto fptr = +[](std::byte* base, const std::byte* end,
	                                 void* invoke_args_ptr) static noexcept(invoker_is_nothrow_) ->
		invoke_fn_return_type{
			static constexpr std::size_t offset = align_forward(sizeof(instr_header), alignof(PayloadT));
			static constexpr std::size_t total_size = align_forward(offset + sizeof(PayloadT), alignof(instr_header));

			std::byte* next_ptr = base + total_size;
			if constexpr(uses_scalar_result_){
				auto& obj = *std::launder(std::assume_aligned<alignof(PayloadT)>(
					static_cast<PayloadT*>(static_cast<void*>(base + offset))));
				Ret result = basic_call_stream_impl::invoke_scalar_payload_(obj, invoke_args_ptr);
				finish_scalar_result_instruction_(invoke_args_ptr, next_ptr);
				return result;
			} else if constexpr(std::is_void_v<Ret>){
				basic_call_stream_impl::record_current_instruction_(invoke_args_ptr, base);
				basic_call_stream_impl::invoke_inline_void_payload_<PayloadT, offset>(
					base, invoke_args_ptr);
				return basic_call_stream_impl::tail_dispatch_after_void_payload_(next_ptr, end, invoke_args_ptr);
			} else{
				auto& obj = *std::launder(std::assume_aligned<alignof(PayloadT)>(
					static_cast<PayloadT*>(static_cast<void*>(base + offset))));
				MO_YANXI_CALL_STREAM_FORCEINLINE_CALLS {
					basic_call_stream_impl::invoke_payload_(obj, invoke_args_ptr);
				}

				return basic_call_stream_impl::finish_result_instruction_(invoke_args_ptr, next_ptr);
			}
		};


	if constexpr(!uses_payload_allocator && is_trivial && can_store_inline){
		PayloadT payload(std::forward<CtorArgs>(args)...);
		this->emit_instruction(fptr, &payload, sizeof(PayloadT), alignof(PayloadT));
	} else if constexpr(!uses_payload_allocator && can_store_inline && std::is_nothrow_move_constructible_v<PayloadT>){
		this->template emit_non_trivial_call_inline<PayloadT>(fptr, std::forward<CtorArgs>(args)...);
	} else{
		static_assert(std::is_same_v<typename TypedAllocTraits::pointer, PayloadT*>,
		              "call_stream requires raw pointers from rebound payload allocators");
		TypedAlloc alloc(this->buffer_.template get_rebound_allocator<PayloadT>());
		PayloadT* heap_obj = TypedAllocTraits::allocate(alloc, 1);

		try{
			basic_call_stream_impl::construct_payload_(alloc, heap_obj, std::forward<CtorArgs>(args)...);
		} catch(...){
			TypedAllocTraits::deallocate(alloc, heap_obj, 1);
			throw;
		}

		try{
			this->emit_non_trivial_call_heap(
				+[](std::byte* base, const std::byte* end,
				    void* invoke_args_ptr) static noexcept(invoker_is_nothrow_) -> invoke_fn_return_type{
					static constexpr std::size_t payload_offset = align_forward(
						sizeof(instr_header), alignof(PayloadT*));
					static constexpr std::size_t total_size = align_forward(
						payload_offset + sizeof(PayloadT*), alignof(instr_header));

					std::byte* next_ptr = base + total_size;
					if constexpr(uses_scalar_result_){
						auto& obj_ptr = *std::launder(std::assume_aligned<alignof(PayloadT*)>(
							static_cast<PayloadT**>(static_cast<void*>(base + payload_offset))));
						Ret result = basic_call_stream_impl::invoke_scalar_payload_(*obj_ptr, invoke_args_ptr);
						finish_scalar_result_instruction_(invoke_args_ptr, next_ptr);
						return result;
					} else if constexpr(std::is_void_v<Ret>){
						basic_call_stream_impl::record_current_instruction_(invoke_args_ptr, base);
						basic_call_stream_impl::invoke_heap_void_payload_<PayloadT, payload_offset>(
							base, invoke_args_ptr);
						return basic_call_stream_impl::tail_dispatch_after_void_payload_(
							next_ptr, end, invoke_args_ptr);
					} else{
						auto& obj_ptr = *std::launder(std::assume_aligned<alignof(PayloadT*)>(
							static_cast<PayloadT**>(static_cast<void*>(base + payload_offset))));
						MO_YANXI_CALL_STREAM_FORCEINLINE_CALLS {
							basic_call_stream_impl::invoke_payload_(*obj_ptr, invoke_args_ptr);
						}

						return basic_call_stream_impl::finish_result_instruction_(invoke_args_ptr, next_ptr);
					}
				},
				heap_obj,
				+[] MO_YANXI_CALL_STREAM_FORCE_INLINE (basic_call_stream_impl& s, void* old_base,
				                                       void* new_base) noexcept{
					if(new_base) return;
					constexpr std::size_t p_off = align_forward(
						sizeof(instr_header), alignof(PayloadT*));
					auto* typed_ptr =
						*std::launder(std::assume_aligned<alignof(PayloadT*)>(
							static_cast<PayloadT**>(static_cast<void*>(static_cast<std::byte*>(old_base) + p_off))));

					TypedAlloc del_alloc(s.buffer_.template get_rebound_allocator<PayloadT>());
					TypedAllocTraits::destroy(del_alloc, typed_ptr);
					TypedAllocTraits::deallocate(del_alloc, typed_ptr, 1);
				}
			);
		} catch(...){
			TypedAllocTraits::destroy(alloc, heap_obj);
			TypedAllocTraits::deallocate(alloc, heap_obj, 1);
			throw;
		}
	}
}

template <call_stream_exception_policy ExceptionPolicy, typename Allocator, typename Ret, typename... Args>
template <typename Fn>
	requires(call_stream_payload_constructible<
			std::decay_t<Fn>,
			typename std::allocator_traits<Allocator>::template rebind_alloc<std::decay_t<Fn>>,
			Fn&&> &&
		call_stream_policy_invocable<ExceptionPolicy, Ret, std::decay_t<Fn>&, Args...>)
inline void basic_call_stream_impl<ExceptionPolicy, Allocator, Ret, Args...>::emplace_back(Fn&& fn){
	this->template emplace_call_<std::decay_t<Fn>, true>(std::forward<Fn>(fn));
}

template <call_stream_exception_policy ExceptionPolicy, typename Allocator, typename Ret, typename... Args>
template <typename FnTy, typename... Ts>
	requires(call_stream_payload_constructible<
			FnTy,
			typename std::allocator_traits<Allocator>::template rebind_alloc<FnTy>,
			Ts&&...> &&
		call_stream_policy_invocable<ExceptionPolicy, Ret, FnTy&, Args...>)
inline void basic_call_stream_impl<ExceptionPolicy, Allocator, Ret, Args...>::emplace_back(Ts&&... args){
	this->template emplace_call_<FnTy, sizeof...(Ts) == 0>(std::forward<Ts>(args)...);
}

template <call_stream_exception_policy ExceptionPolicy, typename Allocator, typename Ret, typename... Args>
template <typename Fn>
	requires(call_stream_payload_constructible<
			Fn,
			typename std::allocator_traits<Allocator>::template rebind_alloc<Fn>,
			const Fn&> &&
		call_stream_policy_invocable<ExceptionPolicy, Ret, Fn&, Args...>)
inline void basic_call_stream_impl<ExceptionPolicy, Allocator, Ret, Args...>::push_back(const cmd_call<Fn>& call){
	this->emplace_back(call.callable);
}

template <call_stream_exception_policy ExceptionPolicy, typename Allocator, typename Ret, typename... Args>
template <typename Fn>
	requires(call_stream_payload_constructible<
			Fn,
			typename std::allocator_traits<Allocator>::template rebind_alloc<Fn>,
			Fn&&> &&
		call_stream_policy_invocable<ExceptionPolicy, Ret, Fn&, Args...>)
inline void basic_call_stream_impl<ExceptionPolicy, Allocator, Ret, Args...>::push_back(cmd_call<Fn>&& call){
	this->emplace_back(std::move(call.callable));
}

export template <call_stream_exception_policy ExceptionPolicy, typename Allocator, typename Ret, typename... Args,
                 typename Fn>
	requires(not_cmd_call<Fn> &&
		requires(basic_call_stream_impl<ExceptionPolicy, Allocator, Ret, Args...>& stream, Fn&& call){
			stream.emplace_back(std::forward<Fn>(call));
		})
basic_call_stream_impl<ExceptionPolicy, Allocator, Ret, Args...>& operator<<(
	basic_call_stream_impl<ExceptionPolicy, Allocator, Ret, Args...>& stream, Fn&& call){
	stream.emplace_back(std::forward<Fn>(call));
	return stream;
}

export template <call_stream_exception_policy ExceptionPolicy, typename Allocator, typename Ret, typename... Args,
                 typename Fn>
	requires requires(basic_call_stream_impl<ExceptionPolicy, Allocator, Ret, Args...>& stream, cmd_call<Fn>&& call){
		stream.push_back(std::move(call));
	}
basic_call_stream_impl<ExceptionPolicy, Allocator, Ret, Args...>& operator<<(
	basic_call_stream_impl<ExceptionPolicy, Allocator, Ret, Args...>& stream, cmd_call<Fn>&& call){
	stream.push_back(std::move(call));
	return stream;
}
#pragma endregion
}
