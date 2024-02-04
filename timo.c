/*
 * Copyright (c) 2003-2010 Alexandre Ratchov <alex@caoua.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * trivial timeouts implementation.
 *
 * A timeout is used to schedule the call of a routine (the callback)
 * there is a global list of timeouts that is processed inside the
 * event loop ie mux_run(). Timeouts work as follows:
 *
 *	first the timo structure must be initialized with timo_set()
 *
 *	then the timeout is scheduled (only once) with timo_add()
 *
 *	if the timeout expires, the call-back is called; then it can
 *	be scheduled again if needed. It's OK to reschedule it again
 *	from the callback
 *
 *	the timeout can be aborted with timo_del(), it is OK to try to
 *	abort a timout that has expired
 *
 */

/**
 * 些末なタイムアウト実装
 *
 * タイムアウトはコールバックルーチンのスケジュールのために使用される.
 * `mux_run()` などのイベントループの中で処理されるグローバルな timeout のリストがある.
 *
 * Timeouts は以下のように動作する
 * 1. timo構造体は必ず `timo_set()` で初期化される
 * 2. timeout は `timo_add()` で一度だけスケジュールされる
 * 3. timeoutが期限切れとなると callback が呼ばれ、必要なら再度スケジュールされる. コールバックから再度スケジュールを変更しても問題ありません.
 * 4. timeout は `timo_del()` で削除される. 期限切れのものを削除しようとしても問題ありません.
 */

#include "utils.h"
#include "timo.h"

unsigned timo_debug = 0;
struct timo *timo_queue; /* グローバルで管理される timo 配列 */
unsigned timo_abstime; /* timeout の絶対管理時間 */

/*
 * initialise a timeout structure, arguments are callback and argument
 * that will be passed to the callback
 */
void
timo_set(struct timo *o, void (*cb)(void *), void *arg)
{
	o->cb = cb;
	o->arg = arg;
	o->set = 0;
}

/**
 * schedule the callback in 'delta' 24-th of microseconds. The timeout
 * must not be already scheduled
 */
void
timo_add(struct timo *o, unsigned delta)
{
	struct timo **i;
	unsigned val;
	int diff;

#ifdef TIMO_DEBUG
	if (o->set) {
		log_puts("timo_add: already set\n");
		panic();
	}
	if (delta == 0) {
		log_puts("timo_add: zero timeout is evil\n");
		panic();
	}
#endif
	val = timo_abstime + delta;
	for (i = &timo_queue; *i != NULL; i = &(*i)->next) {
		diff = (*i)->val - val;
		if (diff > 0) { /* まだ次のtimoまで時間があるなら, そこに新規timoを挿入 */
			break;
		}
	}
	o->set = 1;
	o->val = val;
	o->next = *i; /* [現在指しているアドレス]を新timo->nextに設定 */
	*i = o; /* [現在指しているアドレス]を新規timoで上書き */
}

/*
 * abort a scheduled timeout
 */
void
timo_del(struct timo *o)
{
	struct timo **i;

	for (i = &timo_queue; *i != NULL; i = &(*i)->next) {
		if (*i == o) {
			*i = o->next; /* [現在指してる箇所]を削除対象->nextに設定 */
			o->set = 0;
			return;
		}
	}
	if (timo_debug)
		log_puts("timo_del: not found\n");
}

/**
 * routine to be called by the timer when 'delta' 24-th of microsecond
 * elapsed. This routine updates time referece used by timeouts and
 * calls expired timeouts
 * 24th of micro-sec経過後にタイマーによって呼び出されるルーチン.
 * timoに使用される時間参照を更新し,期限切れの場合はtimeoutを呼び出す.
 */
void
timo_update(unsigned delta)
{
	struct timo *to;
	int diff;

	/*
	 * update time reference
	 */
	timo_abstime += delta;

	/*
	 * remove from the queue and run expired timeouts
	 */
	while (timo_queue != NULL) {
		/*
		 * there is no overflow here because + and - are
		 * modulo 2^32, they are the same for both signed and
		 * unsigned integers
		 */
		diff = timo_queue->val - timo_abstime;
		if (diff > 0) /* expired じゃないので、これ以上のqueue走査は無駄 */
			break;
		to = timo_queue; /* [先頭要素] 取得 */
		timo_queue = to->next; /* [先頭要素]<- (to->next) */
		to->set = 0;
		to->cb(to->arg); /* コールバック呼び出し */
	}
}

/*
 * initialize timeout queue
 */
void
timo_init(void)
{
	timo_queue = NULL;
	timo_abstime = 0;
}

/*
 * destroy timeout queue
 */
void
timo_done(void)
{
	if (timo_queue != NULL) {
		log_puts("timo_done: timo_queue not empty!\n");
		panic();
	}
	timo_queue = (struct timo *)0xdeadbeef;
}
