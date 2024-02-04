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
 * a pool is a large memory block (the pool) that is split into
 * small blocks of equal size (pools entries). Its used for
 * fast allocation of pool entries. Free enties are on a singly
 * linked list
 */

#include "utils.h"
#include "pool.h"

unsigned pool_debug = 0;

/*
 * Eng:
 *   initialises a pool of "itemnum" elements of size "itemsize"
 * Ja:
 *   pool を`itemnum` の個数だけ `itemsize` で初期化
 *   pool.data を起点にメモリをガッと取得してアドレスの区切りをpoolentで取得する
 */
void
pool_init(struct pool *o, char *name, unsigned itemsize, unsigned itemnum)
{
	unsigned i;
	unsigned char *p;

	/*
	 * round item size to sizeof unsigned | itemsize を unsigned に丸める
	 */
	if (itemsize < sizeof(struct poolent)) {
		/* itemsize が poolent そのものより小さいサイズの構造体のとき */
		itemsize = sizeof(struct poolent);
	}
	itemsize += sizeof(unsigned) - 1;    /* unsinged int のバイト数から1を引いて足す */
	itemsize &= ~(sizeof(unsigned) - 1); /* bit反転してand演算 */

/*
    8 + 3 = 11 => 0000_1011
	~3         => 1111_1100
	&=         => 0000_1000 = 8
*/

	o->data = xmalloc(itemsize * itemnum, "pool"); // 使用するデータ領域を連続でガッと取得している
	if (!o->data) {
		log_puts("pool_init(");
		log_puts(name);
		log_puts("): out of memory\n");
		panic();
	}
	o->first = NULL; // リンクリストの先頭をNULLで初期化
	o->itemsize = itemsize;
	o->itemnum = itemnum;
	o->name = name;
#ifdef POOL_DEBUG
	o->maxused = 0;
	o->used = 0;
	o->newcnt = 0;
#endif

	/*
	 * En: create a linked list of all entries
	 * Ja: リンクリストを初期化(firstを更新してるのでstackのように先頭に追加していく)
	 */
	p = o->data; // dataには実際に使用するメモリ領域の開始アドレスが保存される
	for (i = itemnum; i != 0; i--) {
		((struct poolent *)p)->next = o->first; // poolent.next = pool.poolent(1回目のループならNULL)
		o->first = (struct poolent *)p; // pool.first をpoolentにcastしてアドレスを保存
		p += itemsize; // pool.data のアドレスを size分だけ進める
		o->itemnum++;
	}
}


/*
 * free the given pool
 */
void
pool_done(struct pool *o)
{
	xfree(o->data);
#ifdef POOL_DEBUG
	if (o->used != 0) {
		log_puts("pool_done(");
		log_puts(o->name);
		log_puts("): WARNING ");
		log_putu(o->used);
		log_puts(" items still allocated\n");
	}
	if (pool_debug) {
		log_puts("pool_done(");
		log_puts(o->name);
		log_puts("): using ");
		log_putu((1023 + o->itemnum * o->itemsize) / 1024);
		log_puts("kB maxused = ");
		log_putu(100 * o->maxused / o->itemnum);
		log_puts("% allocs = ");
		log_putu(100 * o->newcnt / o->itemnum);
		log_puts("%\n");
	}
#endif
}

/*
 * En: allocate an entry from the pool: just unlink
 *     it from the free list and return the pointer
 * Ja: poolからエントリ領域を確保する.
 *     実際には free-list から unlink してポインタを返すだけ.
 */
void *
pool_new(struct pool *o)
{
#ifdef POOL_DEBUG
	unsigned i;
	unsigned char *buf;
#endif

	struct poolent *e;

	if (!o->first) {
		log_puts("pool_new(");
		log_puts(o->name);
		log_puts("): pool is empty\n");
		panic();
	}

	/*
	 * unlink from the free list
	 */
	e = o->first;       // poolの先頭のpoolentを取得
	o->first = e->next; // pool の先頭を poolent.next に上書き

#ifdef POOL_DEBUG
	o->newcnt++;
	o->used++;
	if (o->used > o->maxused)
		o->maxused = o->used;

	/*
	 * overwrite the entry with garbage so any attempt to use
	 * uninitialized memory will probably segfault
	 */
	buf = (unsigned char *)e;
	for (i = o->itemsize; i > 0; i--)
		*(buf++) = 0xd0;
#endif
	return e;
}

/*
 * En: free an entry: just link it again on the free list
 * Ja: エントリを開放する: 実際には free-list に再びリンクするだけ
 */
void
pool_del(struct pool *o, void *p)
{
	struct poolent *e = (struct poolent *)p;
#ifdef POOL_DEBUG
	unsigned i;
	unsigned char *buf;

	/*
	 * check if we aren't trying to free more
	 * entries than the poll size
	 */
	if (o->used == 0) {
		log_puts("pool_del(");
		log_puts(o->name);
		log_puts("): pool is full\n");
		panic();
	}
	o->used--;

	/*
	 * overwrite the entry with garbage so any attempt to use a
	 * free entry will probably segfault
	 */
	buf = (unsigned char *)e;
	for (i = o->itemsize; i > 0; i--)
		*(buf++) = 0xdf;
#endif
	/*
	 * link on the free list
	 */
	e->next = o->first;
	o->first = e; // poolent をpoolの先頭につなげる
}
