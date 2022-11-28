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

#ifndef OCEANBASE_STORAGE_OB_DDL_CLOG_H_
#define OCEANBASE_STORAGE_OB_DDL_CLOG_H_

#include "storage/ob_i_table.h"
#include "storage/blocksstable/ob_block_sstable_struct.h"
#include "storage/blocksstable/ob_index_block_builder.h"
#include "logservice/ob_append_callback.h"

namespace oceanbase
{

namespace storage
{
enum class ObDDLClogType : int64_t
{
  UNKNOWN = -1,
  DDL_REDO_LOG = 0x1,
  DDL_COMMIT_LOG = 0x2,
  DDL_TABLET_SCHEMA_VERSION_CHANGE_LOG = 0x10,
  DDL_START_LOG = 0x20,
  DDL_PREPARE_LOG = 0x40
};

enum ObDDLClogState : uint8_t
{
  STATE_INIT = 0,
  STATE_SUCCESS = 1,
  STATE_FAILED = 2
};

class ObDDLClogCbStatus final
{
public:
  ObDDLClogCbStatus();
  ~ObDDLClogCbStatus() {}
  void set_state(const ObDDLClogState state) { state_ = state; }
  inline bool is_success() const { return state_ == ObDDLClogState::STATE_SUCCESS; }
  inline bool is_failed() const { return state_ == ObDDLClogState::STATE_FAILED; }
  inline bool is_finished() const { return state_ != ObDDLClogState::STATE_INIT; }
  bool try_set_release_flag();
  void set_ret_code(const int ret_code) { ret_code_ = ret_code; }
  int get_ret_code() const { return ret_code_; }
private:
  bool the_other_release_this_;
  ObDDLClogState state_;
  int ret_code_;
};

class ObDDLClogCb : public logservice::AppendCb
{
public:
  ObDDLClogCb();
  virtual ~ObDDLClogCb() = default;
  virtual int on_success() override;
  virtual int on_failure() override;
  inline bool is_success() const { return status_.is_success(); }
  inline bool is_failed() const { return status_.is_failed(); }
  inline bool is_finished() const { return status_.is_finished(); }
  void try_release();
private:
  ObDDLClogCbStatus status_;
};

class ObDDLMacroBlockClogCb : public logservice::AppendCb
{
public:
  ObDDLMacroBlockClogCb();
  virtual ~ObDDLMacroBlockClogCb() = default;
  int init(const share::ObLSID &ls_id,
           const blocksstable::ObDDLMacroBlockRedoInfo &redo_info,
           const blocksstable::MacroBlockId &macro_block_id);
  virtual int on_success() override;
  virtual int on_failure() override;
  inline bool is_success() const { return status_.is_success(); }
  inline bool is_failed() const { return status_.is_failed(); }
  inline bool is_finished() const { return status_.is_finished(); }
  int get_ret_code() const { return status_.get_ret_code(); }
  void try_release();
private:
  bool is_inited_;
  ObDDLClogCbStatus status_;
  share::ObLSID ls_id_;
  blocksstable::ObDDLMacroBlockRedoInfo redo_info_;
  blocksstable::MacroBlockId macro_block_id_;
  ObArenaAllocator arena_;
  ObSpinLock data_buffer_lock_;
  bool is_data_buffer_freed_;
};

class ObDDLClogHeader final
{
public:
  static const int64_t DDL_CLOG_HEADER_SIZE = sizeof(ObDDLClogType);

  NEED_SERIALIZE_AND_DESERIALIZE;
  ObDDLClogHeader() : ddl_clog_type_(ObDDLClogType::UNKNOWN) {}
  ObDDLClogHeader(const ObDDLClogType &type) : ddl_clog_type_(type) {}
  const ObDDLClogType & get_ddl_clog_type() { return ddl_clog_type_; };
  TO_STRING_KV(K(ddl_clog_type_));
private:
  DISALLOW_COPY_AND_ASSIGN(ObDDLClogHeader);
  ObDDLClogType ddl_clog_type_;
};

class ObDDLStartLog final
{
  OB_UNIS_VERSION_V(1);
public:
  ObDDLStartLog();
  ~ObDDLStartLog() = default;
  int init(const ObITable::TableKey &table_key, const int64_t cluster_version);
  bool is_valid() const { return table_key_.is_valid() && cluster_version_ >= 0; }
  ObITable::TableKey get_table_key() const { return table_key_; }
  int64_t get_cluster_version() const { return cluster_version_; }
  TO_STRING_KV(K_(table_key), K_(cluster_version));
private:
  ObITable::TableKey table_key_;
  int64_t cluster_version_; // used for compatibility
};

class ObDDLRedoLog final
{
public:
  ObDDLRedoLog();
  ~ObDDLRedoLog() = default;
  int init(const blocksstable::ObDDLMacroBlockRedoInfo &redo_info);
  bool is_valid() const { return redo_info_.is_valid(); }
  blocksstable::ObDDLMacroBlockRedoInfo get_redo_info() const { return redo_info_; }
  TO_STRING_KV(K_(redo_info));
  OB_UNIS_VERSION_V(1);
private:
  blocksstable::ObDDLMacroBlockRedoInfo redo_info_;
};

class ObDDLPrepareLog final
{
  OB_UNIS_VERSION_V(1);
public:
  ObDDLPrepareLog();
  ~ObDDLPrepareLog() = default;
  int init(const ObITable::TableKey &table_key,
           const share::SCN &start_scn);
  bool is_valid() const { return table_key_.is_valid() && start_scn_.is_valid(); }
  ObITable::TableKey get_table_key() const { return table_key_; }
  share::SCN get_start_scn() const { return start_scn_; }
  TO_STRING_KV(K_(table_key), K_(start_scn));
private:
  ObITable::TableKey table_key_;
  share::SCN start_scn_;
};

class ObDDLCommitLog final
{
  OB_UNIS_VERSION_V(1);
public:
  ObDDLCommitLog();
  ~ObDDLCommitLog() = default;
  int init(const ObITable::TableKey &table_key,
           const share::SCN &start_scn,
           const share::SCN &prepare_scn);
  bool is_valid() const { return table_key_.is_valid() && start_scn_.is_valid() && prepare_scn_.is_valid(); }
  ObITable::TableKey get_table_key() const { return table_key_; }
  share::SCN get_start_scn() const { return start_scn_; }
  share::SCN get_prepare_scn() const { return prepare_scn_; }
  TO_STRING_KV(K_(table_key), K_(start_scn), K_(prepare_scn));
private:
  ObITable::TableKey table_key_;
  share::SCN start_scn_;
  share::SCN prepare_scn_;
};

class ObTabletSchemaVersionChangeLog final
{
public:
  ObTabletSchemaVersionChangeLog();
  ~ObTabletSchemaVersionChangeLog() = default;
  int init(const common::ObTabletID &tablet_id, const int64_t schema_version);
  bool is_valid() const { return tablet_id_.is_valid() && schema_version_ >= 0; }
  common::ObTabletID get_tablet_id() const { return tablet_id_; }
  int64_t get_schema_version() const { return schema_version_; }
  TO_STRING_KV(K_(tablet_id), K_(schema_version));
  OB_UNIS_VERSION_V(1);
private:
  common::ObTabletID tablet_id_;
  int64_t schema_version_;
};

class ObDDLBarrierLog final {
public:
  ObDDLBarrierLog() : ls_id_(), hidden_tablet_ids_() {}
  ~ObDDLBarrierLog() {}
  bool is_valid() const { return ls_id_.is_valid() && hidden_tablet_ids_.count() > 0; }
  TO_STRING_KV(K_(ls_id), K_(hidden_tablet_ids));
  OB_UNIS_VERSION_V(1);
public:
  share::ObLSID ls_id_;
  common::ObSArray<common::ObTabletID> hidden_tablet_ids_;
};

} // namespace storage
} // namespace oceanbase
#endif
