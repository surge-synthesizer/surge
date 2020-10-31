#!/usr/bin/perl

use File::Find;

@skins = ();
sub tutSkin {
    if( m/skin.xml/ )
    {
        push( @skins, "$File::Find::dir/$_" )
    }
}

find( \&tutSkin, './resources/data/skins/Tutorial/' );

sub byDir {
    $a1 = $a;
    $b1 = $b;
    $a1 =~ s:^.*/([0-9]+) .*$:\1:;
    $b1 =~ s:^.*/([0-9]+) .*$:\1:;

    $a1 = int($a1);
    $b1 = int( $b1);
    print "$a1 <=> $b1\n";
    $a1 <=> $b1;
}

open( OUT, "> skintmp.md" );
sub dumpSkin {
    print "Dumping $_[0]\n";
    $f = $_[0];
    $f =~ s/^[^0-9]*([0-9]+ .*).surge-skin.*$/\1/;
    print OUT "# $f\n\n";
    open( IN, "< $_[0]" );
    $inXml = 0;
    while( <IN> )
    {
        if( m/--\[doc\]/ )
        {
            if( $inXml )
            {
                print OUT "\n```\n\n";
                $inXml = 0;
            }
        }
        elsif (m/\[doc\]--/ )
        {
            print OUT "\n\n```\n";
            $inXml = 1;
        }
        else
        {
            if( ! $inXml  )
            {
                if( m/[A-Za-z]+/ )
                {
                    s/^(\s*)//;
                }
            }
            print OUT;
        }
    }
    if( $inXml )
    {
        print OUT "\n```\n";
    }
}

@skins = sort byDir @skins ;
for $s ( @skins )
{
    dumpSkin($s);
}
