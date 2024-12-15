
#pragma once

#include <cstddef>

#include "rt_protected.hpp"

namespace ptr_reflect::details {

template <typename K, typename V> class UnorderedMap {
  struct Node {
    K key;
    V value;
    __RT_PROTECT Node *next;
    __RT_PROTECT Node(const K &k, const V &v) : key(k), value(v), next(nullptr) {}
    __RT_PROTECT Node(const Node &) = delete;
    __RT_PROTECT Node &operator=(const Node &) = delete;
  };

  struct Bucket {
    Node *head;
    __RT_PROTECT Bucket() : head(nullptr) {}
  };

  using HashFunc = size_t (*)(const K &);

  static constexpr size_t MAX_BUCKETS = 1ULL << 30;

  size_t _minBucketCount, _bucketCount, _size;
  float _loadFactor;
  HashFunc _hashFn;
  Bucket *_buckets;

  __RT_PROTECT void rehash(size_t count) {
    count = (count < MAX_BUCKETS) ? count : MAX_BUCKETS;
    auto *newBuckets = static_cast<Bucket *>(__RT_ALTERNATIVE(malloc)(count * sizeof(Bucket)));
    for (size_t i = 0; i < count; ++i)
      new (&newBuckets[i]) Bucket();
    for (size_t i = 0; i < _bucketCount; ++i) {
      Node *current = _buckets[i].head;
      while (current) {
        Node *next = current->next;
        size_t idx = _hashFn(current->key) % count;
        current->next = newBuckets[idx].head;
        newBuckets[idx].head = current;
        current = next;
      }
    }
    __RT_ALTERNATIVE(free)(_buckets);
    _buckets = newBuckets;
    _bucketCount = count;
  }

public:
  __RT_PROTECT explicit UnorderedMap(const HashFunc hash_func, const float loadFactor = 0.75f, const size_t initialBucketCount = 1024)
      : _minBucketCount(initialBucketCount), _bucketCount(initialBucketCount), _size(0), _loadFactor(loadFactor), _hashFn(hash_func),
        _buckets(static_cast<Bucket *>(__RT_ALTERNATIVE(malloc)(_bucketCount * sizeof(Bucket)))) {
    for (size_t i = 0; i < _bucketCount; ++i)
      new (&_buckets[i]) Bucket();
  }

  __RT_PROTECT bool emplace(const K &key, const V &value) {
    if (_size >= _bucketCount * _loadFactor) {
      auto count = _bucketCount * 5 / 2;
      rehash((count > _minBucketCount) ? count : _minBucketCount);
    }
    const size_t idx = _hashFn(key) % _bucketCount;
    for (const Node *node = _buckets[idx].head; node; node = node->next) {
      if (node->key == key) return false;
    }

    Node *newNode = static_cast<Node *>(__RT_ALTERNATIVE(malloc)(sizeof(Node)));
    new (newNode) Node(key, value);
    newNode->next = _buckets[idx].head;
    _buckets[idx].head = newNode;
    ++_size;
    return true;
  }

  __RT_PROTECT [[nodiscard]] V *find(const K &key) {
    const size_t idx = _hashFn(key) % _bucketCount;
    for (Node *node = _buckets[idx].head; node; node = node->next) {
      if (node->key == key) return &node->value;
    }
    return nullptr;
  }

  template <typename F> __RT_PROTECT void walk(F f) {
    for (size_t idx = 0; idx < _bucketCount; ++idx) {
      for (Node *node = _buckets[idx].head; node; node = node->next) {
        if (f(node->key, &node->value)) return;
      }
    }
  }

  __RT_PROTECT bool erase(const K &key) {
    const size_t idx = _hashFn(key) % _bucketCount;
    Node *current = _buckets[idx].head;
    Node *prev = nullptr;
    while (current) {
      if (current->key == key) {
        if (prev) prev->next = current->next;
        else _buckets[idx].head = current->next;
        current->~Node();
        __RT_ALTERNATIVE(free)(current);
        --_size;
        return true;
      }
      prev = current;
      current = current->next;
    }
    return false;
  }

  __RT_PROTECT void clear() {
    for (size_t i = 0; i < _bucketCount; ++i) {
      Node *current = _buckets[i].head;
      while (current) {
        Node *next = current->next;
        current->~Node();
        __RT_ALTERNATIVE(free)(current);
        current = next;
      }
      _buckets[i].head = nullptr;
    }
    _size = 0;
  }

  __RT_PROTECT ~UnorderedMap() {
    clear();
    __RT_ALTERNATIVE(free)(_buckets);
  }

  __RT_PROTECT [[nodiscard]] size_t size() const { return _size; }
  __RT_PROTECT [[nodiscard]] size_t bucket_count() const { return _bucketCount; }

  __RT_PROTECT UnorderedMap(const UnorderedMap &) = delete;
  __RT_PROTECT UnorderedMap &operator=(const UnorderedMap &) = delete;
};
} // namespace ptr_reflect::details