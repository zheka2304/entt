#ifndef ENTT_ENTITY_POLYMORPHIC_HPP
#define ENTT_ENTITY_POLYMORPHIC_HPP

#include <iostream>
#include <tuple>
#include <variant>
#include <type_traits>
#include "fwd.hpp"
#include "../core/memory.hpp"
#include "../core/type_traits.hpp"


namespace entt {

namespace internal {

// polymorphic component hierarchy accessor, filters out matching types
template<typename Component>
struct component_hierarchy {
public:
    using parent_types = type_list_unique_t<typename Component::parent_types_t>;
    using direct_parent_types = type_list_unique_t<typename Component::direct_parent_types_t>;
    static constexpr bool has_inheritance_cycles = !std::is_same_v<parent_types, typename Component::parent_types_t>;
};

// used to list parent types of both polymorphic and non-polymorphic components, for non-polymorphic returns empty list
template<typename Component, bool isPolymorphic>
struct safe_component_parents_access_helper {
    using parent_types = type_list<>;
    using direct_parent_types = type_list<>;
};

template<typename Component>
struct safe_component_parents_access_helper<Component, true> {
    using parent_types = typename component_hierarchy<Component>::parent_types;
    using direct_parent_types = typename component_hierarchy<Component>::direct_parent_types;
};

// checks, if component is polymorphic
template<typename Component>
struct is_polymorphic_component {
    template<typename U>
    static constexpr decltype(std::declval<typename U::parent_types_t>()) test(U const &) {}

    template<typename U>
    static constexpr void test(...) {}

    static constexpr bool value = !std::is_same_v<decltype(test<Component>(std::declval<Component>())), void>;
};



// stores reference to any polymorphic component and pointer to deleter function, that destroys component value and all its references
struct polymorphic_component_ref {
    void *pointer;
    void *deleter;
};

// used for iteration through entt::every
template< typename T >
struct polymorphic_component_ref_iterator {
    using value_type = T;

    std::conditional_t<std::is_const_v<value_type>, const internal::polymorphic_component_ref*, internal::polymorphic_component_ref*> list;
    int offset;

    polymorphic_component_ref_iterator &operator++() ENTT_NOEXCEPT {
        return ++offset, *this;
    }

    const polymorphic_component_ref_iterator operator++(int) ENTT_NOEXCEPT {
        polymorphic_component_ref_iterator orig = *this;
        return ++(*this), orig;
    }

    polymorphic_component_ref_iterator &operator--() ENTT_NOEXCEPT {
        return --offset, *this;
    }

    const polymorphic_component_ref_iterator operator--(int) ENTT_NOEXCEPT {
        polymorphic_component_ref_iterator orig = *this;
        return --(*this), orig;
    }

    bool operator==(const polymorphic_component_ref_iterator &other) const ENTT_NOEXCEPT {
        return offset == other.offset;
    }

    bool operator!=(const polymorphic_component_ref_iterator &other) const ENTT_NOEXCEPT {
        return offset != other.offset;
    }

    value_type *operator->() const ENTT_NOEXCEPT {
        if constexpr(std::is_same_v<value_type, internal::polymorphic_component_ref>) {
            return list + offset;
        } else {
            if constexpr(std::is_const_v<value_type>) {
                const void *choice[2] = {list[offset].pointer, list};
                return reinterpret_cast<const value_type *>(choice[offset == -1]);
            } else {
                void *choice[2] = {list[offset].pointer, list};
                return reinterpret_cast<value_type *>(choice[offset == -1]);
            }
        }
    }

    value_type &operator*() const ENTT_NOEXCEPT {
        return *operator->();
    }
};

template<typename... T>
struct polymorphic_inherit_alignment;

template<typename First, typename... Other>
struct polymorphic_inherit_alignment<First, Other...> {
    static constexpr std::size_t value = std::max(alignof(First), polymorphic_inherit_alignment<Other...>::value);
};

template<>
struct polymorphic_inherit_alignment<> {
    static constexpr std::size_t value = 4;
};

template<typename... ParentT>
static constexpr std::size_t polymorphic_inherit_alignment_v = polymorphic_inherit_alignment<ParentT...>::value;

} // internal



/**
 * @brief Used for polymorphic component inheritance.
 *
 * @tparam ParentT parent types of for given component, must all be polymorphic
 */
template<typename... ParentT>
class alignas(internal::polymorphic_inherit_alignment_v<ParentT...>) inherit : public ParentT ... {
public:
    using direct_parent_types_t = type_list<ParentT...>;
    using parent_types_t = type_list_cat_t< direct_parent_types_t, typename ParentT::parent_types_t... >;

    static constexpr bool ignore_if_empty = false;
    static constexpr bool in_place_delete = true;
};

/** @brief just a nicer alias for entt::inherit<>, used to mark component polymorphic, but does not add any parents */
using polymorphic = entt::inherit<>;

/** @brief checks, if given component type is polymorphic */
template<typename Component>
static constexpr bool is_polymorphic_component_v = internal::is_polymorphic_component<Component>::value;

/** @brief returns entt::type_list of parent types for given component type, if given type is not polymorphic will return empty list */
template<typename Component>
using polymorphic_component_parents_t = typename internal::safe_component_parents_access_helper<Component, is_polymorphic_component_v<Component> >::parent_types;

/** @brief returns entt::type_list of direct parent types for given component type, if given type is not polymorphic will return empty list */
template<typename Component>
using polymorphic_component_direct_parents_t = typename internal::safe_component_parents_access_helper<Component, is_polymorphic_component_v<Component> >::direct_parent_types;

/** @brief returns, if Parent is parent of polymorphic component Child */
template<typename Parent, typename Child>
static constexpr bool is_parent_of_v = type_list_contains_v<polymorphic_component_parents_t<Child>, Parent>;

/** @brief returns, if Parent is parent of or same as polymorphic component Child */
template<typename Parent, typename Child>
static constexpr bool is_same_or_parent_of_v = is_parent_of_v<Parent, Child> || std::is_same_v<Parent, Child>;

/** @brief returns, if Parent is direct parent of polymorphic component Child */
template<typename Parent, typename Child>
static constexpr bool is_direct_parent_of_v = type_list_contains_v<polymorphic_component_direct_parents_t<Child>, Parent>;

/**
 * Used for iterating over all polymorphic components of the given type for one entity, used in pair with view
 *
 * Example:
 *  for (auto& component : registry.get<entt::every<MyComponent>>) {
 *      ...
 *  }
 *
 *  for (auto [entity, components] : registry.view<entt::every<MyComponent>>) {
 *      for (auto& component : components) {
 *          ...
 *      }
 *  }
 * @tparam T polymorphic component type
 */
template<typename T>
struct every {
    static_assert(is_polymorphic_component_v<T>, "entt::every can be used only for polymorphic components");

    /** @brief component type */
    using type = T;

    // this will choose the correct view template
    static constexpr bool in_place_delete = true;

    /** @brief iterator type */
    using iterator = internal::polymorphic_component_ref_iterator<type>;

    /** @brief const iterator type */
    using const_iterator = internal::polymorphic_component_ref_iterator<type>;

    every( iterator begin, iterator end ) :
        m_begin(begin), m_end(end) {}

    iterator begin() {
        return m_begin;
    }

    iterator end() {
        return m_end;
    }

    iterator begin() const {
        return m_begin;
    }

    iterator end() const {
        return m_end;
    }

private:
    iterator m_begin, m_end;
};


namespace internal {

// polymorphic component reference lists with element count more than 1 are internally stored in the small contiguous lists
// this class manages memory for such lists based on given allocator, allocator is rebound to allocate arrays of void*
template<typename Allocator, typename = void>
struct component_ref_list_page_source {
    using alloc_traits = typename std::allocator_traits<Allocator>;
    static_assert(std::is_same_v<void*, typename alloc_traits::value_type>);

    static constexpr size_t page_size = 1024;
    static constexpr size_t elems_per_ref = sizeof(polymorphic_component_ref) / sizeof(void *);

    struct page {
        void **base = nullptr;
        int32_t elem_size = 0;
        int32_t elem_count = 0;
        int32_t free_list = -1;
    };

    inline static std::vector<page> &pages() {
        static std::vector<page> v;
        return v;
    }

    static page allocate_page(size_t elem_size) {
        typename alloc_traits::allocator_type allocator{};
        return page{
                alloc_traits::allocate(allocator, page_size * (elems_per_ref * elem_size + 2)),
                static_cast<int32_t>(elem_size), 0, -1
        };
    }

    static void **allocate_array(size_t count) {
        // find free page
        auto &ps = pages();
        auto found = std::find_if(ps.begin(), ps.end(), [=](page &p) {
            return p.elem_size == static_cast<int32_t>(count) && (p.elem_count < static_cast<int32_t>(page_size) || p.free_list != -1);
        });

        // allocate if nothing was found
        if (found == ps.end()) {
            ps.template emplace_back(allocate_page(count));
            found = ps.end() - 1;
        }
        page &p = *found;

        // search page for free place
        const int32_t stride = static_cast<int32_t>(count * elems_per_ref + 2);
        int32_t index;
        if (p.free_list != -1) {
            index = std::exchange(p.free_list, *reinterpret_cast<int32_t *>(p.base + p.free_list * stride));
        } else {
            index = p.elem_count++;
        }
        void **start = reinterpret_cast<void **>(p.base + index * stride);
        reinterpret_cast<uintptr_t &>(start[0]) = 0; // count
        reinterpret_cast<uintptr_t &>(start[1]) = count; // capacity
        return start;
    }

    static void free_array(void **array) {
        // find page, that contains array
        auto &ps = pages();
        auto found = std::find_if(ps.begin(), ps.end(), [=](page &p) {
            return p.base <= array && array < p.base + (elems_per_ref * p.elem_size + 2) * page_size;
        });

        ENTT_ASSERT(found != ps.end(), "free_array got address, which does not belong to any page");
        page &p = *found;
        ENTT_ASSERT(static_cast<uintptr_t>(p.elem_size) == reinterpret_cast<uintptr_t &>(array[1]), "array size does not match array size for this page");

        // add array to free list
        const int32_t stride = static_cast<int32_t>(elems_per_ref * p.elem_size + 2);
        int32_t new_free_list = static_cast<int32_t>(array - p.base) / stride;
        *reinterpret_cast<int32_t *>(array) = std::exchange(p.free_list, new_free_list);
    }
};

// alias to rebind allocator to void* and get page source for this allocator type, so same allocators of different components will still result in same page source
template<typename Allocator>
using page_source_from_allocator = component_ref_list_page_source<typename std::allocator_traits<Allocator>::template rebind_alloc<void*>>;

// wraps a void* pointer to reference list memory with some helper methods, does not allocate or deallocate memory on construction/destruction/copy/etc.
template<typename Component, typename PageSource>
struct polymorphic_component_ref_list {
    // returns a list of zero capacity and size
    inline static void** null_list_base() ENTT_NOEXCEPT {
        static void* zero[2] = {nullptr, nullptr};
        return zero;
    }

    // wraps given void pointer as list
    inline explicit polymorphic_component_ref_list(void* b) ENTT_NOEXCEPT :
            base(reinterpret_cast<void**>(b)) {}

    [[nodiscard]] inline void** get_base() ENTT_NOEXCEPT {
        return base;
    }

    [[nodiscard]] inline std::size_t get_size() const ENTT_NOEXCEPT  {
        return static_cast<std::size_t>(*reinterpret_cast<uintptr_t *>(base));
    }

    inline void set_size(std::size_t count) ENTT_NOEXCEPT {
        *reinterpret_cast<uintptr_t *>(base) = static_cast<uintptr_t>(count);
    }

    [[nodiscard]] inline std::size_t get_capacity() const ENTT_NOEXCEPT {
        return static_cast<std::size_t>(*reinterpret_cast<uintptr_t *>(base + 1));
    }

    inline void set_capacity(std::size_t capacity) ENTT_NOEXCEPT {
        *reinterpret_cast<uintptr_t*>(base + 1) = static_cast<uintptr_t>(capacity);
    }

    [[nodiscard]] inline polymorphic_component_ref* get_list() {
        return reinterpret_cast<polymorphic_component_ref *>(base + 2);
    }

    [[nodiscard]] inline const polymorphic_component_ref* get_list() const ENTT_NOEXCEPT {
        return reinterpret_cast<polymorphic_component_ref const*>(base + 2);
    }

    inline void reserve(std::size_t size) {
        if (get_capacity() < size) {
            std::size_t old_size = get_size();
            void** new_base = PageSource::allocate_array(next_power_of_two(size));
            if (old_size > 0) {
                memcpy(new_base + 2, get_list(), sizeof(polymorphic_component_ref) * old_size);
                PageSource::free_array(base);
            }
            base = new_base;
        }
    }

    inline void clear() {
        PageSource::free_array(base);
        base = null_list_base();
    }

    inline void push_back(polymorphic_component_ref ref) {
        std::size_t size = get_size();
        reserve(size + 1);
        get_list()[size] = ref;
        set_size(size + 1);
    }

    inline void pop_back() {
        std::size_t new_size = get_size() - 1;
        set_size(new_size);
        if (new_size == 0) {
            clear();
        }
    }

    inline every<Component> each() ENTT_NOEXCEPT {
        polymorphic_component_ref *list = get_list();
        return {polymorphic_component_ref_iterator<Component>{list, 0}, polymorphic_component_ref_iterator<Component>{list, static_cast<int>(get_size())}};
    }

    inline every<const Component> each() const ENTT_NOEXCEPT {
        const polymorphic_component_ref *list = get_list();
        return {typename every<const Component>::iterator{list, 0}, typename every<const Component>::iterator{list, static_cast<int>(get_size())}};
    }

private:
    void** base;
};

// Flags, which describe internal state of polymorphic component
namespace polymorphic_container_flags {
    static constexpr std::uint8_t REFERENCE_BIT = 1;
    static constexpr std::uint8_t LIST_BIT = 2;
};

// Encapsulates memory access and layout of a polymorphic component container, implementing all required higher level functionalities
// Allows different memory layouts for different types
template<typename Component, typename Allocator, typename = void>
struct polymorphic_container_memory_layout;

// FUNNY-POINTER-TRICKS-MEMORY-LAYOUT
// Internal workings of this memory layout rely on pointer alignment, assuming that all used pointers have at least 2 bits (alignof at list 4, which is provided by entt::inherit by default), that are always zero,
// so additional stored pointer also used for storing 2 bit flags for the state of the container:
//    - bit 1 (pointer & 1) is 0 when container holds component value and 1 otherwise
//    - bit 2 (pointer & 2) is 1 when container holds a pointer to heap-allocated list of references
//
// TODO: this memory layout should be std::enabled_if only if contains polymorphic component fulfills alignment requirements: all of its parents must have alignment of, at least 4
//
// Memory layout (4 variants):
// Only value (pointer bits 00):
//    data = [component value], pointer = [any valid pointer, that can be de-referenced]
// Only reference (pointer bits 01):
//    data = [deleter pointer], pointer = [pointer to referenced component]
// Value + list (pointer bits 10):
//    data = [component value], pointer = [pointer to list, list holds at least 2 references: one to contained value and one to something else]
// Reference + list (pointer bits 11):
//    data = [pointer to list, contains at least 2 references, one is contained in this container, and one is somewhere else] pointer = [pointer to any component from reference list]
template<typename Component, typename Allocator>
struct polymorphic_container_memory_layout<Component, Allocator, void> {
    static constexpr std::size_t buffer_alignment = std::max(alignof(Component), alignof(void*));
    static constexpr std::size_t buffer_size = std::max(sizeof(Component), sizeof(void*));

    using ref_list = polymorphic_component_ref_list<Component, page_source_from_allocator<Allocator>>;

    // flag access
    [[nodiscard]] inline std::uint8_t get_flag(std::uint8_t bit) ENTT_NOEXCEPT {
        return pointer & bit;
    }

    inline void set_flag(std::uint8_t bit) ENTT_NOEXCEPT {
        pointer |= bit;
    }

    inline void clear_flag(std::uint8_t bit) ENTT_NOEXCEPT {
        pointer &= (static_cast<std::uintptr_t>(-1) ^ bit);
    }

    inline void flip_flag(std::uint8_t bit) ENTT_NOEXCEPT {
        pointer ^= bit;
    }

    // return buffer for value
    [[nodiscard]] inline Component* value_base() ENTT_NOEXCEPT {
        return reinterpret_cast<Component*>(&value);
    }

    // return single reference
    [[nodiscard]] inline Component& ref() ENTT_NOEXCEPT {
        constexpr uintptr_t mask = static_cast<uintptr_t>(-1) ^ 3u;
        void* pointers[2] = {value_base(), reinterpret_cast<void*>(pointer & mask)};
        return *reinterpret_cast<Component*>(pointers[get_flag(polymorphic_container_flags::REFERENCE_BIT)]);
    }

    // get list (does not check, if list flag is set, getting list when it is not present is undefined behavior)
    [[nodiscard]] inline ref_list get_list() ENTT_NOEXCEPT {
        constexpr std::uintptr_t mask = static_cast<std::uintptr_t>(-1) ^ 3u;
        void* pointers[2] = {reinterpret_cast<void*>(pointer & mask), *reinterpret_cast<void**>(&value)};
        return ref_list{pointers[get_flag(polymorphic_container_flags::REFERENCE_BIT)]};
    }

    // set list + list flag (if contains single reference + deleter, it will be kept without deleter)
    inline void set_list(ref_list list) ENTT_NOEXCEPT {
        void* pointers[2] = {&pointer, value_base()};
        std::uint8_t reference_bit = get_flag(polymorphic_container_flags::REFERENCE_BIT);
        *reinterpret_cast<std::uintptr_t*>(pointers[reference_bit]) = reinterpret_cast<uintptr_t>(list.get_base());
        set_flag(polymorphic_container_flags::LIST_BIT);
    }

    // set single reference with no list or value + reference flag (other flags are removed)
    inline void set_single_ref(polymorphic_component_ref ref) ENTT_NOEXCEPT {
        pointer = reinterpret_cast<std::uintptr_t>(ref.pointer) | polymorphic_container_flags::REFERENCE_BIT;
        *reinterpret_cast<void**>(value_base()) = ref.deleter;
    }

    // in a single reference state returns full reference
    [[nodiscard]] inline polymorphic_component_ref get_single_ref() ENTT_NOEXCEPT {
        constexpr uintptr_t mask = static_cast<uintptr_t>(-1) ^ 3u;
        return polymorphic_component_ref{reinterpret_cast<void*>(pointer & mask), *reinterpret_cast<void**>(value_base())};
    }

    // when holding a list + reference (both flags are set), replace contained reference with a given pointer
    inline void replace_ref_from_list(void* ptr) ENTT_NOEXCEPT {
        pointer = reinterpret_cast<std::uintptr_t>(ptr) | polymorphic_container_flags::LIST_BIT | polymorphic_container_flags::REFERENCE_BIT;
    }

    // setup state of containing only value and no ref list
    inline void set_only_value() ENTT_NOEXCEPT {
        pointer = reinterpret_cast<std::uintptr_t>(ref_list::null_list_base());
    }

    // memory layout
private:
    // buffer for value or one pointer
    struct alignas(buffer_alignment) {
        uint8_t bytes[buffer_size];
    } value;

    // additional pointer + flags
    std::uintptr_t pointer;
};


// Container for polymorphic components, can hold a value + reference list, can hold 1 component or reference without heap allocation,
// used as an internal type in polymorphic component storages. Has additional memory usage only of one pointer per component.
//
// Container can hold either:
// - value of the component of the exact type Component + optional pointer to reference list
// - reference to some component of any Component's child type + optional pointer to reference list
//
// One reference/value is always stored in the container itself (not the list), so it can be accessed really fast, in case only one component is requested.
// List, if present, always contains duplicate reference to value, contained in this container (if there is any), so iteration does not require any additional checks.
template<typename Entity, typename Component, typename Allocator>
struct polymorphic_component_container {
    using this_type = polymorphic_component_container<Entity, Component, Allocator>;
    using alloc_traits = typename std::allocator_traits<Allocator>;
    using memory_layout = polymorphic_container_memory_layout<Component, Allocator>;
    using ref_list = typename memory_layout::ref_list;

    static_assert(std::is_same_v<typename alloc_traits::value_type, Component>);
    static_assert(is_polymorphic_component_v<Component>);

    static constexpr bool in_place_delete = true;

    // initializes container with single reference
    inline explicit polymorphic_component_container(polymorphic_component_ref ref) {
        layout.set_single_ref(ref);
    }

    // initialize container with value
    template<typename... Args>
    inline explicit polymorphic_component_container(Args &&...args) {
        typename alloc_traits::allocator_type allocator{};
        alloc_traits::construct(allocator, layout.value_base(), std::forward<Args>(args)...);
        layout.set_only_value();
    }

    // no move/copy/swap is allowed, all component pointers must be stable
    polymorphic_component_container(const polymorphic_component_container&) = delete;
    polymorphic_component_container(polymorphic_component_container&& other) = delete;
    void operator=(const polymorphic_component_container&) = delete;
    void operator=(polymorphic_component_container&& other) = delete;
    inline void swap(polymorphic_component_container& other) {
        ENTT_ASSERT(false, "this should never be called, polymorphic container is not swappable");
    }

    // returns reference to any contained component
    inline Component& ref() {
        return layout.ref();
    }

    inline const Component& ref() const {
        return const_cast<this_type *>(this)->ref();
    }

private:
    // creates new list, puts it into container and returns
    inline ref_list create_list() {
        ref_list list{ref_list::null_list_base()};
        list.reserve(4);
        if (layout.get_flag(polymorphic_container_flags::REFERENCE_BIT)) {
            // if contains reference
            list.push_back(layout.get_single_ref());
        } else {
            // if contains a value
            list.push_back({layout.value_base(), reinterpret_cast<void *>(&deleter)});
        }
        layout.set_list(list);
        return list;
    }

    // removes current list and puts remaining reference into container
    // must be called only if already holding a non-empty list (pointer & 2 is set to 1) to remove it (will not delete list, it must be freed manually)
    inline void clear_list(polymorphic_component_ref self_ref) {
        if (layout.get_flag(polymorphic_container_flags::REFERENCE_BIT)) {
            layout.set_single_ref(self_ref);
        } else {
            layout.set_only_value();
        }
    }

    // receives list, contained in this container, deletes reference, that matches given pointer from it, if only one reference left in the list, clears the list and puts reference into container
    inline bool delete_ref_internal(ref_list list, void* ptr) {
        std::size_t size = list.get_size();
        polymorphic_component_ref* mem = list.get_list();
        // iterate over list and search matching ref
        for (std::size_t i = 0; i < size; i++) {
            if (mem[i].pointer == ptr) {
                // swap and pop
                std::swap(mem[i], mem[size - 1]);
                list.pop_back();
                // if only one ref left
                if (size == 2) {
                    // clear list and put one remaining reference into container
                    clear_list(mem[0]);
                    // pop last element to clear the list completely, this will free memory
                    list.pop_back();
                } else {
                    // update list pointer
                    layout.set_list(list);
                }
                return true;
            }
        }
        return false;
    }

public:
    // iterates over component references
    inline every<Component> each() ENTT_NOEXCEPT {
        if (layout.get_flag(polymorphic_container_flags::LIST_BIT)) {
            // if holds component list
            return layout.get_list().each();
        } else {
            // if holds only one component
            using iterator = typename every<Component>::iterator;
            auto* list = reinterpret_cast<polymorphic_component_ref *>(std::addressof(ref()));
            return every<Component>{iterator{list, -1}, iterator{list, 0}};
        }
    }

    // iterates over component references
    inline every<const Component> each() const ENTT_NOEXCEPT {
        if (layout.get_flag(polymorphic_container_flags::LIST_BIT) & 2u) {
            // if holds component list
            return const_cast<this_type*>(this)->layout.get_list().each();
        } else {
            // if holds only one component
            using iterator = typename every<const Component>::iterator;
            const auto* list = reinterpret_cast<const polymorphic_component_ref *>(std::addressof(ref()));
            return every<const Component>{iterator{list, -1}, iterator{list, 0}};
        }
    }

    // adds reference to list
    inline void add_ref(polymorphic_component_ref ref) {
        ENTT_ASSERT(ref.pointer != layout.value_base(), "add_ref must not receive reference to its own value");
        // get or create list, push, and set list back
        ref_list list = layout.get_flag(polymorphic_container_flags::LIST_BIT) ? layout.get_list() : create_list();
        list.push_back(ref);
        layout.set_list(list);
    }

    // deletes component reference from list, returns true, if the container is now empty and can be deleted
    inline bool delete_ref(void* ptr) {
        ENTT_ASSERT(ptr != layout.value_base(), "delete_ref must not receive reference to its own value");
        if (layout.get_flag(polymorphic_container_flags::LIST_BIT)) {
            // if holds component list, remove reference from it, and assert, that it was there
            [[maybe_unused]] bool success = delete_ref_internal(layout.get_list(), ptr);
            ENTT_ASSERT(success, "delete_ref got non-existing reference");
            return false;
        } else {
            // otherwise, ensure, that we are holding the deleted ref
            ENTT_ASSERT(std::addressof(ref()) == ptr, "delete_ref got non-existing reference (only one left)");
            // return true, if we were holding the last reference
            return layout.get_flag(polymorphic_container_flags::REFERENCE_BIT);
        }
    }

private:
    // emplace references to this into containers for all parent types
    template<typename... ParentComponent>
    inline void emplace_hierarchy_references(entt::basic_registry<Entity> &registry, const Entity entity, Component &r, [[maybe_unused]] type_list<ParentComponent...> sequence) {
        (registry.template assure<ParentComponent>().emplace_ref(entity, r, reinterpret_cast<void *>(&deleter)), ...);
    }

    // erase references to this from containers for all parent types
    template<typename... ParentComponent>
    inline void erase_hierarchy_references(entt::basic_registry<Entity> &registry, const Entity entity, Component &r, [[maybe_unused]] type_list<ParentComponent...> sequence) {
        (registry.template assure<ParentComponent>().erase_ref(entity, r), ...);
    }

public:
    // construct Component value inside the container, moves contained references to list (creates one, if required), emplace hierarchy references
    template<typename... Args>
    inline void construct_value(entt::basic_registry<Entity> &registry, const Entity entity, Args &&...args) {
        ENTT_ASSERT(layout.get_flag(polymorphic_container_flags::REFERENCE_BIT), "construct_value called while already holding a value");
        // get or create the list
        ref_list list = (layout.get_flag(polymorphic_container_flags::LIST_BIT) ? layout.get_list() : create_list());
        // construct the value and clear reference flag
        typename alloc_traits::allocator_type allocator{};
        alloc_traits::construct(allocator, layout.value_base(), std::forward<Args>(args)...);
        layout.clear_flag(polymorphic_container_flags::REFERENCE_BIT);
        // add self reference to list and set it back
        list.push_back({layout.value_base(), reinterpret_cast<void *>(&deleter)});
        layout.set_list(list);
        // emplace hierarchy
        emplace_hierarchy_references(registry, entity, ref(), polymorphic_component_parents_t<Component>{});
    }

    // used, when container is constructed with value
    inline void emplace_hierarchy_after_construct(entt::basic_registry<Entity> &registry, const Entity entity) {
        emplace_hierarchy_references(registry, entity, ref(), polymorphic_component_parents_t<Component>{});
    }

    // destroys contained Component value inside the container, return true, if it is now empty, false, if it still contains one or more references
    inline bool destroy_value(entt::basic_registry<Entity> &registry, const Entity entity) {
        ENTT_ASSERT(!layout.get_flag(polymorphic_container_flags::REFERENCE_BIT), "destroy_value called while not holding a value");
        // erase hierarchy
        Component& r = ref();
        erase_hierarchy_references(registry, entity, r, polymorphic_component_parents_t<Component>{});
        // destroy value
        typename alloc_traits::allocator_type allocator{};
        alloc_traits::destroy(allocator, reinterpret_cast<Component *>(this));
        // if we have the list, then it contains at least 2 references
        if (layout.get_flag(polymorphic_container_flags::LIST_BIT)) {
            // get the list and flip the pointer
            ref_list list = layout.get_list();
            layout.set_flag(polymorphic_container_flags::REFERENCE_BIT);
            // delete reference to this from the list and put some reference from the list into container
            [[maybe_unused]] bool success = delete_ref_internal(list, layout.value_base());
            ENTT_ASSERT(success, "self reference was not present inside the list");
            // if we still have a list, copy first reference from it
            if (layout.get_flag(polymorphic_container_flags::LIST_BIT)) {
                layout.replace_ref_from_list(layout.get_list().get_list()->pointer);
            }
            // some references are still remaining in the container
            return false;
        } else {
            // no list, no value, container is empty and can be destroyed, set the reference flag, so destructor will not try to delete the value
            layout.set_flag(polymorphic_container_flags::REFERENCE_BIT);
            return true;
        }
    }

    // destroys all references, returns true, if it is now empty and can be destroyed, false, if it still contains a value
    inline bool destroy_all_refs(entt::basic_registry<Entity> &registry, const Entity entity) {
        if (layout.get_flag(polymorphic_container_flags::LIST_BIT)) {
            // if we have list
            ref_list list = layout.get_list();
            polymorphic_component_ref* mem = list.get_list();
            std::size_t count = list.get_size();
            // iterating list backwards guards us from swap-and-pop delete of the current element
            for (int i = static_cast<int>(count) - 1; i >= 0; i--) {
                if (mem[i].pointer != this) {
                    reinterpret_cast<void (*)(entt::basic_registry<Entity>& registry, const Entity entity)>(mem[i].deleter)(registry, entity);
                }
            }
            ENTT_ASSERT(layout.get_list().get_capacity() == 0, "list is not fully cleaned after destroying all refs");
            // clear list
            layout.set_list(ref_list{ref_list::null_list_base()});
            layout.clear_flag(polymorphic_container_flags::LIST_BIT);
        } else if (layout.get_flag(polymorphic_container_flags::REFERENCE_BIT)) {
            // if we just have one ref
            (reinterpret_cast<void (*)(entt::basic_registry<Entity>& registry, const Entity entity)>(layout.get_single_ref().deleter))(registry, entity);
        }
        // return true, if we did not contain a value
        return layout.get_flag(polymorphic_container_flags::REFERENCE_BIT);
    }

    // destructor does something, only if container still contains a value, this can happen, only when storage, holding this container, is destroyed, erase must not trigger this
    inline ~polymorphic_component_container() {
        if (!layout.get_flag(polymorphic_container_flags::REFERENCE_BIT)) {
            typename alloc_traits::allocator_type allocator{};
            alloc_traits::destroy(allocator, layout.value_base());
        }
    }

    /** @brief static deleter for contained component, will erase it, which will call destroy_value and delete whole hierarchy */
    static void deleter(entt::basic_registry<Entity> &registry, const Entity entity) {
        registry.template assure<Component>().erase_value(registry, entity);
    }

private:
    memory_layout layout;
};


// checks, if given type is polymorphic component container, and extracts component type
template<typename T, typename = void, typename = void>
struct unwrap_polymorphic_component_container {
    using value_type = T;
    static constexpr bool is_container = false;
};

template<typename T, typename Entity_, typename Alloc_>
struct unwrap_polymorphic_component_container<polymorphic_component_container<Entity_, T, Alloc_>> {
    using value_type = T;
    static constexpr bool is_container = true;
};

template<typename T>
using unwrap_polymorphic_component_container_t = typename unwrap_polymorphic_component_container<T>::value_type;

template<typename T>
constexpr bool is_polymorphic_component_container_v = unwrap_polymorphic_component_container<T>::is_container;


// unwraps entt::every<Component> and extracts component type and overall constness
template<typename T>
struct unwrap_every {
    using type = T;
    static constexpr bool is_every = false;
    static constexpr bool is_const = false;
};

template<typename T>
struct unwrap_every<every<T>> {
    using type = T;
    static constexpr bool is_every = true;
    static constexpr bool is_const = false;
};

template<typename T>
struct unwrap_every<const every<T>> {
    using type = T;
    static constexpr bool is_every = true;
    static constexpr bool is_const = true;
};

template<typename T>
struct unwrap_every<every<const T>> {
    using type = T;
    static constexpr bool is_every = true;
    static constexpr bool is_const = true;
};

template<typename T>
using unwrap_every_t = typename unwrap_every<T>::type;

template<typename T>
constexpr bool is_every_wrap_v = unwrap_every<T>::is_every;

template<typename T>
constexpr bool is_const_every_wrap_v = unwrap_every<T>::is_const;

} // internal

} // namespace entt

#endif
