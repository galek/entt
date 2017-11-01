#ifndef ENTT_RESOURCE_RESOURCE_HPP
#define ENTT_RESOURCE_RESOURCE_HPP


#include <memory>
#include <utility>
#include <cassert>
#include <type_traits>
#include <unordered_map>
#include "../core/hashed_string.hpp"


namespace entt {


template<typename Resource>
struct ResourceCache;


/**
 * @brief Base class for resource loaders.
 *
 * Resource loaders must inherit from this class and stay true to the CRTP
 * idiom. Moreover, a resource loader must expose a public, const member
 * function named `load` that accepts a variable number of arguments and return
 * a shared pointer to the resource just created.<br/>
 * As an example:
 *
 * @code{.cpp}
 * struct MyResource {};
 *
 * struct MyLoader: entt::ResourceLoader<MyLoader, MyResource> {
 *     std::shared_ptr<MyResource> load(int) const {
 *         // use the integer value somehow
 *         return std::make_shared<MyResource>();
 *     }
 * };
 * @endcode
 *
 * In general, resource loaders should not have a state or retain data of any
 * type. They should let the cache manage their resources instead.
 *
 * @note
 * Base class and CRTP idiom aren't strictly required with the current
 * implementation. One could argue that a cache can easily work with loaders of
 * any type. However, future changes won't be breaking ones by forcing the use
 * of a base class today and that's why the model is already in its place.
 *
 * @tparam Loader Type of the derived class.
 * @tparam Resource Type of resource for which to use the loader.
 */
template<typename Loader, typename Resource>
class ResourceLoader {
    /*! Resource loaders are friends of their caches. */
    friend class ResourceCache<Resource>;

    template<typename... Args>
    std::shared_ptr<Resource> get(Args&&... args) const {
        return static_cast<const Loader *>(this)->load(std::forward<Args>(args)...);
    }

public:
    /*! @brief Default destructor. */
    virtual ~ResourceLoader() = default;
};


/**
 * @brief Shared resource handle.
 *
 * A shared resource handle is a small class that wraps a resource and keeps it
 * alive even if it's deleted from the cache. It can be either copied or
 * moved. A handle shares a reference to the same resource with all the other
 * handles constructed for the same identifier.<br/>
 * As a rule of thumb, resources should never be copied nor moved. Handles are
 * the way to go to keep references to them.
 *
 * @tparam Resource Type of resource managed by a handle.
 */
template<typename Resource>
class ResourceHandle final {
    /*! Resource handles are friends of their caches. */
    friend class ResourceCache<Resource>;

    ResourceHandle(std::shared_ptr<Resource> res) noexcept
        : resource{std::move(res)}
    {}

public:
    /*! @brief Default copy constructor. */
    ResourceHandle(const ResourceHandle &) noexcept = default;
    /*! @brief Default move constructor. */
    ResourceHandle(ResourceHandle &&) noexcept = default;

    /*! @brief Default copy assignment operator. @return This handle. */
    ResourceHandle & operator=(const ResourceHandle &) noexcept = default;
    /*! @brief Default move assignment operator. @return This handle. */
    ResourceHandle & operator=(ResourceHandle &&) noexcept = default;

    /**
     * @brief Gets a reference to the managed resource.
     *
     * @warning
     * The behavior is undefined if the handle doesn't contain a resource.<br/>
     * An assertion will abort the execution at runtime in debug mode if the
     * handle is empty.
     *
     * @return A reference to the managed resource.
     */
    const Resource & get() const noexcept {
        assert(static_cast<bool>(resource));
        return *resource;
    }

    /**
     * @brief Casts a handle and gets a reference to the managed resource.
     *
     * @warning
     * The behavior is undefined if the handle doesn't contain a resource.<br/>
     * An assertion will abort the execution at runtime in debug mode if the
     * handle is empty.
     */
    inline operator const Resource & () const noexcept { return get(); }

    /**
     * @brief Dereferences a handle to obtain the managed resource.
     *
     * @warning
     * The behavior is undefined if the handle doesn't contain a resource.<br/>
     * An assertion will abort the execution at runtime in debug mode if the
     * handle is empty.
     *
     * @return A reference to the managed resource.
     */
    inline const Resource & operator *() const noexcept { return get(); }

    /**
     * @brief Gets a pointer to the managed resource from a handle .
     *
     * @warning
     * The behavior is undefined if the handle doesn't contain a resource.<br/>
     * An assertion will abort the execution at runtime in debug mode if the
     * handle is empty.
     *
     * @return A pointer to the managed resource or `nullptr` if the handle
     * contains no resource at all.
     */
    inline const Resource * operator ->() const noexcept {
        assert(static_cast<bool>(resource));
        return resource.get();
    }

    /**
     * @brief Returns true if the handle contains a resource, false otherwise.
     */
    explicit operator bool() const { return static_cast<bool>(resource); }

private:
    std::shared_ptr<Resource> resource;
};


/**
 * @brief Simple cache for resources of a given type.
 *
 * Minimal implementation of a cache for resources of a given type. It doesn't
 * offer much functionalities but it's suitable for small or medium sized
 * applications and can be freely inherited to add targeted functionalities for
 * large sized applications.
 *
 * @tparam Resource Type of resources managed by a cache.
 */
template<typename Resource>
struct ResourceCache {
    /*! @brief Type of resources managed by a cache. */
    using resource_type = HashedString;

    /**
     * @brief Loads the resource that corresponds to the given identifier.
     *
     * In case an identifier isn't already present in the cache, it loads its
     * resource and stores it aside for future uses. Arguments are forwarded
     * directly to the loader in order to construct properly the requested
     * resource.
     *
     * @note
     * If the identifier is already present in the cache, this function does
     * nothing and the arguments are simply discarded.
     *
     * @tparam Loader Type of loader to use to load the resource if required.
     * @tparam Args Types of arguments to use to load the resource if required.
     * @param id Unique resource identifier.
     * @param args Arguments to use to load the resource if required.
     * @return True if the resource is ready to use, false otherwise.
     */
    template<typename Loader, typename... Args>
    bool load(resource_type id, Args&&... args) {
        static_assert(std::is_base_of<ResourceLoader<Loader, Resource>, Loader>::value, "!");

        bool loaded = true;

        if(resources.find(id) == resources.cend()) {
            std::shared_ptr<Resource> resource = Loader{}.get(std::forward<Args>(args)...);
            loaded = (static_cast<bool>(resource) ? (resources[id] = std::move(resource), loaded) : false);
        }

        return loaded;
    }

    /**
     * @brief Reloads a resource or loads it for the first time if not present.
     *
     * Equivalent to the following snippet (pseudocode):
     *
     * @code{.cpp}
     * cache.discard(id);
     * cache.load(id, args...);
     * @endcode
     *
     * Arguments are forwarded directly to the loader in order to construct
     * properly the requested resource.
     *
     * @tparam Loader Type of loader to use to load the resource.
     * @tparam Args Types of arguments to use to load the resource.
     * @param id Unique resource identifier.
     * @param args Arguments to use to load the resource.
     * @return True if the resource is ready to use, false otherwise.
     */
    template<typename Loader, typename... Args>
    void reload(resource_type id, Args&&... args) {
        return (discard(id), load(id, std::forward<Args>(args)...));
    }

    /**
     * @brief Creates a handle for the given resource identifier.
     *
     * A resource handle can be in a either valid or invalid state. In other
     * terms, a resource handle is properly initialized with a resource if the
     * cache contains the resource itself. Otherwise the returned handle is
     * uninitialized and accessing it results in undefined behavior.
     *
     * @param id Unique resource identifier.
     * @return A handle for the given resource.
     */
    ResourceHandle<Resource> handle(resource_type id) const {
        auto it = resources.find(id);
        return { it == resources.end() ? nullptr : it->second };
    }

    /**
     * @brief Returns true if a cache contains no resources, false otherwise.
     * @return True if the cache contains no resources, false otherwise.
     */
    bool empty() const noexcept {
        return resources.empty();
    }

    /**
     * @brief Checks if a cache contains the given identifier.
     * @param id Unique resource identifier.
     * @return True if the cache contains the resource, false otherwise.
     */
    bool contains(resource_type id) const noexcept {
        return !(resources.find(id) == resources.cend());
    }

    /**
     * @brief Discards the resource that corresponds to the given identifier.
     *
     * Handles are not invalidated and the memory used by the resource isn't
     * freed as long as at least a handle keeps the resource itself alive.
     *
     * @param id Unique resource identifier.
     */
    void discard(resource_type id) noexcept {
        auto it = resources.find(id);

        if(it != resources.end()) {
            resources.erase(it);
        }
    }

    /**
     * @brief Clears a cache and discards all its resources.
     *
     * Handles are not invalidated and the memory used by a resource isn't
     * freed as long as at least a handle keeps the resource itself alive.
     */
    void clear() noexcept {
        resources.clear();
    }

private:
    std::unordered_map<
        resource_type::hash_type,
        std::shared_ptr<Resource>
    > resources;
};


}


#endif // ENTT_RESOURCE_RESOURCE_HPP
