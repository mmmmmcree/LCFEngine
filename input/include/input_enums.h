#pragma once

#include "enum_flags.h"

namespace lcf {
    enum class MouseButtonFlags
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
        eExtraButton24    = 0x04000000,
        eAllButtons       = 0x07ffffff,
        eMaxMouseButton   = eExtraButton24,
        // 4 high-order bits remain available for future use (0x08000000 through 0x40000000).
        eMouseButtonMask  = 0xffffffff
    };
    MAKE_ENUM_FLAGS(MouseButtonFlags);

    enum class KeyboardKey
    {
        // Unicode Basic Latin block (0x00-0x7f)
        eKeySpace = 0x20,
        eKeyAny = eKeySpace,
        eKeyExclam = 0x21,
        eKeyQuoteDbl = 0x22,
        eKeyNumberSign = 0x23,
        eKeyDollar = 0x24,
        eKeyPercent = 0x25,
        eKeyAmpersand = 0x26,
        eKeyApostrophe = 0x27,
        eKeyParenLeft = 0x28,
        eKeyParenRight = 0x29,
        eKeyAsterisk = 0x2a,
        eKeyPlus = 0x2b,
        eKeyComma = 0x2c,
        eKeyMinus = 0x2d,
        eKeyPeriod = 0x2e,
        eKeySlash = 0x2f,
        eKey0 = 0x30,
        eKey1 = 0x31,
        eKey2 = 0x32,
        eKey3 = 0x33,
        eKey4 = 0x34,
        eKey5 = 0x35,
        eKey6 = 0x36,
        eKey7 = 0x37,
        eKey8 = 0x38,
        eKey9 = 0x39,
        eKeyColon = 0x3a,
        eKeySemicolon = 0x3b,
        eKeyLess = 0x3c,
        eKeyEqual = 0x3d,
        eKeyGreater = 0x3e,
        eKeyQuestion = 0x3f,
        eKeyAt = 0x40,
        eKeyA = 0x41,
        eKeyB = 0x42,
        eKeyC = 0x43,
        eKeyD = 0x44,
        eKeyE = 0x45,
        eKeyF = 0x46,
        eKeyG = 0x47,
        eKeyH = 0x48,
        eKeyI = 0x49,
        eKeyJ = 0x4a,
        eKeyK = 0x4b,
        eKeyL = 0x4c,
        eKeyM = 0x4d,
        eKeyN = 0x4e,
        eKeyO = 0x4f,
        eKeyP = 0x50,
        eKeyQ = 0x51,
        eKeyR = 0x52,
        eKeyS = 0x53,
        eKeyT = 0x54,
        eKeyU = 0x55,
        eKeyV = 0x56,
        eKeyW = 0x57,
        eKeyX = 0x58,
        eKeyY = 0x59,
        eKeyZ = 0x5a,
        eKeyBracketLeft = 0x5b,
        eKeyBackslash = 0x5c,
        eKeyBracketRight = 0x5d,
        eKeyAsciiCircum = 0x5e,
        eKeyUnderscore = 0x5f,
        eKeyQuoteLeft = 0x60,
        eKeyBraceLeft = 0x7b,
        eKeyBar = 0x7c,
        eKeyBraceRight = 0x7d,
        eKeyAsciiTilde = 0x7e,

        // Unicode Latin-1 Supplement block (0x80-0xff)
        eKeynobreakspace = 0x0a0,
        eKeyexclamdown = 0x0a1,
        eKeycent = 0x0a2,
        eKeysterling = 0x0a3,
        eKeycurrency = 0x0a4,
        eKeyyen = 0x0a5,
        eKeybrokenbar = 0x0a6,
        eKeysection = 0x0a7,
        eKeydiaeresis = 0x0a8,
        eKeycopyright = 0x0a9,
        eKeyordfeminine = 0x0aa,
        eKeyguillemotleft = 0x0ab,        // left angle quotation mark
        eKeynotsign = 0x0ac,
        eKeyhyphen = 0x0ad,
        eKeyregistered = 0x0ae,
        eKeymacron = 0x0af,
        eKeydegree = 0x0b0,
        eKeyplusminus = 0x0b1,
        eKeytwosuperior = 0x0b2,
        eKeythreesuperior = 0x0b3,
        eKeyacute = 0x0b4,
        eKeymu = 0x0b5,
        eKeyparagraph = 0x0b6,
        eKeyperiodcentered = 0x0b7,
        eKeycedilla = 0x0b8,
        eKeyonesuperior = 0x0b9,
        eKeymasculine = 0x0ba,
        eKeyguillemotright = 0x0bb,        // right angle quotation mark
        eKeyonequarter = 0x0bc,
        eKeyonehalf = 0x0bd,
        eKeythreequarters = 0x0be,
        eKeyquestiondown = 0x0bf,
        eKeyAgrave = 0x0c0,
        eKeyAacute = 0x0c1,
        eKeyAcircumflex = 0x0c2,
        eKeyAtilde = 0x0c3,
        eKeyAdiaeresis = 0x0c4,
        eKeyAring = 0x0c5,
        eKeyAE = 0x0c6,
        eKeyCcedilla = 0x0c7,
        eKeyEgrave = 0x0c8,
        eKeyEacute = 0x0c9,
        eKeyEcircumflex = 0x0ca,
        eKeyEdiaeresis = 0x0cb,
        eKeyIgrave = 0x0cc,
        eKeyIacute = 0x0cd,
        eKeyIcircumflex = 0x0ce,
        eKeyIdiaeresis = 0x0cf,
        eKeyETH = 0x0d0,
        eKeyNtilde = 0x0d1,
        eKeyOgrave = 0x0d2,
        eKeyOacute = 0x0d3,
        eKeyOcircumflex = 0x0d4,
        eKeyOtilde = 0x0d5,
        eKeyOdiaeresis = 0x0d6,
        eKeymultiply = 0x0d7,
        eKeyOoblique = 0x0d8,
        eKeyUgrave = 0x0d9,
        eKeyUacute = 0x0da,
        eKeyUcircumflex = 0x0db,
        eKeyUdiaeresis = 0x0dc,
        eKeyYacute = 0x0dd,
        eKeyTHORN = 0x0de,
        eKeyssharp = 0x0df,
        eKeydivision = 0x0f7,
        eKeyydiaeresis = 0x0ff,

        // The rest of the Unicode values are skipped here,
        // so that we can represent them along with Qt::Keys
        // in the same data type. The maximum Unicode value
        // is 0x0010ffff, so we start our custom keys at
        // 0x01000000 to not clash with the Unicode values,
        // but still give plenty of room to grow.

        eKeyEscape = 0x01000000,                // misc keys
        eKeyTab = 0x01000001,
        eKeyBacktab = 0x01000002,
        eKeyBackspace = 0x01000003,
        eKeyReturn = 0x01000004,
        eKeyEnter = 0x01000005,
        eKeyInsert = 0x01000006,
        eKeyDelete = 0x01000007,
        eKeyPause = 0x01000008,
        eKeyPrint = 0x01000009,               // print screen
        eKeySysReq = 0x0100000a,
        eKeyClear = 0x0100000b,
        eKeyHome = 0x01000010,                // cursor movement
        eKeyEnd = 0x01000011,
        eKeyLeft = 0x01000012,
        eKeyUp = 0x01000013,
        eKeyRight = 0x01000014,
        eKeyDown = 0x01000015,
        eKeyPageUp = 0x01000016,
        eKeyPageDown = 0x01000017,
        eKeyShift = 0x01000020,                // modifiers
        eKeyControl = 0x01000021,
        eKeyMeta = 0x01000022,
        eKeyAlt = 0x01000023,
        eKeyCapsLock = 0x01000024,
        eKeyNumLock = 0x01000025,
        eKeyScrollLock = 0x01000026,
        eKeyF1 = 0x01000030,                // function keys
        eKeyF2 = 0x01000031,
        eKeyF3 = 0x01000032,
        eKeyF4 = 0x01000033,
        eKeyF5 = 0x01000034,
        eKeyF6 = 0x01000035,
        eKeyF7 = 0x01000036,
        eKeyF8 = 0x01000037,
        eKeyF9 = 0x01000038,
        eKeyF10 = 0x01000039,
        eKeyF11 = 0x0100003a,
        eKeyF12 = 0x0100003b,
        eKeyF13 = 0x0100003c,
        eKeyF14 = 0x0100003d,
        eKeyF15 = 0x0100003e,
        eKeyF16 = 0x0100003f,
        eKeyF17 = 0x01000040,
        eKeyF18 = 0x01000041,
        eKeyF19 = 0x01000042,
        eKeyF20 = 0x01000043,
        eKeyF21 = 0x01000044,
        eKeyF22 = 0x01000045,
        eKeyF23 = 0x01000046,
        eKeyF24 = 0x01000047,
        eKeyF25 = 0x01000048,                // F25 .. F35 only on X11
        eKeyF26 = 0x01000049,
        eKeyF27 = 0x0100004a,
        eKeyF28 = 0x0100004b,
        eKeyF29 = 0x0100004c,
        eKeyF30 = 0x0100004d,
        eKeyF31 = 0x0100004e,
        eKeyF32 = 0x0100004f,
        eKeyF33 = 0x01000050,
        eKeyF34 = 0x01000051,
        eKeyF35 = 0x01000052,
        eKeySuper_L = 0x01000053,                 // extra keys
        eKeySuper_R = 0x01000054,
        eKeyMenu = 0x01000055,
        eKeyHyper_L = 0x01000056,
        eKeyHyper_R = 0x01000057,
        eKeyHelp = 0x01000058,
        eKeyDirection_L = 0x01000059,
        eKeyDirection_R = 0x01000060,

        // International input method support (X keycode - 0xEE00, the
        // definition follows Qt/Embedded 2.3.7) Only interesting if
        // you are writing your own input method

        // International & multi-key character composition
        eKeyAltGr               = 0x01001103,
        eKeyMulti_key           = 0x01001120,  // Multi-key character compose
        eKeyCodeinput           = 0x01001137,
        eKeySingleCandidate     = 0x0100113c,
        eKeyMultipleCandidate   = 0x0100113d,
        eKeyPreviousCandidate   = 0x0100113e,

        // Misc Functions
        eKeyMode_switch         = 0x0100117e,  // Character set switch
        //eKeyscript_switch       = 0x0100117e,  // Alias for mode_switch

        // Japanese keyboard support
        eKeyKanji               = 0x01001121,  // Kanji, Kanji convert
        eKeyMuhenkan            = 0x01001122,  // Cancel Conversion
        //eKeyHenkan_Mode         = 0x01001123,  // Start/Stop Conversion
        eKeyHenkan              = 0x01001123,  // Alias for Henkan_Mode
        eKeyRomaji              = 0x01001124,  // to Romaji
        eKeyHiragana            = 0x01001125,  // to Hiragana
        eKeyKatakana            = 0x01001126,  // to Katakana
        eKeyHiragana_Katakana   = 0x01001127,  // Hiragana/Katakana toggle
        eKeyZenkaku             = 0x01001128,  // to Zenkaku
        eKeyHankaku             = 0x01001129,  // to Hankaku
        eKeyZenkaku_Hankaku     = 0x0100112a,  // Zenkaku/Hankaku toggle
        eKeyTouroku             = 0x0100112b,  // Add to Dictionary
        eKeyMassyo              = 0x0100112c,  // Delete from Dictionary
        eKeyKana_Lock           = 0x0100112d,  // Kana Lock
        eKeyKana_Shift          = 0x0100112e,  // Kana Shift
        eKeyEisu_Shift          = 0x0100112f,  // Alphanumeric Shift
        eKeyEisu_toggle         = 0x01001130,  // Alphanumeric toggle
        //eKeyKanji_Bangou        = 0x01001137,  // Codeinput
        //eKeyZen_Koho            = 0x0100113d,  // Multiple/All Candidate(s)
        //eKeyMae_Koho            = 0x0100113e,  // Previous Candidate

        // Korean keyboard support
        //
        // In fact, many Korean users need only 2 keys, eKeyHangul and
        // eKeyHangul_Hanja. But rest of the keys are good for future.

        eKeyHangul              = 0x01001131,  // Hangul start/stop(toggle)
        eKeyHangul_Start        = 0x01001132,  // Hangul start
        eKeyHangul_End          = 0x01001133,  // Hangul end, English start
        eKeyHangul_Hanja        = 0x01001134,  // Start Hangul->Hanja Conversion
        eKeyHangul_Jamo         = 0x01001135,  // Hangul Jamo mode
        eKeyHangul_Romaja       = 0x01001136,  // Hangul Romaja mode
        //eKeyHangul_Codeinput    = 0x01001137,  // Hangul code input mode
        eKeyHangul_Jeonja       = 0x01001138,  // Jeonja mode
        eKeyHangul_Banja        = 0x01001139,  // Banja mode
        eKeyHangul_PreHanja     = 0x0100113a,  // Pre Hanja conversion
        eKeyHangul_PostHanja    = 0x0100113b,  // Post Hanja conversion
        //eKeyHangul_SingleCandidate   = 0x0100113c,  // Single candidate
        //eKeyHangul_MultipleCandidate = 0x0100113d,  // Multiple candidate
        //eKeyHangul_PreviousCandidate = 0x0100113e,  // Previous candidate
        eKeyHangul_Special      = 0x0100113f,  // Special symbols
        //eKeyHangul_switch       = 0x0100117e,  // Alias for mode_switch

        // dead keys (X keycode - 0xED00 to avoid the conflict)
        eKeyDead_Grave          = 0x01001250,
        eKeyDead_Acute          = 0x01001251,
        eKeyDead_Circumflex     = 0x01001252,
        eKeyDead_Tilde          = 0x01001253,
        eKeyDead_Macron         = 0x01001254,
        eKeyDead_Breve          = 0x01001255,
        eKeyDead_Abovedot       = 0x01001256,
        eKeyDead_Diaeresis      = 0x01001257,
        eKeyDead_Abovering      = 0x01001258,
        eKeyDead_Doubleacute    = 0x01001259,
        eKeyDead_Caron          = 0x0100125a,
        eKeyDead_Cedilla        = 0x0100125b,
        eKeyDead_Ogonek         = 0x0100125c,
        eKeyDead_Iota           = 0x0100125d,
        eKeyDead_Voiced_Sound   = 0x0100125e,
        eKeyDead_Semivoiced_Sound = 0x0100125f,
        eKeyDead_Belowdot       = 0x01001260,
        eKeyDead_Hook           = 0x01001261,
        eKeyDead_Horn           = 0x01001262,
        eKeyDead_Stroke         = 0x01001263,
        eKeyDead_Abovecomma     = 0x01001264,
        eKeyDead_Abovereversedcomma = 0x01001265,
        eKeyDead_Doublegrave    = 0x01001266,
        eKeyDead_Belowring      = 0x01001267,
        eKeyDead_Belowmacron    = 0x01001268,
        eKeyDead_Belowcircumflex = 0x01001269,
        eKeyDead_Belowtilde     = 0x0100126a,
        eKeyDead_Belowbreve     = 0x0100126b,
        eKeyDead_Belowdiaeresis = 0x0100126c,
        eKeyDead_Invertedbreve  = 0x0100126d,
        eKeyDead_Belowcomma     = 0x0100126e,
        eKeyDead_Currency       = 0x0100126f,
        eKeyDead_a              = 0x01001280,
        eKeyDead_A              = 0x01001281,
        eKeyDead_e              = 0x01001282,
        eKeyDead_E              = 0x01001283,
        eKeyDead_i              = 0x01001284,
        eKeyDead_I              = 0x01001285,
        eKeyDead_o              = 0x01001286,
        eKeyDead_O              = 0x01001287,
        eKeyDead_u              = 0x01001288,
        eKeyDead_U              = 0x01001289,
        eKeyDead_Small_Schwa    = 0x0100128a,
        eKeyDead_Capital_Schwa  = 0x0100128b,
        eKeyDead_Greek          = 0x0100128c,
        eKeyDead_Lowline        = 0x01001290,
        eKeyDead_Aboveverticalline = 0x01001291,
        eKeyDead_Belowverticalline = 0x01001292,
        eKeyDead_Longsolidusoverlay = 0x01001293,

        // multimedia/internet keys - ignored by default - see QKeyEvent c'tor
        eKeyBack  = 0x01000061,
        eKeyForward  = 0x01000062,
        eKeyStop  = 0x01000063,
        eKeyRefresh  = 0x01000064,
        eKeyVolumeDown = 0x01000070,
        eKeyVolumeMute  = 0x01000071,
        eKeyVolumeUp = 0x01000072,
        eKeyBassBoost = 0x01000073,
        eKeyBassUp = 0x01000074,
        eKeyBassDown = 0x01000075,
        eKeyTrebleUp = 0x01000076,
        eKeyTrebleDown = 0x01000077,
        eKeyMediaPlay  = 0x01000080,
        eKeyMediaStop  = 0x01000081,
        eKeyMediaPrevious  = 0x01000082,
        eKeyMediaNext  = 0x01000083,
        eKeyMediaRecord = 0x01000084,
        eKeyMediaPause = 0x01000085,
        eKeyMediaTogglePlayPause = 0x01000086,
        eKeyHomePage  = 0x01000090,
        eKeyFavorites  = 0x01000091,
        eKeySearch  = 0x01000092,
        eKeyStandby = 0x01000093,
        eKeyOpenUrl = 0x01000094,
        eKeyLaunchMail  = 0x010000a0,
        eKeyLaunchMedia = 0x010000a1,
        eKeyLaunch0  = 0x010000a2,
        eKeyLaunch1  = 0x010000a3,
        eKeyLaunch2  = 0x010000a4,
        eKeyLaunch3  = 0x010000a5,
        eKeyLaunch4  = 0x010000a6,
        eKeyLaunch5  = 0x010000a7,
        eKeyLaunch6  = 0x010000a8,
        eKeyLaunch7  = 0x010000a9,
        eKeyLaunch8  = 0x010000aa,
        eKeyLaunch9  = 0x010000ab,
        eKeyLaunchA  = 0x010000ac,
        eKeyLaunchB  = 0x010000ad,
        eKeyLaunchC  = 0x010000ae,
        eKeyLaunchD  = 0x010000af,
        eKeyLaunchE  = 0x010000b0,
        eKeyLaunchF  = 0x010000b1,
        eKeyMonBrightnessUp = 0x010000b2,
        eKeyMonBrightnessDown = 0x010000b3,
        eKeyKeyboardLightOnOff = 0x010000b4,
        eKeyKeyboardBrightnessUp = 0x010000b5,
        eKeyKeyboardBrightnessDown = 0x010000b6,
        eKeyPowerOff = 0x010000b7,
        eKeyWakeUp = 0x010000b8,
        eKeyEject = 0x010000b9,
        eKeyScreenSaver = 0x010000ba,
        eKeyWWW = 0x010000bb,
        eKeyMemo = 0x010000bc,
        eKeyLightBulb = 0x010000bd,
        eKeyShop = 0x010000be,
        eKeyHistory = 0x010000bf,
        eKeyAddFavorite = 0x010000c0,
        eKeyHotLinks = 0x010000c1,
        eKeyBrightnessAdjust = 0x010000c2,
        eKeyFinance = 0x010000c3,
        eKeyCommunity = 0x010000c4,
        eKeyAudioRewind = 0x010000c5, // Media rewind
        eKeyBackForward = 0x010000c6,
        eKeyApplicationLeft = 0x010000c7,
        eKeyApplicationRight = 0x010000c8,
        eKeyBook = 0x010000c9,
        eKeyCD = 0x010000ca,
        eKeyCalculator = 0x010000cb,
        eKeyToDoList = 0x010000cc,
        eKeyClearGrab = 0x010000cd,
        eKeyClose = 0x010000ce,
        eKeyCopy = 0x010000cf,
        eKeyCut = 0x010000d0,
        eKeyDisplay = 0x010000d1, // Output switch key
        eKeyDOS = 0x010000d2,
        eKeyDocuments = 0x010000d3,
        eKeyExcel = 0x010000d4,
        eKeyExplorer = 0x010000d5,
        eKeyGame = 0x010000d6,
        eKeyGo = 0x010000d7,
        eKeyiTouch = 0x010000d8,
        eKeyLogOff = 0x010000d9,
        eKeyMarket = 0x010000da,
        eKeyMeeting = 0x010000db,
        eKeyMenuKB = 0x010000dc,
        eKeyMenuPB = 0x010000dd,
        eKeyMySites = 0x010000de,
        eKeyNews = 0x010000df,
        eKeyOfficeHome = 0x010000e0,
        eKeyOption = 0x010000e1,
        eKeyPaste = 0x010000e2,
        eKeyPhone = 0x010000e3,
        eKeyCalendar = 0x010000e4,
        eKeyReply = 0x010000e5,
        eKeyReload = 0x010000e6,
        eKeyRotateWindows = 0x010000e7,
        eKeyRotationPB = 0x010000e8,
        eKeyRotationKB = 0x010000e9,
        eKeySave = 0x010000ea,
        eKeySend = 0x010000eb,
        eKeySpell = 0x010000ec,
        eKeySplitScreen = 0x010000ed,
        eKeySupport = 0x010000ee,
        eKeyTaskPane = 0x010000ef,
        eKeyTerminal = 0x010000f0,
        eKeyTools = 0x010000f1,
        eKeyTravel = 0x010000f2,
        eKeyVideo = 0x010000f3,
        eKeyWord = 0x010000f4,
        eKeyXfer = 0x010000f5,
        eKeyZoomIn = 0x010000f6,
        eKeyZoomOut = 0x010000f7,
        eKeyAway = 0x010000f8,
        eKeyMessenger = 0x010000f9,
        eKeyWebCam = 0x010000fa,
        eKeyMailForward = 0x010000fb,
        eKeyPictures = 0x010000fc,
        eKeyMusic = 0x010000fd,
        eKeyBattery = 0x010000fe,
        eKeyBluetooth = 0x010000ff,
        eKeyWLAN = 0x01000100,
        eKeyUWB = 0x01000101,
        eKeyAudioForward = 0x01000102, // Media fast-forward
        eKeyAudioRepeat = 0x01000103, // Toggle repeat mode
        eKeyAudioRandomPlay = 0x01000104, // Toggle shuffle mode
        eKeySubtitle = 0x01000105,
        eKeyAudioCycleTrack = 0x01000106,
        eKeyTime = 0x01000107,
        eKeyHibernate = 0x01000108,
        eKeyView = 0x01000109,
        eKeyTopMenu = 0x0100010a,
        eKeyPowerDown = 0x0100010b,
        eKeySuspend = 0x0100010c,
        eKeyContrastAdjust = 0x0100010d,

        // We can remove these two for Qt 7:
        eKeyLaunchG  = 0x0100010e,
        eKeyLaunchH  = 0x0100010f,

        eKeyTouchpadToggle = 0x01000110,
        eKeyTouchpadOn = 0x01000111,
        eKeyTouchpadOff = 0x01000112,

        eKeyMicMute = 0x01000113,

        eKeyRed = 0x01000114,
        eKeyGreen = 0x01000115,
        eKeyYellow = 0x01000116,
        eKeyBlue = 0x01000117,

        eKeyChannelUp = 0x01000118,
        eKeyChannelDown = 0x01000119,

        eKeyGuide    = 0x0100011a,
        eKeyInfo     = 0x0100011b,
        eKeySettings = 0x0100011c,

        eKeyMicVolumeUp   = 0x0100011d,
        eKeyMicVolumeDown = 0x0100011e,

        eKeyNew      = 0x01000120,
        eKeyOpen     = 0x01000121,
        eKeyFind     = 0x01000122,
        eKeyUndo     = 0x01000123,
        eKeyRedo     = 0x01000124,

        eKeyMediaLast = 0x0100ffff,

        // Keypad navigation keys
        eKeySelect = 0x01010000,
        eKeyYes = 0x01010001,
        eKeyNo = 0x01010002,

        // Newer misc keys
        eKeyCancel  = 0x01020001,
        eKeyPrinter = 0x01020002,
        eKeyExecute = 0x01020003,
        eKeySleep   = 0x01020004,
        eKeyPlay    = 0x01020005, // Not the same as eKeyMediaPlay
        eKeyZoom    = 0x01020006,
        //eKeyJisho   = 0x01020007, // IME: Dictionary key
        //eKeyOyayubi_Left = 0x01020008, // IME: Left Oyayubi key
        //eKeyOyayubi_Right = 0x01020009, // IME: Right Oyayubi key
        eKeyExit    = 0x0102000a,

        // Device keys
        eKeyContext1 = 0x01100000,
        eKeyContext2 = 0x01100001,
        eKeyContext3 = 0x01100002,
        eKeyContext4 = 0x01100003,
        eKeyCall = 0x01100004,      // set absolute state to in a call (do not toggle state)
        eKeyHangup = 0x01100005,    // set absolute state to hang up (do not toggle state)
        eKeyFlip = 0x01100006,
        eKeyToggleCallHangup = 0x01100007, // a toggle key for answering, or hanging up, based on current call state
        eKeyVoiceDial = 0x01100008,
        eKeyLastNumberRedial = 0x01100009,

        eKeyCamera = 0x01100020,
        eKeyCameraFocus = 0x01100021,

        // WARNING: Do not add any keys in the range 0x01200000 to 0xffffffff,
        // as those bits are reserved for the Qt::KeyboardModifier enum below.

        eKeyUnknown = 0x01ffffff
    };
}