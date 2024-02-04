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

#ifndef MIDISH_MIDIDEV_H
#define MIDISH_MIDIDEV_H

/*
 * timeouts for active sensing
 * (as usual units are 24th of microsecond)
 */
#define MIDIDEV_OSENSTO		(250 * 24 * 1000)
#define MIDIDEV_ISENSTO		(350 * 24 * 1000)

/*
 * modes for devices
 */
#define MIDIDEV_MODE_IN		1	/* can input */
#define MIDIDEV_MODE_OUT	2	/* can output */

/*
 * device output buffer length in bytes
 */
#define MIDIDEV_BUFLEN	0x400

struct pollfd;
struct mididev;
struct ev;

/**
 * MIDIデバイスオペレーションズ
 * alsa/raw/sndio でそれぞれの固有のAPIをこの構造体で抽象化する
 * DEFINE のコンパイルスイッチで {alsa,raw,sndio}_new でdevopsを返す
 * mididev構造体はdevopsを通じてMIDI機器に命令を送受信する
 */
struct devops {
	/*
	 * open the device or set the ``eof'' flag on error
	 */
	void (*open)(struct mididev *);
	/*
	 * try to read the given number of bytes, and return the number
	 * of bytes actually read, set the ``eof'' flag on error
	 */
	unsigned (*read)(struct mididev *, unsigned char *, unsigned);
	/*
	 * try to write the given number of bytes, and return the number
	 * of bytes actually written, set the ``eof'' flag on error
	 */
	unsigned (*write)(struct mididev *, unsigned char *, unsigned);
	/*
	 * En: return the number of pollfd structures the device requires
	 * Ja: デバイスが必要とする pollfd構造体の数を返す
	 */
	unsigned (*nfds)(struct mididev *);
	/*
	 * En:
	 *   fill the given array of pollfd structures with the given
	 *   events so that poll(2) can be called, return the number of
	 *   elements filled
	 * Ja:
	 *   poll(2) を呼び出すことができるように
	 *   "イベントを指定したPollfd構造体"の配列を埋める|埋めた数を返す
	 */
	unsigned (*pollfd)(struct mididev *, struct pollfd *, int);
	/*
	 * return the events set in the array of pollfd structures set
	 * by the poll(2) syscall
	 * poll-syscall でセットされたpollfd構造体の配列に設定されたevent達を返す
	 */
	int (*revents)(struct mididev *, struct pollfd *);
	/*
	 * close the device
	 */
	void (*close)(struct mididev *);
	/*
	 * free the mididev structure and associated resources
	 */
	void (*del)(struct mididev *);
};

/**
 * private structure for the MTC messages parser
 * MTC とは Midi Time Code のこと
 */
struct mtc {
	unsigned char nibble[8];	/* nibbles(1/2byte=4bit) of hr:min:sec:fr */
	unsigned qfr;			/* quarter frame counter */
	unsigned tps;			/* ticks per second */
	unsigned pos;			/* absolute tick */
#define MTC_STOP	0		/* stopped */
#define MTC_START	1		/* got a full frame but no tick yet */
#define MTC_RUN		2		/* got at least 1 tick */
	unsigned state;			/* one of above */
	unsigned timo;
};

/**
 * Midi device を表す構造体
 */
struct mididev {
	struct devops *ops;

	/*
	 * device list and iteration stuff
	 */
	struct pollfd *pfd;
	struct mididev *next;

	/*
	 * device settings
	 */
	unsigned unit;			/* index in the mididev table | mididevテーブルインデクス */
	unsigned ticrate, ticdelta;	/* tick rate (default 96) */
	unsigned sendclk;		/* send MIDI clock */
	unsigned sendmmc;		/* send MMC start/stop/relocate | MMC = Midi Machine Control */
	unsigned isensto, osensto;	/* active sensing timeouts */
	unsigned mode;			/* read, write */
	unsigned ixctlset, oxctlset;	/* bitmap of 14bit controllers */
	unsigned ievset, oevset;	/* bitmap of CONV_{XPC,NRPN,RPN} */
	unsigned eof;			/* i/o error pending */
	unsigned runst;			/* use running status for output */
	unsigned sync;			/* flush buffer after each message */

	/*
	 * midi events parser state
	 */
	unsigned	  istatus;		/* input running status */
	unsigned 	  icount;		/* bytes in idata[] */
	unsigned char	  idata[2];		/* current event's data */
	struct sysex	 *isysex;		/* input sysex */
	struct mtc	  imtc;			/* MTC parser */
	unsigned 	  oused;		/* bytes in obuf */
	unsigned	  ostatus;		/* output running status */
	unsigned char	  obuf[MIDIDEV_BUFLEN];	/* output buffer */
};

void mididev_init(struct mididev *, struct devops *, unsigned);
void mididev_done(struct mididev *);
void mididev_flush(struct mididev *);
void mididev_putstart(struct mididev *);
void mididev_putstop(struct mididev *);
void mididev_puttic(struct mididev *);
void mididev_putack(struct mididev *);
void mididev_putev(struct mididev *, struct ev *);
void mididev_sendraw(struct mididev *, unsigned char *, unsigned);
void mididev_open(struct mididev *);
void mididev_close(struct mididev *);
void mididev_inputcb(struct mididev *, unsigned char *, unsigned);

void mtc_timo(struct mtc *); /* XXX, use timeouts */

extern unsigned mididev_debug;

extern struct mididev *mididev_list;
extern struct mididev *mididev_clksrc;
extern struct mididev *mididev_mtcsrc;
extern struct mididev *mididev_byunit[];

struct mididev *raw_new(char *, unsigned);
struct mididev *alsa_new(char *, unsigned);
struct mididev *sndio_new(char *, unsigned);

void mididev_listinit(void);
void mididev_listdone(void);
unsigned mididev_attach(unsigned, char *, unsigned);
unsigned mididev_detach(unsigned);

#endif /* MIDISH_MIDIDEV_H */
