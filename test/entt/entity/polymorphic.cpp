#include <algorithm>
#include <array>
#include <functional>
#include <numeric>
#include <gtest/gtest.h>
#include <entt/entity/registry.hpp>

struct polymorphic_base : public entt::polymorphic {
    int x;

    int* destroyed_check = nullptr;
    ~polymorphic_base() {
        if (destroyed_check) {
            (*destroyed_check)++;
        }
    }
};

struct polymorphic_parent : public entt::inherit<polymorphic_base> {

};

struct polymorphic_component : public entt::inherit<polymorphic_parent> {

};

struct polymorphic_child : public entt::inherit<polymorphic_component> {

};

struct polymorphic_component_sibling : public entt::inherit<polymorphic_parent> {

};


// ---------------------------------------------------------------------------------------------------------


template<typename Type>
void testSingleComponentGetAndIterate(entt::registry& registry, entt::entity ent, polymorphic_base& emplaced, bool present) {
    // test get
    ASSERT_EQ(registry.template try_get<Type>(ent) != nullptr, present);
    int count;

    // test single component iteration (for loop)
    count = 0;
    for (auto[ e, component ] : registry.template view<Type>().each()) {
        ASSERT_EQ(e, ent);
        ASSERT_EQ(std::addressof(component), std::addressof(emplaced));
        ASSERT_EQ(component.x, 123);
        count++;
    }
    ASSERT_EQ(count, present ? 1 : 0);

    // test single component iteration (lambda)
    count = 0;
    registry.template view<Type>().each([&](entt::entity e, Type& component) {
        ASSERT_EQ(e, ent);
        ASSERT_EQ(std::addressof(component), std::addressof(emplaced));
        ASSERT_EQ(component.x, 123);
        count++;
    });
    ASSERT_EQ(count, present ? 1 : 0);

    // test every component iteration (for loop)
    count = 0;
    for (auto[ e, components ] : registry.template view<entt::every<Type>>().each()) {
        for (Type& component : components) {
            ASSERT_EQ(e, ent);
            ASSERT_EQ(std::addressof(component), std::addressof(emplaced));
            ASSERT_EQ(component.x, 123);
            count++;
        }
    }
    ASSERT_EQ(count, present ? 1 : 0);

    // test every component iteration (lambda)
    count = 0;
    registry.template view<entt::every<Type>>().each([&](entt::entity e, entt::every<Type> components) {
        for (Type& component : components) {
            ASSERT_EQ(e, ent);
            ASSERT_EQ(std::addressof(component), std::addressof(emplaced));
            ASSERT_EQ(component.x, 123);
            count++;
        }
    });
    ASSERT_EQ(count, present ? 1 : 0);
}

template<typename RemoveType>
void testSingleAddAndRemove(entt::registry& registry, entt::entity ent, bool destroyEntityInsteadOfRemove) {
    // add polymorphic component
    auto& emplaced = registry.emplace<polymorphic_component>(ent);
    emplaced.x = 123;
    int destructor_call_count = 0;
    emplaced.destroyed_check = &destructor_call_count;
    // check if it and all its parents can be accessed through get and iteration & all_of
    testSingleComponentGetAndIterate<polymorphic_component>(registry, ent, emplaced, true);
    testSingleComponentGetAndIterate<polymorphic_parent>(registry, ent, emplaced, true);
    testSingleComponentGetAndIterate<polymorphic_base>(registry, ent, emplaced, true);
    ASSERT_TRUE((registry.template all_of<polymorphic_base, polymorphic_parent, polymorphic_component>(ent)));

    // remove
    if (destroyEntityInsteadOfRemove) {
        registry.destroy(ent);
    } else {
        ASSERT_EQ(registry.remove<RemoveType>(ent), 1);
        // check that all components were removed successfully
        testSingleComponentGetAndIterate<polymorphic_component>(registry, ent, emplaced, false);
        testSingleComponentGetAndIterate<polymorphic_parent>(registry, ent, emplaced, false);
        testSingleComponentGetAndIterate<polymorphic_base>(registry, ent, emplaced, false);
        ASSERT_FALSE((registry.template any_of<polymorphic_base, polymorphic_parent, polymorphic_component>(ent)));
    }

    // check, if component was destroyed exactly once
    ASSERT_EQ(destructor_call_count, 1);
}

TEST(Polymorphic, SingleComponent) {
    entt::registry registry;

    // test adding and removing single component hierarchy
    entt::entity ent = registry.create();
    testSingleAddAndRemove<polymorphic_base>(registry, ent, false);
    testSingleAddAndRemove<polymorphic_parent>(registry, ent, false);
    testSingleAddAndRemove<polymorphic_component>(registry, ent, false);

    // same, but with destroying entity instead of removing
    testSingleAddAndRemove<polymorphic_base>(registry, registry.create(), true);
    testSingleAddAndRemove<polymorphic_parent>(registry, registry.create(), true);
    testSingleAddAndRemove<polymorphic_component>(registry, registry.create(), true);
}


// ---------------------------------------------------------------------------------------------------------


template<typename... T>
struct type_list_sub_sequences;

template<typename First, typename... Other>
struct type_list_sub_sequences<entt::type_list<First, Other...>> {
    template<typename Add, typename... Lists>
    struct push_front;

    template<typename Add, typename... Lists>
    struct push_front<Add, entt::type_list<Lists...>> {
        using type = entt::type_list<entt::type_list_cat_t<entt::type_list<Add>, Lists>...>;
    };

    using sub_sequence = typename type_list_sub_sequences<entt::type_list<Other...>>::type;
    using type = entt::type_list_cat_t<typename push_front<First, sub_sequence>::type, sub_sequence>;
};

template<typename T>
struct type_list_sub_sequences<entt::type_list<T>> {
    using type = entt::type_list<entt::type_list<T>, entt::type_list<>>;
};

template<>
struct type_list_sub_sequences<entt::type_list<>> {
    using type = entt::type_list<>;
};

template<typename T>
using type_list_sub_sequences_t = typename type_list_sub_sequences<T>::type;


template<int size, int count>
void forAllPermutations(const std::array<std::array<std::function<void()>, size>, count>& actions, const std::function<void(int)>& callback) {
    std::array<std::array<int, size>, count> permutations{};
    for (int i = 0; i < count; i++) {
        std::iota(permutations[i].begin(), permutations[i].end(), 0);
    }

    bool finish = false;
    while (!finish) {
        callback(-1);
        for (int i = 0; i < count; i++) {
            for (int j = 0; j < size; j++) {
                actions[i][permutations[i][j]]();
            }
            if (callback) {
                callback(i);
            }
        }

        for (int i = count - 1; i >= 0; i--) {
            if (!std::next_permutation(permutations[i].begin(), permutations[i].end())) {
                if (i == 0) {
                    finish = true;
                }
                std::iota(permutations[i].begin(), permutations[i].end(), 0);
            } else {
                break;
            }
        }
    }
}

template<typename Component, typename... AllAddedComponents>
inline void testMultipleComponentsGetAndIterate(entt::registry& registry, entt::entity ent, entt::type_list<AllAddedComponents...> all, bool present) {
    bool is_parent = (entt::is_same_or_parent_of_v<Component, AllAddedComponents> || ...);
    present = present && is_parent;

    // test get
    ASSERT_EQ(registry.template try_get<Component>(ent) != nullptr, present);
    int count;

    // test single component iteration (for loop)
    count = 0;
    for (auto[ e, component ] : registry.template view<Component>().each()) {
        ASSERT_EQ(component.x, 123);
        count++;
    }
    ASSERT_EQ(count > 0, present);

    // test single component iteration (lambda)
    count = 0;
    registry.template view<Component>().each([&](entt::entity e, Component& component) {
        ASSERT_EQ(component.x, 123);
        count++;
    });
    ASSERT_EQ(count > 0, present);

    // test every component iteration (for loop)
    count = 0;
    for (auto[ e, components ] : registry.template view<entt::every<Component>>().each()) {
        for (Component& component : components) {
            ASSERT_EQ(component.x, 123);
            count++;
        }
    }
    ASSERT_EQ(count > 0, present);

    // test every component iteration (lambda)
    count = 0;
    registry.template view<entt::every<Component>>().each([&](entt::entity e, entt::every<Component> components) {
        for (Component& component : components) {
            ASSERT_EQ(component.x, 123);
            count++;
        }
    });
    ASSERT_EQ(count > 0, present);
}

template<typename... AllComponents, typename... AddComponents>
inline void testMultipleComponentsSubSequence(entt::registry& registry, entt::entity ent, entt::type_list<AllComponents...>, entt::type_list<AddComponents...>) {
    if constexpr(sizeof...(AddComponents) > 0)
    {
        int destructor_call_count[sizeof...(AddComponents)] = {}; int index;

        forAllPermutations<sizeof...(AddComponents), 2>({
            std::array<std::function<void()>, sizeof...(AddComponents)>{
                [&]() {
                    // add components
                    auto& ref = registry.template emplace<AddComponents>(ent);
                    ref.x = 123;
                    ref.destroyed_check = std::addressof(destructor_call_count[index++]);
                }...
            },
            std::array<std::function<void()>, sizeof...(AddComponents)>{
                [&]() {
                    registry.template remove<AddComponents>(ent);
                }...
            },
        }, [&] (int stage) {
            // before add
            if (stage == -1) {
                // reset destructor call count
                std::fill(std::begin(destructor_call_count), std::end(destructor_call_count), 0);
                index = 0;
            }
            // between add and remove
            if (stage == 0) {
                // test, that required components are present and others are not
                (testMultipleComponentsGetAndIterate<AllComponents>(registry, ent, entt::type_list<AddComponents...>{}, true), ...);
            }
            // after remove
            else if (stage == 1) {
                // test, that all components are removed
                (testMultipleComponentsGetAndIterate<AllComponents>(registry, ent, entt::type_list<AddComponents...>{}, false), ...);

                // check if all were destroyed exactly once
                index = 0; bool all_destroyed = ((entt::type_list<AddComponents>{}, destructor_call_count[index++] == 1) && ...);
                ASSERT_TRUE(all_destroyed);
            }
        });
    } else {
        // test no components are present
        (testMultipleComponentsGetAndIterate<AllComponents>(registry, ent, entt::type_list<AddComponents...>{}, false), ...);
    }
}

template<typename... AllComponents, typename... AddComponentsLists>
void testMultipleComponentsAllSubSequences(entt::registry& registry, entt::entity ent, entt::type_list<AllComponents...>, entt::type_list<AddComponentsLists...>) {
    (testMultipleComponentsSubSequence(registry, ent, entt::type_list<AllComponents...>{}, AddComponentsLists{}), ...);
}

template<typename... AllComponents>
void testMultipleComponents(entt::registry& registry, entt::entity ent, entt::type_list<AllComponents...>) {
    testMultipleComponentsAllSubSequences(registry, ent, entt::type_list<AllComponents...>{}, type_list_sub_sequences_t<entt::type_list<AllComponents...>>{});
}

TEST(Polymorphic, MultipleComponentsOneEntity) {
    entt::registry registry;
    testMultipleComponents(registry, registry.create(), entt::type_list<polymorphic_parent, polymorphic_component, polymorphic_component_sibling, polymorphic_child>{});
}


// ---------------------------------------------------------------------------------------------------------


struct multi_parent_a : public entt::polymorphic {
    int a;
};

struct multi_parent_b : public entt::polymorphic {
    int b;
};

struct multi_parent_c : public entt::polymorphic {
    int c;
};

struct multi_parent_bc : public entt::inherit<multi_parent_b, multi_parent_c> {

};

struct multi_inherited : public entt::inherit<multi_parent_a, multi_parent_bc> {

};

TEST(Polymorphic, MultipleInheritance) {
    entt::registry registry;

    entt::entity ent = registry.create();
    auto& component = registry.emplace<multi_inherited>(ent);
    component.a = 1;
    component.b = 2;
    component.c = 3;
    auto [p_a, p_b, p_c, p_bc] = registry.get<multi_parent_a, multi_parent_b, multi_parent_c, multi_parent_bc>(ent);

    ASSERT_EQ(p_a.a, component.a);
    ASSERT_EQ(p_b.b, component.b);
    ASSERT_EQ(p_c.c, component.c);
    ASSERT_EQ(p_bc.b, component.b);
    ASSERT_EQ(p_bc.c, component.c);
}


// ---------------------------------------------------------------------------------------------------------


struct transform {
    int64_t x;
    int64_t y;
};

struct ticking : public entt::polymorphic {
    int age = 0;

    virtual void tick(transform&) {
        age++;
    }
};

struct physics_base : public entt::polymorphic {
    struct {
        int x, y;
    } velocity;
};

struct physics : public entt::inherit<physics_base, ticking> {
    void tick(transform& t) override {
        ticking::tick(t);
        t.x += velocity.x;
        t.y += velocity.y;
    }
};

struct tracker : public entt::inherit<ticking> {
    struct pos {
        int64_t x, y;
    };
    std::vector<pos> history;

    void tick(transform& t) override {
        ticking::tick(t);
        history.push_back(pos{t.x, t.y});
    }
};

TEST(Polymorphic, RealWorldUse) {
    entt::registry registry;

    const int entity_count = 10;
    for (int i = 0; i < entity_count; i++) {
        auto ent = registry.create();
        registry.emplace<transform>(ent, 0, 0);
        registry.emplace<physics>(ent).velocity = {2, 3};
        registry.emplace<tracker>(ent);
    }

    const int tick_count = 100;
    for (int i = 0; i < tick_count; i++) {
        registry.view<transform, entt::every<ticking>>().each([&](const entt::entity ent, transform &t, entt::every<ticking> ticking_components) {
            for(ticking &ticking_component: ticking_components) {
                ticking_component.tick(t);
            }
        });
    }

    int iteration_count = 0;
    registry.view<transform,
                  entt::every<ticking>,
                  physics_base,
                  physics,
                  tracker>().each([&] (
                        transform& t,
                        entt::every<ticking> ticking_components,
                        physics_base& physics_base_component,
                        physics& physics_component,
                        tracker& tracker_component) {
        ASSERT_EQ(physics_component.velocity.x, 2);
        ASSERT_EQ(physics_component.velocity.y, 3);
        ASSERT_EQ(physics_component.velocity.x, physics_base_component.velocity.x);
        ASSERT_EQ(physics_component.velocity.y, physics_base_component.velocity.y);
        ASSERT_EQ(t.x, tick_count * physics_base_component.velocity.x);
        ASSERT_EQ(t.y, tick_count * physics_base_component.velocity.y);
        ASSERT_EQ(tracker_component.history.size(), tick_count);

        int ticking_component_count = 0;
        for (ticking& ticking_component : ticking_components) {
            ticking_component_count++;
            ASSERT_EQ(ticking_component.age, tick_count);
        }
        ASSERT_EQ(ticking_component_count, 2);

        iteration_count++;
    });
    ASSERT_EQ(iteration_count, entity_count);
}