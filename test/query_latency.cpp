#include <algorithm>
#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <ctime>

#include "utils/log.h"
#include "utils/timer.h"

#include "lib/treap/query.hpp"
#include "lib/treap/build.hpp"
#include "lib/pam/interface.hpp"

struct query_context {
  size_t ver;
  uint64_t l;
  uint64_t r;
};

uint64_t test_perversion(size_t n, size_t q, uint64_t* elems, query_context* queries) {
  treap::node_ptr t = treap::build_parallel(n, elems);

  Timer tmr;
  tmr.Start();
  uint64_t total = 0;

  for (size_t i = 0; i < q; i++) {
    total += treap::range_estimate(t, queries[i].l, queries[i].r); }

  double duration = tmr.End();
  log_info("avarage query time for seq and con: %lf us", duration * 1000 * 1000 / q);

  return total;
}

struct alignas(64) list_node {
  uint64_t keys[64];
  list_node* next;
};

uint64_t test_scan(size_t n, size_t q, uint64_t* elems, query_context* queries) {
  list_node** list = new list_node*[(n+63)/64];
  for (size_t i = 0; i < (n+63) / 64; i++) {
    list[i] = new list_node{{}, nullptr};
    size_t j = rand() % (i+1);
    std::swap(list[i], list[j]); }

  std::sort(elems, elems+n);

  for (size_t i = 0; i < n; i++) { list[i/64]->keys[i%64] = elems[i]; }
  for (size_t i = 1; i < (n+63) / 64; i++) { list[i-1]->next = list[i]; }

  Timer tmr;
  tmr.Start();

  uint64_t total = 0;

  for (size_t i = 0; i < q; i++) {
    size_t j = std::lower_bound(elems, elems+n, queries[i].l) - elems;
    if (j >= n) { continue; }
    list_node* it = list[j/64];
    while (j < n && it->keys[j%64] <= queries[i].r) {
      total += it->keys[j%64];
      j++;
      if (j % 64 == 0) { it = it->next; } } }

  double duration = tmr.End();
  log_info("avarage query time for scan: %lf us", duration * 1000 * 1000 / q);
  return total;
}

uint64_t test_batch(size_t n, size_t m, size_t b, size_t q, uint64_t* elems, query_context* queries) {
  pam::interface::node_ptr* roots = new pam::interface::node_ptr[m+1];
  roots[0] = pam::interface::build(n, elems);

  for (size_t i = 0; i < m; i++) {
    roots[i+1] = pam::interface::inserted(roots[i], i*b+1, b, elems+n+i*b); }

  Timer tmr;
  tmr.Start();
  uint64_t total = 0;

  for (size_t i = 0; i < q; i++) {
    size_t bid = (queries[i].ver + b - 1) / b;
    total += pam::interface::range_estimate(roots[bid], queries[i].ver, queries[i].l, queries[i].r); }

  double duration = tmr.End();
  log_info("avarage query time for batch size %zu: %lf us", b, duration * 1000 * 1000 / q);

  return total;
}


int main() {
  size_t n = 100000000;
  size_t w = 10000000;
  size_t m, b, q;
  scanf("%zu%zu", &b, &q);
  m = 10000000/b;

  srand(time(0));

  uint64_t* elems = new uint64_t[n+m*b];
  for (size_t i = 0; i < n+m*b; i++) {
    elems[i] = i;
    size_t j = rand() % (i+1);
    std::swap(elems[i], elems[j]); }

  query_context* queries = new query_context[q];
  uint64_t total_size = 0, total_get;

  for (size_t i = 0; i < q; i++) {
    queries[i].ver = rand() % (m*b+1);
    size_t len = rand() % w;
    queries[i].l = rand() % (n+m*b-len);
    queries[i].r = queries[i].l + len;
    total_size += len + 1; }

  if (b == 0) { total_get = test_perversion(n, q, elems, queries); }
  if (b == 1) { total_get = test_scan(n+m, q, elems, queries); }
  else { total_get = test_batch(n, m, b, q, elems, queries); }

  log_info("%lu", total_get);

  return 0;
}
