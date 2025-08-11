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

    std::string strXstr(std::wstring&& Message) noexcept
    {
        // Ensure locale is set for UTF-8 conversion
        std::setlocale(LC_ALL, "en_US.UTF-8");

        const size_t max_bytes = Message.size() * 4 + 1;
        std::vector<char> buffer(max_bytes);
        size_t ret;
        errno_t err = wcstombs_s(&ret, buffer.data(), max_bytes, Message.c_str(), _TRUNCATE);
        if (err != 0)
        {
            assert(false);
        }

        return std::string{ buffer.data(), ret };
    }

    //-----------------------------------------------------------------------------------------------------

    inline std::string wstring_to_ascii_escape(std::wstring_view str)
    {
        if (str.empty())
            return {};

        static_assert(sizeof(wchar_t) >= 2, "wchar_t must be at least 16 bits");

        // Worst case: all characters are non-ASCII to \uXXXX = 6 bytes
        std::string result;
        result.reserve(str.size() * 6);

        for (wchar_t ch : str)
        {
            if (ch >= 0x20 && ch <= 0x7E && ch != '\\') // printable ASCII, skip encoding
            {
                result.push_back(static_cast<char>(ch));
            }
            else if (ch == '\\') // escape backslash
            {
                result += "\\\\";
            }
            else
            {
                result += "\\u";
                for (int shift = 12; shift >= 0; shift -= 4)
                {
                    unsigned digit = (ch >> shift) & 0xF;
                    result.push_back(digit < 10 ? ('0' + digit) : ('A' + (digit - 10)));
                }
            }
        }

        // Safety assertions
        assert(!result.empty());

        return result;
    }

    //------------------------------------------------------------------------------

    inline std::wstring ascii_escape_to_wstring(const std::string_view input)
{
        if (input.empty())
        {
            return {};
        }

        std::wstring result;
        // Rough estimate: 6 chars per \uXXXX
        result.reserve(input.size() / 6); 

        size_t i = 0;
        while (i + 5 < input.size()) 
        {
            if (input[i] == '\\' && input[i + 1] == 'u') 
            {
                unsigned int code = 0;

                // Parse 4 hex digits directly
                for (int j = 0; j < 4; ++j) 
                {
                    char c = input[i + 2 + j];
                    code <<= 4;
                         if (c >= '0' && c <= '9') code |= c - '0';
                    else if (c >= 'A' && c <= 'F') code |= c - 'A' + 10;
                    else if (c >= 'a' && c <= 'f') code |= c - 'a' + 10;
                    else goto FALLBACK_ASCII; // Invalid hex, fallback
                }

                result.push_back(static_cast<wchar_t>(code));
                i += 6;
            }
            else 
            {
                FALLBACK_ASCII:
                result.push_back(static_cast<unsigned char>(input[i]));

                // next
                ++i; 
            }
        }
        return result;
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

    xerr file::openForReading( const std::wstring_view FilePath, bool isBinary ) noexcept
    {
        assert(m_pFP == nullptr);
    #if defined(_MSC_VER)
        auto Err = _wfopen_s( &m_pFP, std::wstring(FilePath).c_str(), isBinary ? L"rb" : L"rt");
        if (Err != 0)
        {
            xerr::LogMessage<state::FAILURE>(strXstr(std::format(L"Error while using _wfopen_s with {} Gave, Error code reported: {}", FilePath, Err)));

            switch (Err)
            {
            case ENOENT: return xerr::create  < state::FILE_NOT_FOUND, "Error: File not found for reading">();
            case EACCES: return xerr::create_f< state, "Error: Permission denied: for reading">();
            case EINVAL: return xerr::create_f< state, "Error: Invalid parameter passed to _wfopen_s. for reading">();
            default:     return xerr::create_f< state, "Error: Failed to open file for reading with error code">();
            }
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

    xerr file::openForWriting( const std::wstring_view FilePath, bool isBinary  ) noexcept
    {
        assert(m_pFP == nullptr);
    #if defined(_MSC_VER)
        auto Err = _wfopen_s(&m_pFP, std::wstring(FilePath).c_str(), isBinary ? L"wb" : L"wt" );
        if (Err != 0)
        {
            xerr::LogMessage<state::FAILURE>(strXstr(std::format(L"Error while using _wfopen_s with {} Gave, Error code reported: {}", FilePath, Err)));

            switch (Err)
            {
            case ENOENT: return xerr::create  <state::FILE_NOT_FOUND, "Error: File not found: for writing">();
            case EACCES: return xerr::create_f<state, "Error: Permission denied: for writing">();
            case EINVAL: return xerr::create_f<state, "Error: Invalid parameter passed to _wfopen_s. for writing">();
            default:     return xerr::create_f<state, "Error: Failed to open file for writing with error code">();
            }
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

    xerr file::ReadingErrorCheck( void ) noexcept
    {
        if( m_States.m_isEOF || feof( m_pFP ) ) 
        {
            m_States.m_isEOF = true;
            return xerr::create<state::UNEXPECTED_EOF, "Found the end of the file unexpectedly while reading" >();
        }
        return xerr::create_f< state, "Fail while reading the file, expected to read more data" >();
    }

    //------------------------------------------------------------------------------
    template< typename T >
    xerr file::Read( T& Buffer, int Size, int Count ) noexcept
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

    xerr file::getC( int& c ) noexcept
    {
        assert(m_pFP);
        if( m_States.m_isEOF ) return ReadingErrorCheck();

        c = fgetc(m_pFP);
        if( c == -1 ) return ReadingErrorCheck();
        return {};
    }

    //------------------------------------------------------------------------------

    template< typename T >
    xerr file::Write( T& Buffer, int Size, int Count ) noexcept
    {
        assert(m_pFP);

    #if defined(_MSC_VER)
        if ( Count != fwrite( &Buffer, Size, Count, m_pFP ) )
        {
            return xerr::create_f< state, "Fail writing the required data" >();
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

    xerr file::WriteStr( const std::string_view Buffer ) noexcept
    {
        assert(m_pFP);
        // assert(Buffer.empty() || Buffer[Buffer.size() - 1] == 0);

    #if defined(_MSC_VER)
        if( m_States.m_isBinary )
        {
            if ( Buffer.size() != fwrite( Buffer.data(), sizeof(char), Buffer.size(), m_pFP ) )
            {
                return xerr::create_f< state, "Fail writing the required data" >();
            }
        }
        else
        {
            const std::uint64_t L           = Buffer.length();
            const std::uint64_t TotalData   = L>Buffer.size()?Buffer.size():L;
            if( TotalData != std::fwrite( Buffer.data(), 1, TotalData, m_pFP ) )
                return xerr::create_f< state, "Fail 'fwrite' writing the required data" >();
        }
    #else
    #endif
        return {};
    }

    //------------------------------------------------------------------------------

    xerr file::WriteFmtStr( const char* pFmt, ... ) noexcept
    {
        va_list Args;
        va_start( Args, pFmt );
        if( std::vfprintf( m_pFP, pFmt, Args ) < 0 )
            return xerr::create_f< state, "Fail 'fprintf' writing the required data" >();
        va_end( Args );
        return {};
    }

    //------------------------------------------------------------------------------

    xerr file::WriteChar( char C, int Count ) noexcept
    {
        while( Count-- )
        {
            if( C != fputc( C, m_pFP ) )
                return xerr::create_f< state, "Fail 'fputc' writing the required data" >();
        }
        return {};
    }

    //------------------------------------------------------------------------------

    xerr file::WriteData( std::string_view Buffer ) noexcept
    {
        assert( m_States.m_isBinary );

        if( Buffer.size() != std::fwrite( Buffer.data(), 1, Buffer.size(), m_pFP ) )
            return xerr::create_f< state, "Fail 'fwrite' binary mode" >();

        return {};
    }

    //------------------------------------------------------------------------------

    xerr file::ReadWhiteSpace( int& c ) noexcept
    {
        // Read any spaces
        do 
        {
            if( auto Err = getC(c); Err ) 
                return Err;

        } while( std::isspace( c ) );

        //
        // check for comments
        //
        while( c == '/' )
        {
            if( auto Err = getC(c); Err ) 
                return Err;

            if( c == '/' )
            {
                // Skip the comment
                do
                {
                    if( auto Err = getC(c); Err ) 
                        return Err;

                } while( c != '\n' );
            }
            else
            {
                return xerr::create_f< state, "Error reading file, unexpected symbol found [/]" >();
            }

            // Skip spaces
            if( auto Err = ReadWhiteSpace(c); Err ) 
                return Err;
        }

        return {};
    }

    //------------------------------------------------------------------------------

    xerr file::HandleDynamicTable( int& Count ) noexcept
    {
        auto        LastPosition    = ftell( m_pFP );
        int         c;
                
        Count           = -2;                   // -1. for the current header line, -1 for the types
        
        if( LastPosition == -1 )
            return xerr::create_f< state, "Fail to get the cursor position in the file" >();

        if( auto Err = getC(c); Err )
        {
            return xerr::create_f< state, "Unexpected end of file while searching the [*] for the dynamic" >(Err);
        }
    
        do
        {
            if( c == '\n' )
            {
                Count++;
                if( auto Err = ReadWhiteSpace(c); Err ) 
                    return Err;
            
                if( c == '[' )
                {
                    break;
                }
            }
            else
            {
                if( auto Err = getC(c); Err ) 
                    return Err;
            
                // if the end of the file is in a line then we need to count it
                if( c == -1 )
                {
                    Count++;
                    break;
                }
            }
    
        } while( true );

    
        if( Count <= 0  )
            return xerr::create_f< state, "Unexpected end of file while counting rows for the dynamic table" >();
    
        // Rewind to the start
        if( fseek( m_pFP, LastPosition, SEEK_SET ) )
            return xerr::create_f< state, "Fail to reposition the cursor back to the right place while reading the file" >();

        return {};
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

    xerr stream::openForReading( const std::wstring_view FilePath ) noexcept
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
            return Err;

        //
        // Okay make sure that we say that we are not reading the file
        // this will force the user_types to stick with the writing functions
        //
        xerr Error;
        xerr::cleanup CleanUp(Error, [&] { close(); });

        //
        // Determine signature (check if we are reading a binary file or not)
        //
        if ( isTextFile < 2 )
        {
            std::uint32_t Signature = 0;
            if(Error = m_File.Read(Signature); Error && (Error.getState<state>() != state::UNEXPECTED_EOF) )
            {
                return Error;
            }
            else 
            {
                Error.clear();
                if( Signature == std::uint32_t('NOIL') || Signature == std::uint32_t('LION') )
                {
                    if( Signature == std::uint32_t('LION') )
                        m_File.m_States.m_isEndianSwap = true;
                }
                else // We are dealing with a text file, if so the reopen it as such
                {
                    m_File.close();
                    if(Error = m_File.openForReading(FilePath, false); Error)
                        return Error;
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
        if(Error = ReadRecord(); Error ) 
            return Error;

        return {};
    }

    //-----------------------------------------------------------------------------------------------------

    xerr stream::openForWriting( const std::wstring_view FilePath, file_type FileType, flags Flags ) noexcept
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
        xerr                Error;
        xerr::cleanup CleanU(Error, [&] { close(); });


        //
        // Determine whether we are binary or text base
        //
        if( FileType == file_type::BINARY )
        {
            // Write binary signature
            const std::uint32_t Signature = std::uint32_t('NOIL');
            if( Error = m_File.Write( Signature ); Error )
                return Error;
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

        return {};
    }

    //-----------------------------------------------------------------------------------------------------

    void stream::close( void ) noexcept
    {
        m_File.close();
    }

    //------------------------------------------------------------------------------------------------

    xerr stream::Open( bool isRead, std::wstring_view View, file_type FileType, flags Flags ) noexcept
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

    xerr stream::WriteRecord( const char* pHeaderName, std::size_t Count ) noexcept
    {
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

        return {};
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

    xerr stream::WriteComment( const std::string_view Comment ) noexcept
    {
        if( m_File.m_States.m_isReading )
            return {};

        if( m_File.m_States.m_isBinary )
        {
            return {};
        }
        else
        {
            const auto  length = Comment.length();
            std::size_t iStart = 0;
            std::size_t iEnd   = iStart;

            if( auto Err = m_File.WriteChar( '\n' ); Err )
                return Err;

            do 
            {
                if( auto Err = m_File.WriteStr( "//" ); Err )
                    return Err;

                while( iEnd < length)
                {
                    if( Comment[iEnd] == '\n' ) { iEnd++; break; }
                    else                          iEnd++;
                }

                if( auto Err = m_File.WriteStr( { Comment.data() + iStart, static_cast<std::size_t>(iEnd - iStart) } ); Err )
                    return Err;

                if( iEnd == length) break;

                // Get ready for the next line
                iStart = iEnd++;

            } while( true );
        }

        return {};
    }

    //------------------------------------------------------------------------------

    xerr stream::WriteUserTypes( void ) noexcept
    {
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
                    if( auto Err = m_File.WriteChar( '<' ); Err )
                        return Err;
                }

                // Write the name 
                if( auto Err = m_File.WriteStr( { UserType.m_Name.data(), static_cast<std::size_t>(UserType.m_NameLength + 1) } ); Err )
                    return Err;

                // Write type/s
                if( auto Err = m_File.WriteStr( { UserType.m_SystemTypes.data(), static_cast<std::size_t>(UserType.m_nSystemTypes+1) } ); Err )
                    return Err;
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
                    if( auto Err = m_File.WriteStr( "\n// New Types\n< " ); Err ) 
                        return Err;

                    bNewTypes = true;
                }

                UserType.m_bAlreadySaved = true;
                std::array<char,256> temp;

                auto length = sprintf_s( temp.data(), temp.size(), "%s:%s ", UserType.m_Name.data(), UserType.m_SystemTypes.data());
                if( auto Err = m_File.WriteStr( {temp.data(), static_cast<std::size_t>(length)} ); Err ) 
                    return Err;
            }

            if( bNewTypes ) if( auto Err = m_File.WriteStr( ">\n" ); Err ) return Err;
        }

        return {};
    }

    //------------------------------------------------------------------------------
    
    xerr stream::WriteColumn( crc32 UserType, const char* pColumnName, std::span<details::arglist::types> Args) noexcept
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
                        FieldInfo.m_iData = align_to(m_iMemOffet, 2);
                        m_iMemOffet = FieldInfo.m_iData;

                        if ( p->empty() == false )
                        {
                            auto length = static_cast<int>(p->length() * sizeof(wchar_t));
                            memcpy( &m_Memory[m_iMemOffet], p->data(), length );
                            if (m_File.m_States.m_isEndianSwap)
                            {
                                for (int i=0; i< length; i += 2 )
                                {
                                    auto& X = reinterpret_cast<std::uint16_t&>(m_Memory[m_iMemOffet + i]);
                                    X = endian::Convert(X);
                                }
                            }
                            m_iMemOffet += length;
                        }

                        // make sure it is properly terminated
                        m_Memory[m_iMemOffet++] = 0;
                        m_Memory[m_iMemOffet++] = 0;

                        FieldInfo.m_Width = m_iMemOffet - FieldInfo.m_iData;
                    }
                    else
                    {
                        if constexpr ( std::is_pointer_v<T> == false 
                            || std::is_same_v<T,void*> 
                            || std::is_same_v<T,const char*>
                            || std::is_same_v<T, const wchar_t*> )
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
                                    if (m_File.m_States.m_isEndianSwap) x = endian::Convert(x);
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
                                    if( m_File.m_States.m_isEndianSwap )  x = endian::Convert(x);
                                }
                                else
                                {
                                    assert(false);
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
                                std::string temp = details::wstring_to_ascii_escape( *p );

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

    xerr stream::WriteLine( void ) noexcept
    {
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
            return {};
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
                if( auto Err = WriteUserTypes(); Err)
                    return Err;
            
                //
                // Write record header
                //

                // First handle the case that is a label  
                if( m_nColumns == -1 )
                {
                    if (auto Err = m_File.WriteChar('@'); Err)
                        return Err;

                    if (auto Err = m_File.WriteChar('['); Err)
                        return Err;

                    if (auto Err = m_File.WriteStr({ m_Record.m_Name.data(), std::strlen(m_Record.m_Name.data()) + 1 }); Err )
                        return Err;

                    goto CLEAR;
                }

                if( auto Err = m_File.WriteChar( '[' ); Err )
                    return Err;

                if( auto Err = m_File.WriteStr( { m_Record.m_Name.data(), std::strlen( m_Record.m_Name.data() )+1 } ); Err )
                    return Err;

                if( auto Err = m_File.Write( m_Record.m_Count ); Err )
                    return Err;

                //
                // Write types
                //
                {
                    std::uint8_t nColumns = static_cast<std::uint8_t>(m_nColumns);
                    if( auto Err = m_File.Write( nColumns ); Err ) 
                        return Err;
                }

                for( int i=0; i<m_nColumns; ++i )
                {
                    auto& Column = m_Columns[i];

                    if( auto Err = m_File.WriteStr( std::string_view{ Column.m_Name.data(), static_cast<std::size_t>(Column.m_NameLength) } ); Err )
                        return Err;

                    if( Column.m_nTypes == -1 )
                    {
                        if( auto Err = m_File.WriteChar( '?' ); Err )
                            return Err;
                    }
                    else
                    {
                        if( Column.m_UserType.m_Value )
                        {
                            if( auto Err = m_File.WriteChar( ';' ); Err )
                                return Err;

                            std::uint8_t Index = static_cast<std::uint8_t>(getUserType(Column.m_UserType) - m_UserTypes.data());  // m_UserTypes.getIndexByEntry<std::uint8_t>( *getUserType(Column.m_UserType) );
                            if( auto Err = m_File.Write( Index ); Err )
                                return Err;
                        }
                        else
                        {
                            if( auto Err = m_File.WriteChar( ':' ); Err )
                                return Err;

                            if( auto Err = m_File.WriteStr( { Column.m_SystemTypes.data(), static_cast<std::size_t>(Column.m_nTypes + 1) } ); Err )
                                return Err;
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
                            std::uint8_t    Index = static_cast<std::uint8_t>(p - m_UserTypes.data());

                            if( auto Err = m_File.WriteChar( ';' ); Err )
                                return Err;

                            if( auto Err = m_File.Write( Index ); Err )
                                return Err;
                        }
                        else
                        {
                            if( auto Err = m_File.WriteChar( ':' ); Err )
                                return Err;

                            if( auto Err = m_File.WriteStr( std::string_view{ DynamicFields.m_SystemTypes.data(), static_cast<std::size_t>(DynamicFields.m_nTypes + 1) } ); Err )
                                return Err;
                        }

                        //
                        // Then write the values
                        //
                        for( int n=0; n<DynamicFields.m_nTypes; ++n )
                        {
                            const auto& FieldInfo   = Column.m_FieldInfo[ DynamicFields.m_iField + n ];
                            if( auto Err = m_File.WriteData( std::string_view{ &m_Memory[ FieldInfo.m_iData ], static_cast<std::size_t>(FieldInfo.m_Width) } ); Err )
                                return Err;
                        }
                    }
                    else
                    {
                        for( int n=0; n<Column.m_nTypes; ++n )
                        {
                            const auto  Index       = l*Column.m_nTypes + n;
                            const auto& FieldInfo   = Column.m_FieldInfo[ Index ];
                            if( auto Err = m_File.WriteData( std::string_view{ &m_Memory[ FieldInfo.m_iData ], static_cast<std::size_t>(FieldInfo.m_Width) } ); Err )
                                return Err;
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
            if ( auto Err = m_File.WriteFmtStr("\n@[ %s ]\n", m_Record.m_Name.data()); Err)
                return Err;
            
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
            if( auto Err = WriteUserTypes(); Err )
                return Err;
        
            //
            // Write header
            //
            if( m_Record.m_bWriteCount )
            {
                if( auto Err = m_File.WriteFmtStr( "\n[ %s : %d ]\n", m_Record.m_Name.data(), m_Record.m_Count ); Err )
                    return Err;
            }
            else
            {
                if( auto Err = m_File.WriteFmtStr( "\n[ %s ]\n", m_Record.m_Name.data() ); Err )
                    return Err;
            }

            //
            // Write the types
            //
            {
                if( auto Err = m_File.WriteStr( "{ " ); Err )
                    return Err;

                for( int i = 0; i<m_nColumns; ++i )
                {
                    auto& Column        = m_Columns[i];

                    if( Column.m_nTypes == -1 )
                    {
                        if( auto Err = m_File.WriteFmtStr( "%s:?", Column.m_Name.data() ); Err )
                            return Err;
                    }
                    else
                    {
                        if( Column.m_UserType.m_Value )
                        {
                            auto p = getUserType(Column.m_UserType);
                            assert(p);

                            if( auto Err = m_File.WriteFmtStr( "%s;%s", Column.m_Name.data(), p->m_Name.data() ); Err )
                                return Err;
                        }
                        else
                        {
                            if( auto Err = m_File.WriteFmtStr( "%s:%s", Column.m_Name.data(), Column.m_SystemTypes.data() ); Err )
                                return Err;
                        }
                    }

                    // Write spaces to reach the end of the column
                    if( Column.m_FormatWidth > Column.m_FormatNameWidth )
                    {
                        if( auto Err = m_File.WriteChar( ' ', Column.m_FormatWidth - Column.m_FormatNameWidth ); Err )
                            return Err;
                    }

                    if( (i+1) != m_nColumns )
                    {
                        // Write spaces between columns
                        if( auto Err = m_File.WriteChar( ' ', m_nSpacesBetweenColumns ); Err ) 
                            return Err;
                    }
                }

                if( auto Err = m_File.WriteFmtStr( " }\n" ); Err )
                    return Err;
            }

            //
            // Write a nice underline for the columns
            //
            {
                if( auto Err = m_File.WriteStr( "//" ); Err )
                    return Err;

                for( int i = 0; i<m_nColumns; ++i )
                {
                    auto& Column  = m_Columns[i];

                    if( auto Err = m_File.WriteChar( '-', Column.m_FormatWidth ); Err )
                        return Err;

                    // Get ready for the next type
                    if( (i+1) != m_nColumns) 
                    {
                        if( auto Err = m_File.WriteChar( ' ', m_nSpacesBetweenColumns ); Err ) 
                            return Err;
                    }
                }

                if( auto Err = m_File.WriteChar( '\n' ); Err )
                    return Err;
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
                if( auto Err = m_File.WriteChar( ' ', 2 ); Err ) 
                    return Err;

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
                            if( auto Err = m_File.WriteFmtStr( ";%s", p->m_Name.data() ); Err )
                                return Err;

                            // Fill spaces to reach the next column
                            if( auto Err = m_File.WriteChar( ' ', Column.m_SubColumn[0].m_FormatWidth - p->m_NameLength -1 + m_nSpacesBetweenFields ); Err )
                                return Err;
                        }
                        else
                        {
                            if( auto Err = m_File.WriteFmtStr( ":%s", DynamicFields.m_SystemTypes.data() ); Err )
                                return Err;

                            // Fill spaces to reach the next column
                            if( auto Err = m_File.WriteChar( ' ', Column.m_SubColumn[0].m_FormatWidth - DynamicFields.m_nTypes -1 + m_nSpacesBetweenFields ); Err )
                                return Err;
                        }

                        //
                        // Then write the values
                        //
                        for( int n=0; n<DynamicFields.m_nTypes; ++n )
                        {
                            const auto& FieldInfo   = Column.m_FieldInfo[ DynamicFields.m_iField + n ];
                    
                            if( auto Err = m_File.WriteStr( std::string{ &m_Memory[ FieldInfo.m_iData ], static_cast<std::size_t>(FieldInfo.m_Width) } ); Err )
                                return Err;
                                
                            // Get ready for the next type
                            if( (DynamicFields.m_nTypes-1) != n)
                            {
                                if(auto Err = m_File.WriteChar( ' ', m_nSpacesBetweenFields ); Err )
                                    return Err;
                            }
                        }

                        // Pad the width to match the columns width
                        if( auto Err = m_File.WriteChar( ' ',    Column.m_FormatWidth 
                                                               - DynamicFields.m_FormatWidth
                                                               - Column.m_SubColumn[0].m_FormatWidth 
                                                               - m_nSpacesBetweenFields  ); Err )
                            return Err;
                    }
                    else
                    {
                        const auto  Center      = Column.m_FormatWidth - Column.m_FormatTotalSubColumns;

                        if( (Center>>1) > 0 )
                        {
                            if ( auto Err = m_File.WriteChar(' ', Center >> 1); Err) 
                                return Err;
                        }
                            

                        for( int n=0; n<Column.m_nTypes; ++n )
                        {
                            const auto  Index       = l*Column.m_nTypes + n;
                            const auto& FieldInfo   = Column.m_FieldInfo[ Index ];
                            const auto& SubColumn   = Column.m_SubColumn[ n ];

                            if( Column.m_SystemTypes[n] == 'f' || Column.m_SystemTypes[n] == 'F' )
                            {
                                // point align Right align
                                if( auto Err = m_File.WriteChar( ' ', SubColumn.m_FormatIntWidth - FieldInfo.m_IntWidth ); Err )
                                    return Err;

                                if( auto Err = m_File.WriteStr( std::string_view{ &m_Memory[ FieldInfo.m_iData ], static_cast<std::size_t>(FieldInfo.m_Width) } ); Err )
                                    return Err;

                                // Write spaces to reach the next sub-column
                                int nSpaces = SubColumn.m_FormatWidth - ( SubColumn.m_FormatIntWidth + FieldInfo.m_Width - FieldInfo.m_IntWidth );
                                if( auto Err = m_File.WriteChar( ' ', nSpaces ); Err ) 
                                    return Err;
                            }
                            else if( Column.m_SystemTypes[n] == 's' || Column.m_SystemTypes[n] == 'S')
                            {
                                // Left align
                                if( auto Err = m_File.WriteStr( std::string_view{ &m_Memory[ FieldInfo.m_iData ], static_cast<std::size_t>(FieldInfo.m_Width) } ); Err )
                                    return Err;

                                if( auto Err = m_File.WriteChar( ' ', SubColumn.m_FormatWidth - FieldInfo.m_Width ); Err )
                                    return Err;
                            }
                            else
                            {
                                // Right align
                                if( auto Err = m_File.WriteChar( ' ', SubColumn.m_FormatWidth - FieldInfo.m_Width ); Err )
                                    return Err;

                                if( auto Err = m_File.WriteStr( std::string_view{ &m_Memory[ FieldInfo.m_iData ], static_cast<std::size_t>(FieldInfo.m_Width) } ); Err )
                                    return Err;
                            }

                            // Write spaces to reach the next sub-column
                            if( (n+1) != Column.m_nTypes ) 
                            {
                                if( auto Err = m_File.WriteChar( ' ', m_nSpacesBetweenFields ); Err ) 
                                    return Err;
                            }
                        }

                        // Add spaces to finish this column
                        if( Center > 0 )
                        {
                            if ( auto Err = m_File.WriteChar(' ', Center - (Center >> 1)); Err ) 
                                return Err;
                        }
                            
                    }

                    // Write spaces to reach the next column
                    if((i+1) != m_nColumns) 
                    {
                        if( auto Err = m_File.WriteChar( ' ', m_nSpacesBetweenColumns ); Err ) 
                            return Err;
                    }
                }

                // End the line
                if( auto Err = m_File.WriteStr( "\n" ); Err )
                    return Err;
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

        return {};
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

    xerr stream::BuildTypeInformation( const char* pFieldName ) noexcept
    {
        xerr Error;
        xerr::cleanup Scope( Error, [&]
        {
            auto StringView = Error.getMessage();
            printf("%.*s [%s] of Record[%s]\n", static_cast<int>(StringView.size()), StringView.data(), pFieldName, m_Record.m_Name.data() );
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
                return Error = xerr::create_f< state, "Fail to read a column named, either string is too long or it has no types">();

            Column.m_Name[Column.m_NameLength] = pFieldName[Column.m_NameLength];
        }

        // Terminate name 
        Column.m_Name[Column.m_NameLength] = 0;

        if( pFieldName[Column.m_NameLength] ==';' )
        {
            Column.m_UserType = crc32::computeFromString( &pFieldName[Column.m_NameLength+1] );
            auto p = getUserType( Column.m_UserType );
            if( p == nullptr )
                return Error = xerr::create_f< state, "Fail to find the user type for a column" >();

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
                    return Error = xerr::create_f<state, "Fail to read a column, type. not system types specified" >();

                // We remove the null termination
                Column.m_nTypes--;
            }

            Column.m_UserType.m_Value = 0;
        }

        return {};
    }

    //------------------------------------------------------------------------------

    xerr stream::ReadFieldUserType( const char* pColumnName ) noexcept
    {
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
                return xerr::create<state::FIELD_NOT_FOUND, "Unable to find the filed requested" >();
            }
        }

        if( -1 == m_DataMapping[m_iColumn] )
            return xerr::create< state::FIELD_NOT_FOUND, "Skipping unknown field" >();

        return {};
    }

    //------------------------------------------------------------------------------

    xerr stream::ReadFieldUserType( crc32& UserType, const char* pColumnName ) noexcept
    {
        xerr Error;
        xerr::cleanup Scope( Error, [&]
        {
            // If we have any error lets make sure we always move to the next column
             m_iColumn++;
        });

        //
        // Read the new field
        //
        if( Error = ReadFieldUserType(pColumnName); Error ) 
            return Error; 

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

        return {};
    }

    //------------------------------------------------------------------------------

    xerr stream::ReadColumn( const crc32 iUserRef, const char* pColumnName, std::span<details::arglist::types> Args ) noexcept
    {
        assert( m_iLine > 0 );

        xerr Error;
        xerr::cleanup Scope( Error, [&]
        {
            // If we have any error lets make sure we always move to the next column
             m_iColumn++;
        });

        //
        // Get the user type if any
        //
        if( Error = ReadFieldUserType( pColumnName ); Error )
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
                        return Error = xerr::create< state::READ_TYPES_DONTMATCH, "Fail to find the correct type" >();
                    }
                }
            }
            else
            {
                // We don't have the same count
                printf("Found the column but the type count did not match. %s\n", pColumnName);
                return Error = xerr::create<state::READ_TYPES_DONTMATCH, "Fail to find the correct type" >();
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
                else if constexpr (std::is_same_v<t, std::wstring*>)
                {
                    if (m_File.m_States.m_isBinary) *p = reinterpret_cast<wchar_t*>(&m_Memory[iData]);
                    else                            *p = details::ascii_escape_to_wstring( &m_Memory[iData] );
                }
                else
                {
                    // We can not handle whatever type you are trying to use...
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

    xerr stream::ReadLine( void ) noexcept
    {
        int                     c;
        int                     Size=0;
        std::array<char,256>    Buffer;

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
                    if( auto Err = m_File.Read(nColumns); Err ) 
                        return Err;

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
                        if( auto Err = m_File.getC(c); Err ) 
                            return Err;

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
                            if( auto Err = m_File.getC(c); Err ) 
                                return Err;

                            Column.m_SystemTypes[Column.m_nTypes++] = c;
                        } while(c);
                        Column.m_nTypes--;
                        Column.m_UserType.m_Value = 0;
                    }
                    else if( c == ';' )
                    { 
                        std::uint8_t Index;
                        if( auto Err = m_File.Read(Index); Err )    
                            return Err;

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
                if( auto Err = m_File.ReadWhiteSpace(c); Err )
                    return Err;

                //
                // we should have the right character by now
                //
                if( c != '{' ) return xerr::create_f< state, "Unable to find the types" >();

                // Get the next token
                if( auto Err = m_File.ReadWhiteSpace(c); Err )
                    return Err;

                do
                {
                    // Read a word
                    Size=0;
                    while( ValidateColumnChar(c) || c == ';' || c == ':' )
                    {
                        Buffer[Size++] = c;                    
                        if( auto Err = m_File.getC(c); Err ) 
                            return Err;
                    }
            
                    // Terminate the string
                    Buffer[Size++] = 0;

                    // Okay build the type information
                    if( auto Err = BuildTypeInformation( Buffer.data() ); Err ) 
                        return Err;

                    // Read any white space
                    if( auto Err = m_File.ReadWhiteSpace(c); Err )
                        return Err;

                } while( c != '}' );
            }
        }

        //
        // Read the actual data
        //
        if( m_File.m_States.m_isBinary )
        {
            auto ReadData = [&]( details::field_info& Info, int SystemType ) ->xerr
            {
                switch( SystemType )
                {
                    case 'c': 
                    case 'h':
                    {
                        std::uint8_t H;
                        if( auto Err = m_File.Read(H); Err ) 
                            return Err;

                        Info.m_iData = align_to( m_iMemOffet, 1); m_iMemOffet = Info.m_iData + 1; reinterpret_cast<std::uint8_t &>(m_Memory[Info.m_iData]) = static_cast<std::uint8_t>(H);
                        Info.m_Width = 1;
                        break;
                    }
                    case 'C':
                    case 'H':
                    {
                        std::uint16_t H;
                        if( auto Err = m_File.Read(H); Err ) 
                            return Err;

                        Info.m_iData = align_to( m_iMemOffet, 2); m_iMemOffet = Info.m_iData + 2; reinterpret_cast<std::uint16_t&>(m_Memory[Info.m_iData]) = static_cast<std::uint16_t>(H);
                        Info.m_Width = 2;
                        break;
                    }
                    case 'f':
                    case 'd':
                    case 'g':
                    {
                        std::uint32_t H;
                        if( auto Err = m_File.Read(H); Err ) 
                            return Err;

                        Info.m_iData = align_to( m_iMemOffet, 4); m_iMemOffet = Info.m_iData + 4; reinterpret_cast<std::uint32_t&>(m_Memory[Info.m_iData]) = static_cast<std::uint32_t>(H);
                        Info.m_Width = 4;
                        break;
                    }
                    case 'F':
                    case 'G':
                    case 'D':
                    {
                        std::uint64_t H;
                        if( auto Err = m_File.Read(H); Err ) 
                            return Err;

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
                            if( auto Err = m_File.getC(c); Err ) 
                                return Err;

                            m_Memory[m_iMemOffet++] = c;
                        } while(c); 
                        Info.m_Width = m_iMemOffet - Info.m_iData;
                        break;
                    }
                    case 'S':
                    {
                        short c;
                        Info.m_iData = align_to(m_iMemOffet, 2);
                        m_iMemOffet = Info.m_iData;
                        do
                        {
                            if (auto Err = m_File.Read( c, 2, 1); Err ) 
                                return Err;

                            m_Memory[m_iMemOffet++] = (c >> 0) & 0xff;
                            m_Memory[m_iMemOffet++] = (c >> 8) & 0xff;

                        } while (c);
                        Info.m_Width = m_iMemOffet - Info.m_iData;
                        break;
                    }
                }

                return {};
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
                    if( auto Err = m_File.getC(c); Err ) 
                        return Err;

                    // Read type information
                    if( c == ':' )
                    {
                        D.m_nTypes = 0;
                        do 
                        {
                            if( auto Err = m_File.getC(c); Err ) 
                                return Err;

                            D.m_SystemTypes[D.m_nTypes++] = c;
                        } while(c);
                        D.m_nTypes--;
                        D.m_UserType.m_Value = 0;
                    }
                    else if( c == ';' )
                    { 
                        std::uint8_t Index;
                        if( auto Err = m_File.Read(Index); Err ) 
                            return Err;

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
                        if( auto Err = ReadData( Column.m_FieldInfo.emplace_back(), D.m_SystemTypes[i] ); Err ) 
                            return Err;
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
                            if( auto Err = ReadData( Column.m_FieldInfo.emplace_back(), UserType.m_SystemTypes[i] ); Err) 
                                return Err;
                        }
                    }
                    else
                    {
                        for( int i=0; i<Column.m_nTypes; i++ )
                        {
                            if( auto Err = ReadData( Column.m_FieldInfo.emplace_back(), Column.m_SystemTypes[i] ); Err ) 
                                return Err;
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
            auto ReadComponent = [&]( details::field_info& Info, char SystemType ) noexcept ->xerr
            {
                if( c == ' ' )
                {
                    if (auto Err = m_File.ReadWhiteSpace(c); Err)
                        return Err;
                }

                Size = 0;
                if ( c == '"' )
                {
                    if( SystemType != 's' && SystemType != 'S')
                        return xerr::create_f< state, "Unexpected string value expecting something else">();

                    Info.m_iData = m_iMemOffet;
                    do 
                    {
                        if( auto Err = m_File.getC(c); Err ) 
                            return Err;

                        m_Memory[m_iMemOffet++] = c;
                    } while( c != '"' );

                    m_Memory[m_iMemOffet-1] = 0;
                }
                else
                {
                    std::uint64_t H;

                    if( c == '#' )
                    {
                        if( auto Err = m_File.getC(c); Err ) 
                            return Err;

                        while(ishex(c) )
                        {
                            Buffer[Size++] = c;                    
                            if( auto Err = m_File.getC(c); Err ) 
                                return Err;
                        }

                        if( Size == 0 )
                        {
                            return xerr::create_f< state, "Fail to read a numeric value" >();
                        }
                            

                        Buffer[Size++] = 0;

                        H = strtoull( Buffer.data(), nullptr, 16);
                    }
                    else
                    {
                        bool isInt = true;

                        if( c == '-' )
                        {
                            Buffer[Size++] = c;                    
                            if( auto Err = m_File.getC(c); Err ) 
                                return Err;
                        }

                        while( std::isdigit(c) ) 
                        {
                            Buffer[Size++] = c;                    
                            if( auto Err = m_File.getC(c); Err ) 
                                return Err;

                            if( c == '.' )
                            {
                                // Continue reading as a float
                                isInt = false;
                                do 
                                {
                                    Buffer[Size++] = c;                    
                                    if( auto Err = m_File.getC(c); Err ) 
                                        return Err;
                                    
                                } while( std::isdigit(c) || c == 'e' || c == 'E' || c == '-' );
                                break;
                            }
                        }

                        if( Size == 0 )
                            return xerr::create_f< state, "Fail to read a numeric value" >();

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
                            return xerr::create< state::MISMATCH_TYPES, "I found a floating point number while trying to load an integer value" >();
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
                        return xerr::create_f< state, "Expecting a space separator but I got a different character" >();

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

                return {};
            };

            for( m_iColumn=0; m_iColumn<m_nColumns; ++m_iColumn )
            {
                auto& Column = m_Columns[m_iColumn];

                // Read any white space
                if( auto Err = m_File.ReadWhiteSpace(c); Err)
                    return Err;

                Column.m_FieldInfo.clear();
                if( Column.m_nTypes == -1 )
                {
                    if( c != ':'  && c != ';' )
                        return xerr::create_f< state, "Expecting a type definition" >();

                    Column.m_DynamicFields.clear();
                    auto& D = Column.m_DynamicFields.emplace_back();
                    D.m_iField = 0;

                    Size = 0;
                    {
                        int x;
                        do 
                        {
                            if( auto Err = m_File.getC(x); Err ) 
                                return Err;

                            Buffer[Size++] = x;
                        } while( std::isspace(x) == false );

                        Buffer[Size-1] = 0;
                    }

                    if( c == ';' )
                    {
                        D.m_UserType = crc32::computeFromString( Buffer.data() );
                        auto p = getUserType( D.m_UserType );
                        if( p == nullptr )
                            return xerr::create_f< state, "Fail to find the user type for a column" >();

                        D.m_nTypes = p->m_nSystemTypes;
                        strcpy_s( D.m_SystemTypes.data(), D.m_SystemTypes.size(), p->m_SystemTypes.data());
                    }
                    else
                    {
                        assert(c ==':');
                        D.m_nTypes = Strcpy_s( D.m_SystemTypes.data(), D.m_SystemTypes.size(), Buffer.data());
                        if( D.m_nTypes <= 0 )
                            return xerr::create_f< state, "Fail to read a column, type. not system types specified" >();

                        // Remove the null termination count
                        D.m_nTypes--;
                    }

                    // Read all the types
                    c = ' ';
                    for( int n=0; n<D.m_nTypes ;n++ )
                    {
                        auto& Field = Column.m_FieldInfo.emplace_back();
                        if( auto Err = ReadComponent( Field, D.m_SystemTypes[n] ); Err )
                            return Err;
                    }
                }
                else
                {
                    for( int n=0; n<Column.m_nTypes ;n++ )
                    {
                        auto& Field = Column.m_FieldInfo.emplace_back();
                        if( auto Err = ReadComponent( Field, Column.m_SystemTypes[n] ); Err )
                            return Err;
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

        return {};
    }

//------------------------------------------------------------------------------
    // Description:
    //      The second thing you do after the read the file is to read a record header which is what
    //      this function will do. After reading the header you probably want to switch base on the 
    //      record name. To do that use GetRecordName to get the actual string containing the name. 
    //      The next most common thing to do is to get how many rows to read. This is done by calling
    //      GetRecordCount. After that you will look throw n times reading first a line and then the fields.
    //------------------------------------------------------------------------------
    xerr stream::ReadRecord( void ) noexcept
    {
        int   c;

        assert( m_File.m_States.m_isReading );

        // if not we expect to read something
        if( m_File.m_States.m_isBinary ) 
        {
            // If it is the end of the file we are done
            do 
            {
                if( auto Err = m_File.getC(c); Err ) 
                    return Err;

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
                if( auto Err = m_File.getC(c); Err ) 
                    return Err;

                if( c == '[' ) break;

                // Read the user_types err
                i=0;
                UserType[i++] = c;
                while( c ) 
                {
                    if( auto Err = m_File.getC(c); Err ) 
                        return Err;

                    UserType[i++] = c;
                }

                UserType[i] = 0;

                // Read the system err
                i=0;
                do 
                {
                    if( auto Err = m_File.getC(c); Err) 
                        return Err;

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
                if ( auto Err = m_File.getC(c); Err ) 
                    return Err;

                m_Record.m_bLabel = true;
            }
            else
            {
                m_Record.m_bLabel = false;
            }

            if( c != '[' ) return xerr::create_f< state, "Unexpected character while reading the binary file." >();

            // Read the record name
            {
                std::size_t NameSize=0;
                do 
                {
                    if( auto Err = m_File.getC(c); Err ) 
                        return Err;

                    if( NameSize >= static_cast<std::size_t>(m_Record.m_Name.size()) ) 
                        return xerr::create_f< state, "A record name was way too long, fail to read the file." >();

                    m_Record.m_Name[NameSize++] = c;
                } while (c);
            }

            // Read the record count
            if( auto Err = m_File.Read( m_Record.m_Count ); Err ) 
                return Err;
        }
        else
        {
            xerr Error;
            xerr::cleanup CleanUp(Error, [&]
            {
                m_Record.m_Name[0u]=0;
            });

            //
            // Skip blank spaces and comments
            //
            if( Error = m_File.ReadWhiteSpace( c ); Error ) 
                return Error;

            //
            // Deal with user_types types
            // We read the err in case the user_types has not registered it already.
            // But the user_types should have register something....
            //
            if( c == '<' )
            {
                // Read any white space
                if( Error = m_File.ReadWhiteSpace( c ); Error ) 
                    return Error;

                do
                {
                    // Create a user type
                    user_defined_types UserType;

                    UserType.m_NameLength=0;
                    while( c != ':' ) 
                    {
                        UserType.m_Name[UserType.m_NameLength++] = static_cast<char>(c);
                        if( Error = m_File.getC(c); Error ) 
                            return Error;

                        if( UserType.m_NameLength >= static_cast<int>(UserType.m_Name.size()) ) return Error = xerr::create_f< state, "Failed to find the termination character ':' for a user type" >();
                    }
                    UserType.m_Name[UserType.m_NameLength]=0;
                    UserType.m_CRC = crc32::computeFromString(UserType.m_Name.data());

                    UserType.m_nSystemTypes=0;
                    do 
                    {
                        if( Error = m_File.getC(c); Error ) 
                            return Error;

                        if( c == '>' || c == ' ' ) break;
                        if( isValidType(c) == false ) return Error = xerr::create_f< state, "Found a non-atomic type in user type definition" >();

                        UserType.m_SystemTypes[UserType.m_nSystemTypes++] = static_cast<char>(c);
                        if( UserType.m_nSystemTypes >= static_cast<int>(UserType.m_SystemTypes.size()) ) return Error = xerr::create_f< state, "Failed to find the termination character '>' for a user type block" >();

                    } while( true );
                    UserType.m_SystemTypes[UserType.m_nSystemTypes]=0;

                    //
                    // Add the User err
                    //
                    AddUserType( UserType );

                    // Read any white space
                    if( std::isspace(c) )
                    {
                        if (Error = m_File.ReadWhiteSpace(c); Error) return Error;
                    }
                        

                } while( c != '>' );

                //
                // Skip spaces
                //
                if ( Error = m_File.ReadWhiteSpace( c ); Error ) 
                    return Error;
            }
        
            //
            // Deal with a record
            //
            if (c == '@')
            {
                if ( Error = m_File.getC(c); Error ) 
                    return Error;
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
                return Error = xerr::create_f< state, "Unable to find the right header symbol '['" >();

            // Skip spaces
            if( Error = m_File.ReadWhiteSpace(c); Error ) 
                return Error;

            int                         NameSize = 0;
            do
            {
                m_Record.m_Name[NameSize++] = c;
            
                if( Error = m_File.getC(c); Error ) 
                    return Error;

            } while( std::isspace( c ) == false && c != ':' && c != ']' );

            // Terminate the string
            m_Record.m_Name[NameSize] = 0;

            // Skip spaces
            if( std::isspace( c ) )
            {
                if ( Error = m_File.ReadWhiteSpace(c); Error ) 
                    return Error;
            }
                
        
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
                    if( Error = m_File.ReadWhiteSpace(c); Error )   
                        return Error;
                
                } while( c == '0' );
            
                //
                // Handle the case of dynamic sizes tables
                //
                if( c == '?' )
                {
                   // TODO: Handle the special reader
                   if( Error = m_File.HandleDynamicTable( m_Record.m_Count ); Error ) 
                    return Error;
                
                    // Read next character
                   if( Error = m_File.getC(c); Error ) 
                    return Error;
                }
                else
                {
                    m_Record.m_Count = 0;
                    while( c >= '0' && c <= '9' )
                    {
                        m_Record.m_Count = m_Record.m_Count * 10 + (c-'0');
                       if( Error = m_File.getC(c); Error ) 
                        return Error;
                    }
                }

                // Skip spaces
                if( std::isspace( c ) )
                {
                    if ( Error = m_File.ReadWhiteSpace(c); Error ) 
                        return Error;
                }
                    
            }

            //
            // Make sure that we are going to conclude the field correctly
            //
            if( c != ']' )
                return Error = xerr::create_f< state, "Fail reading the file. Expecting a '[' but didn't find it." >();
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
