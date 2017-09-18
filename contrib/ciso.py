#!/usr/bin/python

# Copyright (c) 2011 by Virtuous Flame
# Based BOOSTER 1.01 CSO/ZSO Compressor
#
# GNU General Public Licence (GPL)
# 
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; either version 2 of the License, or (at your option) any later
# version.
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc., 59 Temple
# Place, Suite 330, Boston, MA  02111-1307  USA
#

from __future__ import print_function

__author__ = "Virtuous Flame"
__license__ = "GPL"
__version__ = "2.0"

import os
import sys
import lz4
import zlib

from getopt import *
from struct import pack, unpack
from multiprocessing.pool import Pool

CISO_MAGIC = 0x4F534943
ZISO_MAGIC = 0x4F53495A
DEFAULT_ALIGN = 0
COMPRESS_THREHOLD = 100
DEFAULT_PADDING = br'X'

USE_LZ4 = False
MP = False
MP_NR = 1024 * 16

def hexdump(data):
	for i in data:
		print("%02X " % i, end="")
	print("")

def zip_compress(plain, level=9):
	if not USE_LZ4:
		compressed = zlib.compress(plain, level)
		return compressed[2:]
	else:		
		compressed = lz4.compress(plain) if level < 9 else lz4.compressHC(plain)
		return compressed[4:]

	# assert(compressed.startswith(b"\x78"))
	# We have to remove the 0xXX78 header

def zip_compress_mp(i):
	try:
		return zip_compress(i[0], i[1])
	except zlib.error as e:
		print("mp error: %s" % (e))
		sys.exit(-1)


def zip_decompress(compressed, magic):
	#   hexdump(data)
	if magic == CISO_MAGIC:
		return zlib.decompress(compressed, -15)
	elif magic == ZISO_MAGIC:
		return lz4.decompress(b'\x00\x08\x00\x00' + compressed)


def usage():
	print("Usage: ciso [-c level] [-m] [-t percent] [-h] infile outfile")
	print("  -c level: 1-9 compress ISO to CSO (1=fast/large - 9=small/slow")
	print("         0   decompress CSO to ISO")
	print("         When using LZ4, 1-8: normal, 9: HC compression")
	print("  -m Use multiprocessing acceleration for compressing")
	print("  -t percent Compression Threshold (1-100)")
	print("  -a align Padding alignment 0=small/slow 6=fast/large")
	print("  -p pad Padding byte")
	print("  -z use LZ4 compression")
	print("  -h this help")

def open_input_output(fname_in, fname_out):
	try:
		fin = open(fname_in, "rb")
	except IOError:
		print("Can't open %s" % (fname_in))
		sys.exit(-1)

	try:
		fout = open(fname_out, "wb")
	except IOError:
		print("Can't create %s" % (fname_out))
		sys.exit(-1)

	return fin, fout

def seek_and_read(fin, offset, size):
	fin.seek(offset)
	return fin.read(size)

def read_cso_header(fin):
	"""CSO header has 0x18 bytes"""
	data = seek_and_read(fin, 0, 0x18)
	magic, header_size, total_bytes, block_size, ver, align = unpack('IIQIbbxx', data)
	return magic, header_size, total_bytes, block_size, ver, align

def generate_cso_header(magic, header_size, total_bytes, block_size, ver, align):
	data = pack('IIQIbbxx', magic, header_size, total_bytes, block_size, ver, align)
#	assert(len(data) == 0x18)
	return data

def show_cso_info(magic, fname_in, fname_out, total_bytes, block_size, total_block, align):
	if magic == CISO_MAGIC:
		compression_type = "gzip"
	elif magic == ZISO_MAGIC:
		compression_type = "LZ4"
	else:
		compression_type = "unknown"

	print("Decompress '%s' to '%s'" % (fname_in, fname_out))
	print("Compression type: %s" % compression_type)
	print("Total File Size %ld bytes" % total_bytes)
	print("block size	  %d  bytes" % block_size)
	print("total blocks	%d  blocks" % total_block)
	print("index align	 %d" % (1 << align))

def decompress_cso(fname_in, fname_out, level):
	fin, fout = open_input_output(fname_in, fname_out)
	magic, header_size, total_bytes, block_size, ver, align = read_cso_header(fin)

	if (magic != CISO_MAGIC and magic != ZISO_MAGIC) or block_size == 0 or total_bytes == 0:
		print("ciso/ziso file format error")
		return -1

	total_block = total_bytes // block_size
	index_buf = []

	for i in range(total_block + 1):
		index_buf.append(unpack('I', fin.read(4))[0])

	show_cso_info(magic, fname_in, fname_out, total_bytes, block_size, total_block, align)

	block = 0
	percent_period = total_block // 100
	percent_cnt = 0

	while block < total_block:
		percent_cnt += 1
		if percent_cnt >= percent_period and percent_period != 0:
			percent_cnt = 0
			print("decompress %d%%\r" % (block // percent_period), file=sys.stderr, end=""),

		index = index_buf[block]
		plain = index & 0x80000000
		index &= 0x7fffffff
		read_pos = index << (align)

		if plain:
			read_size = block_size
		else:
			index2 = index_buf[block + 1] & 0x7fffffff
			# Have to read more bytes if align was set
			if align != 0:
				read_size = (index2 - index + 1) << align
			else:
				read_size = (index2 - index) << align

		cso_data = seek_and_read(fin, read_pos, read_size)

		if plain:
			dec_data = cso_data
		else:
			try:
				dec_data = zip_decompress(cso_data, magic)
			except zlib.error as e:
				print("%d block: 0x%08X %d %s" % (block, read_pos, read_size, e))
				sys.exit(-1)

		fout.write(dec_data)
		block += 1

	fin.close()
	fout.close()
	print("ciso decompress completed")

def show_comp_info(fname_in, fname_out, total_bytes, block_size, align, level):
	print("Compress '%s' to '%s'" % (fname_in, fname_out))
	print("Compression type: %s" % ("gzip" if not USE_LZ4 else "LZ4"))
	print("Total File Size %ld bytes" % total_bytes)
	print("block size	  %d  bytes" % block_size)
	print("index align	 %d" % (1 << align))
	print("compress level  %d" % level)
	if MP:
		print("multiprocessing %s" % MP)

def set_align(fout, write_pos, align):
	if write_pos % (1 << align):
		align_len = (1 << align) - write_pos % (1 << align)
		fout.write(DEFAULT_PADDING * align_len)
		write_pos += align_len

	return write_pos

def compress_cso(fname_in, fname_out, level):
	fin, fout = open_input_output(fname_in, fname_out)
	fin.seek(0, os.SEEK_END)
	total_bytes = fin.tell()
	fin.seek(0)

	header_size, block_size, ver, align = 0x18, 0x800, 1, DEFAULT_ALIGN
	magic = ZISO_MAGIC if USE_LZ4 else CISO_MAGIC

	# We have to use alignment on any CSO files which > 2GB, for MSB bit of index as the plain indicator
	# If we don't then the index can be larger than 2GB, which its plain indicator was improperly set
	if total_bytes >= 2 ** 31 and align == 0:
		align = 1

	header = generate_cso_header(magic, header_size, total_bytes, block_size, ver, align)
	fout.write(header)

	total_block = total_bytes // block_size
	index_buf = [0 for i in range(total_block + 1)]

	fout.write(b"\x00\x00\x00\x00" * len(index_buf))
	show_comp_info(fname_in, fname_out, total_bytes, block_size, align, level)

	write_pos = fout.tell()
	percent_period = total_block // 100
	percent_cnt = 0

	if MP:
		pool = Pool()
	else:
		pool = None

	block = 0
	while block < total_block:
		if MP:
			percent_cnt += min(total_block - block, MP_NR)
		else:
			percent_cnt += 1

		if percent_cnt >= percent_period and percent_period != 0:
			percent_cnt = 0

			if block == 0:
				print("compress %3d%% avarage rate %3d%%\r" % (
					block // percent_period
					, 0), file=sys.stderr, end="")
			else:
				print("compress %3d%% avarage rate %3d%%\r" % (
					block // percent_period
					, 100 * write_pos // (block * 0x800)), file=sys.stderr, end="")

		if MP:
			iso_data = [(fin.read(block_size), level) for i in range(min(total_block - block, MP_NR))]
			cso_data_all = pool.map_async(zip_compress_mp, iso_data).get(9999999)

			for i in range(len(cso_data_all)):
				write_pos = set_align(fout, write_pos, align)
				index_buf[block] = write_pos >> align
				cso_data = cso_data_all[i]

				if 100 * len(cso_data) // len(iso_data[i][0]) >= min(COMPRESS_THREHOLD, 100):
					cso_data = iso_data[i][0]
					index_buf[block] |= 0x80000000  # Mark as plain
				elif index_buf[block] & 0x80000000:
					print("Align error, you have to increase align by 1 or CFW won't be able to read offset above 2 ** 31 bytes")
					sys.exit(1)

				fout.write(cso_data)
				write_pos += len(cso_data)
				block += 1
		else:
			iso_data = fin.read(block_size)

			try:
				cso_data = zip_compress(iso_data, level)
			except zlib.error as e:
				print("%d block: %s" % (block, e))
				sys.exit(-1)

			write_pos = set_align(fout, write_pos, align)
			index_buf[block] = write_pos >> align

			if 100 * len(cso_data) // len(iso_data) >= COMPRESS_THREHOLD:
				cso_data = iso_data
				index_buf[block] |= 0x80000000  # Mark as plain
			elif index_buf[block] & 0x80000000:
				print("Align error, you have to increase align by 1 or CFW won't be able to read offset above 2 ** 31 bytes")
				sys.exit(1)

			fout.write(cso_data)
			write_pos += len(cso_data)
			block += 1

	# Last position (total size)
	index_buf[block] = write_pos >> align

	# Update index block
	fout.seek(len(header))
	for i in index_buf:
		idx = pack('I', i)
#		assert(len(idx) == 4)
		fout.write(idx)

	print("ciso compress completed , total size = %8d bytes , rate %d%%" % (write_pos, (write_pos * 100 // total_bytes)))

	fin.close()
	fout.close()

def parse_args():
	global MP, COMPRESS_THREHOLD, DEFAULT_PADDING, DEFAULT_ALIGN, USE_LZ4

	if len(sys.argv) < 2:
		usage()
		sys.exit(-1)

	try:
		optlist, args = gnu_getopt(sys.argv, "c:mt:a:p:h:z")
	except GetoptError as err:
		print(str(err))
		usage()
		sys.exit(-1)

	level = None

	for o, a in optlist:
		if o == '-c':
			level = int(a)
		elif o == '-m':
			MP = True
		elif o == '-t':
			COMPRESS_THREHOLD = min(int(a), 100)
		elif o == '-a':
			DEFAULT_ALIGN = int(a)
		elif o == '-p':
			DEFAULT_PADDING = bytes(a[0])
		elif o == '-z':
			USE_LZ4 = True
		elif o == '-h':
			usage()
			sys.exit(0)

	if level == None:
		print("You have to specify compress level")
		sys.exit(-1)

	try:
		fname_in, fname_out = args[1:3]
	except ValueError as e:
		print("You have to specify input/output filename")
		sys.exit(-1)

	return level, fname_in, fname_out

def load_sector_table(sector_table_fn, total_block, default_level = 9):
	""" In future we will support NC """
	sectors = [default_level for i in range(total_block)]

	with open(sector_table_fn) as f:
		for line in f:
			line = line.strip()
			a = line.split(":")

			if len(a) < 2:
				raise ValueError("Invalid line founded: %s" % line)

			if -1 == a[0].find("-"):
				try:
					sector, level = int(a[0]), int(a[1])
				except ValueError:
					raise ValueError("Invalid line founded: %s" % line)
				if level < 1 or level > 9:
					raise ValueError("Invalid line founded: %s" % line)
				sectors[sector] = level
			else:
				b = a[0].split("-")
				try:
					start, end, level = int(b[0]), int(b[1]), int(a[1])
				except ValueError:
					raise ValueError("Invalid line founded: %s" % line)
				i = start
				while i < end:
					sectors[i] = level
					i += 1

	return sectors

def main():
	print("ciso-python %s by %s" % (__version__, __author__))
	level, fname_in, fname_out = parse_args()

	if level == 0:
		decompress_cso(fname_in, fname_out, level)
	else:
		compress_cso(fname_in, fname_out, level)

PROFILE = False

if __name__ == "__main__":
	if PROFILE:
		import cProfile
		cProfile.run("main()")
	else:
		main()
