#include <Klein.h>
#include <thread>
#include <algorithm>
namespace Attack {
    Klein::Klein( const std::set<std::pair<Key, Key>>& input_data, size_t keyLength )
        : finished( false )
        , keyLength( keyLength )
    {
        this->data.reserve( input_data.size() );

        for ( auto val : input_data )
        {
            KnownInfo info;
            info.key = val.first;
            info.KeyStream = val.second;
            if ( info.KeyStream.size() < keyLength )
            {
                throw std::exception( "KeyStream size is too short" );
            }
            this->data.push_back( info );
        }
    }
    Crypto::Key Klein::find_key()
    {
        if ( finished )
            return found_key;

        while ( found_key.size() < keyLength ) {
            interest.assign( 256, 0 );
            const size_t thread_count = 11;
            std::thread t[ thread_count ];
            size_t step = this->data.size() / thread_count;
            for ( int i = 0; i < thread_count; ++i ) {
                t[ i ] = std::thread( &Klein::compute_in_thread, this, i*step, ( i + 1 == thread_count ) ? this->data.size() : ( i + 1 )*step );
            }

            //Join the threads with the main thread
            for ( int i = 0; i < thread_count; ++i ) {
                t[ i ].join();
            }
            std::vector<std::pair<int, Crypto::byte> > res_vec( 256 );
            for ( int i = 0; i < 256; ++i ) {
                res_vec[ i ] = std::make_pair( interest[ i ], i );
            }
            std::sort( res_vec.begin(), res_vec.end() );

            found_key.push_back( res_vec.back().second );
        }

        finished = true;
        free_resources();
        return found_key;
    }
    KnownInfo& Klein::find_permutation( KnownInfo & info )
    {
        const Key& key = info.key;
        Permutation& S = info.S;
        Permutation& Si = info.Si;
        size_t& j = info.j;
        size_t i = 0;
        if ( S.empty() ) {
            S.resize( 256 );
            Si.resize( 256 );
            for ( i = 0; i < 256; ++i )
                Si[ i ] = S[ i ] = i;
            i = 0;
            j = 0;
        }
        else
        {
            i = key.size() - 1;
        }

        for ( i; i < 256 && i < key.size(); ++i ) {
            j = ( j + S[ i ] + key[ i % key.size() ] ) % 256;
            std::swap( Si[ S[ i ] ], Si[ S[ j ] ] );
            std::swap( S[ i ], S[ j ] );
        }

        return info;
    }
    byte Klein::find_next_key_byte( KnownInfo& info )
    {
        find_permutation( info );
        size_t i = info.key.size();

        return ( info.Si[ ( i - info.KeyStream[ i - 1 ] + 256 ) % 256 ] - ( info.j + info.S[ i ] ) + 256 ) % 256;
    }
    void Klein::compute_in_thread( size_t a, size_t b )
    {
        for ( size_t i = a; i < b; ++i ) {
            KnownInfo& info = this->data[ i ];
            if ( !found_key.empty() )
                info.key.push_back( found_key.back() );
            Crypto::byte next_key_byte = Klein::find_next_key_byte( info );

            interest_mutex.lock();
            ++interest[ next_key_byte ];
            interest_mutex.unlock();
        }
    }
}