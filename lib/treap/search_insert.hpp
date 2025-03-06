#pragma once

#include <cstddef>
#include <cstdint>
#include <tuple>

#include "node.hpp"

namespace treap {

std::tuple<node_ptr, node_ptr> split_copy(size_t ver, node_ptr t, uint64_t key) {
  node_ptr t_l, t_r;
  if (t == nullptr) { return std::make_tuple(nullptr, nullptr); }
  else if (key < t->key) {
    t_r = make_weak_copy(t, ver);
    t_r->rch = t->rch;
    std::tie(t_l, t_r->lch) = split_copy(ver, t->lch, key); }
  else /* key > t->key */ {
    t_l = make_weak_copy(t, ver);
    t_l->lch = t->lch;
    std::tie(t_l->rch, t_r) = split_copy(ver, t->rch, key); }
  return std::make_tuple(t_l, t_r);
}

static inline node_ptr process_deploy(size_t ver, node_ptr t_past, uint64_t pry, uint64_t key) {
  node_ptr t_new = make_node(ver, pry, key);
  std::tie(t_new->lch, t_new->rch) = split_copy(ver, t_past, key);
  return t_new;
}

node_ptr search_insert(size_t ver, node_ptr t_past, uint64_t pry, uint64_t key) {
  if (t_past == nullptr) { return make_node(ver, pry, key); }
  else if (t_past->pry > pry) {
    node_ptr t_new = make_weak_copy(t_past, ver);
    if (key < t_past->key) {
      t_new->rch = t_past->rch;
      t_new->lch = search_insert(ver, t_past->lch, pry, key); }
    else /* key > t_past->key */ {
      t_new->lch = t_past->lch;
      t_new->rch = search_insert(ver, t_past->rch, pry, key); }
    return t_new; }
  else if (t_past->key == key) {
    node_ptr t_new = make_weak_copy(t_past, ver);
    t_new->val++;
    t_new->lch = t_past->lch;
    t_new->rch = t_past->rch;
    return t_new; }
  else /* t_past->pry < pry */ { return process_deploy(ver, t_past, pry, key); }
}

}
