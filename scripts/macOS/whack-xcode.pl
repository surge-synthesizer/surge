#/usr/bin/perl

while( <> )
{
    if( m/PRODUCT_NAME\s*=\s*Surge/ )
    {
        print "\t\t\t\tMACOSX_DEPLOYMENT_TARGET = 10.9;\n";
    }
    print;
}
