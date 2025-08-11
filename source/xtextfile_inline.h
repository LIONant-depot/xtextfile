namespace xtextfile
{
    //------------------------------------------------------------------------------------------------
    namespace details
    {
        // Primary template
        template <typename T>
        struct function_traits;

        // Specialization for free functions
        template <typename R, typename... Args>
        struct function_traits<R(Args...)>
        {
            inline static constexpr std::size_t arity_v = sizeof...(Args);
        };

        // Specialization for function pointers
        template <typename R, typename... Args>
        struct function_traits<R(*)(Args...)> : function_traits<R(Args...)> {};

        // Specialization for member functions
        template <typename R, typename C, typename... Args>
        struct function_traits<R(C::*)(Args...)> : function_traits<R(Args...)> {};

        template <typename R, typename C, typename... Args>
        struct function_traits<R(C::*)(Args...) noexcept> : function_traits<R(Args...)> {};

        // Specialization for const member functions (default for lambdas)
        template <typename R, typename C, typename... Args>
        struct function_traits<R(C::*)(Args...) const> : function_traits<R(Args...)> {};

        template <typename R, typename C, typename... Args>
        struct function_traits<R(C::*)(Args...) const noexcept> : function_traits<R(Args...)> {};

        // Helper function to get arity (pass lambda by const reference)
        template <typename F>
        inline static constexpr std::size_t arg_count_v = function_traits<decltype(&F::operator())>::arity_v;

        //------------------------------------------------------------------------------------------------

        template< typename T >
        constexpr static bool is_valid_type_v = false
            || std::is_same_v< bool, T >
            || std::is_same_v< std::uint8_t, T >
            || std::is_same_v< std::uint16_t, T >
            || std::is_same_v< std::uint32_t, T >
            || std::is_same_v< std::uint64_t, T >
            || std::is_same_v< std::int16_t, T >
            || std::is_same_v< std::int32_t, T >
            || std::is_same_v< std::int64_t, T >
            || std::is_same_v< std::int8_t, T >
            || std::is_same_v< float, T >
            || std::is_same_v< double, T >
            || std::is_same_v< std::string, T >
            || std::is_same_v< std::wstring, T >;
    }

    //------------------------------------------------------------------------------------------------

    template< auto N1, auto N2 >
    constexpr user_defined_types::user_defined_types(const char(&Name)[N1], const char(&Types)[N2]) noexcept
    {
        m_CRC = crc32::computeFromString(Name);
        for (int i = 0; (m_Name[i]        = Name[i]);  ++i ){}
        for (int i = 0; (m_SystemTypes[i] = Types[i]); ++i ){}
        m_NameLength   = N1;
        m_nSystemTypes = N2;

    }

    //------------------------------------------------------------------------------------------------

    constexpr user_defined_types::user_defined_types(const char* pName, const char* pTypes) noexcept
    {
        m_CRC = crc32::computeFromString(pName);

        int i;
        for (i = 0; (m_Name[i]        = pName[i]);  ++i ){}
        m_NameLength = i;
        for (i = 0; (m_SystemTypes[i] = pTypes[i]); ++i ){}
        m_nSystemTypes = i;
    }

    //------------------------------------------------------------------------------------------------

    template< std::size_t N, typename... T_ARGS > inline
    xerr stream::Field( crc32 UserType, const char(&pFieldName)[N], T_ARGS&... Args) noexcept
    {
        static_assert((details::is_valid_type_v<T_ARGS> || ...));
        details::arglist::out Out{ &Args... };
        return isReading()
            ? ReadColumn (UserType, pFieldName, Out)
            : WriteColumn(UserType, pFieldName, Out);
    }

    //------------------------------------------------------------------------------------------------

    template< std::size_t N, typename... T_ARGS > inline
    xerr stream::Field(const char(&pFieldName)[N], T_ARGS&... Args) noexcept
    {
        crc32 UserType = crc32{ 0 };
        return Field(UserType, pFieldName, Args...);
    }

    //------------------------------------------------------------------------------------------------

    template< std::size_t N, typename TT, typename T > inline
    xerr stream::Record( const char(&Str)[N], TT&& RecordStar, T&& Callback) noexcept
    {
        xerr Error;
        if (m_File.m_States.m_isReading)
        {
            if (std::strcmp(getRecordName().data(), Str) != 0)
            {
                return xerr::create< xtextfile::state::UNEXPECTED_RECORD, "Unexpected record" >();
            }
            std::size_t Count = getRecordCount();
            RecordStar(Count, Error);
            if (Error) return Error;

            for (std::remove_const_t<decltype(Count)> i = 0; i < Count; i++)
            {
                if (Error = ReadLine(); Error) return Error;
                if constexpr (details::arg_count_v<T> == 2) 
                {
                    Callback(i, Error);
                }
                else
                {
                    static_assert(details::arg_count_v<T> == 1);
                    Callback(Error);
                }
                if (Error) return Error;
            }

            // Read the next record
            if (Error = ReadRecord(); Error )
            {
                if (Error.getState<state>() == state::UNEXPECTED_EOF) 
                {
                    Error.clear();
                }
                else
                {
                    return Error;
                }
            }
        }
        else
        {
            std::size_t Count;
            RecordStar(Count, Error);

            if (Error) return Error;

            if (Error = WriteRecord(Str, Count); Error) 
                return Error;

            if (Count == ~0) Count = 1;
            for (std::remove_const_t<decltype(Count)> i = 0; i < Count; i++)
            {
                if constexpr (details::arg_count_v<T> == 2)
                {
                    Callback(i, Error);
                }
                else
                {
                    static_assert(details::arg_count_v<T> == 1);
                    Callback(Error);
                }

                if (Error) return Error;

                if (Error = WriteLine(); Error ) 
                    return Error;
            }
        }

        return {};
    }

    //------------------------------------------------------------------------------------------------

    template< std::size_t N, typename T > inline
    xerr stream::Record( const char(&Str)[N], T&& Callback) noexcept
    {
        return Record
        ( Str,
        [&](std::size_t& C, xerr& Error) noexcept
        {
            if (m_File.m_States.m_isReading)
            {
                assert(C == 1);
            }
            else
            {
                C = ~0;
            }
        }
        , [&](std::size_t, xerr& Error) constexpr noexcept
        {
            if constexpr (details::arg_count_v<T> == 1)
            {
                Callback(Error);
            }
            else
            {
                static_assert(details::arg_count_v<T> == 2);
                Callback(0, Error);
            }
        });
    }


    //------------------------------------------------------------------------------------------------

    template< std::size_t N > inline
    xerr stream::RecordLabel(const char(&Str)[N]) noexcept
    {
        if (m_File.m_States.m_isReading)
        {
            if (getRecordName() != Str)
                return { state::UNEXPECTED_RECORD, "Unexpected record" };

            return ReadRecord();
        }
        else
        {
            return WriteRecord(Str, -1);
        }
    }
}