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
 * the multiplexer handles :
 *	- midi input and output
 *	- internal external/timer
 *
 * the clock unit is the 24th of microsecond (thus the tempo is stored with the same accuracy as in standard midi files).
 * クロック単位がマイクロ秒の24倍であるため、標準的なMIDIファイルと同じ精度でテンポが保存されます）。
 *
 * the timer has the following states:
 * STOP -> STARTWAIT -> START -> FIRST_TIC -> NEXT_TIC -> STOP
 *
 * STARTWAIT:
 *
 *	we're waiting (forever) for a "start" MIDI event; once it is
 *	received, we switch to the next state (START state). If the
 *	internal clock source is used, then we switch immediately to
 *	next state.
 *
 * START:
 *
 *	we just received the "start" MIDI event, so we wait for the
 *	first "tick" MIDI event; once it's received we switch to the
 *	next state (FIRST_TIC). If the internal clock source is used
 *	we wait MUX_START_DELAY (0.1 second) and we switch to the next
 *	state.
 *
 * FIRST_TIC:
 *
 *	we received the first "tick" event after a "start" event,
 *	the music starts now, so call appropriate call-backs to do so
 *	and wait for the next "tick" event.
 *
 * NEXT_TIC:
 *
 *	we received another "tick" event, move the music one
 *	step forward.
 *
 * STOP:
 *
 *	nothing to do, ignore any MIDI sync events.
 *
 */

#include "utils.h"
#include "ev.h"
#include "cons.h"
#include "defs.h"
#include "mux.h"
#include "mididev.h"
#include "sysex.h"
#include "timo.h"
#include "state.h"
#include "conv.h"

#include "norm.h"
#include "mixout.h"

/*
 * MUX_START_DELAY:
 *
 * delay between the START event and the first TIC in 24ths of a micro
 * second, here we use 1 tic at 30bpm
 */
#define MUX_START_DELAY	  (24000000UL / 3) /* = 8000000UL (UL:unsigned long) */

unsigned mux_isopen = 0; /* マルチプレクサ is Open (グローバル変数) |  */
unsigned mux_debug = 0;
unsigned mux_ticrate;
unsigned long mux_ticlength, mux_curpos, mux_nextpos;
unsigned mux_curtic;
unsigned mux_phase /* 現在のフェーズ */, mux_reqphase; /* 要求フェーズ */
unsigned mux_manualstart = 1;
void *mux_addr;
unsigned long mux_wallclock;


struct statelist mux_istate /* 入力状態 */ , mux_ostate; /* 出力状態 */

/*
 * the following are defined in mdep.c
 */
void mux_mdep_open(void);
void mux_mdep_close(void);

void mux_sendstop(void);
void mux_logphase(unsigned phase);
void mux_chgphase(unsigned phase);

/*
 * initialize all structures and open all midi devices
 * 全ての構造体の初期化とMIDIデバイスのオープンを行う
 */
void
mux_open(void)
{
	struct mididev *i;

	timo_init(); /* timeout init */
	statelist_init(&mux_istate);
	statelist_init(&mux_ostate);
	mixout_start();
	norm_start();

	/*
	 * default tempo is 120 beats per minutes with 24 tics per
	 * beat (time unit = 24th of microsecond)
	 *
	 * デフォルトのテンポは120拍/分、1拍24ティック。(時間単位は24マイクロ秒）
	 */
	mux_ticlength = TEMPO_TO_USEC24(DEFAULT_TEMPO, DEFAULT_TPB);

	/*
	 * default tics per second = 96 | 96tic/second
	 */
	mux_ticrate = DEFAULT_TPU;

	/*
	 * reset tic counters of devices
	 */
	mux_isopen = 1;
	for (i = mididev_list; i != NULL; i = i->next) {
		i->ticdelta = i->ticrate;
		i->isensto = 0;
		i->osensto = MIDIDEV_OSENSTO;
		mididev_open(i);
	}
	mux_mdep_open(); /* シグナルハンドラと setitimer の設定 */

	mux_curpos = 0;
	mux_nextpos = 0;
	mux_reqphase = MUX_STOP;
	mux_phase = MUX_STOP;
	mux_wallclock = 0;
	log_sync = 1;
}

/*
 * release all structures and close midi devices
 */
void
mux_close(void)
{
	struct mididev *i;

	log_sync = 1;
	norm_stop();
	mixout_stop();
	mux_flush();
	for (i = mididev_list; i != NULL; i = i->next) {
		if (i->isysex) {
			cons_err("lost incomplete sysex");
			sysex_del(i->isysex);
		}
		mididev_close(i);
	}
	mux_mdep_close();
	mux_isopen = 0;
	statelist_done(&mux_ostate);
	statelist_done(&mux_istate);
	timo_done();
}

#ifdef MUX_DEBUG
void
mux_logphase(unsigned phase)
{
	switch(phase) {
	case MUX_STARTWAIT:
		log_puts("STARTWAIT");
		break;
	case MUX_START:
		log_puts("START");
		break;
	case MUX_FIRST:
		log_puts("FIRST");
		break;
	case MUX_NEXT:
		log_puts("NEXT");
		break;
	case MUX_STOP:
		log_puts("STOP");
		break;
	default:
		log_puts("unknown");
		break;
	}
}
#endif

/**
 * change the current phase
 * グローバル変数 mux_phase を更新する関数
 */
void
mux_chgphase(unsigned phase)
{
#ifdef MUX_DEBUG
	log_puts("mux_phase: ");
	mux_logphase(mux_phase);
	log_puts(" -> ");
	mux_logphase(phase);
	log_puts("\n");
#endif
	mux_phase = phase;
}

/*
 * send a TIC to all devices that transmit real-time events. The tic
 * is only sent if the device tic_per_unit permits it.
 */
void
mux_sendtic(void)
{
	struct mididev *i;

	for (i = mididev_list; i != NULL; i = i->next) {
		if (i->sendclk && i != mididev_clksrc) {
			while (i->ticdelta >= mux_ticrate) {
				mididev_puttic(i);
				i->ticdelta -= mux_ticrate;
			}
			i->ticdelta += i->ticrate;
		}
	}
}

/*
 * similar to sendtic, but sends a START event
 */
void
mux_sendstart(void)
{
	struct mididev *i;

	for (i = mididev_list; i != NULL; i = i->next) {
		if (i->sendclk && i != mididev_clksrc) {
			i->ticdelta = i->ticrate;
			/*
			 * send a spurious tick just before the start event in order to notify that we are the master
			 *
			 * 私たちがマスターであることを通知するために、開始イベントの直前に偽の tic を送信する
			 */
			mididev_puttic(i);
			mididev_putstart(i);
		}
	}
}

/*
 * similar to sendtic, but send a STOP event
 */
void
mux_sendstop(void)
{
	struct mididev *i;

	for (i = mididev_list; i != NULL; i = i->next) {
		if (i->sendclk && i != mididev_clksrc) {
			mididev_putstop(i);
		}
	}
}

/**
 * send the given voice event to the appropriate device, no
 * other routines should be used to send events
 *
 * 与えられたボイスイベントを指定デバイスに送信する.
 * このメソッド以外でイベント送信をしてはいけない.
 * つまりこのメソッドの呼び出し元をたどればシーケンス元に辿りつくはず
 */
void
mux_putev(struct ev *ev)
{
	unsigned unit;
	struct mididev *dev;
	struct ev rev[CONV_NUMREV];
	unsigned i, nev;

#ifdef MUX_DEBUG
	if (mux_debug) {
		log_puts("mux_putev: ");
		ev_log(ev);
		log_puts("\n");
	}
#endif

	if (!EV_ISVOICE(ev) && !EV_ISSX(ev)) {
		log_puts("mux_putev: ");
		ev_log(ev);
		log_puts(": only voice events allowed\n");
		panic();
	}
	unit = ev->dev;
	if (unit >= DEFAULT_MAXNDEVS) {
		log_puts("mux_putev: ");
		ev_log(ev);
		log_puts(": bogus unit number\n");
		panic();
	}
	dev = mididev_byunit[unit];
	if (dev != NULL) {
		nev = conv_unpackev(&mux_ostate,
		    dev->oxctlset, dev->oevset, ev, rev);
		for (i = 0; i < nev; i++) {
			mididev_putev(dev, &rev[i]);
		}
	}
}

/*
 * send bytes to the given device, typically used to send
 * sysex messages
 */
void
mux_sendraw(unsigned unit, unsigned char *buf, unsigned len)
{
	struct mididev *dev;

	if (unit >= DEFAULT_MAXNDEVS) {
		return;
	}
	if (len == 0) {
		return;
	}
	dev = mididev_byunit[unit];
	if (dev == NULL) {
		return;
	}
	mididev_sendraw(dev, buf, len);
}

/*
 * called when MTC timer starts (full frame message).
 * MTCタイマーがスタートしたときの呼ばれる関数(フルフレームメッセージを受け取った)
 */
void
mux_mtcstart(unsigned mtcpos)
{
	/*
	 * if already started, trigger a MTC stop to enter a state in which we can start
	 * すでに開始している場合は, MTC停止をトリガーし、再度開始できる状態にする。
	 */
	if (mux_phase >= MUX_START && mux_phase <= MUX_NEXT) {
		if (mux_debug)
			log_puts("mux_mtcstart: triggered stop\n");
		mux_mtcstop();
	}

	/*
	 * check if we're trying to start, if not just return
	 * 開始できるかチェックし、ダメなら return
	 */
	if (mux_phase == MUX_STOP) {
		if (mux_debug)
			log_puts("mux_mtcstart: ignored mtc start (stopped)\n");
		return;
	}

	/*
	 * ignore position change if we're not using MTC because it's already set (e.g., internally generated MTC start)
	 * すでに設定されているため、MTCを使用していない場合は位置の変更を無視する（例：内部生成されたMTCスタート）
	 */
	if (mididev_mtcsrc) {
		mux_curpos = song_gotocb(usong, LOC_MTC, mtcpos);
		mux_nextpos = mux_ticlength;
		if (mux_curpos >= mux_nextpos) {
			log_puts("mux_mtcstart: offset larger than 1 tick\n");
			panic();
		}
	}

	/*
	 * generate clock start
	 */
	if (mux_debug)
		log_puts("mux_mtcstart: generated clk start\n");
	mux_startcb();
}

/**
 * called periodically by the MTC timer
 * MTCタイマによって定期的に呼ばれる関数
 */
void
mux_mtctick(unsigned delta)
{
	mux_curpos += delta;

	while (mux_curpos >= mux_nextpos) {
		mux_curpos -= mux_nextpos;
		mux_nextpos = mux_ticlength;

		/*
		 * if in manual mode, dont trigger the 0-th tick (ie
		 * the start signal).
		 */
		if (!mux_manualstart || mux_phase != MUX_START)
			mux_ticcb();
	}
}

/*
 * called when the MTC timer stops
 */
void
mux_mtcstop(void)
{
	/*
	 * if using external clock, ignore MTC
	 */
	if (mididev_clksrc)
		return;

	if (mux_phase >= MUX_START) {
		if (mux_debug)
			log_puts("mux_mtcstop: generated stop\n");
		mux_stopcb();
	}
}

/**
 * XXX: [5] シーケンサのタイマが動いたときに呼ばれるのがここ
 * call-back called every time the clock changes, the argument
 * contains the number of 24th of seconds elapsed since the last call
 *
 * 時間変化毎に呼ばれるコールバック. 引数は前回の呼び出しからの経過マイクロ秒数（24倍の経過時間?）を指定する。 .
 */
void
mux_timercb(unsigned long delta)
{
	struct mididev *dev;

	/*
	 * update wall clock
	 * グローバルな経過時間時計を更新する
	 */
	mux_wallclock += delta;

	/*
	 * run expired timeouts
	 * 時間経過した timo の処理を実施する
	 */
	timo_update(delta);

	/*
	 * handle timeouts not using the timo.c interface
	 * XXX: convert this to timo_xxx() routines
	 */
	for (dev = mididev_list; dev != NULL; dev = dev->next) {
		// アクティブセンシング
		if (dev->isensto) {
			if (dev->isensto <= delta) {
				dev->isensto = 0;
				cons_erru(dev->unit, "sensing timeout, disabled");
			} else {
				dev->isensto -= delta;
			}
		}
		if (dev->osensto) {
			if (dev->osensto <= delta) {
				mididev_putack(dev);
				mididev_flush(dev);
				dev->osensto = MIDIDEV_OSENSTO;
			} else {
				dev->osensto -= delta;
			}
		// MTC のソース解析
		if (dev->imtc.timo) {
			if (dev->imtc.timo <= delta) {
				dev->imtc.timo = 0;
				mtc_timo(&dev->imtc);
			} else {
				dev->imtc.timo -= delta;
			}
		}
	}

	/*
	 * if there's no ext MTC source, then generate one internally using the current sequencer state as hints.
	 *
	 * MTC入力がなければ,現在のシーケンサの状態をヒントに内部的にMTC信号を生成する.
	 * ここから先が実際に演奏するところ -> mux_mtctick の先が putev につながる
	 */
	if (!mididev_mtcsrc && !mididev_clksrc) {
		switch (mux_phase) {
		case MUX_STARTWAIT:
			if (!mux_manualstart) {
				log_puts("mux_timercb: startwait: bad state\n");
				panic();
			}
			break;
		case MUX_START:
			mux_curpos += delta;
			if (mux_curpos >= mux_nextpos) {
				mux_curpos = 0;
				mux_nextpos = 0;
				mux_mtctick(0);
			}
			break;
		case MUX_FIRST:
		case MUX_NEXT:
			mux_mtctick(delta);
			break;
		}
	}
}

/*
 * called when a MIDI TICK is received
 */
void
mux_ticcb(void)
{
	for (;;) {
		if (mididev_clksrc != NULL &&
		    mididev_clksrc->ticdelta < mididev_clksrc->ticrate) {
			mididev_clksrc->ticdelta += mux_ticrate;
			break;
		}
		if (mux_phase == MUX_FIRST) {
			mux_chgphase(MUX_NEXT);
		} else if (mux_phase == MUX_START) {
			mux_curpos = 0;
			mux_nextpos = mux_ticlength;
			mux_chgphase(MUX_FIRST);
		}
		if (mux_phase == MUX_NEXT) {
			mux_curtic++;
			mux_sendtic();
			song_movecb(usong);
		} else if (mux_phase == MUX_FIRST) {
			mux_curtic = 0;
			mux_sendtic();
			song_startcb(usong);
		}
		if (mididev_clksrc == NULL)
			break;
		mididev_clksrc->ticdelta -= mididev_clksrc->ticrate;
	}
}

/**
 * called when a MIDI START event is received from an external device
 * 外部デバイスからMIDI-STARTイベントを受け取った時に呼ばれるコールバック
 */
void
mux_startcb(void)
{
	if (mux_debug)
		log_puts("mux_startcb: got start event\n");
	if (mux_phase != MUX_STARTWAIT) {
		log_puts("mux_startcb: ignored MIDI start (not ready)\n");
		return;
	}

	/*
	 * if the MIDI START event comes from a device
	 * move to the beginning (we don't support SPP yet)
	 */
	if (mididev_clksrc) {
		mux_curpos = 0;
		mux_nextpos = mux_ticlength;
		song_gotocb(usong, LOC_MTC, 0);
	}
	mux_chgphase(MUX_START);
	mux_sendstart();
	mux_flush();
}

/*
 * called when a MIDI STOP event is received from an external device
 */
void
mux_stopcb(void)
{
	if (mux_debug)
		log_puts("mux_stopcb: got stop\n");
	if (mux_phase >= MUX_START && mux_phase <= MUX_NEXT)
		mux_sendstop();
	mux_chgphase(mux_reqphase);
	song_stopcb(usong);
	mux_flush();
}

/*
 * called when a MIDI Active-sensing is received from an external device
 */
void
mux_ackcb(unsigned unit)
{
	struct mididev *dev = mididev_byunit[unit];

	if (dev->isensto == 0) {
		cons_erru(dev->unit, "sensing enabled");
		dev->isensto = MIDIDEV_ISENSTO;
	}
}

/*
 * called when a MIDI voice event is received from an external device
 */
void
mux_evcb(unsigned unit, struct ev *ev)
{
	struct ev rev;
	struct mididev *dev = mididev_byunit[ev->dev];

#ifdef MUX_DEBUG
	if (mux_debug) {
		log_puts("mux_evcb: ");
		ev_log(ev);
		log_puts("\n");
	}
#endif
	if (conv_packev(&mux_istate, dev->ixctlset, dev->ievset, ev, &rev)) {
		norm_evcb(&rev);
	}
}

/*
 * called if an error is detected. currently we send an all note off
 * and all ctls reset
 */
void
mux_errorcb(unsigned unit)
{
	/*
	 * XXX: should stop only failed unit, not all devices
	 */
	norm_shut();
	mux_flush();
}

/*
 * called when an sysex has been received from an external device
 */
void
mux_sysexcb(unsigned unit, struct sysex *sysex)
{
	unsigned char *p, *q, *data;
	struct ev ev;
	unsigned cmd;

	if (sysex->first != NULL &&
	    sysex->first->next == NULL) {
		data = sysex->first->data;

		/*
		 * discard real-time messages, that should not be
		 * recorded
		 */
		if (sysex->first->used >= 6 &&
		    data[0] == 0xf0 &&
		    data[1] == 0x7f &&
		    data[3] == 1) {
			sysex_del(sysex);
			return;
		}

		/*
		 * handle custom events
		 */
		for (cmd = EV_PAT0; cmd < EV_PAT0 + EV_NPAT; cmd++) {
			if (evinfo[cmd].ev == NULL)
				continue;
			ev.v0 = ev.v1 = 0;
			p = evinfo[cmd].pattern;
			q = data;
			for (;; p++, q++) {
				switch (*p) {
				case EV_PATV0_HI:
					ev.v0 |= *q << 7;
					continue;
				case EV_PATV0_LO:
					ev.v0 |= *q;
					continue;
				case EV_PATV1_HI:
					ev.v1 |= *q << 7;
					continue;
				case EV_PATV1_LO:
					ev.v1 |= *q;
					continue;
				}
				if (*p != *q)
					break;
				if (*p == 0xf7) {
					ev.cmd = cmd;
					ev.dev = unit;
					norm_evcb(&ev);
					sysex_del(sysex);
					return;
				}
			}
		}
	}
	song_sysexcb(usong, sysex);
}

/*
 * flush all devices
 */
void
mux_flush(void)
{
	struct mididev *dev;

	for (dev = mididev_list; dev != NULL; dev = dev->next) {
		mididev_flush(dev);
	}
}

/*
 * return the current phase
 */
unsigned
mux_getphase(void)
{
	return mux_phase;
}

/*
 * change the tempo, the argument is tic length in 24th of
 * microseconds
 */
void
mux_chgtempo(unsigned long ticlength)
{
	if (mux_phase == MUX_FIRST || mux_phase == MUX_NEXT) {
		mux_nextpos += ticlength;
		mux_nextpos -= mux_ticlength;
	}
	mux_ticlength = ticlength;
}

/*
 * change the number of ticks per unit note; that's used to know for
 * instance that 1 of "our"ticks equals 2 ticks on that device...
 */
void
mux_chgticrate(unsigned tpu)
{
	mux_ticrate = tpu;
}

/**
 * start waiting for a MIDI START event (or generate one if we're the clock master).
 * MIDI-STARTイベントを待ち始める. 自分がclockマスターならclockを生成する
 */
void
mux_startreq(int manualstart)
{
	struct mididev *dev;
	static unsigned char mmc_start[] = { 0xf0, 0x7f, 0x7f, 0x06, 0x02, 0xf7 }; // mmc = midi machine control

	mux_manualstart = manualstart;
	mux_reqphase = MUX_STARTWAIT; /* スタートイベントを待つフェイズであると指示 */
	if (mux_phase != MUX_STOP) {
		log_puts("bad state to call mux_startreq()\n"); /* MUX_STOP状態以外で呼び出されてたらエラー */
		panic();
	}
	mux_chgphase(MUX_STARTWAIT);
	if (!mididev_clksrc && !mididev_mtcsrc) { // clksrc or mmtsrc が未定義なら自分がclock
		if (mux_debug)
			log_puts("mux_startreq: generated mtc start\n");
		mux_curpos = 0;
		mux_nextpos = MUX_START_DELAY;
		mux_mtcstart(0xdeadbeef);
	} else {
		mux_curpos = 0;
		mux_nextpos = mux_ticlength;
	}

	for (dev = mididev_list; dev != NULL; dev = dev->next) {
		if (dev->sendmmc)
			mididev_sendraw(dev, mmc_start, sizeof(mmc_start));
	}
}

/*
 * stop the MIDI clock
 */
void
mux_stopreq(void)
{
	struct mididev *dev;
	static unsigned char mmc_stop[] = { 0xf0, 0x7f, 0x7f, 0x06, 0x01, 0xf7 };

	mux_reqphase = MUX_STOP;
	if (mux_phase < MUX_STOP)
		mux_stopcb();

	for (dev = mididev_list; dev != NULL; dev = dev->next) {
		if (dev->sendmmc)
			mididev_sendraw(dev, mmc_stop, sizeof(mmc_stop));
	}
}

/*
 * relocate MIDI clock to given position
 */
void
mux_gotoreq(unsigned mmcpos)
{
#if DEFAULT_FPS == 25
#define FPS_ID	(1 << 5)
#endif
	struct mididev *dev;
	unsigned char mmc_reloc[13];

	mmc_reloc[0] =  0xf0;
	mmc_reloc[1] =  0x7f;
	mmc_reloc[2] =  0x7f;
	mmc_reloc[3] =  0x06;
	mmc_reloc[4] =  0x44;
	mmc_reloc[5] =  0x06;
	mmc_reloc[6] =  0x01;
	mmc_reloc[7] =  (mmcpos / (3600 * MTC_SEC)) % 24 | FPS_ID;
	mmc_reloc[8] =  (mmcpos / (60 * MTC_SEC)) % 60;
	mmc_reloc[9] =  (mmcpos / MTC_SEC) % 60;
	mmc_reloc[10] = (mmcpos / (MTC_SEC / DEFAULT_FPS)) % DEFAULT_FPS;
	mmc_reloc[11] = 0;
	mmc_reloc[12] = 0xf7;

	for (dev = mididev_list; dev != NULL; dev = dev->next) {
		if (dev->sendmmc)
			mididev_sendraw(dev, mmc_reloc, sizeof(mmc_reloc));
	}
}

