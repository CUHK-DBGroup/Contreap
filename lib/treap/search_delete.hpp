#pragma once

#include <cstddef>
#include <cstdint>

#include "node.hpp"

namespace treap {

node_ptr process_concat(size_t ver, node_ptr t_l, node_ptr t_r) {
  if (t_l == nullptr) { return t_r; }
  if (t_r == nullptr) { return t_l; }
  node_ptr t_new;
  if (t_l->pry > t_r->pry) {
    t_new = make_weak_copy(t_l, ver);
    t_new->lch = t_l->lch;
    t_new->rch = process_concat(ver, t_l->rch, t_r); }
  else /* t_l->pry < t_r->pry */ {
    t_new = make_weak_copy(t_r, ver);
    t_new->rch = t_r->rch;
    t_new->lch = process_concat(ver, t_l, t_r->lch); }
  return t_new;
}

node_ptr search_delete(size_t ver, node_ptr t_past, uint64_t pry, uint64_t key) {
  if (t_past == nullptr) { return nullptr; }
  else if (t_past->pry > pry) {
    node_ptr t_new = make_weak_copy(t_past, ver);
    if (key < t_past->key) {
      t_new->rch = t_past->rch;
      t_new->lch = search_delete(ver, t_past->lch, pry, key); }
    else /* key > t_past->key */ {
      t_new->lch = t_past->lch;
      t_new->rch = search_delete(ver, t_past->rch, pry, key); }
    return t_new; }
  else if (t_past->key == key) {
    if (t_past->val == 1) { return process_concat(ver, t_past->lch, t_past->rch); }
    else {
      node_ptr t_new = make_weak_copy(t_past, ver);
      t_new->val--;
      t_new->lch = t_past->lch;
      t_new->rch = t_past->rch;
      return t_new; } }
  else /* t_past->pry < pry */ { return t_past; }
}

}
