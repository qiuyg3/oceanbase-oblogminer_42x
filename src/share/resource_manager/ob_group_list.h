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


// [0, 100) for inner group
//CGID_DEF(group_name, group_id, worker_concurrency)
CGID_DEF(OBCG_DEFAULT, 0)
CGID_DEF(OBCG_CLOG, 1)
CGID_DEF(OBCG_ELECTION, 2)
CGID_DEF(OBCG_ID_SERVICE, 5)
CGID_DEF(OBCG_ID_SQL_REQ_LEVEL1, 6, 4)
CGID_DEF(OBCG_ID_SQL_REQ_LEVEL2, 7, 4)
CGID_DEF(OBCG_ID_SQL_REQ_LEVEL3, 8, 4)
CGID_DEF(OBCG_DETECT_RS, 9)
CGID_DEF(OBCG_LOC_CACHE, 10)
CGID_DEF(OBCG_SQL_NIO, 11)
CGID_DEF(OBCG_MYSQL_LOGIN, 12)
CGID_DEF(OBCG_CDCSERVICE, 13)
CGID_DEF(OBCG_DIAG_TENANT, 14)
CGID_DEF(OBCG_LQ, 100)
