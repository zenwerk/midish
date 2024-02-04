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

#ifndef MIDISH_DEFAULT_H
#define MIDISH_DEFAULT_H

/*
 * convert tempo (beats per minute) to tic length (number of 24-th of microsecond)
 *
 * テンポ（BPM）をticの長さに変換します。(24マイクロ秒の倍数)
 * 60 / bpm / timebase
 */
#define TEMPO_TO_USEC24(tempo,tpb) (60L * 24000000L / ((tempo) * (tpb)))

/*
 * units for absolute positions, ie we use MTC_SEC units per second
 * this number must be multiple of all supported quarter frame frequencies | TODO: fix typo mus->must
 * ie 96, 100, 120
 *
 * 絶対位置の単位 -> １秒間を何分割するか？の接待位置のことだろう(多分)
 * MTC_SEC/秒 を使用する場合、この値はサポートされているすべての 1/4フレーム周波数の倍数でなければならない
 * 96, 100, 120...
 */
#define MTC_SEC		2400

/*
 * MTC counters wrap every 24 hours
 *
 * 24時間ごとのMTCカウントラップ数
 */
#define MTC_PERIOD	(24 * 60 * 60 * MTC_SEC)

/*
 * special meaning controller numbers
 */
#define BANK_HI		0
#define BANK_LO		32
#define DATAENT_HI	6
#define DATAENT_LO	38
#define NRPN_HI		99
#define NRPN_LO		98
#define RPN_HI		101
#define RPN_LO		100

/*
 * define MIN and MAX values of event parameters
 */
#define TEMPO_MIN		TEMPO_TO_USEC24(240, TIMESIG_TICS_MAX)
#define TEMPO_MAX		TEMPO_TO_USEC24(20,  24)
#define TIMESIG_TICS_MAX	(TPU_MAX / 4)
#define TIMESIG_BEATS_MAX	100
#define TPU_MAX			(96 * 40)

/*
 * maximum number of midi devices supported by midish
 */
#define DEFAULT_MAXNDEVS	16

/*
 * maximum number of instruments
 */
#define DEFAULT_MAXNCHANS	(DEFAULT_MAXNDEVS * 16)

/*
 * maximum number of events
 */
#define DEFAULT_MAXNSEQEVS	400000

/*
 * maximum number of tracks
 */
#define DEFAULT_MAXNSEQPTRS	200

/*
 * maximum number of filter states (roughly maximum number of
 * simultaneous notes
 */
#define DEFAULT_MAXNSTATES	10000

/*
 * maximum number of system exclusive messages
 */
#define DEFAULT_MAXNSYSEXS	2000

/*
 * maximum number of chunks (each sysex is a set of chunks)
 */
#define DEFAULT_MAXNCHUNKS	(DEFAULT_MAXNSYSEXS * 2)

/*
 * default number of Tick Per Beat -> ４分音符一つを24分割 -> タイムベースの値
 */
#define DEFAULT_TPB		24

/*
 * default beats per measure
 */
#define DEFAULT_BPM		4

/*
 * default number of Tic Per Unit note
 */
#define DEFAULT_TPU		96

/*
 * default tempo (BPM)
 */
#define DEFAULT_TEMPO		120

/*
 * default tempo in 24-th of microsecond period
 */
#define DEFAULT_USEC24		TEMPO_TO_USEC24(DEFAULT_TEMPO, DEFAULT_TPB)

/*
 * default MTC or MMC frames-per-second, used to transmit initial
 * postition when starting. The choice is arbitrary, be we use 25fps as
 * the period is multiple of 1ms.
 */
#define DEFAULT_FPS		25

/*
 * number of milliseconds to wait between the instrumet config is sent
 * and the playback is stared
 */
#define DEFAULT_CHANWAIT	200

/*
 * nmber of milliseconds to wait after each sysex message is sent
 */
#define DEFAULT_SXWAIT		20

/*
 * metronome click length in 24-th of microsecond (30ms)
 */
#define DEFAULT_METRO_CLICKLEN	(24 * 1000 * 30)

/*
 * default metronome device and midi channel
 */
#define DEFAULT_METRO_DEV	0
#define DEFAULT_METRO_CHAN	9

/*
 * default metronome click notes and velocities
 */
#define DEFAULT_METRO_HI_NOTE	67
#define DEFAULT_METRO_HI_VEL	127
#define DEFAULT_METRO_LO_NOTE	68
#define DEFAULT_METRO_LO_VEL	90

/*
 * max memory usage allowed for undo
 */
#define UNDO_MAXSIZE		(4 * 1024 * 1024)

/*
 * output source prioriries
 */
#define PRIO_INPUT		0
#define PRIO_TRACK		1
#define PRIO_CHAN		2

/*
 * how to relocate, used by song_loc() & friends
 */
#define LOC_MEAS 	0	/* measure number */
#define LOC_MTC 	1	/* MTC/MMC absolute time */
#define LOC_SPP 	2	/* MIDI song position pointer */

#endif /* MIDISH_DEFAULT_H */
