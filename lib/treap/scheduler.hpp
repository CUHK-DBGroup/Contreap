#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <tuple>
#include <type_traits>

#include <mimalloc.h>
#include <mimalloc-override.h>
#include <mimalloc-new-delete.h>

#include "utils/log.h"
#include "augment.hpp"
#include "node.hpp"
#include "search_delete.hpp"
#include "search_insert.hpp"

namespace treap {

struct scheduler {
private:
  enum status     : uint8_t { ST_RUNNING, ST_DONE };
  enum operation  : uint8_t { OP_NONE, OP_INSERT, OP_DELETE };
  enum functor    : uint8_t { FN_SEARCH, FN_SETREF, FN_CONCAT };
  enum direction  : uint8_t { DIR_NONE, DIR_LEFT, DIR_RIGHT };

  static constexpr size_t STAGE_DONE = ~size_t{0};

  struct alignas(128) context {
    status st = ST_RUNNING;
    operation op = OP_NONE;
    functor fn = FN_SEARCH;
    direction dir = DIR_NONE;
    node_ptr t_cur = nullptr;
    node_ptr t_past = nullptr;
    size_t ver = 0;
    uint64_t pry = 0;
    uint64_t key = 0;
    node_ptr t_l = nullptr;
    node_ptr t_r = nullptr;
    alignas(64) std::atomic_size_t stage{0};
    node_ptr root = nullptr;
    alignas(128) std::byte _[0];

    void done() {
      st = ST_DONE;
      stage.store(STAGE_DONE, std::memory_order_release);
    }

    bool is_done() {
      return stage.load(std::memory_order_acquire) == STAGE_DONE;
    }
  };

  struct alignas(128) aligned_counter {
    std::atomic_size_t data;
    alignas(128) std::byte _[0];
  };

  const size_t num_pipes_;
  const size_t num_workers_;
  const size_t block_size_;
  std::thread* const pipes_;
  std::thread* const boarder_;
  std::thread* const workers_;
  std::thread* const collector_;
  aligned_counter* const tokens_;
  aligned_counter* const num_estimated_;

  const size_t num_tasks_;
  context* const ctxs_;

  alignas(128) size_t num_issued_;
  alignas(128) std::atomic_size_t num_submitted_;
  alignas(128) std::atomic_size_t num_fetched_;
  alignas(128) std::atomic_size_t num_committed_;

  static inline void wait_task_() {
   //  std::this_thread::yield();
  }

  static inline void wait_dependence_() {
    // std::this_thread::yield();
  }

  void concat_(node_ptr* tp, context* ctx) {
    if (ctx->t_l == nullptr) {
      *tp = ctx->t_r;
      ctx->done(); }
    else if (ctx->t_r == nullptr) {
      *tp = ctx->t_l;
      ctx->done(); }
    else if (ctx->t_l->pry > ctx->t_r->pry) {
      *tp = make_weak_copy(ctx->t_l, ctx->ver);
      ctx->dir = DIR_RIGHT; }
    else /* ctx->t_l->pry < ctx->t_r->pry */ {
      *tp = make_weak_copy(ctx->t_r, ctx->ver);
      ctx->dir = DIR_LEFT; }
  }

  void update_(node_ptr* tp, context* ctx) {
    if (ctx->t_past == nullptr) {
      if (ctx->op == OP_INSERT) { *tp = make_node(ctx->ver, ctx->pry, ctx->key); }
      else /* ctx->op == OP_DELETE */ { *tp = nullptr; }
      ctx->done(); }
    else if (ctx->t_past->pry > ctx->pry) {
      *tp = make_weak_copy(ctx->t_past, ctx->ver);
      ctx->dir = ctx->key < ctx->t_past->key ? DIR_LEFT : DIR_RIGHT; }
    else if (ctx->t_past->key == ctx->key) {
      if (ctx->op == OP_INSERT) {
        *tp = make_weak_copy(ctx->t_past, ctx->ver);
        (*tp)->val++;
        ctx->fn = FN_SETREF; }
      else /* ctx->op == OP_DELETE */ {
        if (ctx->t_past->val == 1) {
          size_t vdep = ctx->t_past->ver;
          while (num_committed_.load(std::memory_order_acquire) < vdep) { wait_dependence_(); }
          ctx->t_l = ctx->t_past->lch;
          ctx->t_r = ctx->t_past->rch;
          ctx->fn = FN_CONCAT;
          concat_(tp, ctx); }
        else /* ctx->t_past->val > 1 */ {
          *tp = make_weak_copy(ctx->t_past, ctx->ver);
          (*tp)->val--;
          ctx->fn = FN_SETREF; } } }
    else /* ctx->t_past->pry < ctx->pry */ {
      if (ctx->op == OP_INSERT) {
        size_t vdep = ctx->t_past->ver;
        while (num_committed_.load(std::memory_order_acquire) < vdep) { wait_dependence_(); }
        *tp = process_deploy(ctx->ver, ctx->t_past, ctx->pry, ctx->key); }
      else /* ctx->op == OP_DELETE */ { *tp = ctx->t_past; }
      ctx->done(); }
  }

  void execute_one_step_(context* ctx) {
    if (ctx->fn == FN_SEARCH) {
      if (ctx->dir == DIR_LEFT) {
        ctx->t_cur->rch = ctx->t_past->rch;
        ctx->t_past = ctx->t_past->lch;
        update_(&ctx->t_cur->lch, ctx);
        ctx->t_cur = ctx->t_cur->lch; }
      else /* ctx->dir == DIR_RIGHT */ {
        ctx->t_cur->lch = ctx->t_past->lch;
        ctx->t_past = ctx->t_past->rch;
        update_(&ctx->t_cur->rch, ctx);
        ctx->t_cur = ctx->t_cur->rch; } }
    else if (ctx->fn == FN_SETREF) {
      ctx->t_cur->lch = ctx->t_past->lch;
      ctx->t_cur->rch = ctx->t_past->rch;
      ctx->done(); }
    else if (ctx->fn == FN_CONCAT) {
      if (ctx->dir == DIR_LEFT) {
        ctx->t_cur->rch = ctx->t_r->rch;
        ctx->t_r = ctx->t_r->lch;
        concat_(&ctx->t_cur->lch, ctx);
        ctx->t_cur = ctx->t_cur->lch; }
      else {
        ctx->t_cur->lch = ctx->t_l->lch;
        ctx->t_l = ctx->t_l->rch;
        concat_(&ctx->t_cur->rch, ctx);
        ctx->t_cur = ctx->t_cur->rch; } }
    else { assert(false); }
  }

  void execute_to_complete_(context* ctx) {
    if (ctx->fn == FN_SEARCH) {
      if (ctx->dir == DIR_LEFT) {
        ctx->t_cur->rch = ctx->t_past->rch;
        ctx->t_past = ctx->t_past->lch;
        if (ctx->op == OP_INSERT) {
          ctx->t_cur->lch = search_insert(ctx->ver, ctx->t_past, ctx->pry, ctx->key); }
        else /* ctx->op == OP_DELETE */ {
          ctx->t_cur->lch = search_delete(ctx->ver, ctx->t_past, ctx->pry, ctx->key); } }
      else /* ctx->dir == DIR_RIGHT */ {
        ctx->t_cur->lch = ctx->t_past->lch;
        ctx->t_past = ctx->t_past->rch;
        if (ctx->op == OP_INSERT) {
          ctx->t_cur->rch = search_insert(ctx->ver, ctx->t_past, ctx->pry, ctx->key); }
        else /* ctx->op == OP_DELETE */ {
          ctx->t_cur->rch = search_delete(ctx->ver, ctx->t_past, ctx->pry, ctx->key); } } }
    else if (ctx->fn == FN_SETREF) {
      ctx->t_cur->lch = ctx->t_past->lch;
      ctx->t_cur->rch = ctx->t_past->rch; }
    else if (ctx->fn == FN_CONCAT) {
      if (ctx->dir == DIR_LEFT) {
        ctx->t_cur->rch = ctx->t_r->rch;
        ctx->t_r = ctx->t_r->lch;
        ctx->t_cur->lch = process_concat(ctx->ver, ctx->t_l, ctx->t_r); }
      else /* ctx->dir == DIR_RIGHT */ {
        ctx->t_cur->lch = ctx->t_l->lch;
        ctx->t_l = ctx->t_l->rch;
        ctx->t_cur->rch = process_concat(ctx->ver, ctx->t_l, ctx->t_r); } }
    else { assert(false); }
    ctx->done();
  }

  void pipe_thread_(size_t id) {
    log_debug("start pipe %zu", id);
    for (size_t pos = 1; pos <= num_tasks_; ++pos) {
      size_t my_token = tokens_[id-1].data.fetch_sub(1, std::memory_order_acquire);
      if (my_token == 0) { std::atomic_wait(&tokens_[id-1].data, ~size_t{0}); }
      log_trace("[pipe %zu] starts executing task %zu", id, pos);
      if (ctxs_[pos].st != ST_DONE) { execute_one_step_(&ctxs_[pos]); }
      log_trace("[pipe %zu] completes task %zu", id, pos);
      size_t next_token = 1+tokens_[id].data.fetch_add(1, std::memory_order_release);
      if (next_token == 0) { tokens_[id].data.notify_one(); } }
    log_debug("stop pipe %zu", id);
  }

  void boarder_thread_() {
    log_debug("start boarder");
    for (size_t local_submitted = 0; local_submitted < num_tasks_; ) {
      size_t num_tokens = tokens_[num_pipes_].data.exchange(0, std::memory_order_acquire);
      if (num_tokens > 0) {
        local_submitted += num_tokens;
        log_trace("[boarder] submits task before %zu", local_submitted);
        num_submitted_.store(local_submitted, std::memory_order_release); } }
    log_debug("stop boarder");
  }

  void work_update_(size_t id, size_t& task_id, size_t& cached_submit) {
    task_id = 1+num_fetched_.fetch_add(1, std::memory_order_acquire);
    if (task_id > num_tasks_) { return; }
    log_trace("[worker %zu] fetches task %zu", id, task_id);
    while (task_id > cached_submit) {
      cached_submit = num_submitted_.load(std::memory_order_acquire);
      if (task_id > cached_submit) { wait_task_(); } }
    context* ctx = &ctxs_[task_id];
    for (size_t stage = 1; ctx->st != ST_DONE; stage++) {
      ctx->stage.store(stage, std::memory_order_release);
      size_t vdep = ctx->t_past->ver;
      if (num_committed_.load(std::memory_order_acquire) >= vdep) { execute_to_complete_(ctx); }
      else {
        while (ctxs_[vdep].stage.load(std::memory_order_acquire) <= stage) { wait_dependence_(); }
        execute_one_step_(ctx); } }
    log_trace("[worker %zu] completes task %zu", id, task_id);
  }

  void work_estimate_(size_t start, size_t end) {
    if (start > end) { return; }
    // estimate by reversed order
    for (size_t task_id = end; task_id >= start; task_id--) { augment(ctxs_[task_id].root); }
  }

  void worker_thread_(size_t id) {
    log_debug("start worker %zu", id);
    size_t task_id = 0;
    size_t cached_submit = 0;
    size_t block_count = 0;
    size_t block_start = (id-1) * block_size_ + 1;
    size_t block_end = std::min(block_start+block_size_-1, num_tasks_);
    while (task_id <= num_tasks_) {
      work_update_(id, task_id, cached_submit);
      if (task_id >= block_end) {
        work_estimate_(block_start, block_end);
        block_count++;
        block_start += num_workers_ * block_size_;
        block_end = std::min(block_start+block_size_-1, num_tasks_);
        num_estimated_[id-1].data.store(block_count, std::memory_order_release); } }
    log_debug("stop worker %zu", id);
  }

  void collector_thread_() {
    log_debug("start collector");
    for (size_t local_committed = 0; local_committed < num_tasks_; ) {
      size_t next_committable = local_committed;
      while (next_committable < num_tasks_) {
        if (!ctxs_[next_committable+1].is_done()) { break; }
        ++next_committable; }
      if (next_committable > local_committed) {
        local_committed = next_committable;
        log_trace("[collector] commits task before %zu", local_committed);
        num_committed_.store(local_committed, std::memory_order_release); } }
    log_debug("stop collector");
  }

  static inline size_t decide_num_pipes_(size_t num_threads) {
    if (num_threads < 16) { return num_threads/2; }
    return ceil(2*log2(num_threads));
  }

  static inline size_t decide_num_workers_(size_t num_threads) {
    size_t p = num_threads - decide_num_pipes_(num_threads);
    return p > 0 ? p : 1;
  }

  inline void init_context_(operation op, uint64_t elem) {
    num_issued_++;
    log_trace("[master] receives task %zu", num_issued_);
    context* ctx = &ctxs_[num_issued_];
    ctx->op = op;
    ctx->ver = num_issued_;
    if (op != OP_NONE) {
      ctx->pry = aes_hash::hash(elem);
      ctx->key = elem;
      ctx->t_past = ctxs_[num_issued_-1].root;
      update_(&ctx->root, ctx);
      ctx->t_cur = ctx->root; }
    else {
      ctx->root = ctxs_[num_issued_-1].root;
      ctx->done(); }
    log_trace("[master] pushes task %zu into pipeline", num_issued_);
    size_t next_token = 1+tokens_[0].data.fetch_add(1, std::memory_order_release);
    if (next_token == 0) { tokens_[0].data.notify_one(); }
  }

public:
  scheduler(size_t num_threads, size_t num_tasks, size_t block_size, node_ptr t0) :
    num_pipes_(decide_num_pipes_(num_threads)),
    num_workers_(decide_num_workers_(num_threads)),
    block_size_(block_size),
    pipes_(new std::thread[num_pipes_]),
    boarder_(new std::thread[1]),
    workers_(new std::thread[num_workers_]),
    collector_(new std::thread[1]),
    tokens_(new aligned_counter[num_pipes_+1]),
    num_estimated_(new aligned_counter[num_workers_]),
    num_tasks_(num_tasks),
    ctxs_(new context[num_tasks+1]),
    num_issued_(0), num_submitted_(0), num_fetched_(0), num_committed_(0)
  {
    ctxs_[0].root = t0;
    ctxs_[0].done();
    for (size_t thread_id = 1; thread_id <= num_pipes_; ++thread_id) {
      new (&pipes_[thread_id-1]) std::thread(&scheduler::pipe_thread_, this, thread_id); }
    new (boarder_) std::thread(&scheduler::boarder_thread_, this);
    for (size_t thread_id = 1; thread_id <= num_workers_; ++thread_id) {
      new (&workers_[thread_id-1]) std::thread(&scheduler::worker_thread_, this, thread_id); }
    new (collector_) std::thread(&scheduler::collector_thread_, this);
  }

  inline void insert_elem(uint64_t elem) {
    init_context_(OP_INSERT, elem);
  }

  inline void delete_elem(uint64_t elem) {
    init_context_(OP_DELETE, elem);
  }

  inline size_t nop() {
    init_context_(OP_NONE, 0);
    return num_issued_;
  }

  void process() {
    for (size_t thread_id = 1; thread_id <= num_pipes_; ++thread_id) { pipes_[thread_id-1].join(); }
    boarder_->join();
    for (size_t thread_id = 1; thread_id <= num_workers_; ++thread_id) { workers_[thread_id-1].join(); }
    collector_->join();
  }

  inline node_ptr get_snapshot(size_t ver) {
    if (ver > num_issued_) { return nullptr; }
    size_t block_id = ver / block_size_;
    size_t block_worker = block_id % num_workers_;
    size_t block_pos = block_id / num_workers_;
    while (num_estimated_[block_worker].data.load(std::memory_order_acquire) <= block_pos) { wait_dependence_(); }
    return ctxs_[ver].root;
  }

  inline size_t last_version() {
    return num_issued_;
  }
};

}
