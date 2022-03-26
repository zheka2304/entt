#include <gtest/gtest.h>
#include <entt/entity/registry.hpp>

struct polymorphic_base : public entt::polymorphic {
    int x;
};

struct polymorphic_parent : public entt::inherit<polymorphic_base> {

};

struct polymorphic_component : public entt::inherit<polymorphic_parent> {
    int* destroyed_check = nullptr;

    ~polymorphic_component() {
        if (destroyed_check) {
            (*destroyed_check)++;
        }
    }
};

struct polymorphic_component_sibling : public entt::inherit<polymorphic_parent> {

};

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

