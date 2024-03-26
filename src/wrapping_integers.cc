#include "wrapping_integers.hh"
#include <cmath>

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // Your code here.
  return zero_point + static_cast<uint32_t>( n % ( 1LL << 32 ) );
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // Your code here.
  uint64_t cur_num = raw_value_ - zero_point.raw_value_;
  int64_t count = checkpoint / ( 1UL << 32 ); //由于需要处理特殊情况，所以这里将count声明为有符号整形

  if ( cur_num + count * ( 1UL << 32 ) < checkpoint ) {
    count = ( checkpoint - cur_num - count * ( 1UL << 32 ) <= cur_num + ( count + 1 ) * ( 1UL << 32 ) - checkpoint )
              ? count
              : count + 1;
  } else {
    count = ( cur_num + count * ( 1UL << 32 ) - checkpoint <= checkpoint - cur_num - ( count - 1 ) * ( 1UL << 32 ) )
              ? count
              : count - 1;
  }
  return cur_num + std::max<int64_t>( count, 0 ) * ( 1UL << 32 );
}
