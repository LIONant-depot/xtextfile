#include <format>
#include <iostream>

namespace xtextfile::unit_test
{
    constexpr static std::array StringBack      = { "String Test",              "Another test",         "Yet another"           };
    constexpr static std::array WStringBack     = { L"\u4f60\u597d\uff0c(Better)\u4e16\u754c\uff01",    L"\u5b66\u4e60\u5feb\u4e50",  L"\u548c\u5e73\u4e07\u5c81" };

    constexpr static std::array Floats32Back    = { -50.0f,                     2.1233f,                1.312323123f            };
    constexpr static std::array Floats64Back    = { -32.323212312312323,        2.2312323233,           1.312323123             };

    constexpr static std::array Int64Back       = { std::int64_t{13452},        std::int64_t{-321},     std::int64_t{1434}      };
    constexpr static std::array Int32Back       = { std::int32_t{0},            std::int32_t{21231},    std::int32_t{4344}      };
    constexpr static std::array Int16Back       = { std::int16_t{-2312},        std::int16_t{211},      std::int16_t{344}       };
    constexpr static std::array Int8Back        = { std::int8_t {16},           std::int8_t {21},       std::int8_t {-44}       };

    constexpr static std::array UInt64Back      = { std::uint64_t{13452323212}, std::uint64_t{321},     std::uint64_t{1434}     };
    constexpr static std::array UInt32Back      = { std::uint32_t{33313452},    std::uint32_t{222321},  std::uint32_t{111434}   };
    constexpr static std::array UInt16Back      = { std::uint16_t{31352},       std::uint16_t{2221},    std::uint16_t{11434}    };
    constexpr static std::array UInt8Back       = { std::uint8_t {33},          std::uint8_t {22},      std::uint8_t {44}       };

    //------------------------------------------------------------------------------
    // Comparing floating points numbers
    //------------------------------------------------------------------------------
    // https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/ 
    // https://www.floating-point-gui.de/errors/comparison/
    template< typename T >
    bool AlmostEqualRelative(T A, T B, T maxRelDiff = sizeof(T) == 4 ? FLT_EPSILON : (DBL_EPSILON * 1000000))
    {
        // Calculate the difference.
        T diff = std::abs(A - B);
        A = std::abs(A);
        B = std::abs(B);

        // Find the largest
        T largest = (B > A) ? B : A;

        if (diff <= largest * maxRelDiff)
            return true;
        return false;
    }

    //------------------------------------------------------------------------------
    // Test different types
    //------------------------------------------------------------------------------
    inline
    xerr AllTypes(xtextfile::stream& TextFile, const bool isRead, const xtextfile::flags Flags) noexcept
    {
        std::array<std::string, 3>  String      = {  StringBack[0],  StringBack[1],  StringBack[2] };
        std::array<std::wstring, 3> WString     = { WStringBack[0], WStringBack[1], WStringBack[2] };

        auto                        Floats32    = Floats32Back;
        auto                        Floats64    = Floats64Back;

        auto                        Int64       = Int64Back;
        auto                        Int32       = Int32Back;
        auto                        Int16       = Int16Back;
        auto                        Int8        = Int8Back;

        auto                        UInt64      = UInt64Back;
        auto                        UInt32      = UInt32Back;
        auto                        UInt16      = UInt16Back;
        auto                        UInt8       = UInt8Back;

        if (auto Err = TextFile.WriteComment(
            "----------------------------------------------------\n"
            " Example that shows how to use all system types\n"
            "----------------------------------------------------\n"
        ); Err) return Err;

        //
        // Save/Load a record
        //
        int     Times = 128;

        if (xerr Err = TextFile.Record( "TestTypes"
            , [&](std::size_t& C, xerr&)
            {
                if (isRead)    assert(C == StringBack.size() * Times);
                else            C = StringBack.size() * Times;
            }
            , [&](std::size_t c, xerr& Error)
            {
                const auto i = c % StringBack.size();
                    0
                    || (Error = TextFile.Field("String" , String[i]))
                    || (Error = TextFile.Field("WString", WString[i]))
                    || (Error = TextFile.Field("Floats" , Floats64[i]
                                                        , Floats32[i]))
                    || (Error = TextFile.Field("Ints"   , Int64[i]
                                                        , Int32[i]
                                                        , Int16[i]
                                                        , Int8[i]))
                    || (Error = TextFile.Field("UInts"  , UInt64[i]
                                                        , UInt32[i]
                                                        , UInt16[i]
                                                        , UInt8[i]))
                    ;


                //
                // Sanity check
                //
                if (isRead)
                {
                    assert(StringBack[i]  == String[i]);
                    assert(WStringBack[i] == WString[i]);

                    //
                    // If we tell the file system to write floats as floats then we will leak precision
                    // so we need to take that into account when checking.
                    //
                    if (Flags.m_isWriteFloats)
                    {
                        assert(AlmostEqualRelative(Floats64Back[i], Floats64[i]));
                        assert(AlmostEqualRelative(Floats32Back[i], Floats32[i]));
                    }
                    else
                    {
                        assert(Floats64Back[i] == Floats64[i]);
                        assert(Floats32Back[i] == Floats32[i]);
                    }

                    assert(Int64Back[i] == Int64[i]);
                    assert(Int32Back[i] == Int32[i]);
                    assert(Int16Back[i] == Int16[i]);
                    assert(Int8Back[i]  == Int8[i]);

                    assert(UInt64Back[i] == UInt64[i]);
                    assert(UInt32Back[i] == UInt32[i]);
                    assert(UInt16Back[i] == UInt16[i]);
                    assert(UInt8Back[i]  == UInt8[i]);
                }
            }
        ); Err ) return Err;

        return {};
    }

    //------------------------------------------------------------------------------
    // Test different types
    //------------------------------------------------------------------------------
    inline
    xerr SimpleVariableTypes(xtextfile::stream& TextFile, const bool isRead, const xtextfile::flags Flags) noexcept
    {
        std::array<std::string, 3>      String      = {  StringBack[0],  StringBack[1],  StringBack[2] };
        std::array<std::wstring, 3>     WString     = { WStringBack[0], WStringBack[1], WStringBack[2] };

        auto                            Floats32    = Floats32Back;
        auto                            Floats64    = Floats64Back;

        auto                            Int64       = Int64Back;
        auto                            Int32       = Int32Back;
        auto                            Int16       = Int16Back;
        auto                            Int8        = Int8Back;

        auto                            UInt64      = UInt64Back;
        auto                            UInt32      = UInt32Back;
        auto                            UInt16      = UInt16Back;
        auto                            UInt8       = UInt8Back;

        if ( auto Err = TextFile.WriteComment(
            "----------------------------------------------------\n"
            " Example that shows how to use per line (rather than per column) types\n"
            "----------------------------------------------------\n"
        ); Err ) return Err;

        //
        // Save/Load record
        //
        int Times = 128;
        if (auto Err = TextFile.Record( "VariableTypes"
            , [&](std::size_t& C, xerr&)
            {
                if (isRead) assert(C == StringBack.size() * Times);
                else        C = StringBack.size() * Times;
            }
            , [&](std::size_t i, xerr& Error)
            {
                i %= StringBack.size();

                // Just save some integers to show that we can still safe normal fields at any point
                if (Error = TextFile.Field("Ints", Int32[i]); Error ) return;

                // Here we are going to save different types and give meaning to 'type' from above
                // so in a way here we are saving two columns first the "type:<?>" then the "value<type>"
                // the (i&1) shows that we can choose whatever types we want
                switch (i)
                {
                case 0: if (Error = TextFile.Field("value:?", String[0]); Error) return; break;
                case 1: if (Error = TextFile.Field("value:?", WString[0]); Error) return; break;
                case 2: if (Error = TextFile.Field("value:?", Floats64[0],  Floats32[0]); Error) return; break;
                case 3: if (Error = TextFile.Field("value:?", Int64[0],     Int32[0],   Int16[0],   Int8[0]); Error ) return; break;
                case 4: if (Error = TextFile.Field("value:?", UInt64[0],    UInt32[0],  UInt16[0],  UInt8[0]); Error ) return; break;
                }
            }
        ); Err ) return Err;

        //
        // Sanity check
        //
        if (isRead)
        {
            //
            // If we tell the file system to write floats as floats then we will leak precision
            // so we need to take that into account when checking.
            //
            if (Flags.m_isWriteFloats)
            {
                assert(AlmostEqualRelative(Floats64Back[0], Floats64[0]));
                assert(AlmostEqualRelative(Floats32Back[0], Floats32[0]));
            }
            else
            {
                assert(Floats64Back[0] == Floats64[0]);
                assert(Floats32Back[0] == Floats32[0]);
            }

            assert(StringBack[0] == String[0]);
            assert(Int64Back[0] == Int64[0]);
            assert(Int32Back[0] == Int32[0]);
            assert(Int16Back[0] == Int16[0]);
            assert(Int8Back[0] == Int8[0]);
            assert(UInt64Back[0] == UInt64[0]);
            assert(UInt32Back[0] == UInt32[0]);
            assert(UInt16Back[0] == UInt16[0]);
            assert(UInt8Back[0] == UInt8[0]);
        }

        return {};
    }

    //------------------------------------------------------------------------------
    // Property examples
    //------------------------------------------------------------------------------
    inline
    xerr Properties(xtextfile::stream& TextFile, const bool isRead, const xtextfile::flags Flags) noexcept
    {
        std::array<std::string, 3>              String{ StringBack[0], StringBack[1], StringBack[2] };
        std::array<std::wstring, 3>             WString{ WStringBack[0], WStringBack[1], WStringBack[2] };
        std::string                             Name{ "Hello" };
        std::wstring                            WName{ L"Wide Hello" };
        bool                                    isValid{ true };
        std::array<float, 3>                    Position{ 0.1f, 0.5f, 0.6f };
        constexpr static std::array             Types
        { xtextfile::user_defined_types{     "V3", "fff"  }
        , xtextfile::user_defined_types{   "BOOL",   "c"  }
        , xtextfile::user_defined_types{ "STRING",   "s"  }
        , xtextfile::user_defined_types{ "WSTRING",  "S"  }
        };

        //
        // Save/Load a record
        //
        if (auto Err = TextFile.Record( "Properties"
            , [&](std::size_t& C, xerr&)
            {
                if (isRead) assert(C == 3);
                else        C = 3;
            }
            , [&](std::size_t i, xerr& Error)
            {
                // Just save some integers to show that we can still safe normal fields at any point
                if (Error = TextFile.Field("Name", String[i]); Error ) return;

                // Handle the data
                xtextfile::crc32 Type;
                if (isRead)
                {
                    if (Error = TextFile.ReadFieldUserType(Type, "Value:?"); Error ) return;
                }
                else
                {
                    // Property_to_type
                    Type = Types[i].m_CRC;
                }

                switch (Type.m_Value)
                {
                case xtextfile::crc32::computeFromString("V3").m_Value:      if (Error = TextFile.Field(Type, "Value:?", Position[0], Position[1], Position[2]); Error ) return; break;
                case xtextfile::crc32::computeFromString("BOOL").m_Value:    if (Error = TextFile.Field(Type, "Value:?", isValid); Error) return; break;
                case xtextfile::crc32::computeFromString("STRING").m_Value:  if (Error = TextFile.Field(Type, "Value:?", Name); Error) return; break;
                case xtextfile::crc32::computeFromString("WSTRING").m_Value: if (Error = TextFile.Field(Type, "Value:?", WName); Error) return; break;
                }                                                               
            }
        ); Err ) return Err;

        return {};
    }

    //------------------------------------------------------------------------------
    // Test different types
    //------------------------------------------------------------------------------
    inline
    xerr UserTypes(xtextfile::stream& TextFile, const bool isRead, const xtextfile::flags Flags) noexcept
    {
        constexpr xtextfile::user_defined_types v3  { "V3",     "fff" };
        constexpr xtextfile::user_defined_types bo  { "BOOL",   "c" };
        constexpr xtextfile::user_defined_types str { "STRING", "s" };

        constexpr static std::array Types{ v3, bo, str };

        std::string     Name        = { "Hello" };
        bool            isValid     = true;
        std::array      Position    = { 0.1f, 0.5f, 0.6f };

        if (auto Err = TextFile.WriteComment(
            "----------------------------------------------------\n"
            " Example that shows how to use user types\n"
            "----------------------------------------------------\n"
        ); Err ) return Err;

        //
        // Tell the file system about the user types
        //
        TextFile.AddUserTypes(Types);

        //
        // Save/Load a record
        //
        if ( auto Err = TextFile.Record( "TestUserTypes"
            , [&](std::size_t, xerr& Error)
            {
                0
                || (Error = TextFile.Field(v3.m_CRC, "Position", Position[0], Position[1], Position[2]))
                || (Error = TextFile.Field(bo.m_CRC, "IsValid", isValid))
                || (Error = TextFile.Field(str.m_CRC, "Name", Name ))
                ;
            }
        ); Err ) return Err;

        //
        // Save/Load a record
        //
        if (auto Err = TextFile.Record( "VariableUserTypes"
            , [&](std::size_t& C, xerr&)
            {
                if (isRead) assert(C == 3);
                else        C = 3;
            }
            , [&](std::size_t i, xerr& Error)
            {
                switch (i)
                {
                case 0: if (Error = TextFile.Field(v3.m_CRC, "Value:?", Position[0], Position[1], Position[2]); Error ) return; break;
                case 1: if (Error = TextFile.Field(bo.m_CRC, "Value:?", isValid); Error) return; break;
                case 2: if (Error = TextFile.Field(str.m_CRC, "Value:?", Name ); Error) return; break;
                }

                Error = TextFile.Field(bo.m_CRC, "IsValid", isValid);
            }
        ); Err) return Err;

        return {};
    }

    //------------------------------------------------------------------------------
    // Test different types
    //------------------------------------------------------------------------------
    inline
    xerr Test01(std::wstring_view FileName, bool isRead, xtextfile::file_type FileType, xtextfile::flags Flags) noexcept
    {
        xtextfile::stream   TextFile;

        //
        // Open File
        //
        if (auto Err = TextFile.Open(isRead, FileName, FileType, Flags); Err)
        {
            std::cout << "Failed to open " << Err.getMessage() << "\n";
            return Err;
        }

        //
        // Run tests
        // 
        if (auto Err = AllTypes(TextFile, isRead, Flags); Err) 
            return Err;

        if (auto Err = SimpleVariableTypes(TextFile, isRead, Flags); Err) 
            return Err;

        if (auto Err = UserTypes(TextFile, isRead, Flags); Err ) 
            return Err;
      
        if (auto Err = Properties(TextFile, isRead, Flags); Err ) 
            return Err;

        //
        // When we are reading and we are done dealing with records we can check if we are done reading a file like this
        //
        if (isRead && TextFile.isEOF())
            return {};

        return {};
    }

    //-----------------------------------------------------------------------------------------

    void Test(void)
    {
        constexpr static auto FileName = L"./x64/TextFileTest.la";
        xerr Error;
        //
        // Test write and read (Text Style)
        //
        if (true) if (0
            || (Error = Test01(std::format(L"{}{}.txt", FileName, 1).c_str(), false, xtextfile::file_type::TEXT, {xtextfile::flags{}}))
            || (Error = Test01(std::format(L"{}{}.txt", FileName, 1).c_str(), true,  xtextfile::file_type::TEXT, { xtextfile::flags{} }))
            || (Error = Test01(std::format(L"{}{}.txt", FileName, 2).c_str(), false, xtextfile::file_type::TEXT, { .m_isWriteFloats = true }))
            || (Error = Test01(std::format(L"{}{}.txt", FileName, 2).c_str(), true,  xtextfile::file_type::TEXT, { .m_isWriteFloats = true }))
            )
        {
            assert(false);
        }

        //
        // Test write and read (Binary Style)
        //
        if (true) if (0
            || (Error = Test01(std::format(L"{}{}.bin", FileName, 1).c_str(), false, xtextfile::file_type::BINARY, { xtextfile::flags{} }))
            || (Error = Test01(std::format(L"{}{}.bin", FileName, 1).c_str(), true, xtextfile::file_type::BINARY, { xtextfile::flags{} }))
            || (Error = Test01(std::format(L"{}{}.bin", FileName, 2).c_str(), false, xtextfile::file_type::BINARY, { .m_isWriteFloats = true }))
            || (Error = Test01(std::format(L"{}{}.bin", FileName, 2).c_str(), true,  xtextfile::file_type::BINARY, { .m_isWriteFloats = true }))
            )
        {
            assert(false);
        }
    }
}

