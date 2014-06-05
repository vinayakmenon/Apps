#!/usr/bin/perl
# 
# This script can be used to decode the messages received
# via STM using MIPI or phonet protocol.
#
# ost and phonet decoder.
# Version 1.0.
# Author: Vinayak Menon <vinayakm.list@gmail.com>
#
#****************************************************************************/
use strict;

# OST
use constant OST_VERSION => 0x10;
use constant OST_ENTITY_ID => 0x00;
use constant OST_PROTOCOL_XTIV3 => 0x84;

# Phonet
use constant TB_MEDIA_TCPIP => 0x1D;
use constant TB_DEV_PC => 0x10;
use constant TB_DEV_TRACEBOX => 0x4C;

use constant OST => 0;
use constant PHONET => 1;

# ost message format
use constant OST_VERSIONNUM => 0;
use constant OST_ENTITY => 1;
use constant OST_PROTOCOL_ID => 2;
use constant OST_LENGTH => 3;
use constant OST_EXTENTED_LENGTH => 4;
use constant OST_TRACEBOX_PORT => OST_EXTENTED_LENGTH;
use constant OST_MASTER_ID => OST_TRACEBOX_PORT + 1;
use constant OST_CHANNEL_ID => OST_TRACEBOX_PORT + 2;
use constant OST_TIMESTAMP => OST_TRACEBOX_PORT + 3;
use constant OST_EXT_TRACEBOX_PORT => 8;
use constant OST_EXT_MASTER_ID => OST_EXT_TRACEBOX_PORT + 1;
use constant OST_EXT_CHANNEL_ID => OST_EXT_TRACEBOX_PORT + 2;
use constant OST_EXT_TIMESTAMP => OST_EXT_TRACEBOX_PORT + 3;
use constant OST_DATA => 15;
use constant OST_EXT_DATA => 19;

use constant PH_MEDIA => 0;
use constant PH_RECEIVER_DEVICE => 1;
use constant PH_SENDER_DEVICE => 2;
use constant PH_UNUSED => 3;
use constant PH_MESSAGE_LENGTH => 4;
use constant PH_RECEIVER_OBJECT => 6;
use constant PH_SENDER_OBJECT => 7;
use constant PH_TRANSACTION_ID => 8;
use constant PH_MESSAGE_ID => 9;
use constant PH_MASTER_ID => 10;
use constant PH_CHANNEL_ID => 11;
use constant PH_TIMESTAMP => 12;
use constant PH_BODY => 20;

use constant SEEK_SET => 0;
use constant SEEK_CUR => 1;

my $header;

sub help {
	print "\n\nost_phonet_parser:\n";
	print "Usage:\n";
	print "perl ost_phonet_parser.pl [ost/phonet file]\n";
	print "--> If second argument is present and 1, header is displayed\n";
	print "Header display format:\n";
	print "OST--> <Version, Entity ID, Protocol ID, Length, Extended length (if present),\n";
	print "Tracebox port, Master ID, Channel ID, Timestamp flag, Timestamp>\n";
	print "Phonet--> <Media, Receiver device, Sender device, 0x7C, Message length,\n";
	print "Receiver object, Sender object, Transaction ID, Message ID, Master ID,\n";
	print "Channel ID, Timestamp>\n\n";
}

sub detect_format
{
	my $buf;
	my @chars;

	if (3 != read(FP, $buf, 3)) {
		print "Error detecting format: $!: ".(caller(0))[3]." ".__LINE__."\n";
		return -1;
	}

	@chars = split(//, $buf);

	if ((ord($chars[0]) == OST_VERSION) &&
		(ord($chars[1]) == OST_ENTITY_ID) && 
		(ord($chars[2]) == OST_PROTOCOL_XTIV3)) {
		print "OST format detected\n";
		return OST;
	} elsif ((ord($chars[0]) == TB_MEDIA_TCPIP) &&
		(ord($chars[1]) == TB_DEV_PC) &&
		(ord($chars[2]) == TB_DEV_TRACEBOX)) {
		print "Phonet format detected\n";
		return PHONET;
	} else {
		print "Error detecting format\n";
		return -1;
	}

}

sub b_to_l
{
	my $ret;

	$ret = (((0xFF00000000000000 & $_[0]) >> 56) |
			((0x00FF000000000000 & $_[0]) >> 40) |
			((0x0000FF0000000000 & $_[0]) >> 24) |
			((0x000000FF00000000 & $_[0]) >> 8)  |
			((0x00000000000000FF & $_[0]) << 56) |
			((0x000000000000FF00 & $_[0]) << 40) |
			((0x0000000000FF0000 & $_[0]) << 24) |
			((0x00000000FF000000 & $_[0]) << 8));
	return $ret;
}

sub decode_ost
{
	my $length;
	my $buf = 0;
	my $ext_length = 0;
	my $data_length;
	my $data_pointer;
	my $timestamp;
	my $le_timestamp;
	my @chars;

	while (!eof(FP)) {
		if ($header) {
			printf "<0x%x, ", ord(getc(FP));
			printf "0x%x, ", ord(getc(FP));
			printf "0x%x, ", ord(getc(FP));
		} else {
			seek(FP, OST_LENGTH, SEEK_CUR);
		}
		
		$length = ord(getc(FP));

		if ($header) {
			printf "%d, ", $length;
		}

		if (!$length) {
			if (4 != read(FP, $buf, 4)) {
				print "read error: $!: ".(caller(0))[3]." ".__LINE__."\n";
				return -1;
			}
			@chars = split(//, $buf);
			$ext_length = ord($chars[0]) | (ord($chars[1]) << 8) |
				(ord($chars[2]) << 16) | (ord($chars[3]) << 24);
			$data_length = $ext_length - (OST_EXT_DATA - OST_EXT_TRACEBOX_PORT);
			$data_pointer = OST_EXT_DATA - OST_EXT_TRACEBOX_PORT;
			if ($header) {
				printf "%d, ", $ext_length;
			}
		} else {
			$data_length = $length - (OST_DATA - OST_TRACEBOX_PORT);
			$data_pointer = OST_DATA - OST_TRACEBOX_PORT;
		}
		
		if ($header) {
			
			$buf = 0;
			read(FP, $buf, 3);
			@chars = split(//, $buf);
			printf "%d, %d, %d ", ord($chars[2]), ord($chars[1]), ord($chars[0]);

			$buf = 0;
			read(FP, $buf, 8);
			@chars = split(//, $buf);
			$timestamp = ord($chars[0]) | (ord($chars[1]) << 8) |
				(ord($chars[2]) << 16) | (ord($chars[3]) << 24) |
				(ord($chars[4]) << 32) | (ord($chars[5]) << 40) |
				(ord($chars[6]) << 48) | (ord($chars[7]) << 56);
			$le_timestamp = b_to_l($timestamp);
			printf "0x%x,", (0xF000000000000000 & $le_timestamp) >> 56;
			printf "%llu> ", ($le_timestamp & 0x0FFFFFFFFFFFFFFF);
		} else {
			seek(FP, $data_pointer, SEEK_CUR);
		}
		
		$buf = 0;

		if ($data_length != read(FP, $buf, $data_length)) {
			print "read error: $!: ".(caller(0))[3]." ".__LINE__."\n";
			return -1;
		}

		# There exist an undocumented 'r' at the begining
		# of each data packet. Remove it.
		#
		@chars = split(//,$buf);
		shift(@chars);
		$buf = join('',@chars);
		printf "%s", $buf;
		
	}
	
	return 0;
}

sub swap_short
{
	my $ret;

	$ret = ((0xFF00 & $_[0]) >> 8) | ((0x00FF & $_[0]) << 8);
	
	return $ret;
}

sub decode_phonet
{
	my $length;
	my $buf = 0;
	my $data_length;
	my $timestamp;
	my $le_timestamp;
	my @chars;

	while (!eof(FP)) {
		if ($header) {
			printf "<0x%x, ", ord(getc(FP));
			printf "0x%x, ", ord(getc(FP));
			printf "0x%x, ", ord(getc(FP));
			printf "0x%x, ", ord(getc(FP));
		} else {
			seek(FP, PH_MESSAGE_LENGTH, SEEK_CUR);
		}
		
		if (2 != read(FP, $data_length, 2)) {
			print "read error: $!: ".(caller(0))[3]." ".__LINE__."\n";
			return -1;
		}

		@chars = split(//, $data_length);
		$data_length = ord($chars[0]) | (ord($chars[1]) << 8);
		$data_length = swap_short($data_length);

		if ($header) {
			printf "%d, ", $data_length;
		}

		$data_length -= (PH_BODY - PH_RECEIVER_OBJECT);

		if ($header) {
			
			$buf = 0;
			read(FP, $buf, 6);
			@chars = split(//, $buf);
			printf "0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, ",
				ord($chars[5]), ord($chars[4]), ord($chars[3]),
				ord($chars[2]), ord($chars[1]), ord($chars[0]);

			$buf = 0;
			read(FP, $buf, 8);
			@chars = split(//, $buf);
			$timestamp = ord($chars[0]) | (ord($chars[1]) << 8) |
				(ord($chars[2]) << 16) | (ord($chars[3]) << 24) |
				(ord($chars[4]) << 32) | (ord($chars[5]) << 40) |
				(ord($chars[6]) << 48) | (ord($chars[7]) << 56);
			$le_timestamp = b_to_l($timestamp);
			printf "0x%x,", (0xF000000000000000 & $le_timestamp) >> 56;
			printf "%llu> ", ($le_timestamp & 0x0FFFFFFFFFFFFFFF);
		} else {
			seek(FP, (PH_BODY - PH_RECEIVER_OBJECT), SEEK_CUR);
		}
		
		$buf = 0;

		if ($data_length != read(FP, $buf, $data_length)) {
			print "read error: $!: ".(caller(0))[3]." ".__LINE__."\n";
			return -1;
		}

		# There exist an undocumented 'r' at the begining
		# of each data packet. Remove it.
		#
		@chars = split(//,$buf);
		shift(@chars);
		$buf = join('',@chars);
		printf "%s", $buf;
		
	}
	
	return 0;
}

if (($#ARGV < 0) || ($#ARGV > 1)) {
	help();
	die "Input proper arguments\n"
}

if (($#ARGV == 1) && ($ARGV[1] == 1)) {
	$header = 1;
}

open(FP, "< :raw", $ARGV[0]) || die "cant open $ARGV[0]: $!";

my $format = detect_format();

if ($format < 0) {
	goto END;
}

seek(FP, 0, SEEK_SET);

if ($format == OST) {
	decode_ost();
} elsif ($format == PHONET) {
	decode_phonet();
}

END:
close(FP);
