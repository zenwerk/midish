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

#ifndef MIDISH_POOL_H
#define MIDISH_POOL_H

/*
 * Eng:
 *   entry from the pool. Any real pool entry is cast to this structure
 *   by the pool code. The actual size of a pool entry is in 'itemsize'
 *   field of the pool structure
 *
 * Ja:
 *   poolのエントリ. 実際のpoolのエントリデータはこの構造体にキャストされる.
 *   エントリの実際のサイズは`itemsize`フィールドを参照する
 */
struct poolent {
	struct poolent *next; /* 内部にポインタが一つあるだけの構造体なので sizeof の結果は (64bit環境では) 8 */
};

/*
 * Eng:
 *   the pool is a linked list of 'itemnum' blocks of size
 *   'itemsize'. The pool name is for debugging prurposes only
 *
 * Ja:
 *   pool は 'itemsize'のサイズを持つアイテムが 'itemnum' の数だけあるリンクリスト.
 *   なお名前`name`はデバッグ用途でしか使わない.
 */
struct pool {
	unsigned char *data;	/* memory block of the pool | データブロック */
	struct poolent *first;	/* head of linked list      | ヘッド */
#ifdef POOL_DEBUG
	unsigned maxused;	/* max pool usage */
	unsigned used;		/* current pool usage */
	unsigned newcnt;	/* current items allocated */
#endif
	unsigned itemnum;	/* total number of entries | エントリ総数 */
	unsigned itemsize;	/* size of a sigle entry   | エントリのサイズ TODO: fix typo (sigle -> single) */
	char *name;		/* name of the pool            | 名前(デバッグ用途) */
};

void  pool_init(struct pool *, char *, unsigned, unsigned);
void  pool_done(struct pool *);

void *pool_new(struct pool *);
void  pool_del(struct pool *, void *);

#endif /* MIDISH_POOL_H */
