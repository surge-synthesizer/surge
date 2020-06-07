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

inline double fastmodulus(double a, double b)
{
   return a - b * floor( a / b );
}

int main( int argc, char **argv )
{
    
    int tm = 100000000;
    for( int it = 0; it < 10; ++it )
    {
       {
          TimeBlock t("fmod([0,PI],PI)" );
          double d = 0;
          for( int q=0; q<tm/2000; ++q )
             for( int i=0; i<2000; ++i )
             {
                d += fmod( i / 2000.0 * 3.14, 3.14 );
             }
          std::cout << "\nfmod1 is " << d << std::endl;
       }
       {
          TimeBlock t("fmod([0,10 PI],PI)" );
          double d = 0;
          for( int q=0; q<tm/2000; ++q )
             for( int i=0; i<2000; ++i )
             {
                d += fmod( i / 2000.0 * 3.14 * 10, 3.14 );
             }
          std::cout << "\nfmod1 is " << d << std::endl;
       }
       {
          TimeBlock t("fmod([-5,5 PI],PI)" );
          double d = 0;
          for( int q=0; q<tm/2000; ++q )
             for( int i=0; i<2000; ++i )
             {
                d += fmod( ( i / 2000.0 - 0.5 ) * 3.14 * 10, 3.14 );
             }
          std::cout << "\nfmod1 is " << d << std::endl;
       }
       {
          TimeBlock t("fmod([-500PI,500PI],PI)" );
          double d = 0;
          for( int q=0; q<tm/2000; ++q )
             for( int i=0; i<2000; ++i )
             {
                d += fmod( (i / 2000.0 - 0.5) * 10000 * 3.14, 3.14 );
             }
          std::cout << "\nfmod1 is " << d << std::endl;
       }
       {
          TimeBlock t("fmod([0,500PI],PI)" );
          double d = 0;
          for( int q=0; q<tm/2000; ++q )
             for( int i=0; i<2000; ++i )
             {
                d += fmod( (i / 2000.0 ) * 5000 * 3.14, 3.14 );
             }
          std::cout << "\nfmod1 is " << d << std::endl;
       }
       {
          TimeBlock t("fastmod([0,500PI],PI)" );
          double d = 0;
          for( int q=0; q<tm/2000; ++q )
             for( int i=0; i<2000; ++i )
             {
                d += fastmodulus( (i / 2000.0 ) * 5000 * 3.14, 3.14 );
             }
          std::cout << "\nfmod1 is " << d << std::endl;
        }
    }
}
