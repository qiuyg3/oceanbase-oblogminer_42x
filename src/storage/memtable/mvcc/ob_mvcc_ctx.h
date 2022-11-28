/**
 * Copyright (c) 2021 OceanBase
 * OceanBase CE is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan PubL v2.
 * You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 */

#ifndef OCEANBASE_MEMTABLE_MVCC_OB_MVCC_CTX_
#define OCEANBASE_MEMTABLE_MVCC_OB_MVCC_CTX_

#include "share/ob_define.h"
#include "lib/container/ob_iarray.h"
#include "lib/stat/ob_diagnose_info.h"
#include "lib/allocator/ob_allocator.h"
#include "lib/utility/ob_print_utils.h"
#include "lib/utility/utility.h"
#include "storage/memtable/mvcc/ob_mvcc_row.h"
#include "storage/memtable/mvcc/ob_mvcc.h"
#include "storage/memtable/mvcc/ob_mvcc_trans_ctx.h"
#include "storage/tx/ob_trans_define.h"

namespace oceanbase
{
namespace storage
{
class ObTxTableGuard;
}
namespace transaction
{
class ObPartTransCtx;
namespace tablelock
{
class ObMemCtxLockOpLinkNode;
class ObOBJLockCallback;
class ObLockMemtable;
}
}

namespace storage
{
class ObLsmtTransNode;
class ObFreezer;
}

using namespace transaction::tablelock;
namespace memtable
{
class ObMemtableCtxCbAllocator;
class ObMemtableKey;
class ObMvccRow;
class ObIMvccCtx
{
public:
  ObIMvccCtx(ObMemtableCtxCbAllocator &cb_allocator)
    : alloc_type_(0),
    trans_mgr_(*this, cb_allocator),
    //记录一个事务内第一次执行的table version
    min_table_version_(0),
    //记录一个事务内存最大的一次table version
    max_table_version_(0),
    trans_version_(share::SCN::max_scn()),
    commit_version_(share::SCN::min_scn()),
    lock_start_time_(0),
    redo_scn_(share::SCN::min_scn()),
    redo_log_id_(0),
    lock_wait_start_ts_(0),
    replay_compact_version_(share::SCN::min_scn())
  {
  }
  virtual ~ObIMvccCtx() {}
public: // for mvcc engine invoke
  // for write
  virtual int write_auth(const bool exclusive) = 0;
  virtual int write_done() = 0;
  virtual storage::ObTxTableGuard *get_tx_table_guard() = 0;
  virtual void *old_row_alloc(const int64_t size) = 0;
  virtual void old_row_free(void *row) = 0;
  virtual void *callback_alloc(const int64_t size) = 0;
  virtual void callback_free(ObITransCallback *cb) = 0;
  virtual common::ObIAllocator &get_query_allocator() = 0;
  virtual void set_conflict_trans_id(const uint32_t descriptor)
  { UNUSED(descriptor); }
  virtual int add_conflict_trans_id(const transaction::ObTransID conflict_trans_id) = 0;
  virtual int read_lock_yield() { return common::OB_SUCCESS; }
  virtual int write_lock_yield() { return common::OB_SUCCESS; }
  virtual void inc_lock_for_read_retry_count() = 0;
  virtual void add_lock_for_read_elapse(const int64_t n) = 0;
  virtual int64_t get_lock_for_read_elapse() const = 0;
  virtual void on_tsc_retry(const ObMemtableKey& key,
                            const share::SCN snapshot_version,
                            const share::SCN max_trans_version,
                            const transaction::ObTransID &conflict_tx_id) = 0;
  virtual void on_wlock_retry(const ObMemtableKey& key, const transaction::ObTransID &conflict_tx_id) = 0;
  virtual bool is_can_elr() const = 0;
  virtual void inc_truncate_cnt() = 0;
  virtual void add_trans_mem_total_size(const int64_t size) = 0;
  virtual void update_max_submitted_seq_no(const int64_t seq_no) = 0;
  virtual transaction::ObTransID get_tx_id() const = 0;
  virtual transaction::ObPartTransCtx *get_trans_ctx() const = 0;
  // statics maintainness for txn logging
  virtual void inc_unsubmitted_cnt() = 0;
  virtual void dec_unsubmitted_cnt() = 0;
  virtual void inc_unsynced_cnt() = 0;
  virtual void dec_unsynced_cnt() = 0;
  virtual share::SCN get_tx_end_scn() const { return share::SCN::max_scn(); };
public:
  inline int get_alloc_type() const { return alloc_type_; }
  inline share::SCN get_trans_version() const { return trans_version_.atomic_get(); }
  inline share::SCN get_commit_version() const { return commit_version_.atomic_get(); }
  inline int64_t get_min_table_version() const { return min_table_version_; }
  inline int64_t get_max_table_version() const { return max_table_version_; }
  inline void set_alloc_type(const int alloc_type) { alloc_type_ = alloc_type; }
  void before_prepare(const share::SCN version = share::SCN::min_scn());
  bool is_prepared() const;
  inline void set_prepare_version(const share::SCN version) { set_trans_version(version); }
  inline void set_trans_version(const share::SCN trans_version) { trans_version_.atomic_set(trans_version); }
  inline void set_commit_version(const share::SCN trans_version) { commit_version_.atomic_set(trans_version); }
  inline void set_table_version(const int64_t table_version)
  {
    if (INT64_MAX == min_table_version_) {
      //第一次更新，需要防御入参为INT64_MAX
      if (INT64_MAX == table_version) {
        TRANS_LOG(WARN, "unexpected table version", K(table_version), K(*this));
      } else {
        min_table_version_ = table_version;
        max_table_version_ = table_version;
      }
      //table version取最小值
    } else if (table_version < max_table_version_) {
      TRANS_LOG(DEBUG, "current table version lower the last one", K(table_version), K(*this));
      //非第一次更新table version，预期不会是int64_max
    } else if (INT64_MAX == table_version) {
      TRANS_LOG(ERROR, "unexpected table version", K(table_version), K(*this));
    } else {
      max_table_version_ = table_version;
    }
  }
  inline bool is_commit_version_valid() const { return commit_version_ != share::SCN::min_scn() && commit_version_ != share::SCN::max_scn(); }
  inline void set_lock_start_time(const int64_t start_time) { lock_start_time_ = start_time; }
  inline int64_t get_lock_start_time() { return lock_start_time_; }
  inline void set_for_replay(const bool for_replay) { trans_mgr_.set_for_replay(for_replay); }
  inline bool is_for_replay() const { return trans_mgr_.is_for_replay(); }
  inline void set_redo_scn(const share::SCN redo_scn) { redo_scn_ = redo_scn; }
  inline void set_redo_log_id(const int64_t redo_log_id) { redo_log_id_ = redo_log_id; }
  inline share::SCN get_redo_scn() const { return redo_scn_; }
  inline int64_t get_redo_log_id() const { return redo_log_id_; }
  inline void set_lock_wait_start_ts(const int64_t lock_wait_start_ts)
  { lock_wait_start_ts_ = lock_wait_start_ts; }
  share::SCN get_replay_compact_version() const { return replay_compact_version_; }
  void  set_replay_compact_version(const share::SCN v) { replay_compact_version_ = v; }
  inline int64_t get_lock_wait_start_ts() const { return lock_wait_start_ts_; }
  void acquire_callback_list() { trans_mgr_.acquire_callback_list(); }
  void revert_callback_list() { trans_mgr_.revert_callback_list(); }

  int register_row_commit_cb(
      const ObMemtableKey *key,
      ObMvccRow *value,
      ObMvccTransNode *node,
      const int64_t data_size,
      const ObRowData *old_row,
      ObMemtable *memtable,
      const int64_t seq_no);
  int register_row_replay_cb(
      const ObMemtableKey *key,
      ObMvccRow *value,
      ObMvccTransNode *node,
      const int64_t data_size,
      ObMemtable *memtable,
      const int64_t seq_no,
      const share::SCN scn);
  int register_table_lock_cb(
      transaction::tablelock::ObLockMemtable *memtable,
      ObMemCtxLockOpLinkNode *lock_op);
  int register_table_lock_replay_cb(
      ObLockMemtable *memtable,
      ObMemCtxLockOpLinkNode *lock_op,
      const share::SCN scn);
  int inc_pending_log_size(const int64_t size);
public:
  virtual void reset()
  {
    ctx_descriptor_ = 0;
    trans_mgr_.reset();
    min_table_version_ = INT64_MAX;
    max_table_version_ = 0;
    trans_version_ = share::SCN::max_scn();
    commit_version_ = share::SCN::min_scn();
    lock_start_time_ = 0;
    redo_scn_ = share::SCN::min_scn();
    redo_log_id_ = 0;
    lock_wait_start_ts_ = 0;
    replay_compact_version_ = share::SCN::min_scn();
  }
  virtual int64_t to_string(char *buf, const int64_t buf_len) const
  {
    int64_t pos = 0;
    common::databuff_printf(
        buf, buf_len, pos,
        "ObIMvccCtx={"
        "alloc_type=%d "
        "ctx_descriptor=%u "
        "min_table_version=%ld "
        "max_table_version=%ld "
        "trans_version=%s "
        "commit_version=%s "
        "lock_wait_start_ts=%ld "
        "replay_compact_version=%s}",
        alloc_type_,
        ctx_descriptor_,
        min_table_version_,
        max_table_version_,
        to_cstring(trans_version_),
        to_cstring(commit_version_),
        lock_wait_start_ts_,
        to_cstring(replay_compact_version_));
    return pos;
  }
public:
  virtual ObOBJLockCallback *alloc_table_lock_callback(
      ObIMvccCtx &ctx,
      transaction::tablelock::ObLockMemtable *memtable) = 0;
  virtual void free_table_lock_callback(ObITransCallback *cb) = 0;
  ObMvccRowCallback *alloc_row_callback(ObIMvccCtx &ctx, ObMvccRow &value, ObMemtable *memtable);
  ObMvccRowCallback *alloc_row_callback(ObMvccRowCallback &cb, ObMemtable *memtable);
  int append_callback(ObITransCallback *cb);
private:
  void check_row_callback_registration_between_stmt_();
  int register_table_lock_cb_(
      ObLockMemtable *memtable,
      ObMemCtxLockOpLinkNode *lock_op,
      ObOBJLockCallback *&cb);
protected:
  DISALLOW_COPY_AND_ASSIGN(ObIMvccCtx);
  int alloc_type_;
  uint32_t ctx_descriptor_;
  ObTransCallbackMgr trans_mgr_;
  int64_t min_table_version_;
  int64_t max_table_version_;
  share::SCN trans_version_;
  share::SCN commit_version_;
  int64_t lock_start_time_;
  share::SCN redo_scn_;
  int64_t redo_log_id_;
  int64_t lock_wait_start_ts_;
  share::SCN replay_compact_version_;
};

class ObMemtableCtx;
class ObMvccWriteGuard
{
public:
  ObMvccWriteGuard(const bool exclusive = false)
    : exclusive_(exclusive),
      ctx_(NULL),
      memtable_(NULL) {}
  ~ObMvccWriteGuard();
  void set_memtable(ObMemtable *memtable) {
    memtable_ = memtable;
  }
  /*
   * purpose of ensure replica writable
   *
   * accomplish with transaction ctx's state
   * so, must acquire transaction ctx at first
   *
   * for detail of authorization see transaction ctx's wirte_auth.
   */
  int write_auth(storage::ObStoreCtx &store_ctx);
private:
  DISALLOW_COPY_AND_ASSIGN(ObMvccWriteGuard);
  const bool exclusive_;  // if true multiple write_auth will be serialized
  ObMemtableCtx *ctx_;
  ObMemtable *memtable_;
};
}
}

#endif //OCEANBASE_MEMTABLE_MVCC_OB_MVCC_CTX_
