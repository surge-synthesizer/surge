#!/usr/bin/perl

use File::Copy;

$f = $ARGV[0];
@files = ();
if( -f $ARGV[0] )
{
    push(@files, $ARGV[0])
}
if( -d $ARGV[0] )
{
    print "Converting Directory $ARGV[0]\n";
    opendir( $dh, $ARGV[0] ) || die "Can't scan directory $ARGV[0]";
    while( readdir $dh )
    {
        if( m/\.svg$/ )
        {
            push( @files, "$ARGV[0]/$_")
        }
    }
}

for $F (@files) {
    open( IN, "< $F" ) || die "Can't open SVG file $F";
    print "Moving $F -> ${F}.bak and upgrading\n";
    @lines = <IN>;
    close( IN );

    move( "$F", "${F}.bak");

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

    open (OUT, "> $F" ) || die "Can't open $F for writing";
    foreach( @lines )
    {
        if( m/<defs>/ )
        {
            $inDef = 1;
        }
        if( ! $inDef )
        {
            print OUT;
        }
        if( m:<svg: ) {
            print OUT $defSection;
            $defSection = "";
        }
        if( m:</defs>: )
        {
            $inDef = 0;
        }
    }
}
