#include <algorithm>
#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <random>

#include "utils/log.h"
#include "utils/timer.h"

#include "ycsbc/core/scrambled_zipfian_generator.h"

#include "lib/treap/augment.hpp"
#include "lib/treap/build.hpp"
#include "lib/treap/search_delete.hpp"
#include "lib/treap/search_insert.hpp"
#include "lib/treap/node.hpp"
#include "lib/treap/query.hpp"
#include "lib/treap/scheduler.hpp"

#include "lib/pam/interface.hpp"

struct query_context {
  size_t ver;
  uint64_t l;
  uint64_t r;
};

constexpr size_t num_threads = 64;
constexpr size_t block_size = 10000;

uint64_t test_contreap(size_t n, size_t m, size_t q, uint64_t* records, query_context* queries) {
  treap::node_ptr t = treap::build_parallel(n, records);
  treap::scheduler* contreap = new treap::scheduler(num_threads, m, 10000, t);

  Timer tmr;
  tmr.Start();

  for (size_t i = 0; i < m; i++) { contreap->insert_elem(records[n+i]); }

  contreap->process();

  double duration = tmr.End();
  log_info("Avarage Update Time : %lf(us)", duration * 1000 * 1000 / m);
  log_info("Thoughput : %lf(ktps)", m / duration / 1000);

  tmr.Start();
  uint64_t tot = 0;

  for (size_t i = 0; i < q; i++) {
    tot += treap::range_estimate(contreap->get_snapshot(queries[i].ver), queries[i].l, queries[i].r); }

  duration = tmr.End();
  log_info("Avarage Query Time : %lf(us)", duration * 1000 * 1000 / q);
  return tot;
}

uint64_t test_seq(size_t n, size_t m, size_t q, uint64_t* records, query_context* queries) {
  treap::node_ptr* t = new treap::node_ptr[m+1];

  t[0] = treap::build_parallel(n, records);

  Timer tmr;
  tmr.Start();

  for (size_t i = 0; i < m; i++) { t[i+1] = treap::search_insert(i+1, t[i], aes_hash::hash(records[i]), records[i]); }

  double duration = tmr.End();
  log_info("Avarage Update Time : %lf(us)", duration * 1000 * 1000 / m);
  log_info("Thoughput : %lf(ktps)", m / duration / 1000);

  tmr.Start();
  uint64_t tot = 0;

  for (size_t i = 0; i < q; i++) {
    tot += treap::range_estimate(t[queries[i].ver], queries[i].l, queries[i].r); }

  duration = tmr.End();
  log_info("Avarage Query Time : %lf(us)", duration * 1000 * 1000 / q);
  return tot;
}

uint64_t test_pam(size_t n, size_t m, size_t b, size_t q, uint64_t* records, query_context* queries) {
  pam::interface::node_ptr* roots = new pam::interface::node_ptr[(m+b-1)/b+1];
  roots[0] = pam::interface::build(n, records);

  Timer tmr;
  tmr.Start();

  for (size_t i = 0; i < m/b; i++) {
    roots[i+1] = pam::interface::inserted(roots[i], i*b+1, b, records+n+i*b); }

  if (m % b != 0) {
    roots[m/b+1] = pam::interface::inserted(roots[m/b], (m/b)*b+1, m%b, records+n+(m/b)*b); }

  double duration = tmr.End();
  log_info("Avarage Update Time : %lf(us)", duration * 1000 * 1000 / m);
  log_info("Thoughput : %lf(ktps)", m / duration / 1000);

  tmr.Start();
  uint64_t tot = 0;

  for (size_t i = 0; i < q; i++) {
    size_t bid = (queries[i].ver + b - 1) / b;
    tot += pam::interface::range_estimate(roots[bid], queries[i].ver, queries[i].l, queries[i].r); }

  duration = tmr.End();
  log_info("Avarage Query Time : %lf(us)", duration * 1000 * 1000 / q);
  return tot;
}

struct alignas(64) list_node {
  uint64_t keys[64];
  list_node* next;
};

uint64_t test_scan(size_t k, size_t q, uint64_t* records, query_context* queries) {
  list_node** list = new list_node*[(k+63)/64];
  for (size_t i = 0; i < (k+63) / 64; i++) {
    list[i] = new list_node{{}, nullptr};
    size_t j = rand() % (i+1);
    std::swap(list[i], list[j]); }

  std::sort(records, records+k);

  for (size_t i = 0; i < k; i++) { list[i/64]->keys[i%64] = records[i]; }
  for (size_t i = 1; i < (k+63) / 64; i++) { list[i-1]->next = list[i]; }

  Timer tmr;
  tmr.Start();

  uint64_t tot = 0;

  for (size_t i = 0; i < q; i++) {
    size_t j = std::lower_bound(records, records+k, queries[i].l) - records;
    if (j >= k) { continue; }
    list_node* it = list[j/64];
    while (j < k && it->keys[j%64] <= queries[i].r) {
      tot += it->keys[j%64];
      j++;
      if (j % 64 == 0) { it = it->next; } } }

  double duration = tmr.End();
  log_info("Avarage Query Time : %lf(us)", duration * 1000 * 1000 / q);

  // std::sort(records, records+k);

  // Timer tmr;
  // tmr.Start();

  // uint64_t tot = 0;

  // for (size_t i = 0; i < q; i++) {
  //   size_t j = std::lower_bound(records, records+k, queries[i].l) - records;
  //   while (j < k && records[j] <= queries[i].r) { tot++; j++; } }

  // double duration = tmr.End();
  // log_info("Avarage Query Time : %lf(us)", duration * 1000 * 1000 / q);
  return tot;
}

int main() {
  size_t k = 100000000;  // num of elements
  size_t n = 100000000; // initial updates
  size_t m = 10000000;  // following updates
  size_t b; // batch size, 0 for contreap, 1 for seq
  size_t q; // num queries, 0 for scan test
  bool is_scan = false;
  scanf("%zu%zu", &b, &q);

  log_info("generating prices...");

  std::random_device rd{};
  std::mt19937 gen{rd()};
  std::normal_distribution d{1.0, 0.20};
  uint64_t* prices = new uint64_t[k];
  for (size_t i = 0; i < k; i++) {
    double w;
    do { w = d(gen); } while (w <= 0);
    prices[i] = ceil(k * w); }

  if (q == 0) { q = b; is_scan = true; }

  log_info("generating transactions...");

  auto upd_keygen = ycsbc::ScrambledZipfianGenerator(k);
  uint64_t* records = new uint64_t[n+m];

  for (size_t i = 0; i < n+m; i++) { records[i] = prices[upd_keygen.Next()]; }

  log_info("generating queries...");

  query_context* queries = new query_context[q];

  std::uniform_int_distribution<size_t> unid{0, m};

  for (size_t i = 0; i < q; i++) {
    queries[i].ver = unid(gen);
    double w;
    do { w = d(gen); } while (w <= 0);
    queries[i].l = ceil(k * w);
    do { w = d(gen); } while (w <= 0);
    queries[i].r = ceil(k * w);
    if (queries[i].l > queries[i].r) { std::swap(queries[i].l, queries[i].r); } }

  log_info("processing...");

  if (is_scan) { test_scan(k, q, prices, queries); }
  else if (b == 0) { test_contreap(n, m, q, records, queries); }
  else if (b == 1) { test_seq(n, m, q, records, queries); }
  else { test_pam(n, m, b, q, records, queries); }

  return 0;
}
