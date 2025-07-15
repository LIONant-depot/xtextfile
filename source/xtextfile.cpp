#include "xtextfile.h"
#include <format>
#include <cstdarg>
#include <filesystem>
#include <variant>

//-----------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------
// helpful functions
//-----------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------
namespace xtextfile
{
    template< typename T_B >
    struct scope_end_callback
    {
        ~scope_end_callback()
        {
            m_Callback();
        }

        T_B     m_Callback;
    };

    //-----------------------------------------------------------------------------------------------------
    template<typename T>
    static constexpr T align_to(T X, std::size_t alignment) noexcept
    {
        std::size_t x = static_cast<std::size_t>(X);
        return static_cast<T>((x + alignment - 1) & ~(alignment - 1));
    }

    //-----------------------------------------------------------------------------------------------------

    static bool ishex(const char C) noexcept
    {
        return(((C >= 'A' ) && (C <=  'F' )) || ((C >= 'a' ) && (C <= 'f' )) || ((C >= '0' ) && (C <= '9' )));
    }

    //-----------------------------------------------------------------------------------------------------

    int Strcpy_s( char* pBuff, std::size_t BuffSize, const char* pSrc )
    {
        assert(pBuff);
        assert(pSrc);
        assert(BuffSize>0);

        std::size_t length = 0;
        for (; length <BuffSize; ++length)
        {
            pBuff[length] = pSrc[length];
            if (pBuff[length] == 0 ) 
            {
                length++;
                break;
            }
        }
        assert(length < BuffSize);

        return static_cast<int>(length);
    }

    //-----------------------------------------------------------------------------------------------------

    namespace endian
    {
        static std::uint16_t Convert(std::uint16_t value) noexcept
        {
            return (value >> 8) | (value << 8);
        }

        static std::uint32_t Convert(std::uint32_t value) noexcept
        {
            return ((value >> 24) & 0xFF) | ((value >> 8) & 0xFF00) |
                ((value << 8) & 0xFF0000) | (value << 24);
        }

        static std::uint64_t Convert(std::uint64_t value) noexcept
        {
            return ((value >> 56) & 0xFF)               | ((value >> 40) & 0xFF00) |
                   ((value >> 24) & 0xFF0000)           | ((value >> 8) & 0xFF000000) |
                   ((value << 8) & 0xFF00000000)        | ((value << 24) & 0xFF0000000000) |
                   ((value << 40) & 0xFF000000000000)   |  (value << 56);
        }

        static float Convert(float value) noexcept
        {
            std::uint32_t temp;
            std::memcpy(&temp, &value, sizeof(float));
            temp = Convert(temp);
            float result;
            std::memcpy(&result, &temp, sizeof(float));
            return result;
        }

        static double Convert(double value) noexcept
        {
            std::uint64_t temp;
            std::memcpy(&temp, &value, sizeof(double));
            temp = Convert(temp);
            double result;
            std::memcpy(&result, &temp, sizeof(double));
            return result;
        }
    }
}

//-----------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------
// Details
//-----------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------
namespace xtextfile::details
{
    //-----------------------------------------------------------------------------------------------------
    // convert wstring to UTF-8 string
    //-----------------------------------------------------------------------------------------------------

    // Convert std::wstring to UTF-8 std::string
    inline std::string wstring_to_utf8(const std::wstring_view str)
{
        if (str.empty()) {
            return {};
        }

#if defined(_WIN32)
        std::wstring temp{ str };
        const size_t max_bytes = temp.size() * 4 + 1;
        std::vector<char> buffer(max_bytes);
        size_t ret;
        errno_t err = wcstombs_s(&ret, buffer.data(), max_bytes, temp.c_str(), _TRUNCATE);
        if (err != 0) {
            assert(false);
            return {};
        }
        return { buffer.data(), ret - 1 }; // Exclude null terminator
#else
        std::wstring temp{ str };
        const size_t max_bytes = temp.size() * 4 + 1;
        std::vector<char> buffer(max_bytes);
        const size_t written = wcstombs(buffer.data(), temp.c_str(), max_bytes);
        if (written == static_cast<size_t>(-1)) {
            assert(false);
            return {};
        }
        return { buffer.data(), written };
#endif
    }

    //------------------------------------------------------------------------------

    // Convert UTF-8 std::string to std::wstring
    inline std::wstring utf8_to_wstring(const std::string_view str)
{
        if (str.empty()) {
            return {};
        }

#if defined(_WIN32)
        const size_t max_chars = str.size() + 1;
        std::vector<wchar_t> buffer(max_chars);
        size_t written;
        errno_t err = mbstowcs_s(&written, buffer.data(), max_chars, str.data(), str.size());
        if (err != 0) {
            assert(false);
            return {};
        }
        return { buffer.data(), written - 1 }; // Exclude null terminator
#else
        std::vector<wchar_t> buffer(str.size() + 1);
        size_t written;
        errno_t err = mbstowcs_s(&written, buffer.data(), buffer.size(), str.data(), str.size());
        if (err != 0) {
            assert(false);
            return {};
        }
        return { buffer.data(), written - 1 }; // Exclude null terminator
#endif
    }

    //------------------------------------------------------------------------------

    inline std::string wstring_to_string(const std::wstring_view str)
    {
        return wstring_to_utf8(str);
    }

    //------------------------------------------------------------------------------

    details::file& file::setup( std::FILE& File, details::states States ) noexcept
    {
        close();
        m_pFP    = &File;
        m_States = States; 
        return *this;
    }

    //------------------------------------------------------------------------------

    file::~file( void ) noexcept 
    { 
        close(); 
    }

    //------------------------------------------------------------------------------

    err2 file::openForReading( const std::wstring_view FilePath, bool isBinary ) noexcept
    {
        assert(m_pFP == nullptr);
    #if defined(_MSC_VER)
        auto Err = fopen_s( &m_pFP, wstring_to_string(FilePath).c_str(), isBinary ? "rb" : "rt" );
        if( Err )
        {
            return { err::state::FAILURE, std::format( L"#Fail to open {} for reading", FilePath ) };
        }
    #else
        m_pFile->m_pFP = fopen( wstring_to_utf8(FilePath).c_str(), pAttr );
        if( m_pFP )
        {
            return xerr_failure_s( "Fail to open a file");
        }
    #endif

        m_States.m_isBinary  = isBinary;
        m_States.m_isReading = true;
        m_States.m_isView    = false;
        return {};
    }

    //------------------------------------------------------------------------------

    err2 file::openForWriting( const std::wstring_view FilePath, bool isBinary  ) noexcept
    {
        assert(m_pFP == nullptr);
    #if defined(_MSC_VER)
        auto Err = fopen_s( &m_pFP, wstring_to_string(FilePath).c_str(), isBinary ? "wb" : "wt" );
        if( Err )
        {
            return { err::state::FAILURE, std::format(L"#Fail to open {} for writing", FilePath) };
        }
    #else
        m_pFile->m_pFP = fopen( wstring_to_utf8(FilePath).c_str(), pAttr );
        if( m_pFP )
        {
            return xerr_failure_s( "Fail to open a file");
        }
    #endif

        m_States.m_isBinary  = isBinary;
        m_States.m_isReading = false;
        m_States.m_isView    = false;

        return {};
    }

    //------------------------------------------------------------------------------

    void file::close( void ) noexcept
    {
        if(m_pFP && m_States.m_isView == false ) fclose(m_pFP);
        m_pFP = nullptr;
    }

    //------------------------------------------------------------------------------

    err file::ReadingErrorCheck( void ) noexcept
    {
        if( m_States.m_isEOF || feof( m_pFP ) ) 
        {
            m_States.m_isEOF = true;
            return err::create<err::state::UNEXPECTED_EOF, "Found the end of the file unexpectedly while reading" >();
        }
        return err::create_f< "Fail while reading the file, expected to read more data" >();
    }

    //------------------------------------------------------------------------------
    template< typename T >
    err file::Read( T& Buffer, int Size, int Count ) noexcept
    {
        assert(m_pFP);
        if( m_States.m_isEOF ) return ReadingErrorCheck();

    #if defined(_MSC_VER)
        if ( Count != fread_s( &Buffer, Size, Size, Count, m_pFP ) )
            return ReadingErrorCheck();
    #else
    #endif
        return {};
    }

    //------------------------------------------------------------------------------

    err file::getC( int& c ) noexcept
    {
        assert(m_pFP);
        if( m_States.m_isEOF ) return ReadingErrorCheck();

        c = fgetc(m_pFP);
        if( c == -1 ) return ReadingErrorCheck();
        return {};
    }

    //------------------------------------------------------------------------------

    template< typename T >
    err file::Write( T& Buffer, int Size, int Count ) noexcept
    {
        assert(m_pFP);

    #if defined(_MSC_VER)
        if ( Count != fwrite( &Buffer, Size, Count, m_pFP ) )
        {
            return err::create_f< "Fail writing the required data" >();
        }
    #else
    #endif
        return {};
    }

    //------------------------------------------------------------------------------

    int file::Tell() noexcept
    {
        return ftell( m_pFP );
    }

    //------------------------------------------------------------------------------

    err file::WriteStr( const std::string_view Buffer ) noexcept
    {
        assert(m_pFP);
        // assert(Buffer.empty() || Buffer[Buffer.size() - 1] == 0);

    #if defined(_MSC_VER)
        if( m_States.m_isBinary )
        {
            if ( Buffer.size() != fwrite( Buffer.data(), sizeof(char), Buffer.size(), m_pFP ) )
            {
                return err::create_f< "Fail writing the required data" >();
            }
        }
        else
        {
            const std::uint64_t L           = Buffer.length();
            const std::uint64_t TotalData   = L>Buffer.size()?Buffer.size():L;
            if( TotalData != std::fwrite( Buffer.data(), 1, TotalData, m_pFP ) )
                return err::create_f< "Fail 'fwrite' writing the required data" >();
        }
    #else
    #endif
        return {};
    }

    //------------------------------------------------------------------------------

    err file::WriteFmtStr( const char* pFmt, ... ) noexcept
    {
        va_list Args;
        va_start( Args, pFmt );
        if( std::vfprintf( m_pFP, pFmt, Args ) < 0 )
            return err::create_f< "Fail 'fprintf' writing the required data" >();
        va_end( Args );
        return {};
    }

    //------------------------------------------------------------------------------

    err file::WriteChar( char C, int Count ) noexcept
    {
        while( Count-- )
        {
            if( C != fputc( C, m_pFP ) )
                return err::create_f< "Fail 'fputc' writing the required data" >();
        }
        return {};
    }

    //------------------------------------------------------------------------------

    err file::WriteData( std::string_view Buffer ) noexcept
    {
        assert( m_States.m_isBinary );

        if( Buffer.size() != std::fwrite( Buffer.data(), 1, Buffer.size(), m_pFP ) )
            return err::create_f< "Fail 'fwrite' binary mode" >();

        return {};
    }

    //------------------------------------------------------------------------------

    err file::ReadWhiteSpace( int& c ) noexcept
    {
        // Read any spaces
        err  Error;
    
        do 
        {
            if( getC(c).isError(Error) ) return Error;
        } while( std::isspace( c ) );

        //
        // check for comments
        //
        while( c == '/' )
        {
            if( getC(c).isError(Error) ) return Error;
            if( c == '/' )
            {
                // Skip the comment
                do
                {
                    if( getC(c).isError(Error) ) return Error;
                } while( c != '\n' );
            }
            else
            {
                return err::create_f< "Error reading file, unexpected symbol found [/]" >();
            }

            // Skip spaces
            if( ReadWhiteSpace(c).isError(Error) ) return Error;
        }

        return Error;
    }

    //------------------------------------------------------------------------------

    err file::HandleDynamicTable( int& Count ) noexcept
    {
        err         Error;
        auto        LastPosition    = ftell( m_pFP );
        int         c;
                
        Count           = -2;                   // -1. for the current header line, -1 for the types
        
        if( LastPosition == -1 )
            return err::create_f< "Fail to get the cursor position in the file" >();

        if( getC(c).isError(Error) )
        {
            Error.clear();
            return err::create_f< "Unexpected end of file while searching the [*] for the dynamic" >();
        }
    
        do
        {
            if( c == '\n' )
            {
                Count++;
                if( ReadWhiteSpace(c).isError(Error) ) return Error;
            
                if( c == '[' )
                {
                    break;
                }
            }
            else
            {
                if( getC(c).isError(Error) ) return Error;
            
                // if the end of the file is in a line then we need to count it
                if( c == -1 )
                {
                    Count++;
                    break;
                }
            }
    
        } while( true );

    
        if( Count <= 0  )
            return err::create_f< "Unexpected end of file while counting rows for the dynamic table" >();
    
        // Rewind to the start
        if( fseek( m_pFP, LastPosition, SEEK_SET ) )
            return err::create_f< "Fail to reposition the cursor back to the right place while reading the file" >();

        return Error;
    }
}

//-----------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------
// Class functions
//-----------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------
namespace xtextfile
{
    //------------------------------------------------------------------------------------------------
    // Some static variables
    //------------------------------------------------------------------------------------------------

    //------------------------------------------------------------------------------------------------
    bool stream::isValidType( int Type ) const noexcept
    {
        switch( Type )
        {
            // Lets verify that the user_types enter a valid atomic err
            case 'f': case 'F':
            case 'd': case 'D':
            case 'c': case 'C':
            case 's': 
            case 'g': case 'G':
            case 'h': case 'H':
            return true;
        }

        // Sorry but I don't know what kind of syntax / err information 
        // the user_types is trying to provide.
        // Make sure that there are not extra character after the err ex:
        // "Pepe:fff " <- this is bad. "Pepe:fff" <- this is good.
        return false;
    }

    //-----------------------------------------------------------------------------------------------------

    err2 stream::openForReading( const std::wstring_view FilePath ) noexcept
    {
        //
        // Check to see if we can get a hint from the file name to determine if it is binary or text
        //
        int isTextFile = 0;
        {
            auto length = FilePath.length();
            if (FilePath[length - 1] == 't' &&
                FilePath[length - 2] == 'x' &&
                FilePath[length - 3] == 't' &&
                FilePath[length - 4] == '.')
            {
                isTextFile = 2;
            }
            else if (FilePath[length - 1] == 'n' &&
                     FilePath[length - 2] == 'i' &&
                     FilePath[length - 3] == 'b' &&
                     FilePath[length - 4] == '.')
            {
                isTextFile = 1;
            }
        }

        // Open the file in binary or in text mode... if we don't know we will open in binary
        if( auto Err = m_File.openForReading(FilePath, isTextFile < 2 ); Err )
        {
            std::error_code ec;
            if (std::filesystem::exists(FilePath, ec) == false)
            {
                return { err::state::FILE_NOT_FOUND, std::format(L"#Fail to find the file in the specify directory", FilePath) };
            }

            if (ec)
            {
                return { err::state::FILE_NOT_FOUND, std::format(L"#Fail to detect if the file {} is in the correct directory", FilePath) };
            }

            return Err;
        }
            

        //
        // Okay make sure that we say that we are not reading the file
        // this will force the user_types to stick with the writing functions
        //
        err2 Failure;
        scope_end_callback CleanUp([&] { if (Failure) close(); });

        //
        // Determine signature (check if we are reading a binary file or not)
        //
        if ( isTextFile < 2 )
        {
            std::uint32_t Signature = 0;
            if( m_File.Read(Signature).isError(Failure) && (Failure.getState() != err::state::UNEXPECTED_EOF) )
            {
                return Failure;
            }
            else 
            {
                Failure.clear();
                if( Signature == std::uint32_t('NOIL') || Signature == std::uint32_t('LION') )
                {
                    if( Signature == std::uint32_t('LION') )
                        m_File.m_States.m_isEndianSwap = true;
                }
                else // We are dealing with a text file, if so the reopen it as such
                {
                    m_File.close();
                    if( m_File.openForReading(FilePath, false).isError(Failure) )
                        return Failure;
                }
            }
        }

        //
        // get ready to start reading
        //
        m_Memory.clear();

        // Growing this guy is really slow so we create a decent count from the start
        if( m_Memory.capacity() < 1048 ) m_Memory.resize(m_Memory.size() + 1048 );

        //
        // Read the first record
        //
        if( ReadRecord().isError( Failure ) ) return Failure;

        return Failure;
    }

    //-----------------------------------------------------------------------------------------------------

    err2 stream::openForWriting( const std::wstring_view FilePath, file_type FileType, flags Flags ) noexcept
    {
        //
        // Open the file
        //
        if( auto Err = m_File.openForWriting( FilePath, FileType == file_type::BINARY ); Err ) 
            return Err;

        //
        // Okay make sure that we say that we are not reading the file
        // this will force the user_types to stick with the writing functions
        //
        err2                Failure;
        scope_end_callback  CleanUp([&] { if (Failure) close(); });


        //
        // Determine whether we are binary or text base
        //
        if( FileType == file_type::BINARY )
        {
            // Write binary signature
            const std::uint32_t Signature = std::uint32_t('NOIL');
            if( m_File.Write( Signature ).isError( Failure ) )
                return Failure;
        }

        //
        // Handle flags
        //
        m_File.m_States.m_isEndianSwap = Flags.m_isWriteEndianSwap;
        m_File.m_States.m_isSaveFloats = Flags.m_isWriteFloats;

        //
        // Initialize some of the Write variables
        //
        m_Memory.clear();

        // Growing this guy is really slow so we create a decent count from the start
        if( m_Memory.capacity() < 2048 ) m_Memory.resize(m_Memory.size() + 2048 );

        return Failure;
    }

    //-----------------------------------------------------------------------------------------------------

    void stream::close( void ) noexcept
    {
        m_File.close();
    }

    //------------------------------------------------------------------------------------------------

    err2 stream::Open( bool isRead, std::wstring_view View, file_type FileType, flags Flags ) noexcept
    {
        if( isRead )
        {
            if( auto Err = openForReading( View ); Err ) return Err;
        }
        else
        {
            if(auto Err = openForWriting( View, FileType, Flags ); Err ) return Err;
        }

        return {};
    }

    //------------------------------------------------------------------------------

    std::uint32_t stream::AddUserType( const user_defined_types& UserType ) noexcept
    {
        if( const auto It = m_UserTypeMap.find(UserType.m_CRC); It != m_UserTypeMap.end() )
        {
            const auto Index = It->second;
            
            // Make sure that this registered err is fully duplicated
            assert( UserType.m_SystemTypes == m_UserTypes[Index].m_SystemTypes );
        
            // we already have it so we don't need to added again
            return Index;
        }

        //
        // Sanity check the user_types types
        //
#ifdef _DEBUG
        {
            for( int i=0; UserType.m_Name[i]; i++ )
            {
                assert( false == std::isspace(UserType.m_Name[i]) );
            }
    
            for( int i=0; UserType.m_SystemTypes[i]; i++ )
            {
                assert( false == std::isspace(UserType.m_SystemTypes[i]) );
            }
        }
#endif
        //
        // Fill the entry
        //
        std::uint32_t Index  = static_cast<std::uint32_t>(m_UserTypes.size());
        m_UserTypes.emplace_back(UserType);

        m_UserTypeMap.insert( {UserType.m_CRC, Index} );

        return Index;
    }

    //------------------------------------------------------------------------------
    void stream::AddUserTypes( std::span<user_defined_types> UserTypes ) noexcept
    {
        for( auto& T : UserTypes )
            AddUserType( T );
    }

    //------------------------------------------------------------------------------
    void stream::AddUserTypes( std::span<const user_defined_types> UserTypes ) noexcept
    {
        for( auto& T : UserTypes )
            AddUserType( T );
    }

    //------------------------------------------------------------------------------

    err stream::WriteRecord( const char* pHeaderName, std::size_t Count ) noexcept
    {
        err Error;

        assert( pHeaderName );
        assert( m_File.m_States.m_isReading == false );

        //
        // Fill the record info
        //
        strcpy_s( m_Record.m_Name.data(), m_Record.m_Name.size(), pHeaderName);
        if( Count == ~0 ) 
        { 
            m_Record.m_bWriteCount  = false;  
            m_Record.m_Count        = 1; 
        }
        else if (Count == (~0 - 1))
        {
            m_Record.m_bWriteCount  = false;
            m_Record.m_Count        = 1;
            m_iLine     = 0;
            m_nColumns  = -1;
            return WriteLine();
        }
        else
        { 
            m_Record.m_bWriteCount  = true; 
            m_Record.m_Count        = static_cast<int>(Count); 
        }

        //
        // Reset the line count
        //
        m_iLine         = 0;
        m_iColumn       = 0;
        m_iMemOffet     = 0;
        m_nColumns      = 0;

        return Error;
    }

    //------------------------------------------------------------------------------
    static 
    bool isCompatibleTypes( char X, details::arglist::types Y ) noexcept
    {
        return std::visit([&]( auto Value )
        {
            using t = std::decay_t<decltype(Value)>;
                    if constexpr ( std::is_same_v<t,bool*>               ) return X == 'c';
            else    if constexpr ( std::is_same_v<t,std::uint8_t*>       ) return X == 'h';
            else    if constexpr ( std::is_same_v<t,std::uint16_t*>      ) return X == 'H';
            else    if constexpr ( std::is_same_v<t,std::uint32_t*>      ) return X == 'g';
            else    if constexpr ( std::is_same_v<t,std::uint64_t*>      ) return X == 'G';
            else    if constexpr ( std::is_same_v<t,std::int8_t*>        ) return X == 'c';
            else    if constexpr ( std::is_same_v<t,std::int16_t*>       ) return X == 'C';
            else    if constexpr ( std::is_same_v<t,std::int32_t*>       ) return X == 'd';
            else    if constexpr ( std::is_same_v<t,std::int64_t*>       ) return X == 'D';
            else    if constexpr ( std::is_same_v<t,float*>              ) return X == 'f';
            else    if constexpr ( std::is_same_v<t,double*>             ) return X == 'F';
            else    if constexpr ( std::is_same_v<t,std::string*>        ) return X == 's';
            else    if constexpr ( std::is_same_v<t,std::wstring*>       ) return X == 'S';
            else    return false;
        }, Y );
    }

    //------------------------------------------------------------------------------

    err stream::WriteComment( const std::string_view Comment ) noexcept
    {
        err Error;

        if( m_File.m_States.m_isReading )
            return Error;

        if( m_File.m_States.m_isBinary )
        {
            return Error;
        }
        else
        {
            const auto  length = Comment.length();
            std::size_t iStart = 0;
            std::size_t iEnd   = iStart;

            if( m_File.WriteChar( '\n' ).isError( Error ) )
                return Error;

            do 
            {
                if( m_File.WriteStr( "//" ).isError( Error ) )
                    return Error;

                while( iEnd < length)
                {
                    if( Comment[iEnd] == '\n' ) { iEnd++; break; }
                    else                          iEnd++;
                }

                if( m_File.WriteStr( { Comment.data() + iStart, static_cast<std::size_t>(iEnd - iStart) } ).isError( Error ) )
                    return Error;

                if( iEnd == length) break;

                // Get ready for the next line
                iStart = iEnd++;

            } while( true );
        }

        return Error;
    }

    //------------------------------------------------------------------------------

    err stream::WriteUserTypes( void ) noexcept
    {
        err Error;

        if( m_File.m_States.m_isBinary )
        {
            bool bHaveUserTypes = false;
            for( auto& UserType : m_UserTypes )
            {
                if( UserType.m_bAlreadySaved )
                    continue;
            
                UserType.m_bAlreadySaved = true;
            
                // First write the standard symbol for the user_types types
                if( bHaveUserTypes == false )
                {
                    bHaveUserTypes = true;
                    if( m_File.WriteChar( '<' ).isError(Error) )
                        return Error;
                }

                // Write the name 
                if( m_File.WriteStr( { UserType.m_Name.data(), static_cast<std::size_t>(UserType.m_NameLength + 1) } ).isError(Error) )
                    return Error;

                // Write type/s
                if( m_File.WriteStr( { UserType.m_SystemTypes.data(), static_cast<std::size_t>(UserType.m_nSystemTypes+1) } ).isError(Error) )
                    return Error;
            }
        }
        else
        {
            //
            // Dump all the new types if we have some
            //
            bool bNewTypes = false;
            for( auto& UserType : m_UserTypes )
            {
                if( UserType.m_bAlreadySaved )
                    continue;

                if( bNewTypes == false ) 
                {
                    if( m_File.WriteStr( "\n// New Types\n< " ).isError(Error) ) return Error;
                    bNewTypes = true;
                }

                UserType.m_bAlreadySaved = true;
                std::array<char,256> temp;

                auto length = sprintf_s( temp.data(), temp.size(), "%s:%s ", UserType.m_Name.data(), UserType.m_SystemTypes.data());
                if( m_File.WriteStr( {temp.data(), static_cast<std::size_t>(length)} ) .isError(Error) ) return Error;
            }

            if( bNewTypes ) if( m_File.WriteStr( ">\n" ).isError(Error) ) return Error;
        }

        return Error;
    }

    //------------------------------------------------------------------------------
    
    err stream::WriteColumn( crc32 UserType, const char* pColumnName, std::span<details::arglist::types> Args) noexcept
    {
        //
        // Make sure we always have enough memory
        //
        if( (m_iMemOffet + 1024*2) > m_Memory.size() ) 
            m_Memory.resize( m_Memory.size() + 1024*4 );

        //
        // When we are at the first line we must double check the syntax of the user_types
        //
        if( m_iLine == 0 )
        {
            // Make sure we have enough columns to store our data
            if( m_Columns.size() <= m_nColumns ) 
            {
                m_Columns.push_back({});
            }
            else
            {
                m_Columns[m_nColumns].clear();
            }

            auto& Column = m_Columns[m_nColumns++];
            
            assert( Args.size() > 0 );
            Column.m_nTypes   = static_cast<int>(Args.size());
            Column.m_UserType = UserType;

            // Copy name of the field
            for( Column.m_NameLength=0; (Column.m_Name[Column.m_NameLength] = pColumnName[Column.m_NameLength]); Column.m_NameLength++ )
            {
                // Handle dynamic types
                if( pColumnName[Column.m_NameLength] == ':' && pColumnName[Column.m_NameLength+1] == '?' )
                {
                    Column.m_nTypes                     = -1;
                    Column.m_UserType.m_Value           = 0;
                    Column.m_Name[Column.m_NameLength]  = 0;
                    break;
                }

                // Make sure the field name falls inside these constraints 
                assert(  (pColumnName[Column.m_NameLength] >= 'a' && pColumnName[Column.m_NameLength] <= 'z')
                     ||  (pColumnName[Column.m_NameLength] >= 'A' && pColumnName[Column.m_NameLength] <= 'Z')
                     ||  (pColumnName[Column.m_NameLength] >= '0' && pColumnName[Column.m_NameLength] <= '9')
                     ||  (pColumnName[Column.m_NameLength] == '_' ) 
                );
            }
        }

        //
        // Get the column we are processing
        //
        auto& Column = m_Columns[ m_iColumn++ ];

        //
        // Validate the user_types type if any
        //
#ifdef _DEBUG
        {
            if( UserType.m_Value && ( Column.m_nTypes == -1 || m_iLine==0 ) )
            {
                auto pUserType = getUserType( UserType );
                    
                // Make sure the user_types register this user_types type
                assert(pUserType);
                assert(pUserType->m_nSystemTypes == Args.size() );
                for( auto& A  : Args )
                {
                    assert(isCompatibleTypes( pUserType->m_SystemTypes[ static_cast<std::size_t>( &A - Args.data() )], A ));
                }
            }
        }
#endif
        //
        // Create all the fields for this column
        //
        if( Column.m_nTypes == -1 || m_iLine == 0 )
        {
            //
            // If is a dynamic type we must add the infos
            //
            if( Column.m_nTypes == -1 )
            {
                auto& DynamicFields = Column.m_DynamicFields.emplace_back();
                DynamicFields.m_nTypes   = static_cast<int>(Args.size());
                DynamicFields.m_UserType = UserType;
                DynamicFields.m_iField   = static_cast<int>(Column.m_FieldInfo.size());
            }

            //
            // Write each of the fields types
            //
            {
                auto& FieldInfo = (Column.m_nTypes == -1) ? Column.m_DynamicFields.back() : Column; 
                for( auto& A : Args )
                {
                    FieldInfo.m_SystemTypes[static_cast<std::size_t>(&A - Args.data())] = std::visit( [&]( auto Value ) constexpr
                    {
                        using t = std::decay_t<decltype(Value)>;
                                if constexpr ( std::is_same_v<t,bool*>               ) return 'c';
//                        else    if constexpr ( std::is_same_v<t,char*>               ) return 'c';
                        else    if constexpr ( std::is_same_v<t,std::uint8_t*>       ) return 'h';
                        else    if constexpr ( std::is_same_v<t,std::uint16_t*>      ) return 'H';
                        else    if constexpr ( std::is_same_v<t,std::uint32_t*>      ) return 'g';
                        else    if constexpr ( std::is_same_v<t,std::uint64_t*>      ) return 'G';
                        else    if constexpr ( std::is_same_v<t,std::int8_t*>        ) return 'c';
                        else    if constexpr ( std::is_same_v<t,std::int16_t*>       ) return 'C';
                        else    if constexpr ( std::is_same_v<t,std::int32_t*>       ) return 'd';
                        else    if constexpr ( std::is_same_v<t,std::int64_t*>       ) return 'D';
                        else    if constexpr ( std::is_same_v<t,float*>              ) return 'f';
                        else    if constexpr ( std::is_same_v<t,double*>             ) return 'F';
                        else    if constexpr ( std::is_same_v<t,std::string*>        ) return 's';
                        else    if constexpr ( std::is_same_v<t,std::wstring*>       ) return 'S';
                        else    { assert(false); return char{0}; }
                    }, A );
                }

                // Terminate types as a proper string
                FieldInfo.m_SystemTypes[ static_cast<int>(Args.size()) ] = 0;
            }
        }

        //
        // Ready to buffer the actual fields
        //
        if( m_File.m_States.m_isBinary )
        {
            //
            // Write to a buffer the data
            //
            for( auto& A : Args )
            {
                auto& FieldInfo = Column.m_FieldInfo.emplace_back(); 

                std::visit( [&]( auto p ) constexpr
                {
                    using T = std::decay_t<decltype(p)>;

                    if constexpr( std::is_same_v<T, std::string*> )
                    {
                        FieldInfo.m_iData = m_iMemOffet;

                        std::string_view a(m_Memory.begin() + FieldInfo.m_iData, m_Memory.end());

                        m_iMemOffet += Strcpy_s(const_cast<char*>(a.data()), a.size(), p->c_str());
                        FieldInfo.m_Width = m_iMemOffet - FieldInfo.m_iData;
                    }
                    else if constexpr (std::is_same_v<T, std::wstring*>)
                    {
                        FieldInfo.m_iData = m_iMemOffet;

                        std::string FinalString = details::wstring_to_utf8( *p );

                        for (auto E : FinalString) 
                        {
                            m_Memory[m_iMemOffet++] = E;
                        }

                        m_Memory[m_iMemOffet++] = 0;

                        FieldInfo.m_Width = m_iMemOffet - FieldInfo.m_iData;
                    }
                    else
                    {
                        if constexpr ( std::is_pointer_v<T> == false 
                            || std::is_same_v<T,void*> 
                            || std::is_same_v<T,const char*> ) 
                        {
                            assert(false);
                        }
                        else 
                        {
                            if constexpr (std::is_same_v<T,bool*> )
                            {
                                // No taken any changes with bools... let us make sure that they are 1 byte
                                static_assert (std::is_pointer_v<T>);
                                constexpr auto size = sizeof(std::uint8_t);
                                FieldInfo.m_Width = size;
                                FieldInfo.m_iData = align_to(m_iMemOffet, 1);
                                m_iMemOffet = FieldInfo.m_iData + FieldInfo.m_Width;

                                auto& x = reinterpret_cast<std::uint8_t&>(m_Memory[FieldInfo.m_iData]);
                                x = *p?1:0;
                            }
                            else
                            {
                                static_assert ( std::is_pointer_v<T> );
                                constexpr auto size = sizeof(decltype(*p)); 
                                FieldInfo.m_Width = size;
                                FieldInfo.m_iData = align_to( m_iMemOffet, std::alignment_of_v<decltype(*p)> );
                                m_iMemOffet = FieldInfo.m_iData + FieldInfo.m_Width;

                                if constexpr ( size ==  1 ) 
                                {
                                    auto& x = reinterpret_cast<std::uint8_t& >(m_Memory[FieldInfo.m_iData]);
                                    x = reinterpret_cast<std::uint8_t&>(*p);
                                }
                                else    if constexpr ( size == 2 ) 
                                {
                                    auto& x = reinterpret_cast<std::uint16_t& >(m_Memory[FieldInfo.m_iData]);
                                    x = reinterpret_cast<std::uint16_t&>(*p);
                                    if( m_File.m_States.m_isEndianSwap ) x = endian::Convert(x);
                                }
                                else    if constexpr ( size == 4 ) 
                                {
                                    auto& x = reinterpret_cast<std::uint32_t& >(m_Memory[FieldInfo.m_iData]);
                                    x = reinterpret_cast<std::uint32_t&>(*p);
                                    if( m_File.m_States.m_isEndianSwap ) x = endian::Convert(x);
                                }
                                else    if constexpr ( size == 8 ) 
                                {
                                    auto& x = reinterpret_cast<std::uint64_t& >(m_Memory[FieldInfo.m_iData]);
                                    x = reinterpret_cast<std::uint64_t&>(*p);

                                    if( m_File.m_States.m_isEndianSwap ) 
                                        x = endian::Convert(x);
                                }
                            }
                        }
                    }
                }, A );
            }
        }
        else
        {
            //
            // Lambda Used to write the fields
            //
            auto Numerics = [&]( details::field_info& FieldInfo, auto p, const char* pFmt ) noexcept
            {
                using T = std::decay_t<decltype(p)>;

                FieldInfo.m_iData = m_iMemOffet;
                std::string_view temp(m_Memory.begin() + m_iMemOffet, m_Memory.end() );
                m_iMemOffet += 1 + sprintf_s(const_cast<char*>(temp.data()), temp.size(), pFmt, p);
                FieldInfo.m_Width = m_iMemOffet - FieldInfo.m_iData - 1;

                if constexpr ( std::is_integral_v<T> )
                {
                    FieldInfo.m_IntWidth = FieldInfo.m_Width;
                }
                else
                {
                    for( FieldInfo.m_IntWidth = 0; m_Memory[FieldInfo.m_iData + FieldInfo.m_IntWidth] != '.' && m_Memory[FieldInfo.m_iData+FieldInfo.m_IntWidth]; FieldInfo.m_IntWidth++ );
                }
            };

            //
            // Write to a buffer the data
            //
            for( auto& A : Args )
            {
                auto& Field = Column.m_FieldInfo.emplace_back(); 

                std::visit( [&]( auto p ) constexpr
                {
                    using t = std::decay_t<decltype(p)>;
                            if constexpr ( std::is_same_v<t,std::uint8_t*>      ) Numerics( Field, *p, "#%X" );
                    else    if constexpr ( std::is_same_v<t,std::uint16_t*>     ) Numerics( Field, *p, "#%X" );
                    else    if constexpr ( std::is_same_v<t,std::uint32_t*>     ) Numerics( Field, *p, "#%X" );
                    else    if constexpr ( std::is_same_v<t,std::uint64_t*>     ) Numerics( Field, *p, "#%llX" );
                    else    if constexpr ( std::is_same_v<t,bool*>              ) Numerics( Field, (int)*p, "%d" );
                    else    if constexpr ( std::is_same_v<t,std::int8_t*>       ) Numerics( Field, *p, "%d" );
                    else    if constexpr ( std::is_same_v<t,std::int16_t*>      ) Numerics( Field, *p, "%d" );
                    else    if constexpr ( std::is_same_v<t,std::int32_t*>      ) Numerics( Field, *p, "%d" );
                    else    if constexpr ( std::is_same_v<t,std::int64_t*>      ) Numerics( Field, *p, "%lld" );
                    else    if constexpr ( std::is_same_v<t,float*>             ) 
                            { 
                                if( m_File.m_States.m_isSaveFloats )    Numerics( Field, static_cast<double>(*p), "%.9g" );
                                else                                    Numerics( Field, reinterpret_cast<std::uint32_t&>(*p),  "#%X" ); 
                            }
                    else    if constexpr ( std::is_same_v<t,double*>            ) 
                            { 
                                if( m_File.m_States.m_isSaveFloats )    Numerics( Field, *p, "%.17g" );
                                else                                    Numerics( Field, reinterpret_cast<std::uint64_t&>(*p),  "#%llX" ); 
                            }
                    else    if constexpr ( std::is_same_v<t, std::string*> )
                            {
                                Field.m_iData = m_iMemOffet;
                                std::string_view temp(m_Memory.begin() + Field.m_iData, m_Memory.end() );
                                m_iMemOffet += 1 + sprintf_s(const_cast<char*>(temp.data()), temp.size(), "\"%s\"", p->c_str());
                                Field.m_Width = m_iMemOffet - Field.m_iData - 1;
                            }
                    else    if constexpr (std::is_same_v<t, std::wstring*>)
                            {
                                Field.m_iData = m_iMemOffet;
                                std::string temp = details::wstring_to_utf8( {reinterpret_cast<wchar_t*>(m_Memory.data() + Field.m_iData), reinterpret_cast<wchar_t*>(m_Memory.data() + m_Memory.size())});

                                m_Memory[m_iMemOffet++] = '"';
                                for (auto E : temp) m_Memory[m_iMemOffet++] = E;
                                m_Memory[m_iMemOffet++] = '"';
                                m_Memory[m_iMemOffet++] = 0;

                                Field.m_Width = m_iMemOffet - Field.m_iData - 1;
                            }
                    else
                    { 
                        assert(false); 
                    }
                }, A );
            }
        }

        return {};
    }

    //------------------------------------------------------------------------------

    err stream::WriteLine( void ) noexcept
    {
        err  Error;

        assert( m_File.m_States.m_isReading == false );

        // Make sure that the user_types don't try to write more lines than expected
        assert( m_iLine < m_Record.m_Count );

        //
        // Increment the line count
        // and reset the column index
        //
        m_iLine++;
        m_iColumn   = 0;

        //
        // We will wait writing the line if we can so we can format
        //
        if( (m_iLine < m_Record.m_Count && (m_iLine%m_nLinesBeforeFileWrite) != 0) )
        {
            return Error;
        }


        //
        // Lets handle the binary case first
        //
        if( m_File.m_States.m_isBinary )
        {
            if( m_iLine <= m_nLinesBeforeFileWrite )
            {
                //
                // Write any pending user_types types
                //
                if( WriteUserTypes().isError(Error))
                    return Error;
            
                //
                // Write record header
                //

                // First handle the case that is a label  
                if( m_nColumns == -1 )
                {
                    if (m_File.WriteChar('@').isError(Error))
                        return Error;

                    if (m_File.WriteChar('[').isError(Error))
                        return Error;

                    if (m_File.WriteStr({ m_Record.m_Name.data(), std::strlen(m_Record.m_Name.data()) + 1 }).isError(Error))
                        return Error;

                    goto CLEAR;
                }

                if( m_File.WriteChar( '[' ).isError(Error) )
                    return Error;

                if( m_File.WriteStr( { m_Record.m_Name.data(), std::strlen( m_Record.m_Name.data() )+1 } ).isError( Error ) )
                    return Error;

                if( m_File.Write( m_Record.m_Count ).isError( Error ) )
                    return Error;

                //
                // Write types
                //
                {
                    std::uint8_t nColumns = static_cast<std::uint8_t>(m_nColumns);
                    if( m_File.Write( nColumns ).isError( Error ) ) return Error;
                }

                for( int i=0; i<m_nColumns; ++i )
                {
                    auto& Column = m_Columns[i];

                    if( m_File.WriteStr( std::string_view{ Column.m_Name.data(), static_cast<std::size_t>(Column.m_NameLength) } ).isError( Error ) )
                        return Error;

                    if( Column.m_nTypes == -1 )
                    {
                        if( m_File.WriteChar( '?' ).isError( Error ) )
                            return Error;
                    }
                    else
                    {
                        if( Column.m_UserType.m_Value )
                        {
                            if( m_File.WriteChar( ';' ).isError( Error ) )
                                return Error;

                            std::uint8_t Index = static_cast<std::uint8_t>(getUserType(Column.m_UserType) - m_UserTypes.data());  // m_UserTypes.getIndexByEntry<std::uint8_t>( *getUserType(Column.m_UserType) );
                            if( m_File.Write( Index ).isError( Error ) )
                                return Error;
                        }
                        else
                        {
                            if( m_File.WriteChar( ':' ).isError( Error ) )
                                return Error;

                            if( m_File.WriteStr( { Column.m_SystemTypes.data(), static_cast<std::size_t>(Column.m_nTypes + 1) } ).isError( Error ) )
                                return Error;
                        }
                    }
                }
            } // End of first line

            //
            // Dump line info
            //
            int L = m_iLine%m_nLinesBeforeFileWrite;
            if( L == 0 ) L = m_nLinesBeforeFileWrite;
            for( int l = 0; l<L; ++l )
            {
                for( int i = 0; i<m_nColumns; ++i )
                {
                    const auto& Column        = m_Columns[i];

                    if( Column.m_nTypes == -1 )
                    {
                        const auto& DynamicFields = Column.m_DynamicFields[l];

                        //
                        // First write the type
                        //
                        if( DynamicFields.m_UserType.m_Value ) 
                        {
                            auto            p     = getUserType( DynamicFields.m_UserType );
                            std::uint8_t    Index = static_cast<std::uint8_t>(p - m_UserTypes.data()); //m_UserTypes.getIndexByEntry<std::uint8_t>( *p );

                            if( m_File.WriteChar( ';' ).isError(Error) )
                                return Error;

                            if( m_File.Write( Index ).isError(Error) )
                                return Error;
                        }
                        else
                        {
                            if( m_File.WriteChar( ':' ).isError(Error) )
                                return Error;

                            if( m_File.WriteStr( std::string_view{ DynamicFields.m_SystemTypes.data(), static_cast<std::size_t>(DynamicFields.m_nTypes + 1) } ).isError(Error) )
                                return Error;
                        }

                        //
                        // Then write the values
                        //
                        for( int n=0; n<DynamicFields.m_nTypes; ++n )
                        {
                            const auto& FieldInfo   = Column.m_FieldInfo[ DynamicFields.m_iField + n ];
                            if( m_File.WriteData( std::string_view{ &m_Memory[ FieldInfo.m_iData ], static_cast<std::size_t>(FieldInfo.m_Width) } ).isError(Error) )
                                return Error;
                        }
                    }
                    else
                    {
                        for( int n=0; n<Column.m_nTypes; ++n )
                        {
                            const auto  Index       = l*Column.m_nTypes + n;
                            const auto& FieldInfo   = Column.m_FieldInfo[ Index ];
                            if( m_File.WriteData( std::string_view{ &m_Memory[ FieldInfo.m_iData ], static_cast<std::size_t>(FieldInfo.m_Width) } ).isError(Error) )
                                return Error;
                        }
                    }
                }
            }

            //
            // Clear the memory pointer
            //
            goto CLEAR ;
        }

        //
        // Initialize the columns
        //
        if( m_iLine <= m_nLinesBeforeFileWrite )
        {
            for( int i=0; i<m_nColumns; ++i )
            {
                auto& Column            = m_Columns[i];
                
                Column.m_FormatTotalSubColumns = 0;
                Column.m_FormatWidth           = 0;

                if( Column.m_nTypes == -1 )
                {
                    Column.m_SubColumn.clear();
                    Column.m_SubColumn.resize(Column.m_SubColumn.size() + 2);
                }
                else
                {
                    Column.m_SubColumn.clear();
                    Column.m_SubColumn.resize(Column.m_SubColumn.size() + Column.m_nTypes);
                }
            }
        }

        //
        // If it is a label just print it
        //
        if(m_nColumns == -1)
        {
            if (m_File.WriteFmtStr("\n@[ %s ]\n", m_Record.m_Name.data()).isError(Error))
                return Error;
            
            //
            // Clear the memory pointer
            //
            goto CLEAR;
        }

        //
        // Compute the width for each column also for each of its types
        //
        for( int i = 0; i<m_nColumns; ++i )
        {
            auto& Column        = m_Columns[i];

            if( Column.m_nTypes == -1 )
            {
                auto& TypeSubColumn   = Column.m_SubColumn[0];
                auto& ValuesSubColumn = Column.m_SubColumn[1];

                //
                // Compute the width with all the fields 
                //
                for( auto& DField : Column.m_DynamicFields )
                {
                    {
                        // Add the spaces between each field
                        DField.m_FormatWidth = (DField.m_nTypes-1) * m_nSpacesBetweenFields;

                        // count all the widths of the types
                        for( int n=0; n<DField.m_nTypes; n++ )
                        {
                            const auto& FieldInfo   = Column.m_FieldInfo[ DField.m_iField + n ];
                            DField.m_FormatWidth += FieldInfo.m_Width;
                        }

                        ValuesSubColumn.m_FormatWidth = std::max( ValuesSubColumn.m_FormatWidth, DField.m_FormatWidth );
                    }

                    // Dynamic types must include the type inside the column
                    if( DField.m_UserType.m_Value )
                    {
                        auto p = getUserType(DField.m_UserType);
                        assert(p);
                        TypeSubColumn.m_FormatWidth = std::max( TypeSubColumn.m_FormatWidth, p->m_NameLength + 1 ); // includes ";"
                    }
                    else
                    {
                        TypeSubColumn.m_FormatWidth = std::max( TypeSubColumn.m_FormatWidth, DField.m_nTypes + 1 ); // includes ":"
                    }
                }

                //
                // Handle the column name as well 
                //
                Column.m_FormatNameWidth = Column.m_NameLength + 1 + 1; // includes ":?"
                Column.m_FormatWidth           = std::max( Column.m_FormatWidth, ValuesSubColumn.m_FormatWidth + TypeSubColumn.m_FormatWidth + m_nSpacesBetweenFields );
                Column.m_FormatTotalSubColumns = std::max( Column.m_FormatTotalSubColumns, Column.m_FormatWidth );
                Column.m_FormatWidth           = std::max( Column.m_FormatWidth, Column.m_FormatNameWidth );
            }
            else
            {
                //
                // For any sub-columns that are floats we need to compute the max_int first
                //
                for( int it =0; it<Column.m_nTypes; ++it )
                {
                    if( Column.m_SystemTypes[it] != 'f' && Column.m_SystemTypes[it] != 'F' )
                        continue;
                    
                    auto& SubColumn = Column.m_SubColumn[it];
                    for( int iff = it; iff < Column.m_FieldInfo.size(); iff += Column.m_nTypes )
                    {
                        auto& Field = Column.m_FieldInfo[iff];
                        SubColumn.m_FormatIntWidth = std::max( SubColumn.m_FormatIntWidth, Field.m_IntWidth );
                    }
                }

                //
                // Computes the sub-columns sizes
                //
                for( auto& Field : Column.m_FieldInfo )
                {
                    const int iSubColumn = static_cast<int>(&Field - Column.m_FieldInfo.data()) % Column.m_nTypes; //Column.m_FieldInfo.getIndexByEntry(Field)%Column.m_nTypes;
                    auto&     SubColumn  = Column.m_SubColumn[iSubColumn];
                    if( Column.m_SystemTypes[iSubColumn] == 'f' || Column.m_SystemTypes[iSubColumn] == 'F' )
                    {
                        const int Width = (Field.m_Width - Field.m_IntWidth) + SubColumn.m_FormatIntWidth;
                        SubColumn.m_FormatWidth = std::max( SubColumn.m_FormatWidth, Width );
                    }
                    else
                    {
                        SubColumn.m_FormatWidth = std::max( SubColumn.m_FormatWidth, Field.m_Width );
                    }
                }

                //
                // Compute the columns sizes
                //

                // Add all the spaces between fields
                Column.m_FormatWidth = (Column.m_nTypes - 1) * m_nSpacesBetweenFields;

                assert( Column.m_nTypes == Column.m_SubColumn.size() );
                // Add all the sub-columns widths
                for( auto& Subcolumn : Column.m_SubColumn )
                {
                    Column.m_FormatWidth += Subcolumn.m_FormatWidth;
                }

                //
                // Handle the column name as well 
                //
                Column.m_FormatNameWidth = Column.m_NameLength + 1; // includes ":"
                if( Column.m_UserType.m_Value )
                {
                    auto p = getUserType( Column.m_UserType );
                    assert(p);
                    Column.m_FormatNameWidth += p->m_NameLength;
                }
                else
                {
                    Column.m_FormatNameWidth += Column.m_nTypes; 
                }

                // Decide the final width for this column
                Column.m_FormatTotalSubColumns = std::max( Column.m_FormatTotalSubColumns, Column.m_FormatWidth );
                Column.m_FormatWidth = std::max( Column.m_FormatWidth, Column.m_FormatNameWidth );
            }
        }

        //
        // Save the record info
        //
        if( m_iLine <= m_nLinesBeforeFileWrite )
        {
            //
            // Write any pending user_types types
            //
            if( WriteUserTypes().isError( Error ) )
                return Error;
        
            //
            // Write header
            //
            if( m_Record.m_bWriteCount )
            {
                if( m_File.WriteFmtStr( "\n[ %s : %d ]\n", m_Record.m_Name.data(), m_Record.m_Count ).isError( Error ) )
                    return Error;
            }
            else
            {
                if( m_File.WriteFmtStr( "\n[ %s ]\n", m_Record.m_Name.data() ).isError(Error) )
                    return Error;
            }

            //
            // Write the types
            //
            {
                if( m_File.WriteStr( "{ " ).isError(Error) )
                    return Error;

                for( int i = 0; i<m_nColumns; ++i )
                {
                    auto& Column        = m_Columns[i];

                    if( Column.m_nTypes == -1 )
                    {
                        if( m_File.WriteFmtStr( "%s:?", Column.m_Name.data() ).isError(Error) )
                            return Error;
                    }
                    else
                    {
                        if( Column.m_UserType.m_Value )
                        {
                            auto p = getUserType(Column.m_UserType);
                            assert(p);

                            if( m_File.WriteFmtStr( "%s;%s", Column.m_Name.data(), p->m_Name.data() ).isError(Error) )
                                return Error;
                        }
                        else
                        {
                            if( m_File.WriteFmtStr( "%s:%s", Column.m_Name.data(), Column.m_SystemTypes.data() ).isError(Error) )
                                return Error;
                        }
                    }

                    // Write spaces to reach the end of the column
                    if( Column.m_FormatWidth > Column.m_FormatNameWidth )
                    {
                        if( m_File.WriteChar( ' ', Column.m_FormatWidth - Column.m_FormatNameWidth ).isError(Error) )
                            return Error;
                    }

                    if( (i+1) != m_nColumns )
                    {
                        // Write spaces between columns
                        if( m_File.WriteChar( ' ', m_nSpacesBetweenColumns ).isError(Error) ) return Error;
                    }
                }

                if( m_File.WriteFmtStr( " }\n" ).isError(Error) )
                    return Error;
            }

            //
            // Write a nice underline for the columns
            //
            {
                if( m_File.WriteStr( "//" ).isError(Error) )
                    return Error;

                for( int i = 0; i<m_nColumns; ++i )
                {
                    auto& Column  = m_Columns[i];

                    if( m_File.WriteChar( '-', Column.m_FormatWidth ).isError(Error) )
                        return Error;

                    // Get ready for the next type
                    if( (i+1) != m_nColumns) if( m_File.WriteChar( ' ', m_nSpacesBetweenColumns ).isError(Error) ) return Error;
                }

                if( m_File.WriteChar( '\n' ).isError(Error) )
                    return Error;
            }
        }

        //
        // Print all the data
        //
        {
            int L = m_iLine%m_nLinesBeforeFileWrite;
            if( L == 0 ) L = m_nLinesBeforeFileWrite;
            for( int l = 0; l<L; ++l )
            {
                // Prefix with two spaces to align things
                if( m_File.WriteChar( ' ', 2 ).isError(Error) ) return Error;

                for( int i = 0; i<m_nColumns; ++i )
                {
                    const auto& Column        = m_Columns[i];

                    if( Column.m_nTypes == -1 )
                    {
                        const auto& DynamicFields = Column.m_DynamicFields[l];

                        //
                        // First write the type
                        //
                        if( DynamicFields.m_UserType.m_Value ) 
                        {
                            auto p = getUserType( DynamicFields.m_UserType );
                            assert(p);
                            if( m_File.WriteFmtStr( ";%s", p->m_Name.data() ).isError(Error) )
                                return Error;

                            // Fill spaces to reach the next column
                            if( m_File.WriteChar( ' ', Column.m_SubColumn[0].m_FormatWidth - p->m_NameLength -1 + m_nSpacesBetweenFields ).isError(Error) )
                                return Error;
                        }
                        else
                        {
                            if( m_File.WriteFmtStr( ":%s", DynamicFields.m_SystemTypes.data() ).isError(Error) )
                                return Error;

                            // Fill spaces to reach the next column
                            if( m_File.WriteChar( ' ', Column.m_SubColumn[0].m_FormatWidth - DynamicFields.m_nTypes -1 + m_nSpacesBetweenFields ).isError(Error) )
                                return Error;
                        }

                        //
                        // Then write the values
                        //
                        for( int n=0; n<DynamicFields.m_nTypes; ++n )
                        {
                            const auto& FieldInfo   = Column.m_FieldInfo[ DynamicFields.m_iField + n ];
                    
                            if( m_File.WriteStr( std::string{ &m_Memory[ FieldInfo.m_iData ], static_cast<std::size_t>(FieldInfo.m_Width) } ).isError(Error) )
                                return Error;
                                
                            // Get ready for the next type
                            if( (DynamicFields.m_nTypes-1) != n) if( m_File.WriteChar( ' ', m_nSpacesBetweenFields ).isError(Error) )
                                return Error;
                        }

                        // Pad the width to match the columns width
                        if( m_File.WriteChar( ' ', Column.m_FormatWidth 
                            - DynamicFields.m_FormatWidth
                            - Column.m_SubColumn[0].m_FormatWidth 
                            - m_nSpacesBetweenFields  ).isError(Error) )
                            return Error;
                    }
                    else
                    {
                        const auto  Center      = Column.m_FormatWidth - Column.m_FormatTotalSubColumns;

                        if( (Center>>1) > 0 )
                            if( m_File.WriteChar( ' ', Center>>1).isError(Error) ) return Error;

                        for( int n=0; n<Column.m_nTypes; ++n )
                        {
                            const auto  Index       = l*Column.m_nTypes + n;
                            const auto& FieldInfo   = Column.m_FieldInfo[ Index ];
                            const auto& SubColumn   = Column.m_SubColumn[ n ];

                            if( Column.m_SystemTypes[n] == 'f' || Column.m_SystemTypes[n] == 'F' )
                            {
                                // point align Right align
                                if( m_File.WriteChar( ' ', SubColumn.m_FormatIntWidth - FieldInfo.m_IntWidth ).isError(Error) )
                                    return Error;

                                if( m_File.WriteStr( std::string_view{ &m_Memory[ FieldInfo.m_iData ], static_cast<std::size_t>(FieldInfo.m_Width) } ).isError(Error) )
                                    return Error;

                                // Write spaces to reach the next sub-column
                                int nSpaces = SubColumn.m_FormatWidth - ( SubColumn.m_FormatIntWidth + FieldInfo.m_Width - FieldInfo.m_IntWidth );
                                if( m_File.WriteChar( ' ', nSpaces ).isError(Error) ) return Error;
                            }
                            else if( Column.m_SystemTypes[n] == 's' )
                            {
                                // Left align
                                if( m_File.WriteStr( std::string_view{ &m_Memory[ FieldInfo.m_iData ], static_cast<std::size_t>(FieldInfo.m_Width) } ).isError(Error) )
                                    return Error;

                                if( m_File.WriteChar( ' ', SubColumn.m_FormatWidth - FieldInfo.m_Width ).isError(Error) )
                                    return Error;
                            }
                            else
                            {
                                // Right align
                                if( m_File.WriteChar( ' ', SubColumn.m_FormatWidth - FieldInfo.m_Width ).isError(Error) )
                                    return Error;

                                if( m_File.WriteStr( std::string_view{ &m_Memory[ FieldInfo.m_iData ], static_cast<std::size_t>(FieldInfo.m_Width) } ).isError(Error) )
                                    return Error;
                            }

                            // Write spaces to reach the next sub-column
                            if( (n+1) != Column.m_nTypes ) 
                            {
                                if( m_File.WriteChar( ' ', m_nSpacesBetweenFields ).isError(Error) ) return Error;
                            }
                        }

                        // Add spaces to finish this column
                        if( Center > 0 )
                            if( m_File.WriteChar( ' ', Center - (Center>>1) ).isError(Error) ) return Error;
                    }

                    // Write spaces to reach the next column
                    if((i+1) != m_nColumns) if( m_File.WriteChar( ' ', m_nSpacesBetweenColumns ).isError(Error) ) return Error;
                }

                // End the line
                if( m_File.WriteStr( "\n" ).isError(Error) )
                    return Error;
            }
        }

        //
        // Reset to get ready for the next block of lines
        //
        CLEAR:
        // Clear the columns
        if( m_iLine < m_Record.m_Count )
        {
            for( int i=0; i<m_nColumns; ++i )
            {
                auto& C = m_Columns[i];

                C.m_DynamicFields.clear();
                C.m_FieldInfo.clear();
            }
        }
        // Clear the memory pointer
        m_iMemOffet = 0;

        return Error;
    }

    //------------------------------------------------------------------------------
    inline
    bool stream::ValidateColumnChar( int c ) const noexcept
    {
        return false
            || (c >= 'a' && c <= 'z')
            || (c >= 'A' && c <= 'Z')
            || (c=='_')
            || (c==':')
            || (c=='>')
            || (c=='<')
            || (c=='?')
            || (c >= '0' && c <= '9' )
            ;
    }

    //------------------------------------------------------------------------------

    err stream::BuildTypeInformation( const char* pFieldName ) noexcept
    {
        err Error;
        scope_end_callback Scope( [&]
        {
            if ( Error )
            {
                printf("%s [%s] of Record[%s]\n", Error.m_pMessage, pFieldName, m_Record.m_Name.data() );
            }
        });

        // Make sure we have enough columns to store our data
        if( m_Columns.size() <= m_nColumns ) 
        {
            m_Columns.push_back({});
        }
        else
        {
            m_Columns[m_nColumns].clear();
        }

        auto& Column = m_Columns[m_nColumns++];
    
        //
        // Copy the column name 
        //
        for( Column.m_NameLength=0; pFieldName[Column.m_NameLength] !=';' && pFieldName[Column.m_NameLength] !=':'; ++Column.m_NameLength )
        {
            if( pFieldName[Column.m_NameLength]==0 || Column.m_NameLength >= static_cast<int>(Column.m_Name.size()) ) 
                return err::create_f< "Fail to read a column named, either string is too long or it has no types">();

            Column.m_Name[Column.m_NameLength] = pFieldName[Column.m_NameLength];
        }

        // Terminate name 
        Column.m_Name[Column.m_NameLength] = 0;

        if( pFieldName[Column.m_NameLength] ==';' )
        {
            Column.m_UserType = crc32::computeFromString( &pFieldName[Column.m_NameLength+1] );
            auto p = getUserType( Column.m_UserType );
            if( p == nullptr )
                return err::create_f< "Fail to find the user type for a column" >();

            Column.m_nTypes = p->m_nSystemTypes;
            strcpy_s( Column.m_SystemTypes.data(), Column.m_SystemTypes.size(), p->m_SystemTypes.data());

        }
        else
        {
            assert(pFieldName[Column.m_NameLength] ==':');

            if( pFieldName[Column.m_NameLength+1] == '?' )
            {
                Column.m_nTypes = -1;
                Column.m_SystemTypes[0]=0;
            }
            else
            {
                Column.m_nTypes = Strcpy_s( Column.m_SystemTypes.data(), Column.m_SystemTypes.size(), &pFieldName[Column.m_NameLength + 1]);
                if( Column.m_nTypes <= 0 )
                    return err::create_f<"Fail to read a column, type. not system types specified" >();

                // We remove the null termination
                Column.m_nTypes--;
            }

            Column.m_UserType.m_Value = 0;
        }

        return Error;
    }

    //------------------------------------------------------------------------------

    err stream::ReadFieldUserType( const char* pColumnName ) noexcept
    {
        err Error;

        if( m_iLine == 1 )
        {
            m_DataMapping.emplace_back() = -1;
            //xassert( m_iColumn == (m_DataMapping.size()-1) );

            // Find the column name
            bool bFound = false;
            for( int i=0; i<m_nColumns; ++i )
            {
                auto& Column = m_Columns[i];

                if( pColumnName[ Column.m_NameLength ] != 0 )
                {
                    if( pColumnName[ Column.m_NameLength ] != ':' || pColumnName[ Column.m_NameLength+1 ] != '?' )
                        continue;
                }

                // Make sure that we have a match
                {
                    int j;
                    for( j=Column.m_NameLength-1; j >= 0 && (Column.m_Name[j] == pColumnName[j]) ; --j );

                    // The string must not be the same
                    if( j != -1 )
                        continue;

                    m_DataMapping.back() = i;
                    bFound = true;
                    break;
                }
            }

            if( bFound == false )
            {
                printf( "Error: Unable to find the field %s\n", pColumnName);
                return err::create<err::state::FIELD_NOT_FOUND, "Unable to find the filed requested" >();
            }
        }

        if( -1 == m_DataMapping[m_iColumn] )
            return err::create< err::state::FIELD_NOT_FOUND, "Skipping unknown field" >();

        return Error;
    }

    //------------------------------------------------------------------------------

    err stream::ReadFieldUserType( crc32& UserType, const char* pColumnName ) noexcept
    {
        err Error;
        scope_end_callback Scope( [&]
        {
            // If we have any error lets make sure we always move to the next column
             if (Error) m_iColumn++;
        });

        //
        // Read the new field
        //
        if( ReadFieldUserType(pColumnName).isError(Error) ) return Error; 

        //
        // Get ready to read to column
        //
        auto& Column = m_Columns[m_DataMapping[m_iColumn]];

        //
        // if the type is '?' then check the types every call
        //
        if( Column.m_nTypes == -1 )
        {
            UserType = Column.m_DynamicFields[0].m_UserType;
        }
        else
        {
            UserType = Column.m_UserType;
        }

        //
        // Let the system know that we have moved on
        //
        m_iColumn++;

        return Error;
    }

    //------------------------------------------------------------------------------

    err stream::ReadColumn( const crc32 iUserRef, const char* pColumnName, std::span<details::arglist::types> Args ) noexcept
    {
        assert( m_iLine > 0 );

        err Error;
        scope_end_callback Scope( [&]
        {
            // If we have any error lets make sure we always move to the next column
             if (Error) m_iColumn++;
        });

        //
        // Get the user type if any
        //
        if( ReadFieldUserType( pColumnName ).isError(Error) )
            return Error;

        //
        // Create a mapping from user_types order to file order of fields
        //
        if( m_iLine == 1 )
        {
            // If we have a mapping then lets see if we can determine the types
            if( m_DataMapping[m_iColumn] == - 1 )
            {
                auto& Column = m_Columns[m_DataMapping[m_iColumn]];
                if( Column.m_nTypes >= Args.size() )
                {
                    if( Column.m_FieldInfo.size() == Args.size() )
                    {
                        for( int i=0; i<Column.m_FieldInfo.size(); i++ )
                        {
                            if( isCompatibleTypes( Column.m_SystemTypes[i], Args[i] ) == false )
                            {
                                m_DataMapping.back() = -1;
                                printf("Found the column but the types did not match. %s\n", pColumnName);
                                break;
                            }
                        }
                    }
                    else
                    {
                        // We don't have the same count
                        m_DataMapping.back() = -1;
                        printf("Found the column but the type count did not match. %s\n", pColumnName);
                    }
                }
            }
        }

        //
        // Get ready to read to column
        //
        auto& Column = m_Columns[m_DataMapping[m_iColumn]];

        //
        // if the type is '?' then check the types every call
        //
        if( Column.m_nTypes == -1 )
        {
            if( Column.m_FieldInfo.size() == Args.size() )
            {
                auto& D = Column.m_DynamicFields[0];
                for( int i=0; i<D.m_nTypes; i++ )
                {
                    if( isCompatibleTypes( D.m_SystemTypes[i], Args[i] ) == false )
                    {
                        printf("Found the column but the types did not match. %s\n", pColumnName);
                        return err::create< err::state::READ_TYPES_DONTMATCH, "Fail to find the correct type" >();
                    }
                }
            }
            else
            {
                // We don't have the same count
                printf("Found the column but the type count did not match. %s\n", pColumnName);
                return err::create<err::state::READ_TYPES_DONTMATCH, "Fail to find the correct type" >();
            }
        }

        //
        // Read each type
        //
        for( int i=0; i<Args.size(); i++ )
        {
            const auto& E    = Args[i];
            const auto iData = Column.m_FieldInfo[i].m_iData;

            std::visit( [&]( auto p )
            {
                using t = std::decay_t<decltype(p)>;

                if constexpr ( std::is_same_v<t,std::uint8_t*> || std::is_same_v<t,std::int8_t*> || std::is_same_v<t,bool*> )
                {
                    reinterpret_cast<std::uint8_t&>(*p) = reinterpret_cast<const std::uint8_t&>( m_Memory[ iData ]);
                }
                else if constexpr( std::is_same_v<t,std::uint16_t*> || std::is_same_v<t,std::int16_t*> )
                {
                    reinterpret_cast<std::uint16_t&>(*p) = reinterpret_cast<const std::uint16_t&>( m_Memory[ iData ]);
                }
                else if constexpr ( std::is_same_v<t,std::uint32_t*> || std::is_same_v<t,std::int32_t*> || std::is_same_v<t,float*> )
                {
                    reinterpret_cast<std::uint32_t&>(*p) = reinterpret_cast<const std::uint32_t&>( m_Memory[ iData ]);
                }
                else if constexpr ( std::is_same_v<t,std::uint64_t*> || std::is_same_v<t,std::int64_t*> || std::is_same_v<t,double*> )
                {
                    reinterpret_cast<std::uint64_t&>(*p) = reinterpret_cast<const std::uint64_t&>( m_Memory[ iData ]);
                }
                else if constexpr (std::is_same_v<t, std::string*> )
                {
                    *p = &m_Memory[iData];
                }
                else
                {
                    assert( false );
                }
            }, E );
        }

        //
        // The user_types reference
        //
     //   iUserRef = static_cast<int>(Column.m_UserType.m_Value);

        //
        // Ready to read the next column
        //
        m_iColumn++;

        return Error;
    }

    //------------------------------------------------------------------------------

    err stream::ReadLine( void ) noexcept
    {
        int                     c;
        int                     Size=0;
        std::array<char,256>    Buffer;
        err                     Error;

        // Make sure that the user_types doesn't read more lines than the record has
        assert( m_iLine <= m_Record.m_Count );

        //
        // If it is the first line we must read the type information before hand
        //
        if( m_iLine == 0 )
        {
            // Reset the user_types field offsets
            m_DataMapping.clear();

            // Solve types
            if( m_File.m_States.m_isBinary )
            {
                // Read the number of columns
                {
                    std::uint8_t nColumns;
                    if( m_File.Read(nColumns).isError(Error) ) 
                        return Error;

                    m_nColumns = nColumns;
                    m_Columns.clear();
                    m_Columns.resize( m_nColumns );
                }

                //
                // Read all the types
                //
                for( int l=0; l<m_nColumns; l++)
                {
                    auto& Column = m_Columns[l];

                    // Name
                    Column.m_NameLength = 0;
                    do 
                    {
                        if( m_File.getC(c).isError(Error) ) return Error;
                        Column.m_Name[Column.m_NameLength++] = c;
                    } while( c != ':'
                          && c != ';'
                          && c != '?' );

                    Column.m_NameLength--;
                    Column.m_Name[Column.m_NameLength] = 0;

                    // Read type information
                    if( c == ':' )
                    {
                        Column.m_nTypes = 0;
                        do 
                        {
                            if( m_File.getC(c).isError(Error) ) return Error;
                            Column.m_SystemTypes[Column.m_nTypes++] = c;
                        } while(c);
                        Column.m_nTypes--;
                        Column.m_UserType.m_Value = 0;
                    }
                    else if( c == ';' )
                    { 
                        std::uint8_t Index;
                        if( m_File.Read(Index).isError(Error) ) return Error;

                        auto& UserType = m_UserTypes[Index];
                        Column.m_UserType       = UserType.m_CRC;
                        Column.m_nTypes         = UserType.m_nSystemTypes;
                        Column.m_FormatWidth    = Index;
                    }
                    else if( c == '?' )
                    {
                        Column.m_nTypes = -1;
                        Column.m_UserType.m_Value = 0;
                    }
                }
            }
            else
            {
                // Read out all the white space
                if( m_File.ReadWhiteSpace(c).isError( Error ) )
                    return Error;

                //
                // we should have the right character by now
                //
                if( c != '{' ) return err::create_f< "Unable to find the types" >();

                // Get the next token
                if( m_File.ReadWhiteSpace(c).isError( Error ) )
                    return Error;

                do
                {
                    // Read a word
                    Size=0;
                    while( ValidateColumnChar(c) || c == ';' || c == ':' )
                    {
                        Buffer[Size++] = c;                    
                        if( m_File.getC(c).isError(Error) ) return Error;
                    }
            
                    // Terminate the string
                    Buffer[Size++] = 0;

                    // Okay build the type information
                    if( BuildTypeInformation( Buffer.data() ).isError(Error) ) 
                        return Error;

                    // Read any white space
                    if( m_File.ReadWhiteSpace(c).isError( Error ) )
                        return Error;

                } while( c != '}' );
            }
        }

        //
        // Read the actual data
        //
        if( m_File.m_States.m_isBinary )
        {
            auto ReadData = [&]( details::field_info& Info, int SystemType )
            {
                err Error;

                switch( SystemType )
                {
                    case 'c': 
                    case 'h':
                    {
                        std::uint8_t H;
                        if( m_File.Read(H).isError(Error) ) return Error;
                        Info.m_iData = align_to( m_iMemOffet, 1); m_iMemOffet = Info.m_iData + 1; reinterpret_cast<std::uint8_t &>(m_Memory[Info.m_iData]) = static_cast<std::uint8_t>(H);
                        Info.m_Width = 1;
                        break;
                    }
                    case 'C':
                    case 'H':
                    {
                        std::uint16_t H;
                        if( m_File.Read(H).isError(Error) ) return Error;
                        Info.m_iData = align_to( m_iMemOffet, 2); m_iMemOffet = Info.m_iData + 2; reinterpret_cast<std::uint16_t&>(m_Memory[Info.m_iData]) = static_cast<std::uint16_t>(H);
                        Info.m_Width = 2;
                        break;
                    }
                    case 'f':
                    case 'd':
                    case 'g':
                    {
                        std::uint32_t H;
                        if( m_File.Read(H).isError(Error) ) return Error;
                        Info.m_iData = align_to( m_iMemOffet, 4); m_iMemOffet = Info.m_iData + 4; reinterpret_cast<std::uint32_t&>(m_Memory[Info.m_iData]) = static_cast<std::uint32_t>(H);
                        Info.m_Width = 4;
                        break;
                    }
                    case 'F':
                    case 'G':
                    case 'D':
                    {
                        std::uint64_t H;
                        if( m_File.Read(H).isError(Error) ) return Error;
                        Info.m_iData = align_to( m_iMemOffet, 8); m_iMemOffet = Info.m_iData + 8; reinterpret_cast<std::uint64_t&>(m_Memory[Info.m_iData]) = static_cast<std::uint64_t>(H);
                        Info.m_Width = 8;
                        break;
                    }
                    case 's':
                    {
                        int c;
                        Info.m_iData = m_iMemOffet;
                        do
                        {
                            if( m_File.getC(c).isError(Error) ) return Error;
                            m_Memory[m_iMemOffet++] = c;
                        } while(c); 
                        Info.m_Width = m_iMemOffet - Info.m_iData;
                        break;
                    }
                }

                return Error;
            };

            for( m_iColumn=0; m_iColumn<m_nColumns; ++m_iColumn )
            {
                auto& Column = m_Columns[m_iColumn];

                Column.m_FieldInfo.clear();
                if( Column.m_nTypes == -1 )
                {
                    Column.m_DynamicFields.clear();
                    auto& D = Column.m_DynamicFields.emplace_back();
                    D.m_iField = 0;

                    // Get the first key code
                    if( m_File.getC(c).isError(Error) ) return Error;

                    // Read type information
                    if( c == ':' )
                    {
                        D.m_nTypes = 0;
                        do 
                        {
                            if( m_File.getC(c).isError(Error) ) return Error;
                            D.m_SystemTypes[D.m_nTypes++] = c;
                        } while(c);
                        D.m_nTypes--;
                        D.m_UserType.m_Value = 0;
                    }
                    else if( c == ';' )
                    { 
                        std::uint8_t Index;
                        if( m_File.Read(Index).isError(Error) ) return Error;

                        auto& UserType = m_UserTypes[Index];
                        D.m_UserType = UserType.m_CRC;
                        D.m_nTypes   = UserType.m_nSystemTypes;
                        for( int i=0; i<D.m_nTypes; ++i) D.m_SystemTypes[i] = UserType.m_SystemTypes[i];
                    }
                    else
                    {
                        assert(false);
                    }

                    //
                    // Read all the data
                    //
                    for( int i=0; i<D.m_nTypes; i++ )
                    {
                        if( ReadData( Column.m_FieldInfo.emplace_back(), D.m_SystemTypes[i] ).isError(Error) ) return Error;
                    }
                }
                else
                {
                    //
                    // Read all the data
                    //
                    if( Column.m_UserType.m_Value )
                    {
                        auto& UserType = m_UserTypes[Column.m_FormatWidth];
                        for( int i=0; i<UserType.m_nSystemTypes; i++ )
                        {
                            if( ReadData( Column.m_FieldInfo.emplace_back(), UserType.m_SystemTypes[i] ).isError(Error) ) return Error;
                        }
                    }
                    else
                    {
                        for( int i=0; i<Column.m_nTypes; i++ )
                        {
                            if( ReadData( Column.m_FieldInfo.emplace_back(), Column.m_SystemTypes[i] ).isError(Error) ) return Error;
                        }
                    }
                }
            }
        }
        else
        {
            //
            // Okay now we must read a line worth of data
            //    
            auto ReadComponent = [&]( details::field_info& Info, char SystemType ) noexcept
            {
                err Error;

                if( c == ' ' )
                    if( m_File.ReadWhiteSpace(c).isError( Error ) )
                        return Error;

                Size = 0;
                if ( c == '"' )
                {
                    if( SystemType != 's' && SystemType != 'S')
                        return err::create_f<"Unexpected string value expecting something else">();

                    Info.m_iData = m_iMemOffet;
                    do 
                    {
                        if( m_File.getC(c).isError(Error) ) return Error;
                        m_Memory[m_iMemOffet++] = c;
                    } while( c != '"' );

                    m_Memory[m_iMemOffet-1] = 0;
                }
                else
                {
                    std::uint64_t H;

                    if( c == '#' )
                    {
                        if( m_File.getC(c).isError(Error) ) return Error;
                        while(ishex(c) )
                        {
                            Buffer[Size++] = c;                    
                            if( m_File.getC(c).isError(Error) ) return Error;
                        }

                        if( Size == 0 )
                            return err::create_f< "Fail to read a numeric value" >();

                        Buffer[Size++] = 0;

                        H = strtoull( Buffer.data(), nullptr, 16);
                    }
                    else
                    {
                        bool isInt = true;

                        if( c == '-' )
                        {
                            Buffer[Size++] = c;                    
                            if( m_File.getC(c).isError(Error) ) return Error;
                        }

                        while( std::isdigit(c) ) 
                        {
                            Buffer[Size++] = c;                    
                            if( m_File.getC(c).isError(Error) ) return Error;
                            if( c == '.' )
                            {
                                // Continue reading as a float
                                isInt = false;
                                do 
                                {
                                    Buffer[Size++] = c;                    
                                    if( m_File.getC(c).isError(Error) ) return Error;
                                    
                                } while( std::isdigit(c) || c == 'e' || c == 'E' || c == '-' );
                                break;
                            }
                        }

                        if( Size == 0 )
                            return err::create_f< "Fail to read a numeric value" >();

                        Buffer[Size++] = 0;


                        if( SystemType == 'F' ) 
                        {
                            double x = atof(Buffer.data());
                            reinterpret_cast<double&>(H) = x;
                        }
                        else if( SystemType == 'f' )
                        {
                            float x = static_cast<float>(atof(Buffer.data()));
                            reinterpret_cast<float&>(H)  = x;
                        }
                        else if( isInt == false )
                        {
                            return err::create< err::state::MISMATCH_TYPES, "I found a floating point number while trying to load an integer value" >();
                        }
                        else if( Buffer[0] == '-' )
                        {
                            if(    SystemType == 'g' 
                                || SystemType == 'G' 
                                || SystemType == 'h' 
                                || SystemType == 'H' )
                            {
                                printf("Reading a sign integer into a field which is unsigned-int form this record [%s](%d)\n", m_Record.m_Name.data(), m_iLine);
                            }

                            H = static_cast<std::uint64_t>(strtoll( Buffer.data(), nullptr, 10));
                        }
                        else
                        {
                            H = static_cast<std::uint64_t>(strtoull( Buffer.data(), nullptr, 10));
                            if(    (SystemType == 'c' && H >= static_cast<std::uint64_t>(std::numeric_limits<std::int8_t>::max() ))
                                || (SystemType == 'C' && H >= static_cast<std::uint64_t>(std::numeric_limits<std::int16_t>::max()))
                                || (SystemType == 'd' && H >= static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()))
                                || (SystemType == 'D' && H >= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
                                )
                            {
                                printf("Reading a sign integer but the value in the file exceeds the allow positive integer portion in record [%s](%d)\n", m_Record.m_Name.data(), m_iLine);
                            }
                        }
                    }

                    if( c != ' ' && c != '\n' ) 
                        return err::create_f< "Expecting a space separator but I got a different character" >();

                    switch( SystemType )
                    {
                        case 'c': 
                        case 'h':
                                    Info.m_iData = align_to( m_iMemOffet, 1); m_iMemOffet = Info.m_iData + 1; reinterpret_cast<std::uint8_t &>(m_Memory[Info.m_iData]) = static_cast<std::uint8_t>(H);
                                    break;
                        case 'C':
                        case 'H':
                                    Info.m_iData = align_to( m_iMemOffet, 2); m_iMemOffet = Info.m_iData + 2; reinterpret_cast<std::uint16_t&>(m_Memory[Info.m_iData]) = static_cast<std::uint16_t>(H);
                                    break;
                        case 'f':
                        case 'd':
                        case 'g':
                                    Info.m_iData = align_to( m_iMemOffet, 4); m_iMemOffet = Info.m_iData + 4; reinterpret_cast<std::uint32_t&>(m_Memory[Info.m_iData]) = static_cast<std::uint32_t>(H);
                                    break;
                        case 'F':
                        case 'G':
                        case 'D':
                                    Info.m_iData = align_to( m_iMemOffet, 8); m_iMemOffet = Info.m_iData + 8; reinterpret_cast<std::uint64_t&>(m_Memory[Info.m_iData]) = static_cast<std::uint64_t>(H);
                                    break;
                    }
                }

                return Error;
            };

            for( m_iColumn=0; m_iColumn<m_nColumns; ++m_iColumn )
            {
                auto& Column = m_Columns[m_iColumn];

                // Read any white space
                if( m_File.ReadWhiteSpace(c).isError( Error ) )
                    return Error;

                Column.m_FieldInfo.clear();
                if( Column.m_nTypes == -1 )
                {
                    if( c != ':'  && c != ';' )
                        return err::create_f< "Expecting a type definition" >();

                    Column.m_DynamicFields.clear();
                    auto& D = Column.m_DynamicFields.emplace_back();
                    D.m_iField = 0;

                    Size = 0;
                    {
                        int x;
                        do 
                        {
                            if( m_File.getC(x).isError(Error) ) return Error;
                            Buffer[Size++] = x;
                        } while( std::isspace(x) == false );

                        Buffer[Size-1] = 0;
                    }

                    if( c == ';' )
                    {
                        D.m_UserType = crc32::computeFromString( Buffer.data() );
                        auto p = getUserType( D.m_UserType );
                        if( p == nullptr )
                            return err::create_f< "Fail to find the user type for a column" >();

                        D.m_nTypes = p->m_nSystemTypes;
                        strcpy_s( D.m_SystemTypes.data(), D.m_SystemTypes.size(), p->m_SystemTypes.data());
                    }
                    else
                    {
                        assert(c ==':');
                        D.m_nTypes = Strcpy_s( D.m_SystemTypes.data(), D.m_SystemTypes.size(), Buffer.data());
                        if( D.m_nTypes <= 0 )
                            return err::create_f< "Fail to read a column, type. not system types specified" >();

                        // Remove the null termination count
                        D.m_nTypes--;
                    }

                    // Read all the types
                    c = ' ';
                    for( int n=0; n<D.m_nTypes ;n++ )
                    {
                        auto& Field = Column.m_FieldInfo.emplace_back();
                        if( ReadComponent( Field, D.m_SystemTypes[n] ).isError(Error) )
                            return Error;
                    }
                }
                else
                {
                    for( int n=0; n<Column.m_nTypes ;n++ )
                    {
                        auto& Field = Column.m_FieldInfo.emplace_back();
                        if( ReadComponent( Field, Column.m_SystemTypes[n] ).isError(Error) )
                            return Error;
                    }
                }
            }
        }

        //
        // Increment the line count
        // reset the memory count
        //
        m_iLine++;
        m_iColumn   = 0;
        m_iMemOffet = 0;

        return Error;
    }

//------------------------------------------------------------------------------
    // Description:
    //      The second thing you do after the read the file is to read a record header which is what
    //      this function will do. After reading the header you probably want to switch base on the 
    //      record name. To do that use GetRecordName to get the actual string containing the name. 
    //      The next most common thing to do is to get how many rows to read. This is done by calling
    //      GetRecordCount. After that you will look throw n times reading first a line and then the fields.
    //------------------------------------------------------------------------------
    err stream::ReadRecord( void ) noexcept
    {
        err   Failure;
        int   c;

        assert( m_File.m_States.m_isReading );

        // if not we expect to read something
        if( m_File.m_States.m_isBinary ) 
        {
            // If it is the end of the file we are done
            do 
            {
                if( m_File.getC(c).isError(Failure) ) return Failure;

            } while( c != '@' && c != '[' && c != '<');
        
            //
            // Let's deal with user_types types
            //
            if( c == '<' ) do
            {
                std::array<char,64> SystemType;
                std::array<char,64> UserType;
                int i;

                // Read the first character of the user type
                if( m_File.getC(c).isError(Failure) ) return Failure;
                if( c == '[' ) break;

                // Read the user_types err
                i=0;
                UserType[i++] = c;
                while( c ) 
                {
                    if( m_File.getC(c).isError(Failure) ) return Failure;
                    UserType[i++] = c;
                }

                UserType[i] = 0;

                // Read the system err
                i=0;
                do 
                {
                    if( m_File.getC(c).isError(Failure) ) return Failure;
                    if( c == 0 ) break;
                    SystemType[i++] = c;
                } while( true );

                SystemType[i] = 0;

                //
                // Add the err
                //
                {
                    user_defined_types Type{ UserType.data(), SystemType.data() };
                    AddUserType( Type );
                }

            } while( true );

            //
            // Deal with a record
            //
            if( c == '@' ) 
            {
                if (m_File.getC(c).isError(Failure)) return Failure;
                m_Record.m_bLabel = true;
            }
            else
            {
                m_Record.m_bLabel = false;
            }

            if( c != '[' ) return err::create_f< "Unexpected character while reading the binary file." >();

            // Read the record name
            {
                std::size_t NameSize=0;
                do 
                {
                    if( m_File.getC(c).isError(Failure) ) return Failure;
                    if( NameSize >= static_cast<std::size_t>(m_Record.m_Name.size()) ) return err::create_f< "A record name was way too long, fail to read the file." >();
                    m_Record.m_Name[NameSize++] = c;
                } while (c);
            }

            // Read the record count
            if( m_File.Read( m_Record.m_Count ).isError(Failure) ) return Failure;
        }
        else
        {
            scope_end_callback CleanUp([&]
            {
                if (Failure) m_Record.m_Name[0u]=0;
            });

            //
            // Skip blank spaces and comments
            //
            if( m_File.ReadWhiteSpace( c ).isError(Failure) ) return Failure;

            //
            // Deal with user_types types
            // We read the err in case the user_types has not registered it already.
            // But the user_types should have register something....
            //
            if( c == '<' )
            {
                // Read any white space
                if( m_File.ReadWhiteSpace( c ).isError(Failure) ) return Failure;

                do
                {
                    // Create a user type
                    user_defined_types UserType;

                    UserType.m_NameLength=0;
                    while( c != ':' ) 
                    {
                        UserType.m_Name[UserType.m_NameLength++] = static_cast<char>(c);
                        if( m_File.getC(c).isError(Failure) ) return Failure;
                        if( UserType.m_NameLength >= static_cast<int>(UserType.m_Name.size()) ) return err::create_f< "Failed to find the termination character ':' for a user type" >();
                    }
                    UserType.m_Name[UserType.m_NameLength]=0;
                    UserType.m_CRC = crc32::computeFromString(UserType.m_Name.data());

                    UserType.m_nSystemTypes=0;
                    do 
                    {
                        if( m_File.getC(c).isError(Failure) ) return Failure;
                        if( c == '>' || c == ' ' ) break;
                        if( isValidType(c) == false ) return err::create_f< "Found a non-atomic type in user type definition" >();
                        UserType.m_SystemTypes[UserType.m_nSystemTypes++] = static_cast<char>(c);
                        if( UserType.m_nSystemTypes >= static_cast<int>(UserType.m_SystemTypes.size()) ) return err::create_f< "Failed to find the termination character '>' for a user type block" >();
                    } while( true );
                    UserType.m_SystemTypes[UserType.m_nSystemTypes]=0;

                    //
                    // Add the User err
                    //
                    AddUserType( UserType );

                    // Read any white space
                    if( std::isspace(c) )
                        if( m_File.ReadWhiteSpace( c ).isError(Failure) ) return Failure;

                } while( c != '>' );

                //
                // Skip spaces
                //
                if ( m_File.ReadWhiteSpace( c ).isError(Failure) ) return Failure;
            }
        
            //
            // Deal with a record
            //
            if (c == '@')
            {
                if (m_File.getC(c).isError(Failure)) return Failure;
                m_Record.m_bLabel = true;
            }
            else
            {
                m_Record.m_bLabel = false;
            }

            //
            // Make sure that we are dealing with a header now
            //
            if( c != '[' ) 
                return err::create_f< "Unable to find the right header symbol '['" >();

            // Skip spaces
            if( m_File.ReadWhiteSpace(c).isError(Failure) ) return Failure;

            int                         NameSize = 0;
            do
            {
                m_Record.m_Name[NameSize++] = c;
            
                if( m_File.getC(c).isError(Failure) ) return Failure;

            } while( std::isspace( c ) == false && c != ':' && c != ']' );

            // Terminate the string
            m_Record.m_Name[NameSize] = 0;

            // Skip spaces
            if( std::isspace( c ) )
                if( m_File.ReadWhiteSpace(c).isError(Failure) ) return Failure;
        
            //
            // Read the record count number 
            //
            if( m_Record.m_bLabel )
            {
                m_Record.m_Count = 0;
            }
            else
            {
                m_Record.m_Count = 1;
            }

            if( c == ':' )
            {
                // skip spaces and zeros
                do
                {
                    if( m_File.ReadWhiteSpace(c).isError(Failure) ) return Failure;
                
                } while( c == '0' );
            
                //
                // Handle the case of dynamic sizes tables
                //
                if( c == '?' )
                {
                   // TODO: Handle the special reader
                   if( m_File.HandleDynamicTable( m_Record.m_Count ).isError(Failure) ) return Failure;
                
                    // Read next character
                   if( m_File.getC(c).isError(Failure) ) return Failure;
                }
                else
                {
                    m_Record.m_Count = 0;
                    while( c >= '0' && c <= '9' )
                    {
                        m_Record.m_Count = m_Record.m_Count * 10 + (c-'0');
                       if( m_File.getC(c).isError(Failure) ) return Failure;
                    }
                }

                // Skip spaces
                if( std::isspace( c ) )
                    if( m_File.ReadWhiteSpace(c).isError(Failure) ) return Failure;
            }

            //
            // Make sure that we are going to conclude the field correctly
            //
            if( c != ']' )
                return err::create_f< "Fail reading the file. Expecting a '[' but didn't find it." >();
        }

        //
        // Reset the line count
        //
        m_iLine         = 0;
        m_iMemOffet     = 0;
        m_nColumns      = 0;

        return {};
    }
}
