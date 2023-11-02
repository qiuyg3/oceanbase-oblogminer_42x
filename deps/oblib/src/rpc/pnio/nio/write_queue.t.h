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

static void my_flush_cb_exception(my_t* io, my_req_t* r) {
  return my_flush_cb(io, r);
}

static void my_write_queue_on_sk_destroy(my_t* io, my_sk_t* s) {
  link_t* h = queue_top(&s->wq.queue);
  while(NULL != h) {
    my_req_t* req = structof(h, my_req_t, link);
    h = h->next;
    my_flush_cb_exception(io, req);
  }
}

static void my_flush_cb_on_post_fail(my_t* io, my_req_t* r) {
  return my_flush_cb_exception(io, r);
}

static void my_flush_cb_after_flush(my_t* io, my_req_t* r) {
  return my_flush_cb(io, r);
}

static int my_sk_do_flush(my_sk_t* s, int64_t* remain) {
  link_t* h = NULL;
  int err = wq_flush((sock_t*)s, &s->wq, &h);
  my_t* io = structof(s->fty, my_t, sf);
  if (0 == err && NULL != h) {
    link_t* stop = queue_top(&s->wq.queue);
    while(h != stop) {
      my_req_t* req = structof(h, my_req_t, link);
      h = h->next;
      my_flush_cb_after_flush(io, req);
    }
  }
  *remain = !queue_empty(&s->wq.queue);
  return err;
}
