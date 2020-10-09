#!/usr/bin/perl

$x = 904;
$y = 569;
$frac = "../../Noodle/fractal/fractal";

@res = ( 100, 125, 150, 200, 250, 300, 400 );

foreach $r ( @res )
{
    print "RESOLUTION $r\n";
    $xl = $x * $r / 100;
    $yl = int( $y * $r / 100 );
    print "  $xl $yl\n";
    $cmd = "cd ../../Noodle/fractal && python -m fractal  mandelbrot --size=${xl}x${yl} --depth=256 --zoom=6.5 --center=-1.2x0.31 --out /tmp/fracbg_${r}.png --cmap Spectral";
    system $cmd;
}