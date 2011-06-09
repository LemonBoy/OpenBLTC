#!/usr/local/bin/perl

# debin.pl
# The Lemon Man (C) 2011
# Tool to extract sb blobs out of usbsnoop logs
# Pass your packet size as argument if different from 0x41

sub serialize 
{
    my @bytes = split(' ', $_[0]);
    my $len = @bytes;
    for my $j (0 .. $len-1) {
        if ($j == 0 and $_[1] == 1) {
            next;
        }
        print OUTPUT chr(hex($bytes[$j])); 
    } 
}

if ($#ARGV != 0) {
    $packetSize = 0x41;
} else {
    $packetSize = hex($ARGV[0]);
}

print "Packet size is $packetSize\n";

open OUTPUT, ">blob.sb";
binmode OUTPUT;

while (<STDIN>) {
	my ($line) = $_;
	chomp($line);
	
    if ($line =~ /^-- /) {
	    if ($line =~ /URB_FUNCTION_CLASS_INTERFACE/) {
		    $valid = 1;
		    $firstLine = 1;
	    } else {
	        $valid = 0;
	    }
	}
    elsif ($valid == 1 and $line =~ /TransferBufferLength = (\d{8})/) {
	    $writeSize = hex($1);
	    if ($writeSize != $packetSize) {
	        $valid = 0;
	    }
	} elsif ($valid == 1 and $line =~ /    (\d{8}): (.*)/) {
        serialize($2, $firstLine);
        if ($firstLine == 1) {
            $firstLine = 0;
        }
	}
}
