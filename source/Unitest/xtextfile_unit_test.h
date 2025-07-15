#include <format>

namespace xtextfile::unit_test
{
    constexpr static std::array StringBack      = { "String Test",              "Another test",         "Yet another"           };

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
    xtextfile::err AllTypes(xtextfile::stream& TextFile, const bool isRead, const xtextfile::flags Flags) noexcept
    {
        std::array<std::string, 3>  String      = { StringBack[0], StringBack[1], StringBack[2] };

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

        xtextfile::err              Error;

        if (TextFile.WriteComment(
            "----------------------------------------------------\n"
            " Example that shows how to use all system types\n"
            "----------------------------------------------------\n"
        ).isError(Error)) return Error;

        //
        // Save/Load a record
        //
        int Times = 128;
        if (TextFile.Record(Error, "TestTypes"
            , [&](std::size_t& C, xtextfile::err&)
            {
                if (isRead)    assert(C == StringBack.size() * Times);
                else            C = StringBack.size() * Times;
            }
            , [&](std::size_t c, xtextfile::err& Error)
            {
                const auto i = c % StringBack.size();
                0
                    || TextFile.Field("String", String[i]).isError(Error)
                    || TextFile.Field("Floats", Floats64[i]
                                              , Floats32[i]).isError(Error)
                    || TextFile.Field("Ints", Int64[i]
                                            , Int32[i]
                                            , Int16[i]
                                            , Int8[i]).isError(Error)
                    || TextFile.Field("UInts", UInt64[i]
                                             , UInt32[i]
                                             , UInt16[i]
                                             , UInt8[i]).isError(Error)
                    ;


                //
                // Sanity check
                //
                if (isRead)
                {
                    assert(StringBack[i] == String[i]);

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
        )) return Error;

        return Error;
    }

    //------------------------------------------------------------------------------
    // Test different types
    //------------------------------------------------------------------------------
    inline
        xtextfile::err SimpleVariableTypes(xtextfile::stream& TextFile, const bool isRead, const xtextfile::flags Flags) noexcept
    {
        std::array<std::string, 3>      String      = { StringBack[0], StringBack[1], StringBack[2] };

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

        xtextfile::err                  Error;

        if (TextFile.WriteComment(
            "----------------------------------------------------\n"
            " Example that shows how to use per line (rather than per column) types\n"
            "----------------------------------------------------\n"
        ).isError(Error)) return Error;

        //
        // Save/Load record
        //
        int Times = 128;
        if (TextFile.Record(Error, "VariableTypes"
            , [&](std::size_t& C, xtextfile::err&)
            {
                if (isRead) assert(C == StringBack.size() * Times);
                else        C = StringBack.size() * Times;
            }
            , [&](std::size_t i, xtextfile::err& Error)
            {
                i %= StringBack.size();

                // Just save some integers to show that we can still safe normal fields at any point
                if (TextFile.Field("Ints", Int32[i]).isError(Error)) return;

                // Here we are going to save different types and give meaning to 'type' from above
                // so in a way here we are saving two columns first the "type:<?>" then the "value<type>"
                // the (i&1) shows that we can choose whatever types we want
                switch (i)
                {
                case 0: if (TextFile.Field("value:?", String[0]).isError(Error)) return; break;
                case 1: if (TextFile.Field("value:?", Floats64[0],  Floats32[0]).isError(Error)) return; break;
                case 2: if (TextFile.Field("value:?", Int64[0],     Int32[0],   Int16[0],   Int8[0]).isError(Error)) return; break;
                case 3: if (TextFile.Field("value:?", UInt64[0],    UInt32[0],  UInt16[0],  UInt8[0]).isError(Error)) return; break;
                }
            }
        )) return Error;

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

        return Error;
    }

    //------------------------------------------------------------------------------
    // Property examples
    //------------------------------------------------------------------------------
    inline
    xtextfile::err Properties(xtextfile::stream& TextFile, const bool isRead, const xtextfile::flags Flags) noexcept
    {
        xtextfile::err                          Error;
        std::array<std::string, 3>              String{ StringBack[0], StringBack[1], StringBack[2] };
        std::string                             Name{ "Hello" };
        bool                                    isValid{ true };
        std::array<float, 3>                    Position{ 0.1f, 0.5f, 0.6f };
        constexpr static std::array             Types
        { xtextfile::user_defined_types{     "V3", "fff"  }
        , xtextfile::user_defined_types{   "BOOL",   "c"  }
        , xtextfile::user_defined_types{ "STRING",   "s"  }
        };

        //
        // Save/Load a record
        //
        if (TextFile.Record(Error, "Properties"
            , [&](std::size_t& C, xtextfile::err&)
            {
                if (isRead) assert(C == 3);
                else        C = 3;
            }
            , [&](std::size_t i, xtextfile::err& Error)
            {
                // Just save some integers to show that we can still safe normal fields at any point
                if (TextFile.Field("Name", String[i]).isError(Error)) return;

                // Handle the data
                xtextfile::crc32 Type;
                if (isRead)
                {
                    if (TextFile.ReadFieldUserType(Type, "Value:?").isError(Error)) return;
                }
                else
                {
                    // Property_to_type
                    Type = Types[i].m_CRC;
                }

                switch (Type.m_Value)
                {
                case xtextfile::crc32::computeFromString("V3").m_Value:     if (TextFile.Field(Type, "Value:?", Position[0], Position[1], Position[2]).isError(Error)) return; break;
                case xtextfile::crc32::computeFromString("BOOL").m_Value:   if (TextFile.Field(Type, "Value:?", isValid).isError(Error)) return; break;
                case xtextfile::crc32::computeFromString("STRING").m_Value: if (TextFile.Field(Type, "Value:?", Name).isError(Error)) return; break;
                }
            }
        )) return Error;

        return Error;
    }

    //------------------------------------------------------------------------------
    // Test different types
    //------------------------------------------------------------------------------
    inline
    xtextfile::err UserTypes(xtextfile::stream& TextFile, const bool isRead, const xtextfile::flags Flags) noexcept
    {
        xtextfile::err                    Error;
        constexpr static std::array Types
        { xtextfile::user_defined_types{     "V3", "fff"  }
        , xtextfile::user_defined_types{   "BOOL",   "c"  }
        , xtextfile::user_defined_types{ "STRING",   "s"  }
        };

        std::string     Name        = { "Hello" };
        bool            isValid     = true;
        std::array      Position    = { 0.1f, 0.5f, 0.6f };

        if (TextFile.WriteComment(
            "----------------------------------------------------\n"
            " Example that shows how to use user types\n"
            "----------------------------------------------------\n"
        ).isError(Error)) return Error;

        //
        // Tell the file system about the user types
        //
        TextFile.AddUserTypes(Types);

        //
        // Save/Load a record
        //
        if (TextFile.Record(Error, "TestUserTypes"
            , [&](std::size_t, xtextfile::err& Error)
            {
                0
                || TextFile.Field(Types[0].m_CRC, "Position", Position[0], Position[1], Position[2]).isError(Error)
                || TextFile.Field(Types[1].m_CRC, "IsValid", isValid).isError(Error)
                || TextFile.Field(Types[2].m_CRC, "Name", Name ).isError(Error)
                ;
            }
        )) return Error;

        //
        // Save/Load a record
        //
        if (TextFile.Record(Error, "VariableUserTypes"
            , [&](std::size_t& C, xtextfile::err&)
            {
                if (isRead) assert(C == 3);
                else        C = 3;
            }
            , [&](std::size_t i, xtextfile::err& Error)
            {
                switch (i)
                {
                case 0: if (TextFile.Field(Types[0].m_CRC, "Value:?", Position[0], Position[1], Position[2]).isError(Error)) return; break;
                case 1: if (TextFile.Field(Types[1].m_CRC, "Value:?", isValid).isError(Error)) return; break;
                case 2: if (TextFile.Field(Types[2].m_CRC, "Value:?", Name ).isError(Error)) return; break;
                }

                TextFile.Field(Types[1].m_CRC, "IsValid", isValid).isError(Error);
            }
        )) return Error;

        return Error;
    }

    //------------------------------------------------------------------------------
    // Test different types
    //------------------------------------------------------------------------------
    inline
    xtextfile::err Test01(std::wstring_view FileName, bool isRead, xtextfile::file_type FileType, xtextfile::flags Flags) noexcept
    {
        xtextfile::stream   TextFile;

        //
        // Open File
        //
        if (auto Err = TextFile.Open(isRead, FileName, FileType, Flags); Err)
        {
            printf( "%s \n", Err.m_pMessage);
            return xtextfile::err::create_f<"Open file operation failed">();
        }

        //
        // Run tests
        // 
        xtextfile::err Error;

        if (AllTypes(TextFile, isRead, Flags).isError(Error)) return Error;

        if (SimpleVariableTypes(TextFile, isRead, Flags).isError(Error)) return Error;

        if (UserTypes(TextFile, isRead, Flags).isError(Error)) return Error;

        if (Properties(TextFile, isRead, Flags).isError(Error)) return Error;

        //
        // When we are reading and we are done dealing with records we can check if we are done reading a file like this
        //
        if (isRead && TextFile.isEOF())
            return {};

        return Error;
    }

    //-----------------------------------------------------------------------------------------

    void Test(void)
    {
        xtextfile::err Error;
        constexpr static auto FileName = L"./x64/TextFileTest.la";

        //
        // Test write and read (Text Style)
        //
        if (true) if (0
            || Test01(std::format(L"{}{}.txt", FileName, 1).c_str(), false, xtextfile::file_type::TEXT, {xtextfile::flags{}}).isError(Error)
            || Test01(std::format(L"{}{}.txt", FileName, 1).c_str(), true,  xtextfile::file_type::TEXT, { xtextfile::flags{} }).isError(Error)
            || Test01(std::format(L"{}{}.txt", FileName, 2).c_str(), false, xtextfile::file_type::TEXT, { .m_isWriteFloats = true }).isError(Error)
            || Test01(std::format(L"{}{}.txt", FileName, 2).c_str(), true,  xtextfile::file_type::TEXT, { .m_isWriteFloats = true }).isError(Error)
            )
        {
            assert(false);
        }

        //
        // Test write and read (Binary Style)
        //
        if (true) if (0
            || Test01(std::format(L"{}{}.bin", FileName, 1).c_str(), false, xtextfile::file_type::BINARY, { xtextfile::flags{} }).isError(Error)
            || Test01(std::format(L"{}{}.bin", FileName, 1).c_str(), true,  xtextfile::file_type::BINARY, { xtextfile::flags{} }).isError(Error)
            || Test01(std::format(L"{}{}.bin", FileName, 2).c_str(), false, xtextfile::file_type::BINARY, { .m_isWriteFloats = true }).isError(Error)
            || Test01(std::format(L"{}{}.bin", FileName, 2).c_str(), true,  xtextfile::file_type::BINARY, { .m_isWriteFloats = true }).isError(Error)
            )
        {
            assert(false);
        }
    }




}