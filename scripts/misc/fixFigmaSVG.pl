#!/usr/bin/perl

open( IN, "< $ARGV[0]" ) || die "Can't open SVG file $ARGV[0]";
@lines = <IN>;
$defSection = "";
$inDef = 0;
foreach( @lines )
{
	if( m/<defs>/ )
	{
		$inDef = 1;
	}
	if( $inDef )
	{
		$defSection .= $_;
	}
	if( m:</defs>: )
	{
		$inDef = 0;
	}
}

foreach( @lines )
{
	if( m/<defs>/ )
	{
		$inDef = 1;
	}
	if( ! $inDef )
	{
		print;
	}
	if( m:<svg: ) {
		print $defSection;
		$defSection = "";
	}	
	if( m:</defs>: )
	{
		$inDef = 0;
	}
}
