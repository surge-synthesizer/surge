#include <cmath>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <string>

struct TimeBlock {
    TimeBlock(std::string m) {
        msg = m;
        start = std::chrono::high_resolution_clock::now();
    }
    ~TimeBlock() {
        auto d = std::chrono::high_resolution_clock::now() - start;
        std::cout << "TIME[" << msg << "] " << std::chrono::duration_cast<std::chrono::milliseconds>(d).count()  << " milis" << std::endl;
    }
    std::chrono::time_point<std::chrono::high_resolution_clock> start;
    std::string msg;
};

inline float fastsin(float x)
{
        auto x2 = x * x;
        auto numerator = -x * (-11511339840 + x2 * (1640635920 + x2 * (-52785432 + x2 * 479249)));
        auto denominator = 11511339840 + x2 * (277920720 + x2 * (3177720 + x2 * 18361));
        return numerator / denominator;
}
inline float fastcos(float x)
{
       auto x2 = x * x;
       auto numerator = -(-39251520 + x2 * (18471600 + x2 * (-1075032 + 14615 * x2)));
       auto denominator = 39251520 + x2 * (1154160 + x2 * (16632 + x2 * 127));
       return numerator / denominator;
}

inline float clampSlow( float x )
{
    while( x > M_PI )
        x -= 2.0 * M_PI;
    while( x < -M_PI )
        x += 2.0 * M_PI;

    return x;
}

inline float clampFast( float x )
{
    if( x <= M_PI && x >= -M_PI ) return x;
    float y = x + M_PI; // so now I am 0,2PI
    float p = fmod( y, 2.0 * M_PI );
    if( p < 0 )
        p += 2.0 * M_PI;
    return p - M_PI;
}

inline float clampFastest( float x )
{
    if( x <= M_PI && x >= -M_PI ) return x;
    float y = x + M_PI; // so now I am 0,2PI
    constexpr float oo2p = 1.0 / ( 2.0 * M_PI );
    float p = y - 2.0 * M_PI * (int)( y * oo2p );
    if( p < 0 )
        p += 2.0 * M_PI;
    return p - M_PI;
}

int main( int argc, char **argv )
{
    int nclamps = 10000;
    for( int i=0; i<nclamps; ++i )
    {
        float x = ( i - nclamps / 2.f ) * 13.2 * M_PI / nclamps;
        float csx = clampSlow( x );
        float cfx = clampFast( x );
        if( fabs( csx - cfx ) > 1e-5 )
        {
            std::cout << x << " " << csx << " " << cfx << " " << csx - cfx << std::endl;
        }
    }
    
    int tm = 100000000;
    for( int it = 0; it < 10; ++it )
    {
        {
            TimeBlock t("std::sin/std::cos" );
            double d = 0;
            for( int q=0; q<tm/2000; ++q )
                for( int i=0; i<2000; ++i )
                {
                    d += std::sin( -2.7 + 0.0001 * i ) + std::cos( -3 + 0.0001 * i ); 
                }
            std::cout << "\nsin SUM is " << d << std::endl;
        }
        {
            TimeBlock t( "std::sinf/std::cosf" );
            float f = 0;
            for( int q=0; q<tm/2000; ++q )
                for( int i=0; i<2000; ++i )
                {
                    f += std::sinf( -2.7f + 0.0001f * i ) + std::cosf( -3.f + 0.0001f * i ); 
                }
            std::cout << "\nsinf SUM is " << f << std::endl;
        }
        {
            TimeBlock t( "fastsin/fastcos" );
            float fa = 0;
            for( int q=0; q<tm/2000; ++q )
                for( int i=0; i<2000; ++i )
                {
                    fa += fastsin( -2.7 + 0.0001f * i ) + fastcos( -3 + 0.0001f * i ); 
                }
            std::cout << "\nfast SUM is " << fa << std::endl;
        }

        {
            TimeBlock t("slowclamp in range");
            float fa = 0;
            for( int q=0; q<tm/2000; ++q )
                for( int i=0; i<2000; ++i )
                {
                    float p = -2.7 + 0.0001f * i;
                    p = clampSlow(p);
                    fa += fastsin( p ) + fastcos( p );
                }
            std::cout << "\nclampslow SUM is " << fa << std::endl;
        }

        {
            TimeBlock t("slowclamp med range");
            float fa = 0;
            for( int q=0; q<tm/2000; ++q )
                for( int i=0; i<2000; ++i )
                {
                    float p = -27 + 0.001f * i;
                    p = clampSlow(p);
                    fa += fastsin( p ) + fastcos( p );
                }
            std::cout << "\nclampslow SUM is " << fa << std::endl;
        }

        {
            TimeBlock t("fastclamp med range");
            float fa = 0;
            for( int q=0; q<tm/2000; ++q )
                for( int i=0; i<2000; ++i )
                {
                    float p = -2.7 + 0.001f * i;
                    p = clampFast(p);
                    fa += fastsin( p ) + fastcos( p );
                }
            std::cout << "\nclampfast SUM is " << fa << std::endl;
        }

        {
            TimeBlock t("fastclampest med range");
            float fa = 0;
            for( int q=0; q<tm/2000; ++q )
                for( int i=0; i<2000; ++i )
                {
                    float p = -2.7 + 0.001f * i;
                    p = clampFastest(p);
                    fa += fastsin( p ) + fastcos( p );
                }
            std::cout << "\nclampfastest SUM is " << fa << std::endl;
        }


        {
            TimeBlock t("fastclamp huge range");
            float fa = 0;
            for( int q=0; q<tm/2000; ++q )
                for( int i=0; i<2000; ++i )
                {
                    float p = -163.0 + 0.105f * i;
                    p = clampFast(p);
                    fa += fastsin( p ) + fastcos( p );
                }
            std::cout << "\nclampfast SUM is " << fa << std::endl;
        }

        {
            TimeBlock t("fastestclamp huge range");
            float fa = 0;
            for( int q=0; q<tm/2000; ++q )
                for( int i=0; i<2000; ++i )
                {
                    float p = -163 + 0.105f * i;
                    p = clampFastest(p);
                    fa += fastsin( p ) + fastcos( p );
                }
            std::cout << "\nclampfastest SUM is " << fa << std::endl;
        }

        float maxe = -1, accumerr = 0, accumabs = 0;
        float dp = 3.14159265 / 10000;
        for( int i=0; i<10000; ++i )
        {
            float diff = std::sin( i * dp ) - fastsin( i * dp );
            if( diff > maxe ) maxe = diff;
            accumerr += diff;
            accumabs += abs( diff );
        }
        std::cout << "\nERROR " << maxe << " " << accumabs << " " << accumerr << " " << accumerr / 10000 << std::endl;
  }
}
