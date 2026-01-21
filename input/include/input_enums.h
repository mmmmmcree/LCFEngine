#pragma once

#include "enums/enum_flags.h"

namespace lcf {
    enum class MouseButtonFlags : uint32_t
    {
        eNoButton         = 0x00000000,
        eLeftButton       = 0x00000001,
        eRightButton      = 0x00000002,
        eMiddleButton     = 0x00000004,
        eBackButton       = 0x00000008,
        eXButton1         = eBackButton,
        eExtraButton1     = eXButton1,
        eForwardButton    = 0x00000010,
        eXButton2         = eForwardButton,
        eExtraButton2     = eForwardButton,
        eTaskButton       = 0x00000020,
        eExtraButton3     = eTaskButton,
        eExtraButton4     = 0x00000040,
        eExtraButton5     = 0x00000080,
        eExtraButton6     = 0x00000100,
        eExtraButton7     = 0x00000200,
        eExtraButton8     = 0x00000400,
        eExtraButton9     = 0x00000800,
        eExtraButton10    = 0x00001000,
        eExtraButton11    = 0x00002000,
        eExtraButton12    = 0x00004000,
        eExtraButton13    = 0x00008000,
        eExtraButton14    = 0x00010000,
        eExtraButton15    = 0x00020000,
        eExtraButton16    = 0x00040000,
        eExtraButton17    = 0x00080000,
        eExtraButton18    = 0x00100000,
        eExtraButton19    = 0x00200000,
        eExtraButton20    = 0x00400000,
        eExtraButton21    = 0x00800000,
        eExtraButton22    = 0x01000000,
        eExtraButton23    = 0x02000000,
        eDoubleClicked       = 0x04000000,
        eAllButtons       = 0x07ffffff,
    };
    template <> struct is_enum_flags<MouseButtonFlags> : std::true_type {};

    enum class KeyboardKey : uint8_t
    {
        eUnknown,
        // 字母键
        eA,
        eB,
        eC,
        eD,
        eE,
        eF,
        eG,
        eH,
        eI,
        eJ,
        eK,
        eL,
        eM,
        eN,
        eO,
        eP,
        eQ,
        eR,
        eS,
        eT,
        eU,
        eV,
        eW,
        eX,
        eY,
        eZ,

        // 数字键
        e0,
        e1,
        e2,
        e3,
        e4,
        e5,
        e6,
        e7,
        e8,
        e9,

        // 功能键
        eF1,
        eF2,
        eF3,
        eF4,
        eF5,
        eF6,
        eF7,
        eF8,
        eF9,
        eF10,
        eF11,
        eF12,

        // 方向键
        eLeft,
        eUp,
        eRight,
        eDown,

        // 修饰键
        eShift,
        eControl,
        eAlt,
        eSuper,        // Win/Command 键
        eCapsLock,
        eTab,

        // 常用编辑键
        eSpace,
        eEnter,
        eBackspace,
        eDelete,
        eEscape,
        eInsert,
        eHome,
        eEnd,
        ePageUp,
        ePageDown,

        // 标点符号
        eMinus,         // -
        eEqual,         // =
        eBracketLeft,   // [
        eBracketRight,  // ]
        eBackslash,     /* \ */
        eSemicolon,     // ;
        eApostrophe,    // '
        eComma,         // ,
        ePeriod,        // .
        eSlash,         // /
        eGrave,         // `

    };
}