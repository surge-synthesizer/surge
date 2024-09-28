#!/usr/bin/perl


use File::Find;
use File::Basename;

find(
    {
        wanted => \&findfiles,
    },
    'src'
);

sub findfiles
{
    $f = $File::Find::name;
    print $f . "\n";
    if ($f =~ m/\.cpp$/ || $f =~ m/\.h$/)
    {
        $q = basename($f);

        open (IN, "< $q") || die "Cant open $q from $f";
        open (OUT, "> $q.bak");
        while(<IN>)
        {
            if (m/getStringWidth/)
            {
                print;
                # s/([^\s\{]+).getStringWidthFloat\(/juce::GlyphArrangement::getStringWidth\($1, /;
                s/\(int\)juce::GlyphArrangement::getStringWidth/juce::GlyphArrangement::getStringWidthInt/;
                print;
                #die;
            }
            print OUT;

        }
        close(IN);
        close(OUT);
        system("mv ${q}.bak ${q}");
    }
}
