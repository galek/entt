#include <gtest/gtest.h>
#include <entt/process/process.hpp>

struct FakeProcess: entt::Process<FakeProcess, int> {
    using process_type = entt::Process<FakeProcess, int>;

    void succeed() noexcept { process_type::succeed(); }
    void fail() noexcept { process_type::fail(); }
    void pause() noexcept { process_type::pause(); }
    void unpause() noexcept { process_type::unpause(); }

    void init() { initInvoked = true; }
    void update(delta_type) { updateInvoked = true; }
    void succeeded() { succeededInvoked = true; }
    void failed() { failedInvoked = true; }
    void aborted() { abortedInvoked = true; }

    bool initInvoked{false};
    bool updateInvoked{false};
    bool succeededInvoked{false};
    bool failedInvoked{false};
    bool abortedInvoked{false};
};

TEST(Process, Basics) {
    FakeProcess process;

    ASSERT_FALSE(process.alive());
    ASSERT_FALSE(process.dead());
    ASSERT_FALSE(process.paused());

    process.succeed();
    process.fail();
    process.abort();
    process.pause();
    process.unpause();

    ASSERT_FALSE(process.alive());
    ASSERT_FALSE(process.dead());
    ASSERT_FALSE(process.paused());

    process.tick(0);

    ASSERT_TRUE(process.alive());
    ASSERT_FALSE(process.dead());
    ASSERT_FALSE(process.paused());

    process.pause();

    ASSERT_TRUE(process.alive());
    ASSERT_FALSE(process.dead());
    ASSERT_TRUE(process.paused());

    process.unpause();

    ASSERT_TRUE(process.alive());
    ASSERT_FALSE(process.dead());
    ASSERT_FALSE(process.paused());
}

TEST(Process, Succeeded) {
    FakeProcess process;

    process.tick(0);
    process.succeed();
    process.tick(0);

    ASSERT_FALSE(process.alive());
    ASSERT_TRUE(process.dead());
    ASSERT_FALSE(process.paused());

    ASSERT_TRUE(process.initInvoked);
    ASSERT_TRUE(process.updateInvoked);
    ASSERT_TRUE(process.succeededInvoked);
    ASSERT_FALSE(process.failedInvoked);
    ASSERT_FALSE(process.abortedInvoked);
}

TEST(Process, Fail) {
    FakeProcess process;

    process.tick(0);
    process.fail();
    process.tick(0);

    ASSERT_FALSE(process.alive());
    ASSERT_TRUE(process.dead());
    ASSERT_FALSE(process.paused());

    ASSERT_TRUE(process.initInvoked);
    ASSERT_TRUE(process.updateInvoked);
    ASSERT_FALSE(process.succeededInvoked);
    ASSERT_TRUE(process.failedInvoked);
    ASSERT_FALSE(process.abortedInvoked);
}

TEST(Process, AbortNextTick) {
    FakeProcess process;

    process.tick(0);
    process.abort();
    process.tick(0);

    ASSERT_FALSE(process.alive());
    ASSERT_TRUE(process.dead());
    ASSERT_FALSE(process.paused());

    ASSERT_TRUE(process.initInvoked);
    ASSERT_TRUE(process.updateInvoked);
    ASSERT_FALSE(process.succeededInvoked);
    ASSERT_FALSE(process.failedInvoked);
    ASSERT_TRUE(process.abortedInvoked);
}

TEST(Process, AbortImmediately) {
    FakeProcess process;

    process.tick(0);
    process.abort(true);

    ASSERT_FALSE(process.alive());
    ASSERT_TRUE(process.dead());
    ASSERT_FALSE(process.paused());

    ASSERT_TRUE(process.initInvoked);
    ASSERT_TRUE(process.updateInvoked);
    ASSERT_FALSE(process.succeededInvoked);
    ASSERT_FALSE(process.failedInvoked);
    ASSERT_TRUE(process.abortedInvoked);
}

TEST(ProcessAdaptor, Resolved) {
    bool updated = false;
    auto lambda = [&updated](uint64_t, auto resolve, auto) {
        ASSERT_FALSE(updated);
        updated = true;
        resolve();
    };

    auto process = entt::ProcessAdaptor<decltype(lambda), uint64_t>{lambda};

    process.tick(0);

    ASSERT_TRUE(process.dead());
    ASSERT_TRUE(updated);
}

TEST(ProcessAdaptor, Rejected) {
    bool updated = false;
    auto lambda = [&updated](uint64_t, auto, auto rejected) {
        ASSERT_FALSE(updated);
        updated = true;
        rejected();
    };

    auto process = entt::ProcessAdaptor<decltype(lambda), uint64_t>{lambda};

    process.tick(0);

    ASSERT_TRUE(process.rejected());
    ASSERT_TRUE(updated);
}
