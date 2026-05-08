#pragma once

#include "json.hpp"
#include <string>
#include <map>
#include <vector>
#include <optional>
#include <functional>

// Generic JSON-backed CRUD repository.
//
// T must satisfy:
//   int64_t        T::getId() const
//   void           T::setId(int64_t)
//   JsonValue      T::toJson() const
//   static T       T::fromJson(const JsonValue&)
//
// Data is persisted to a JSON file with this layout:
//   { "nextId": N, "entities": [ {...}, ... ] }

template<typename T>
class JsonRepository {
public:
    explicit JsonRepository(std::string filePath)
        : filePath_(std::move(filePath))
    {
        load();
    }

    // ── Create ────────────────────────────────────────────────────────────────

    // Assigns a new auto-incremented id, stores the entity, and persists.
    // Returns the assigned id.
    int64_t create(T entity) {
        int64_t id = nextId_++;
        entity.setId(id);
        store_[id] = std::move(entity);
        save();
        return id;
    }

    // ── Read ──────────────────────────────────────────────────────────────────

    std::vector<T> readAll() const {
        std::vector<T> result;
        result.reserve(store_.size());
        for (const auto& [id, e] : store_)
            result.push_back(e);
        return result;
    }

    std::optional<T> readById(int64_t id) const {
        auto it = store_.find(id);
        if (it == store_.end()) return std::nullopt;
        return it->second;
    }

    std::vector<T> readWhere(std::function<bool(const T&)> pred) const {
        std::vector<T> result;
        for (const auto& [id, e] : store_)
            if (pred(e)) result.push_back(e);
        return result;
    }

    // ── Update ────────────────────────────────────────────────────────────────

    // Replaces the entity with the same id. Returns false if not found.
    bool update(const T& entity) {
        auto it = store_.find(entity.getId());
        if (it == store_.end()) return false;
        it->second = entity;
        save();
        return true;
    }

    // ── Delete ────────────────────────────────────────────────────────────────

    bool remove(int64_t id) {
        if (!store_.erase(id)) return false;
        save();
        return true;
    }

    size_t count() const { return store_.size(); }

private:
    std::string          filePath_;
    std::map<int64_t, T> store_;
    int64_t              nextId_ = 1;

    void load() {
        JsonValue root = JsonValue::parseFile(filePath_);
        if (root.isNull()) return;

        if (root.contains("nextId"))
            nextId_ = root.at("nextId").getInt();

        if (root.contains("entities") && root.at("entities").isArray()) {
            for (const auto& item : root.at("entities").getArray()) {
                T entity = T::fromJson(item);
                store_[entity.getId()] = std::move(entity);
            }
        }
    }

    void save() const {
        JsonValue root;
        root["nextId"] = JsonValue(nextId_);

        JsonValue arr;
        for (const auto& [id, e] : store_)
            arr.push_back(e.toJson());
        root["entities"] = arr;

        root.saveToFile(filePath_);
    }
};
