#include <iostream>
#include <string>
#include <iomanip>
#include "json.hpp"
#include "JsonRepository.h"

// ── User entity ───────────────────────────────────────────────────────────────

struct User {
    int64_t     id    = 0;
    std::string name;
    std::string email;
    int         age   = 0;

    int64_t getId()        const { return id; }
    void    setId(int64_t v)     { id = v; }

    JsonValue toJson() const {
        JsonValue j;
        j["id"]    = JsonValue(id);
        j["name"]  = JsonValue(name);
        j["email"] = JsonValue(email);
        j["age"]   = JsonValue(age);
        return j;
    }

    static User fromJson(const JsonValue& j) {
        return {
            j.at("id").getInt(),
            j.at("name").getString(),
            j.at("email").getString(),
            static_cast<int>(j.at("age").getInt())
        };
    }

    void print() const {
        std::cout << "  [" << std::setw(2) << id << "] "
                  << std::left << std::setw(10) << name
                  << " | " << std::setw(25) << email
                  << " | age=" << age << '\n';
    }
};

// ── Product entity ────────────────────────────────────────────────────────────

struct Product {
    int64_t     id    = 0;
    std::string name;
    double      price = 0.0;
    int         stock = 0;

    int64_t getId()        const { return id; }
    void    setId(int64_t v)     { id = v; }

    JsonValue toJson() const {
        JsonValue j;
        j["id"]    = JsonValue(id);
        j["name"]  = JsonValue(name);
        j["price"] = JsonValue(price);
        j["stock"] = JsonValue(stock);
        return j;
    }

    static Product fromJson(const JsonValue& j) {
        return {
            j.at("id").getInt(),
            j.at("name").getString(),
            j.at("price").getNumber(),
            static_cast<int>(j.at("stock").getInt())
        };
    }

    void print() const {
        std::cout << "  [" << std::setw(2) << id << "] "
                  << std::left << std::setw(12) << name
                  << " | $" << std::fixed << std::setprecision(2) << price
                  << " | stock=" << stock << '\n';
    }
};

// ── Helpers ───────────────────────────────────────────────────────────────────

template<typename T>
void printAll(const JsonRepository<T>& repo, std::string_view label) {
    std::cout << "\n  -- " << label << " (" << repo.count() << ") --\n";
    for (const auto& e : repo.readAll())
        e.print();
}

static void separator(std::string_view title) {
    std::cout << "\n============================================================\n";
    std::cout << "  " << title << '\n';
    std::cout << "============================================================\n";
}

// ── User CRUD demo ────────────────────────────────────────────────────────────

void userDemo() {
    separator("USER CRUD DEMO  ->  users.json");

    JsonRepository<User> repo("users.json");

    // CREATE
    std::cout << "\n[CREATE]\n";
    int64_t id1 = repo.create({0, "Alice",   "alice@example.com",   30});
    int64_t id2 = repo.create({0, "Bob",     "bob@example.com",     25});
    int64_t id3 = repo.create({0, "Charlie", "charlie@example.com", 35});
    std::cout << "  Created ids: " << id1 << ", " << id2 << ", " << id3 << '\n';
    printAll(repo, "All Users");

    // READ by id
    std::cout << "\n[READ by id=" << id1 << "]\n";
    if (auto u = repo.readById(id1))
        u->print();
    else
        std::cout << "  Not found.\n";

    // READ with predicate
    std::cout << "\n[READ WHERE age >= 30]\n";
    for (const auto& u : repo.readWhere([](const User& u) { return u.age >= 30; }))
        u.print();

    // UPDATE
    std::cout << "\n[UPDATE id=" << id2 << "  - new email & age]\n";
    if (auto opt = repo.readById(id2)) {
        User updated = *opt;
        updated.email = "bob.updated@example.com";
        updated.age   = 26;
        repo.update(updated);
    }
    printAll(repo, "After Update");

    // DELETE
    std::cout << "\n[DELETE id=" << id3 << "]\n";
    repo.remove(id3);
    printAll(repo, "After Delete");
}

// ── Product CRUD demo ─────────────────────────────────────────────────────────

void productDemo() {
    separator("PRODUCT CRUD DEMO  ->  products.json");

    JsonRepository<Product> repo("products.json");

    // CREATE
    std::cout << "\n[CREATE]\n";
    repo.create({0, "Keyboard", 79.99,  50});
    repo.create({0, "Mouse",    29.99, 120});
    repo.create({0, "Monitor", 299.99,  15});
    repo.create({0, "Webcam",   49.99,   0});
    printAll(repo, "All Products");

    // READ out-of-stock
    std::cout << "\n[READ WHERE stock == 0]\n";
    for (const auto& p : repo.readWhere([](const Product& p) { return p.stock == 0; }))
        p.print();

    // UPDATE - restock Keyboard
    std::cout << "\n[UPDATE - restock Keyboard: 50 -> 45]\n";
    for (auto p : repo.readWhere([](const Product& p) { return p.name == "Keyboard"; })) {
        p.stock = 45;
        repo.update(p);
    }
    printAll(repo, "After Restock");

    // DELETE - remove out-of-stock items
    std::cout << "\n[DELETE WHERE stock == 0]\n";
    for (const auto& p : repo.readWhere([](const Product& p) { return p.stock == 0; }))
        repo.remove(p.id);
    printAll(repo, "After Cleanup");
}

// ── JSON parse / stringify demo ───────────────────────────────────────────────

void jsonDemo() {
    separator("JSON PARSE / STRINGIFY DEMO");

    const std::string raw = R"({
  "project": "DataPersistence",
  "version": 1,
  "active": true,
  "tags": ["cpp", "json", "crud"],
  "config": { "indent": 2, "encoding": "utf-8" }
})";

    std::cout << "\n[Parse string]\n";
    JsonValue doc = JsonValue::parse(raw);

    std::cout << "  project : " << doc.at("project").getString() << '\n';
    std::cout << "  version : " << doc.at("version").getInt()    << '\n';
    std::cout << "  active  : " << (doc.at("active").getBool() ? "true" : "false") << '\n';
    std::cout << "  tags[0] : " << doc.at("tags")[size_t(0)].getString() << '\n';
    std::cout << "  indent  : " << doc.at("config").at("indent").getInt() << '\n';

    std::cout << "\n[Compact stringify]\n  " << doc.stringify() << '\n';
    std::cout << "\n[Pretty stringify (indent=2)]\n" << doc.stringify(2) << '\n';
}

// ── Entry point ───────────────────────────────────────────────────────────────

int main() {
    jsonDemo();
    userDemo();
    productDemo();

    std::cout << "\n[Files saved]\n"
              << "  users.json    - User records\n"
              << "  products.json - Product records\n";

    return 0;
}
