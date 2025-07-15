#ifndef TEXTFILE_H
#define TEXTFILE_H
#pragma once

#include <vector>
#include <array>
#include <unordered_map>
#include <string>
#include <assert.h>
#include <span>
#include <locale>
#include <codecvt>
#include <variant>

namespace xtextfile
{
    struct crc32
    {
        static constexpr std::array<std::uint32_t, 256> crc32_table_v = []() consteval noexcept
        {
            std::array<std::uint32_t, 256> table{};
            for (std::uint32_t i = 0; i < 256; ++i) 
            {
                std::uint32_t crc = i;
                for (int j = 0; j < 8; ++j) 
                {
                    crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320U : (crc >> 1);
                }
                table[i] = crc;
            }
            return table;
        }();

        constexpr static crc32 computeFromString( const char* str, std::uint32_t crc = 0xFFFFFFFFU ) noexcept
        {
            while(*str) 
            {
                crc = (crc >> 8) ^ crc32_table_v[(crc ^ static_cast<unsigned char>(*str)) & 0xFF];
                ++str;
            }

            return { crc ^ 0xFFFFFFFFU };
        }

        constexpr bool operator == (const crc32& other) const noexcept
        {
            return m_Value == other.m_Value;
        }

        std::uint32_t m_Value;
    };

    namespace details
    {
        namespace arglist
        {
            //------------------------------------------------------------------------------
            using types = std::variant
            < bool
            , float, double
            , std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t
            , std::int8_t,  std::int16_t,  std::int32_t,  std::int64_t
            , bool*
            , std::uint8_t*, std::uint16_t*, std::uint32_t*, std::uint64_t*
            , std::int8_t*,  std::int16_t*,  std::int32_t*,  std::int64_t*
            , float*, double*
            , void*
            , const char*
            , std::string*
            , std::wstring*
            >;

            //------------------------------------------------------------------------------

            template< typename... T_ARGS >
            struct out
            {
                std::array< types, sizeof...(T_ARGS) > m_Params;
                constexpr out(T_ARGS... Args) : m_Params{ Args... } {}
                operator std::span<types>() noexcept { return m_Params; }
            };
        }
    }
}

namespace std
{
    template <>
    struct hash<xtextfile::crc32>
    {
        inline std::size_t operator()(const xtextfile::crc32& c) const noexcept
        {
            return std::hash<std::uint32_t>{}(c.m_Value);
        }
    };
}


//-----------------------------------------------------------------------------------------------------
// Please remember that text file are lossy files due to the floats don't survive text-to-binary conversion
//
//     Type    Description
//     ------  ----------------------------------------------------------------------------------------
//      f      32 bit float
//      F      64 bit double
//      d      32 bit integer
//      g      32 bit unsigned integer
//      D      64 bit integer
//      G      64 bit unsigned integer
//      c       8 bit integer
//      h       8 bit unsigned integer
//      C      16 bit integer
//      H      16 bit unsigned integer
//      s      this is a xcore::string
//-----------------------------------------------------------------------------------------------------
namespace xtextfile
{
    //-----------------------------------------------------------------------------------------------------
    // Error and such
    //-----------------------------------------------------------------------------------------------------
    struct err
    {
        enum class state : std::uint8_t
        { OK                        = 0
        , FAILURE
        , FILE_NOT_FOUND
        , UNEXPECTED_EOF
        , READ_TYPES_DONTMATCH
        , MISMATCH_TYPES
        , FIELD_NOT_FOUND
        , UNEXPECTED_RECORD
        };

        constexpr operator bool () const noexcept
        {
            return !!m_pMessage;
        }

        constexpr state getState() const noexcept
        {
            return m_pMessage ? static_cast<state>(m_pMessage[-1]) : state::OK;
        }

        inline void clear() noexcept
        {
            m_pMessage =  nullptr;
        }

        constexpr bool isError( err& Error ) const noexcept
        {
            const bool is = !!m_pMessage;
            if (is) Error = *this;
            return is;
        }

        template<std::size_t N>
        struct string_literal
        {
            std::array<char, N> m_Value;
            consteval string_literal(const char(&str)[N]) noexcept
            { 
                for (std::size_t i = 0; i < N; ++i)
                    m_Value[i] = str[i];
            }
        };

        template <string_literal T_STR_V, state T_STATE_V>
        inline constexpr static auto data_v = []() constexpr noexcept
        {
            std::array< char, T_STR_V.m_Value.size() + 1> temp = {};
            temp[0] = static_cast<char>(T_STATE_V);
            for (std::size_t i = 1; i < T_STR_V.m_Value.size(); ++i)
                temp[i] = T_STR_V.m_Value[i-1];
            return temp;
        }();

        constexpr err() = default;
        constexpr err( const char* p ) noexcept : m_pMessage{p} {}

        template <state T_STATE_V, string_literal T_STR_V>
        consteval static err create() noexcept
        {
            return err::data_v<T_STR_V, T_STATE_V>.data() + 1;
        }

        template <string_literal T_STR_V>
        consteval static err create_f() noexcept
        {
            return err::data_v<T_STR_V, err::state::FAILURE>.data() + 1;
        }

        const char*       m_pMessage = nullptr;
    };

    struct err2 : err
    {
        err2() = default;
        constexpr err2(err&& E) : err{ E } {}
        err2(state State, std::string&& Message ) noexcept
            : m_MessageFull{std::move(Message)}
        {
            assert(Message[0] == '#');
            m_MessageFull[0] = static_cast<char>(State);
            m_pMessage       = m_MessageFull.c_str();
        }

        err2(state State, std::wstring&& Message) noexcept
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

            m_MessageFull = std::string{ buffer.data(), ret};
            m_MessageFull[0] = static_cast<char>(State);
            m_pMessage = m_MessageFull.c_str() + 1;
        }

        std::string m_MessageFull;
    };

    enum class file_type
    { TEXT
    , BINARY
    };

    union flags
    {
        std::uint8_t    m_Value = 0;
        struct
        {
            bool        m_isWriteFloats:1               // Writes floating point numbers as floating point rather than hex
            ,           m_isWriteEndianSwap:1;          // Swaps endian before writing (Only useful when writing binary)
        };
    };

    struct user_defined_types
    {
        template< auto N1, auto N2 >
        constexpr user_defined_types    ( const char(&Name)[N1], const char(&Types)[N2] ) noexcept;
        constexpr user_defined_types    ( const char* pName, const char* pTypes ) noexcept;
        constexpr user_defined_types    ( void ) = default;

        std::array<char,32>     m_Name          {};
        std::array<char,32>     m_SystemTypes   {};
        crc32                   m_CRC           {};
        int                     m_NameLength    {};
        int                     m_nSystemTypes  {};     // How many system types we are using, this is basically the length of the m_SystemType string
    };

    //-----------------------------------------------------------------------------------------------------
    // private interface
    //-----------------------------------------------------------------------------------------------------
    namespace details
    {
        //-----------------------------------------------------------------------------------------------------
        union states
        {
            std::uint32_t       m_Value{ 0 };
            struct
            {
                  bool            m_isView        : 1       // means we don't own the pointer
                                , m_isEOF         : 1       // We have reach end of file so no io operations make sense after this
                                , m_isBinary      : 1       // Tells if we are dealing with a binary file or text file
                                , m_isEndianSwap  : 1       // Tells if when reading we should swap endians
                                , m_isReading     : 1       // Tells the system whether we are reading or writing
                                , m_isSaveFloats  : 1;      // Save floats as hex
            };
        };

        //-----------------------------------------------------------------------------------------------------
        struct file
        {
            std::FILE*      m_pFP       = { nullptr };
            states          m_States    = {};

                            file                ( void )                                                                    noexcept = default;
                           ~file                ( void )                                                                    noexcept;

            file&           setup               ( std::FILE& File, states States )                                          noexcept;
            err2            openForReading      ( const std::wstring_view FilePath, bool isBinary )                         noexcept;
            err2            openForWriting      ( const std::wstring_view FilePath, bool isBinary )                         noexcept;
            void            close               ( void )                                                                    noexcept;
            err             ReadingErrorCheck   ( void )                                                                    noexcept;
            template< typename T >
            err             Read                ( T& Buffer, int Size = sizeof(T), int Count = 1 )                          noexcept;
            err             getC                ( int& c )                                                                  noexcept;
            err             WriteStr            ( std::string_view Buffer )                                                 noexcept;
            err             WriteFmtStr         ( const char* pFmt, ... )                                                   noexcept;
            template< typename T >
            err             Write               ( T& Buffer, int Size = sizeof(T), int Count = 1 )                          noexcept;
            err             WriteChar           ( char C, int Count = 1 )                                                   noexcept;
            err             WriteData           ( std::string_view Buffer )                                                 noexcept;
            err             ReadWhiteSpace      ( int& c )                                                                  noexcept;
            err             HandleDynamicTable  ( int& Count )                                                              noexcept;
            int             Tell                ()                                                                          noexcept;
        };

        //-----------------------------------------------------------------------------------------------------
        struct field_info
        {
            int                                 m_IntWidth;             // Integer part 
            int                                 m_Width;                // Width of this field
            int                                 m_iData;                // Index to the data
        };

        //-----------------------------------------------------------------------------------------------------
        struct field_type
        {
            int                                 m_nTypes;               // How many types does this dynamic field has
            crc32                               m_UserType;             // if 0 then is not valid
            std::array<char,16>                 m_SystemTypes;          // System types
            int                                 m_iField;               // Index to the m_FieldInfo from the column 

            int                                 m_FormatWidth;          // Width of the column
        };

        //-----------------------------------------------------------------------------------------------------
        struct sub_column
        {
            int                                 m_FormatWidth      {0};
            int                                 m_FormatIntWidth   {0};
        };

        //-----------------------------------------------------------------------------------------------------
        struct column : field_type
        {
            std::array<char,128>                m_Name;                 // Type name
            int                                 m_NameLength;           // string Length for Name
            std::vector<field_type>             m_DynamicFields;        // if this column has dynamic fields here is where the info is
            std::vector<field_info>             m_FieldInfo;            // All fields for this column
            std::vector<sub_column>             m_SubColumn;            // Each of the types inside of a column is a sub_column.

            int                                 m_FormatNameWidth;      // Text Formatting name width 
            int                                 m_FormatTotalSubColumns;// Total width taken by the subcolumns

            void clear ( void ) noexcept { m_DynamicFields.clear(); m_FieldInfo.clear(); m_Name[0]=0; }
        };

        //-----------------------------------------------------------------------------------------------------
        struct user_types : user_defined_types
        {
            bool                                m_bAlreadySaved     {false};    
        };

        //-----------------------------------------------------------------------------------------------------
        struct record
        {
            std::array<char,256>                    m_Name              {};     // Name of the record
            int                                     m_Count             {};     // How many entries in this record
            bool                                    m_bWriteCount       {};     // If we need to write out the count
            bool                                    m_bLabel            {};     // Tells if the recrod is a label or not
        };
    }

    //-----------------------------------------------------------------------------------------------------
    // public interface
    //-----------------------------------------------------------------------------------------------------
    class stream
    {
    public:

        constexpr                       stream              ( void )                                                                    noexcept = default;
        void                            close               ( void )                                                                    noexcept;
                        err2            Open                ( bool isRead, std::wstring_view View, file_type FileType, flags Flags={} ) noexcept;

                        template< std::size_t N, typename... T_ARGS >
        inline          err             Field               ( crc32 UserType, const char(&pFieldName)[N], T_ARGS&... Args )    noexcept;

                        err             ReadFieldUserType   ( crc32& UserType, const char* pFieldName )                        noexcept;

                        template< std::size_t N, typename... T_ARGS >
        inline          err             Field               ( const char(&pFieldName)[N], T_ARGS&... Args )                             noexcept;

        inline          const auto*     getUserType         ( crc32 UserType )                                         const   noexcept { if( auto I = m_UserTypeMap.find(UserType); I == m_UserTypeMap.end() ) return (details::user_types*)nullptr; else return &m_UserTypes[I->second]; }

                        template< std::size_t N, typename TT, typename T >
        inline          bool            Record              ( err& Error, const char (&Str)[N]
                                                                , TT&& RecordStar, T&& Callback )                                       noexcept;

                        template< std::size_t N, typename TT, typename T >
        inline          err             Record              ( const char (&Str)[N]
                                                                , TT&& RecordStar, T&& Callback )                                       noexcept;

                        template< std::size_t N, typename T >
        inline          bool            Record              ( err& Error, const char (&Str)[N]
                                                                , T&& Callback )                                                        noexcept;

                        template< std::size_t N >
        inline          err             RecordLabel         ( const char(&Str)[N] )                                                     noexcept;

                        err             WriteComment        ( const std::string_view Comment )                                  noexcept;


        constexpr       bool            isReading           ( void )                                                            const   noexcept { return m_File.m_States.m_isReading; }
        constexpr       bool            isEOF               ( void )                                                            const   noexcept { return m_File.m_States.m_isEOF; }
        constexpr       bool            isWriteFloats       ( void )                                                            const   noexcept { return m_File.m_States.m_isSaveFloats; }
        inline          auto&           getRecordName       ( void )                                                            const   noexcept { return m_Record.m_Name;  }
        inline          int             getRecordCount      ( void )                                                            const   noexcept { return m_Record.m_Count; }
        inline          int             getUserTypeCount    ( void )                                                            const   noexcept { return static_cast<int>(m_UserTypes.size()); }
                        std::uint32_t   AddUserType         ( const user_defined_types& UserType )                                      noexcept;
                        void            AddUserTypes        ( std::span<user_defined_types> UserTypes )                                 noexcept;
                        void            AddUserTypes        ( std::span<const user_defined_types> UserTypes )                           noexcept;

    protected:

                        stream&         setup               ( std::FILE& File, details::states States )                                 noexcept;
                        err2            openForReading      ( const std::wstring_view FilePath )                                        noexcept;
                        err2            openForWriting      ( const std::wstring_view FilePath
                                                                , file_type FileType, flags Flags )                                     noexcept;
                        bool            isValidType         ( int Type )                                                        const   noexcept;
                        template< typename T >
                        err             Read                ( T& Buffer, int Size = sizeof(T), int Count = 1 )                          noexcept;
                        err             ReadRecord          ( void )                                                                    noexcept;
                        err             ReadingErrorCheck   ( void )                                                                    noexcept;
                        err             ReadWhiteSpace      ( int& c )                                                                  noexcept;
                        err             ReadLine            ( void )                                                                    noexcept;
                        err             getC                ( int& c )                                                                  noexcept;
                        err             ReadColumn          ( crc32 UserType, const char* pFieldName, std::span<details::arglist::types> Args )  noexcept;
                        err             ReadFieldUserType   ( const char* pFieldName )                                                  noexcept;

                        template< typename T >
                        err             Write               ( T& Buffer, int Size = sizeof(T), int Count = 1 )                          noexcept;
                        err             WriteLine           ( void )                                                                    noexcept;
                        err             WriteStr            ( std::string_view Buffer )                                                 noexcept;
                        err             WriteFmtStr         ( const char* pFmt, ... )                                                   noexcept;
                        err             WriteChar           ( char C, int Count = 1 )                                                   noexcept;
                        err             WriteColumn         ( crc32 UserType, const char* pFieldName, std::span<details::arglist::types> Args )  noexcept;
                        err             WriteUserTypes      ( void )                                                                    noexcept;

                        err             HandleDynamicTable  ( int& Count )                                                              noexcept;

                        err             WriteRecord         ( const char* pHeaderName, std::size_t Count )                              noexcept;

        inline          bool            ValidateColumnChar  ( int c )                                                           const   noexcept;
                        err             BuildTypeInformation( const char* pFieldName )                                                  noexcept;


    protected:

        details::file                                       m_File                  {};     // File pointer
        details::record                                     m_Record                {};     // This contains information about the current record
        std::vector<details::column>                        m_Columns               {};
        std::vector<char>                                   m_Memory                {};
        std::vector<details::user_types>                    m_UserTypes             {};
        std::vector<int>                                    m_DataMapping           {};
        std::unordered_map<crc32, std::uint32_t>            m_UserTypeMap           {};     // First uint32 is the CRC32 of the err name
                                                                                            // Second uint32 is the index in the UserTypes vector which contains the actual data
        int                                                 m_nColumns              {};
        int                                                 m_iLine                 {};     // Which line we are in the current record
        int                                                 m_iMemOffet             {};
        int                                                 m_iColumn               {};

        constexpr static int                                m_nSpacesBetweenFields  { 1 };
        constexpr static int                                m_nSpacesBetweenColumns { 2 };
        constexpr static int                                m_nLinesBeforeFileWrite { 64 };
    };
}

#include "xtextfile_inline.h"

#endif
