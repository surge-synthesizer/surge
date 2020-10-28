#!/usr/bin/perl

$aw = $ARGV[1] || "../../../airwindows/";
$pl = $ARGV[0];

print( "Grabbing '$pl' from '$aw'\n" );

open( IN, "< $aw/plugins/MacVST/$pl/source/${pl}.h" ) || die " $aw/plugins/MacVST/$pl/${pl}.h $!";
open( OUT, "> src/$pl.h" );
$openNS = 0;
while( <IN> )
{
    s/audioeffectx.h/airwindows\/AirWinBaseClass.h/;
    if( m/enum/ && !$openNS )
    {
        $openNS = 1;
        print OUT "namespace $pl {\n\n";
    }
    if( m/\#endif/ && $openNS )
    {
        print( OUT "} // end namespace $pl\n\n" );
    }
    print OUT;
}
close( IN );
close( OUT );

open( IN, "< $aw/plugins/MacVST/$pl/source/${pl}Proc.cpp" ) || die " $aw/plugins/MacVST/$pl/source/${pl}Proc.cpp $!";
open( OUT, "> src/${pl}Proc.cpp" );
while( <IN> )
{
    print OUT;

    if( m/\#endif/ )
    {
        print( OUT "\nnamespace $pl {\n\n" );
    }
}

print( OUT "\n\n} // end namespace $pl\n\n" );
close( IN );
close( OUT );


open( IN, "< $aw/plugins/MacVST/$pl/source/${pl}.cpp" ) || die " $aw/plugins/MacVST/$pl/source/${pl}.cpp $!";
open( OUT, "> src/${pl}.cpp" );
while( <IN> )
{
    s:^(AudioEffect.*create):// \1:;

    print OUT;
    
    if( m/\#endif/ )
    {
        print( OUT "\nnamespace $pl {\n\n" );
    }
}

print( OUT "\n\n} // end namespace $pl\n\n" );
close( IN );
close( OUT );

    
