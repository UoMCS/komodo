#! /bin/sh
exec perl -x $0 ${1+"$@"}
	if 0;
#!perl

while (<>)
{
	chomp;
    s/\\/\\\\/g;
    s/"/\\"/g;
	print "\"$_\\n\"\n";
}
