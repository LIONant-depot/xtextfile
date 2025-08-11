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

#include "source/xerr.h"

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
    //
    // Error and such
    //
    enum class state : std::uint8_t
    { OK = 0
    , FAILURE
    , FILE_NOT_FOUND
    , UNEXPECTED_EOF
    , READ_TYPES_DONTMATCH
    , MISMATCH_TYPES
    , FIELD_NOT_FOUND
    , UNEXPECTED_RECORD
    };

    //
    // Tell which type binary or text...
    //
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
            xerr            openForReading      ( const std::wstring_view FilePath, bool isBinary )                         noexcept;
            xerr            openForWriting      ( const std::wstring_view FilePath, bool isBinary )                         noexcept;
            void            close               ( void )                                                                    noexcept;
            xerr            ReadingErrorCheck   ( void )                                                                    noexcept;
            template< typename T >
            xerr            Read                ( T& Buffer, int Size = sizeof(T), int Count = 1 )                          noexcept;
            xerr            getC                ( int& c )                                                                  noexcept;
            xerr            WriteStr            ( std::string_view Buffer )                                                 noexcept;
            xerr            WriteFmtStr         ( const char* pFmt, ... )                                                   noexcept;
            template< typename T >
            xerr            Write               ( T& Buffer, int Size = sizeof(T), int Count = 1 )                          noexcept;
            xerr            WriteChar           ( char C, int Count = 1 )                                                   noexcept;
            xerr            WriteData           ( std::string_view Buffer )                                                 noexcept;
            xerr            ReadWhiteSpace      ( int& c )                                                                  noexcept;
            xerr            HandleDynamicTable  ( int& Count )                                                              noexcept;
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
                        xerr            Open                ( bool isRead, std::wstring_view View, file_type FileType, flags Flags={} ) noexcept;

                        template< std::size_t N, typename... T_ARGS >
        inline          xerr            Field               ( crc32 UserType, const char(&pFieldName)[N], T_ARGS&... Args )    noexcept;

                        xerr            ReadFieldUserType   ( crc32& UserType, const char* pFieldName )                        noexcept;

                        template< std::size_t N, typename... T_ARGS >
        inline          xerr            Field               ( const char(&pFieldName)[N], T_ARGS&... Args )                             noexcept;

        inline          const auto*     getUserType         ( crc32 UserType )                                         const   noexcept { if( auto I = m_UserTypeMap.find(UserType); I == m_UserTypeMap.end() ) return (details::user_types*)nullptr; else return &m_UserTypes[I->second]; }

                        template< std::size_t N, typename TT, typename T >
        inline          xerr            Record              ( const char (&Str)[N]
                                                                , TT&& RecordStar, T&& Callback )                                       noexcept;

                        template< std::size_t N, typename T >
        inline          xerr            Record              ( const char (&Str)[N]
                                                                , T&& Callback )                                                        noexcept;

                        template< std::size_t N >
        inline          xerr            RecordLabel         ( const char(&Str)[N] )                                                     noexcept;

                        xerr            WriteComment        ( const std::string_view Comment )                                  noexcept;


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
                        xerr            openForReading      ( const std::wstring_view FilePath )                                        noexcept;
                        xerr            openForWriting      ( const std::wstring_view FilePath
                                                                , file_type FileType, flags Flags )                                     noexcept;
                        bool            isValidType         ( int Type )                                                        const   noexcept;
                        template< typename T >
                        xerr            Read                ( T& Buffer, int Size = sizeof(T), int Count = 1 )                          noexcept;
                        xerr            ReadRecord          ( void )                                                                    noexcept;
                        xerr            ReadingErrorCheck   ( void )                                                                    noexcept;
                        xerr            ReadWhiteSpace      ( int& c )                                                                  noexcept;
                        xerr            ReadLine            ( void )                                                                    noexcept;
                        xerr            getC                ( int& c )                                                                  noexcept;
                        xerr            ReadColumn          ( crc32 UserType, const char* pFieldName, std::span<details::arglist::types> Args )  noexcept;
                        xerr            ReadFieldUserType   ( const char* pFieldName )                                                  noexcept;

                        template< typename T >
                        xerr            Write               ( T& Buffer, int Size = sizeof(T), int Count = 1 )                          noexcept;
                        xerr            WriteLine           ( void )                                                                    noexcept;
                        xerr            WriteStr            ( std::string_view Buffer )                                                 noexcept;
                        xerr            WriteFmtStr         ( const char* pFmt, ... )                                                   noexcept;
                        xerr            WriteChar           ( char C, int Count = 1 )                                                   noexcept;
                        xerr            WriteColumn         ( crc32 UserType, const char* pFieldName, std::span<details::arglist::types> Args )  noexcept;
                        xerr            WriteUserTypes      ( void )                                                                    noexcept;

                        xerr            HandleDynamicTable  ( int& Count )                                                              noexcept;

                        xerr            WriteRecord         ( const char* pHeaderName, std::size_t Count )                              noexcept;

        inline          bool            ValidateColumnChar  ( int c )                                                           const   noexcept;
                        xerr            BuildTypeInformation( const char* pFieldName )                                                  noexcept;


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

#include "implementation/xtextfile_inline.h"

#endif
