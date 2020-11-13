#!/usr/bin/perl

die "script file idname idnum" unless $#ARGV == 2;

print "Adding $ARGV[0] as $ARGV[1] with id $ARGV[2]\n";
$file = $ARGV[0];
$id = $ARGV[1];
$idnum = $ARGV[2];

open( IN, "< src/common/resource.h" );
open( OUT , " > hacktmp.h" );

while( <IN>) {
    if( m:\#define\s+$id\s+: )
    {
        die "You already added this one (resource)\n";
    }
    if( m:== /SVG: )
    {
        print OUT "#define $id $idnum\n";
    }
    print OUT;
}

close( IN );
close( OUT );
system( "mv hacktmp.h src/common/resource.h" );

open( IN, "< src/common/gui/SurgeBitmaps.cpp" );
open( OUT , " > hacktmp2.h" );

while( <IN>) {
    if( m:addEntry\($id[\s,]: )
    {
        die "You already added this one (entry)\n";
    }
    if( m:== /SVG: )
    {
        print OUT "   addEntry($id, f);\n";
    }
    print OUT;
}

close( IN );
close( OUT );
system( "mv hacktmp2.h src/common/gui/SurgeBitmaps.cpp" );

open( IN, "< src/windows/svgresources.rc" );
open( OUT , " > hacktmp3.h" );

while( <IN>) {
    if( m:${id}_SCALE_SVG: )
    {
        die "You already added this one (svg)\n";
    }
    print OUT;
}
print OUT "${id}_SCALE_SVG DATA \"$file\"\n";

close( IN );
close( OUT );
system( "mv hacktmp3.h src/windows/svgresources.rc" );

open( IN, "< src/windows/scalableresource.h" );
open( OUT , " > hacktmp4.h" );

while( <IN>) {
    if( m:\#define ${id}_SCALE_SVG: )
    {
        die "You already added this one (scalable)\n";
    }
    print OUT;
}
$q = $idnum + 80000;
print OUT "\n\n#define ${id}_SCALE_SVG $q\n";

close( IN );
close( OUT );
system( "mv hacktmp4.h src/windows/scalableresource.h" );

system( "cat src/common/resource.h | perl scripts/misc/idmap.pl > src/common/gui/SkinImageMaps.h" );
