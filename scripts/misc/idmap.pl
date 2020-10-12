#!/usr/bin/perl

print "// cat src/common/resource.h | perl scripts/misc/idmap.pl > src/common/gui/SkinImageMaps.h if you want to regen this\n";
print "inline std::unordered_map<std::string, int> createIdNameMap() {\n";
print "   std::unordered_map<std::string, int> res;\n";
        
$all = "";
while( <> )
{
    if( m/define IDB_(\S+) (\d+)/ )
    {
        $all .= "   allowed.insert( $2 );\n";
        print "   res[\"" . $1 . "\"] = " . $2 . ";\n";
    }
}

print "   return res;\n}\n";

print "\ninline std::unordered_set<int> allowedImageIds() {\n   std::unordered_set<int> allowed;\n";
print "$all\n";
print "   return allowed;\n}\n";
