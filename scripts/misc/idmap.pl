#!/usr/bin/perl

print "// see scripts/misc/idmap.pl if you want to regen this\n";
print "std::map<std::string, iant> createIdNameMap() {\n";
print "   std::map<std::string, int> res;\n";
        
while( <> )
{
    if( m/define IDB_(\S+) (\d+)/ )
    {
        print "   res[\"" . $1 . "\"] = " . $2 . ";\n";
    }
}

print "   return res;\n}\n";
